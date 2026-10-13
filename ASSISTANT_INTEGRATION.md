# ATT&CK assistant — desktop console

The **Assistant** screen puts an operator in front of the `attack_rag`
Django backend: they ask about an alert, the backend retrieves matching
MITRE ATT&CK entries and an LLM answers from them.

The console speaks the backend's **OpenAI-compatible** surface —
`OPENAI_API_DOCUMENTATION.md`, `POST /v1/chat/completions`. The native
`/api/attack/chat/` endpoints (`API_DOCUMENTATION.md`) are kept in this
directory for reference but are no longer used; §8 records what moving
between them cost.

## 1. Where it sits

The assistant is **not on the telemetry path**. Nothing a board publishes
reaches the backend on its own — an operator has to put it in a question.

```
boards ──MQTT──► broker ──MQTT──► console ──┐
                                            │  (operator types / clicks
                                            │   "Ask the ATT&CK assistant")
                                            ▼
                                   HTTP ──► attack_rag ──► Ollama
                                                  └──► ATT&CK vector store
```

Two independent connections, two independent failure modes. The broker
being down does not stop the assistant, and vice versa; the status bar and
the Assistant header report them separately.

## 2. What is already in place

```
AppController
  ├─ ChatClient        (Assistant screen)   ctor → QNetworkAccessManager
  └─ AlertExplainer                              → its own ChatClient
       └─ both share BackendSettings (base URL, bearer key, generation slot)
```

```
ChatClient::ask(question)
    openAiMessages(6) ─┐            read *before* the new turn is appended
                       ├─► POST /v1/chat/completions
    + {role:user, …}   │            Authorization: Bearer …
    appendUser         │            Accept: text/event-stream
    appendAssistantPlaceholder      setTransferTimeout(0)
                                              │
                                              ▼
  readyRead ─► buffer ─► split on "\n\n" ─► handleFrame()
                                              ├─ data: {choices[0].delta.content}
                                              │     └─► applyDelta ─► publishAnswer
                                              │            ├─ body  → ChatModel::setAnswerText
                                              │            └─ footer→ ChatModel::setSources
                                              ├─ choices[0].finish_reason  → stop
                                              └─ data: [DONE]
  finished  ─► pre-stream 400/401/502, `[error: …]` in the body, or a
               stream that stopped with neither stop nor [DONE]
```

QML binds to `App.chat.*` and to `App.chat.transcript`. Nothing in QML
parses SSE.

**Non-obvious things the implementation does**, each because the doc or a
test run demanded it:

- **The `**Sources:**` footer is parsed back out of the answer.** This
  surface has no `sources` event — §3.1 puts the retrieved items in a
  markdown footer delivered as ordinary content deltas. `splitSourcesFooter`
  removes it from the visible body (including a *partially received* marker,
  so "**Sou" never flashes on screen) and `parseSourcesFooter` turns it into
  the same `{id, url}` shape the UI already renders. `type` and `score` do
  not exist here and are not invented — the chips show ids and the tooltip
  shows the URL.
- **`answerTextChanged` carries the display text**, separately from
  `tokenReceived`, which carries raw deltas. Anything rendering the answer
  binds to the former; a consumer that concatenates raw deltas ends up
  showing the markdown footer, which is a bug this signal exists to prevent.
- **`[error: …]` in the body is treated as a failure**, not as prose. §5
  says a mid-stream failure arrives as a content delta on an otherwise 200
  stream, so that marker is the only signal that the text on screen is a
  failure. It is reported as the turn's error and stripped from the body,
  rather than being shown twice.
- **A stream is finished only on `finish_reason` or `[DONE]`.** Neither
  arriving means the connection stopped early, which is a failure.

- **`setTransferTimeout(0)`** (§6.7). An SSE stream is idle between tokens
  by design, which is exactly what a transfer timeout is watching for.
  Verified against an 8-second silent stream.
- **Frames are split on `\n\n` after normalizing CRLF.** The SSE spec
  allows either ending; the backend's example trace uses bare LF. A raw CR
  inside `data` cannot occur — JSON escapes it — so normalizing is safe.
- **A trailing frame with no blank line is flushed on `finished`**, rather
  than dropping the last token of an answer that ends abruptly.
- **A 200 that ends without `done` or `error` is a failure.** The frames
  parsed fine and then the connection stopped: the backend died, the model
  was unloaded, or a proxy timed the stream out. The partial answer stays
  on screen, marked incomplete. Presenting it as finished would be the
  worst possible outcome for a security tool.
- **A `done` with no tokens is a failure too.** Generation produced
  nothing; the transcript says so instead of showing a blank answer.
- **History excludes failed and in-flight turns.** Feeding a truncated
  answer back as context poisons the next question.
- **One request in flight at a time** — the backend serializes on a single
  GPU. Enforced in `ask()` against a slot held in `BackendSettings`, so it
  holds across *both* clients — the Assistant screen and the right-click
  panel — not just within one.
- **No `system` message is sent.** §4 says the server ignores them and its
  own grounding prompt always governs; sending one would imply an override
  that does not exist.
- **Sampling fields are omitted, not sent-and-ignored** (§7). Sending
  `temperature` would suggest the console controls generation when it does
  not.

## 3. Point it at a backend

**Integrations → Backend**. One host, two APIs, and — importantly — **two
auth schemes**:

| Field | Meaning |
|---|---|
| Base URL | `http://<backend-host>:8000` — scheme optional, it is added |
| SIEM token | DRF token, sent to `/api/siem/` as `Authorization: Token …` |
| Chat API key | sent to `/v1/` as `Authorization: Bearer …` |
| Model name | `attack-rag` by default; cosmetic (§7) but configurable server-side |
| Chunks (k) | retrieval breadth, default 6 (§3.1 extension) |
| Stream tokens | on = `stream:true`, off = the blocking form of the same endpoint |
| **Test** | `GET /v1/models` (§3.2) |

The chat key can be left empty: §2 says the server also accepts a valid
backend token as the bearer key, so the SIEM token is used as a fallback and
the screen says so. Set an `sk-attack-…` key when one has been issued.

**Press Test before anything else when something is wrong.** `/v1/models`
separates "wrong URL" from "wrong key" without spending a generation to find
out, and it reports the model the server actually serves — a name that
doesn't match usually means the URL points somewhere unexpected.

Settings persist in `QSettings`, so this is a one-time step.

**Turn streaming off** if tokens arrive all at once at the end: that means
the backend is served by WSGI rather than ASGI (§6.7), and the blocking
endpoint is the honest way to present the same behaviour.

> **How tokens are issued is a backend question.** The doc says token auth
> is assumed and that the scheme is set in the Django project's
> `DEFAULT_AUTHENTICATION_CLASSES` — typically `POST /api/auth/token/` with
> username and password returns `{"token": "..."}`. Ask whoever runs the
> backend. The console does not implement a login flow; it takes the token
> it is given.

## 4. Using it

Three ways in, for three different questions:

| Gesture | What it does |
|---|---|
| **Assistant** in the sidebar | A full conversation. Multi-turn, keeps history. |
| **Ask the ATT&CK assistant** in the Detections inspector | Drafts a question from the selected row into the composer, unsent, so it can be edited. |
| **Right-click a row** in Detections or Events | Asks immediately and streams a short answer into a panel beside the table. |

### The right-click panel

The fastest path: right-click an alert, get a few sentences about it without
leaving the table. It has its **own** `ChatClient` (see `AlertExplainer`), for
two reasons that both come down to not surprising the operator:

- A popup answer is one throwaway question about one row. Pushing it into the
  Assistant transcript would rewrite a conversation they are in the middle of
  and — because the backend keeps the last 6 turns as context — silently steer
  their next question too.
- The popup sends **no `history`**, so each explanation is grounded only in
  the alert it is about.

Right-clicking a different row replaces the answer, cancelling the previous
request rather than holding the backend's generation slot for a panel nobody
is looking at. **Open in Assistant** hands the same question to the full
screen for follow-up. Clicking away dismisses it, and the click still reaches
the row underneath, so dismissing and selecting are one gesture.

The panel applies the same citation rule as the Assistant screen: a `[Txxxx]`
becomes a link only if the backend actually retrieved it.

### One generation at a time, across both

§6.7 says the backend serializes on a single GPU. That constraint now lives in
`BackendSettings` as a generation slot rather than inside one client, because
two things can ask. Whoever holds it finishes first; the other is told why
instead of being silently queued behind a request it cannot see.

### Everything else

- The inspector button **drafts** a question from the selected detection —
  board, class label, class index, confidence, board timestamp — and drops
  it in the composer. It is not sent until the operator presses Send, so
  they can edit it first.
- The draft states plainly that the class name is *the model's own label*.
  It is not an ATT&CK id, and the backend must not be led into treating it
  as one.
- Enter sends, Shift+Enter is a newline.
- Citations like `[T0803]` become links **only when the id appears in the
  `sources` the backend returned**. An LLM will happily cite ids that were
  never retrieved; linking those to a guessed MITRE URL would dress a
  hallucination up as a grounded reference. Ungrounded citations render as
  plain text, and a footnote counts them.
- The **GROUNDED IN** chips appear before the first token, because the
  `sources` event arrives first. Hovering one shows its type and cosine
  similarity; clicking opens the MITRE page.

## 5. Verify

Independently of the GUI, with `iw-chat-probe`:

```bash
# does the backend answer this token at all?
./build/iw-chat-probe --url http://10.0.0.5:8000 --token <tok> \
    --ask "How do adversaries block PLC command messages?"
```

It links the real `ChatClient`, so a clean run means the request shape,
auth header and SSE parser all work against that deployment. It prints one
`.` per token event, then sources / token / char counts and time-to-first-
token — which is the number that tells you whether the stream is actually
incremental or the backend is buffering it.

Exit code is 0 only if every question produced a finished answer.

Inside the app, the signals that it is working: the header badge goes
`ready` → `generating`, chips appear before the text, and the answer grows
a word at a time.

## 6. Testing without a backend

`tools/iw_chat_stub.py` implements the three endpoints closely enough to
exercise every path the console has code for, including the ones that are
awkward to provoke on a real backend:

```bash
python3 tools/iw_chat_stub.py --port 8000 --token devtoken

# the whole suite, through the real client
./build/iw-chat-probe --url http://127.0.0.1:8000 --token devtoken --scenarios
```

To exercise the **right-click panel** you need a row to right-click *and* the
chat API on the same origin, since the console holds one base URL for the
whole backend. `tools/iw_stub_backend.py` runs both stubs on one port:

```bash
python3 tools/iw_stub_backend.py --port 8000 --token devtoken
```

Scenarios: `normal slow fast midstream truncated ungrounded empty unicode
err400 err401 err502 slowstart`. Name one in the question to select it, so
the GUI can be driven through them by hand ("midstream: what happens
here?"). There is no retrieval and no model behind it — every answer is
canned.

## 7. Failure modes worth knowing

**`401 Unauthorized`.** The token is missing or the backend does not accept
it. Note this is also what a *session*-auth backend returns to a token
header; the doc flags that the scheme is deployment-specific.

**`HTTP 502 — ollama: …`.** The backend reached Ollama and Ollama failed;
the message is Ollama's own. Nothing to fix on this side.

**The answer arrives all at once at the end.** The backend is not under an
ASGI server. Functionally fine; turn streaming off so the UI stops implying
otherwise.

**"The backend closed the stream before it finished."** A 200, valid
frames, then silence. Most often the model was unloaded or a reverse proxy
imposed its own timeout on a long response.

**Everything is slow.** Expected on a 6 GB card. Time-to-first-token
covers retrieval, embedding and model load; only after that do tokens
flow.

## 8. If you extend it

- **`POST /api/attack/ingest/`** (§4.3) is not wired up. It is an admin
  action, it is synchronous and can take tens of seconds, and it would need
  its *own* generous timeout — the opposite of the chat requests. Deciding
  whether operators should be able to rewrite the knowledge base from the
  console is a policy question, not a UI one.
- **Retrieval filters** (`attack_types`, `tactic`, `platform`) are
  deliberately not sent. A detection carries a model class label, not an
  ATT&CK tactic, and mapping one to the other in the console would silently
  narrow retrieval on an assumption nothing supports. If a mapping is ever
  established on the board side, that is where it belongs.
- **Token storage**: `QSettings` keeps it in the clear (registry on
  Windows), exactly like the broker password. A DRF token is a bearer
  credential for the whole backend. Before this is anything but a lab tool,
  move both to the platform keychain.
- **Transport**: plain HTTP on a LAN is what the doc allows for a
  prototype. The Integrations screen says so out loud when the base URL is
  not HTTPS, because the token crosses the network with every request.


## 8. Moving from the native API to the OpenAI-compatible one

For anyone reading the old `API_DOCUMENTATION.md` and wondering what changed.

| | native `/api/attack/chat/` | OpenAI `/v1/chat/completions` |
|---|---|---|
| Auth | `Authorization: Token …` | `Authorization: Bearer …` |
| Question | `{"question": "…"}` | last `user` message in `messages[]` |
| History | separate `history[]` field | earlier `messages[]` entries |
| Streaming | `event: token` / `sources` / `done` / `error` | `data:` chunks, `delta.content`, `[DONE]` |
| Sources | a `sources` event, before any token | a markdown `**Sources:**` footer inside the answer |
| Source fields | `id, type, title, url, score` | `id`, `url` — **that is all** |
| Mid-stream error | `event: error` | `[error: …]` as content on a 200 stream |
| Errors | `{"error": "…"}` | `{"error": {"message", "type", …}}` |
| Connection test | none | `GET /v1/models` |

Two of those are worth calling out because they cost real behaviour:

**Sources arrive late, not early.** On the native API the citation chips
appeared *before* the first token, because the `sources` event led. Here they
appear at the *end*, because the footer trails the answer. The console shows
them as soon as each `[id](url)` pair completes, so they fill in during the
last moments of the stream rather than at the start. Nothing is lost, but the
ordering an operator sees is genuinely different.

**`type` and `score` are gone.** The native chips showed "technique ·
similarity 0.713"; this surface carries no such fields. Rather than fake
them, the chips show the id and the tooltip shows the URL. If retrieval
scores matter to you, they exist on the native endpoint and that is a reason
to keep it around.

What did *not* change: the rule that a `[Txxxx]` becomes a link only when the
backend actually retrieved it. The grounded set now comes from the footer
instead of a JSON array, but the guarantee is identical — and the stub's
`ungrounded` scenario still proves it, citing `[T9999]` inline while omitting
it from the footer.
