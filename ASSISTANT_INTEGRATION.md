# ATT&CK assistant — desktop console

The **Assistant** screen puts an operator in front of the `attack_rag`
Django backend: they ask about an alert, the backend retrieves matching
MITRE ATT&CK entries and an LLM answers from them. The wire contract is
`API_DOCUMENTATION.md` in this directory; this is how the console
implements it.

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
  └─ ChatClient                         ctor  → QNetworkAccessManager
       ├─ loadSettings()                      → QSettings (baseUrl, token, k, streaming)
       └─ ChatModel transcript                → the QML ListView's model
```

```
ChatClient::ask(question)
    history(6) ─┐                    read *before* the new turn is appended
                ├─► POST /api/attack/chat/stream/    Authorization: Token …
                │                                    Accept: text/event-stream
    appendUser  │                                    setTransferTimeout(0)
    appendAssistantPlaceholder                                │
                                                              ▼
  readyRead ──► buffer ──► split on "\n\n" ──► handleFrame()
                                                 ├─ sources → ChatModel::setSources
                                                 ├─ token   → ChatModel::appendToken
                                                 ├─ done    → ChatModel::finish
                                                 └─ error   → ChatModel::fail
  finished  ──► pre-stream 400/401/502, or a stream that stopped early
```

QML binds to `App.chat.*` and to `App.chat.transcript`. Nothing in QML
parses SSE.

**Non-obvious things the implementation does**, each because the doc or a
test run demanded it:

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
- **One request in flight at a time** (§6.7 — the backend serializes on a
  single GPU). Enforced in `ask()`, not only by disabling the button.

## 3. Point it at a backend

**Integrations → ATT&CK assistant**:

| Field | Meaning |
|---|---|
| Base URL | `http://<backend-host>:8000` — scheme optional, it is added |
| API token | the DRF token, sent as `Authorization: Token <token>` |
| Chunks (k) | retrieval breadth, default 6 |
| Stream tokens | on = `/chat/stream/` (SSE), off = `/chat/` (blocking) |

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

- **Assistant** in the sidebar, or **Ask the ATT&CK assistant** in the
  Detections inspector.
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
