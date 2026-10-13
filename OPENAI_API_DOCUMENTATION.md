# ATT&CK RAG — OpenAI-Compatible Chat API

Reference for the `/v1` endpoints that expose the MITRE ATT&CK RAG assistant
through an OpenAI-compatible interface. Any tool that speaks the OpenAI Chat
Completions API — Obsidian AI plugins, the `openai` SDKs, LangChain, curl — can
use it by pointing its base URL at this server.

> This is the OpenAI-compatible surface. The native desktop endpoints
> (`/api/attack/chat/` and `/api/attack/chat/stream/`) and the SIEM data API are
> documented separately. All three can run on the same backend at once.

---

## 1. Conventions

| Item | Value |
|------|-------|
| Base URL | `http://<backend-host>:8000/v1` |
| Content type | `application/json` (responses); `text/event-stream` when streaming |
| Encoding | UTF-8 |
| Model name | `attack-rag` (configurable; a single model is served) |

Native desktop/tool client, so there is no CORS concern. Use **HTTPS** in
production.

---

## 2. Authentication

Send an API key as a bearer token:

```
Authorization: Bearer <api-key>
```

The key is either one of the server's configured keys
(`ATTACK_RAG_OPENAI_API_KEYS`, e.g. `sk-attack-...`) or a valid backend token.
Missing/invalid keys return **401**. In OpenAI clients, paste the key wherever
they ask for the "API key".

---

## 3. Endpoints

### 3.1 `POST /v1/chat/completions`

Generate a grounded, cited answer. Supports blocking and streaming modes.
(Trailing slash optional: `/v1/chat/completions` and `/v1/chat/completions/`
both work.)

#### Request body

```json
{
  "model": "attack-rag",
  "messages": [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "What is Block Command Message?"},
    {"role": "assistant", "content": "It is technique T0803."},
    {"role": "user", "content": "How do adversaries perform it, and how do I mitigate it?"}
  ],
  "stream": false
}
```

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `model` | string | no | echoed back; any value maps to the one model |
| `messages` | array | **yes** | OpenAI chat messages (see §4 for how they're used) |
| `stream` | bool | no | `true` for SSE token streaming (default `false`) |
| `k` | int | no | *(extension)* number of context chunks to retrieve (default 6) |
| `attack_types` | string[] | no | *(extension)* restrict retrieval, e.g. `["technique","mitigation"]` |

Standard OpenAI sampling fields (`temperature`, `top_p`, `max_tokens`, `n`,
`stop`, …) are **accepted but ignored** — generation is governed by the server's
model settings. `n` > 1 is not supported (always one choice).

#### Response — blocking (`stream:false`) `200 OK`

```json
{
  "id": "chatcmpl-ad3060cb097f...",
  "object": "chat.completion",
  "created": 1755000000,
  "model": "attack-rag",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Adversaries block command messages by ... [T0803]. Mitigations include ... [M0810].\n\n**Sources:** [T0803](https://attack.mitre.org/techniques/T0803) · [M0810](https://attack.mitre.org/mitigations/M0810)"
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {"prompt_tokens": 812, "completion_tokens": 96, "total_tokens": 908}
}
```

`usage` counts are **approximate** (char-based estimate), not exact model tokens.

#### Response — streaming (`stream:true`) `200 OK`

`Content-Type: text/event-stream`. Standard OpenAI chunk protocol: a first chunk
carrying the assistant role, then content deltas, a final chunk with
`finish_reason: "stop"`, then the `[DONE]` sentinel.

```
data: {"id":"chatcmpl-...","object":"chat.completion.chunk","created":1755000000,"model":"attack-rag","choices":[{"index":0,"delta":{"role":"assistant"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{"content":"Block "},"finish_reason":null}]}

data: {"id":"chatcmpl-...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{"content":"command messages [T0803]. "},"finish_reason":null}]}

data: {"id":"chatcmpl-...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{"content":"\n\n**Sources:** [T0803](https://attack.mitre.org/techniques/T0803)"},"finish_reason":null}]}

data: {"id":"chatcmpl-...","object":"chat.completion.chunk",...,"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]}

data: [DONE]

```

The `**Sources:**` footer arrives as ordinary content deltas just before the
final chunk, so any client that concatenates `delta.content` gets it for free.

### 3.2 `GET /v1/models`

Lists the available model (some clients call this to validate the connection).

```json
{
  "object": "list",
  "data": [{"id": "attack-rag", "object": "model", "created": 0, "owned_by": "attack_rag"}]
}
```

---

## 4. How messages map to the RAG

The server turns an OpenAI conversation into a grounded retrieval query:

- The **last `user` message** is the question sent to retrieval.
- Earlier **`user`/`assistant`** messages become conversation history (most
  recent few) for follow-ups.
- Incoming **`system`** messages are **ignored** — the server's own grounding
  prompt (answer only from retrieved ATT&CK context, cite IDs, admit gaps)
  always governs. You cannot override it via a system message.
- `content` may be a plain string or an OpenAI content-parts array; only the
  text parts are used.

Every answer is grounded in the retrieved ATT&CK for ICS context and cites IDs
inline as `[T0803]`, `[M0810]`, `[G0034]`; the retrieved items are also listed
in the markdown `**Sources:**` footer with links.

---

## 5. Errors

OpenAI-style error envelope:

```json
{"error": {"message": "…", "type": "invalid_request_error", "param": null, "code": null}}
```

| Status | `type` | Cause |
|--------|--------|-------|
| 400 | `invalid_request_error` | `messages` missing or has no `user` message |
| 401 | — | missing/invalid API key |
| 502 | `upstream_error` | embedding or LLM (Ollama) failure; `message` carries the reason |

In streaming mode, a failure that occurs *after* the stream has started is
emitted as a content delta containing `[error: …]` followed by the `stop` chunk
and `[DONE]` (the HTTP status is already `200` by then).

---

## 6. Client examples

### curl (blocking)

```bash
curl http://BACKEND:8000/v1/chat/completions \
  -H "Authorization: Bearer sk-attack-changeme" \
  -H "Content-Type: application/json" \
  -d '{"model":"attack-rag","messages":[{"role":"user","content":"How do adversaries block PLC command messages?"}]}'
```

### curl (streaming)

```bash
curl -N http://BACKEND:8000/v1/chat/completions \
  -H "Authorization: Bearer sk-attack-changeme" \
  -H "Content-Type: application/json" \
  -d '{"model":"attack-rag","stream":true,"messages":[{"role":"user","content":"Explain T0836"}]}'
```

### OpenAI Python SDK

```python
from openai import OpenAI

client = OpenAI(base_url="http://BACKEND:8000/v1", api_key="sk-attack-changeme")

resp = client.chat.completions.create(
    model="attack-rag",
    messages=[{"role": "user", "content": "How is Modify Parameter (T0836) detected?"}],
)
print(resp.choices[0].message.content)

# streaming
for chunk in client.chat.completions.create(
    model="attack-rag",
    messages=[{"role": "user", "content": "Mitigations for xmrig cryptomining?"}],
    stream=True,
):
    print(chunk.choices[0].delta.content or "", end="")
```

### Obsidian (any AI plugin with a custom OpenAI endpoint)

In the plugin settings (Copilot, ChatGPT MD, AI-Vault, …):

| Setting | Value |
|---------|-------|
| Base URL / API host | `http://BACKEND:8000/v1` |
| Model | `attack-rag` |
| API key | `sk-attack-changeme` |

No Obsidian plugin code is required. Answers stream into the note and the
`**Sources:**` footer renders as clickable ATT&CK links.

---

## 7. Compatibility notes

Supported: `chat/completions` (blocking + streaming), `models`, OpenAI request
and response schemas, bearer-key auth, the `[DONE]` stream sentinel.

Intentionally different from OpenAI:

- **One model**, RAG-grounded; the model name is cosmetic.
- **System messages ignored**; the grounding prompt is fixed server-side.
- **Sampling params ignored**; determinism is set by the server.
- **`usage` is approximate.**
- **No** `tools`/function-calling, `logprobs`, `n`>1, or an `embeddings`
  endpoint. Requests using those fields still work — the unsupported fields are
  ignored.

---

## 8. Quick reference

| Method | Path | Purpose |
|--------|------|---------|
| POST | `/v1/chat/completions` | grounded answer (blocking or `stream:true`) |
| GET | `/v1/models` | list the `attack-rag` model |

Auth: `Authorization: Bearer <key>`. Body: OpenAI chat schema (`model`,
`messages`, `stream`), plus optional `k` / `attack_types`. Citations appear
inline as `[Txxxx]` and in a markdown `**Sources:**` footer.
