# ATT&CK RAG Backend — API Documentation (Qt/QML client)

Reference for building the Qt/QML desktop chat window against the `attack_rag`
Django backend. Covers the chat endpoints (blocking + streaming) and the STIX
ingest endpoint, the exact request/response shapes, the Server-Sent Events
protocol, and Qt-specific integration guidance.

---

## 1. Conventions

| Item | Value |
|------|-------|
| Base URL | `http://<backend-host>:8000` (set per deployment; the app is on a separate machine) |
| API root | `/api/attack/` |
| Request body | `application/json` for chat; `multipart/form-data` for file upload |
| Response body | `application/json`, except the stream endpoint (`text/event-stream`) |
| Character encoding | UTF-8 |
| Transport | Plain HTTP on a LAN is fine for a prototype; use **HTTPS** in production (this is a security tool) |

All endpoints require an **authenticated user**. There is no CORS concern: the
Qt client is a native app, not a browser.

---

## 2. Authentication

The backend uses DRF's `IsAuthenticated`. The recommended scheme for a desktop
client is **Token authentication**: obtain a token once, then send it on every
request.

```
Authorization: Token <your-token>
```

> The exact scheme is set in the Django project settings
> (`DEFAULT_AUTHENTICATION_CLASSES`). Token auth is assumed throughout this doc;
> session auth also works if you prefer cookie-based login. Ask the backend
> owner which is enabled and how tokens are issued (typically a
> `POST /api/auth/token/` with username/password, returning `{"token": "..."}`).

If the token is missing or invalid, endpoints return **401 Unauthorized**.

---

## 3. Data models

### 3.1 Source object

Returned with every answer; represents one retrieved ATT&CK item the answer is
grounded in. Pair these with the `[Txxxx]` citations in the answer text to make
them clickable.

```json
{
  "id": "T0803",
  "type": "technique",
  "title": "Block Command Message (T0803)",
  "url": "https://attack.mitre.org/techniques/T0803",
  "score": 0.7134
}
```

| Field | Type | Notes |
|-------|------|-------|
| `id` | string | ATT&CK external id: `T####` technique, `M####` mitigation, `G####` group, `S####` software, `C####` campaign |
| `type` | string | one of `technique, tactic, mitigation, group, software, campaign, asset` |
| `title` | string | display name including the id |
| `url` | string | canonical MITRE URL (may be empty for some objects) |
| `score` | number | cosine similarity in `[0,1]`, higher is more relevant (rounded to 4 dp) |

### 3.2 History turn

Optional multi-turn context. The server keeps at most the **last 6** turns and
ignores malformed entries.

```json
{"role": "user", "content": "What is Block Command Message?"}
```

`role` must be `"user"` or `"assistant"`; `content` is non-empty text.

### 3.3 Retrieval filters (optional)

| Param | Type | Meaning |
|-------|------|---------|
| `k` | int | number of chunks to retrieve (default `6`) |
| `attack_types` | string or string[] | restrict retrieval to these types (values from §3.1 `type`); invalid values are dropped |
| `tactic` | string | restrict to a tactic display name, e.g. `"Inhibit Response Function"` |
| `platform` | string | restrict to a platform, e.g. `"Windows"`, `"Field Controller/RTU/PLC/IED"` |

---

## 4. Endpoints

### 4.1 `POST /api/attack/chat/` — blocking answer

Returns the full answer once generation finishes. Simplest to integrate; use it
for a first pass or non-interactive calls. Note the whole answer can take
several seconds on the 6 GB backend, so prefer the streaming endpoint (§4.2) for
the live chat window.

**Request**

```http
POST /api/attack/chat/
Authorization: Token <token>
Content-Type: application/json

{
  "question": "How do adversaries block PLC command messages, and how do I mitigate it?",
  "k": 6,
  "attack_types": ["technique", "mitigation"],
  "tactic": null,
  "platform": null,
  "history": []
}
```

Only `question` is required.

**Response `200 OK`**

```json
{
  "answer": "Adversaries block command messages by ... [T0803]. Mitigations include ... [M0810].",
  "sources": [
    {"id": "T0803", "type": "technique", "title": "Block Command Message (T0803)", "url": "https://attack.mitre.org/techniques/T0803", "score": 0.71},
    {"id": "M0810", "type": "mitigation", "title": "Out-of-Band Communications Channel (M0810)", "url": "https://attack.mitre.org/mitigations/M0810", "score": 0.63}
  ]
}
```

**Errors**

| Status | Body | Cause |
|--------|------|-------|
| 400 | `{"error": "field 'question' is required"}` | empty/missing question |
| 401 | — | missing/invalid token |
| 502 | `{"error": "..."}` | embedding or LLM/Ollama failure (message contains Ollama's reason) |

---

### 4.2 `POST /api/attack/chat/stream/` — streaming answer (SSE)

Same request body as §4.1. Returns a **Server-Sent Events** stream so the UI can
render tokens as they are produced. **This is the endpoint the chat window
should use.**

**Response `200 OK`, `Content-Type: text/event-stream`**

Response headers include `Cache-Control: no-cache` and `X-Accel-Buffering: no`.

Each event is a frame of the form:

```
event: <name>
data: <json>

```

(one blank line terminates a frame). Event sequence:

| Order | `event` | `data` payload | Meaning |
|-------|---------|----------------|---------|
| 1 (once) | `sources` | `{"sources": [ <source>, ... ]}` | retrieved context — show citation chips immediately |
| 2..n | `token` | `{"text": "<piece>"}` | append `text` to the current answer bubble |
| last | `done` | `{}` | generation finished successfully |
| — | `error` | `{"detail": "<message>"}` | LLM failed mid-stream; stream ends after this |

**Example wire trace**

```
event: sources
data: {"sources": [{"id": "T0803", "type": "technique", "title": "Block Command Message (T0803)", "url": "https://attack.mitre.org/techniques/T0803", "score": 0.71}]}

event: token
data: {"text": "Block "}

event: token
data: {"text": "Command "}

event: token
data: {"text": "Message [T0803] blocks ..."}

event: done
data: {}

```

**Errors.** Failures that happen *before* streaming starts (missing question,
retrieval/embedding failure) are returned as a normal JSON response, **not** as
SSE:

| Status | Body | Cause |
|--------|------|-------|
| 400 | `{"error": "field 'question' is required"}` | empty/missing question |
| 401 | — | missing/invalid token |
| 502 | `{"error": "..."}` | retrieval/embedding failed before generation |

A failure *during* generation arrives as an `error` SSE event on an otherwise
`200` stream. The client must handle both.

---

### 4.3 `POST /api/attack/ingest/` — add a STIX bundle

Adds a MITRE ATT&CK STIX bundle (e.g. a new campaign layer in the same format)
to the RAG store. Typically an admin action; expose it in the desktop app only
if operators should upload bundles.

**Request (recommended: multipart)**

```http
POST /api/attack/ingest/
Authorization: Token <token>
Content-Type: multipart/form-data

file=<bundle.json>        ; required, a .json STIX bundle
wipe=false                ; optional; "true" rebuilds the store from scratch
```

A raw `application/json` body containing the bundle is also accepted (add
`?wipe=true` to rebuild). ATT&CK bundles are several MB, so the backend must set
`DATA_UPLOAD_MAX_MEMORY_SIZE` / `FILE_UPLOAD_MAX_MEMORY_SIZE` accordingly.

**Response `201 Created`**

```json
{
  "documents": 224,
  "chunks_total": 592,
  "chunks_written": 592,
  "chunks_skipped": 0,
  "wiped": 0,
  "embedder": "ollama:embeddinggemma"
}
```

| Field | Meaning |
|-------|---------|
| `documents` | enriched documents parsed from the bundle |
| `chunks_total` | chunks produced |
| `chunks_written` | chunks (re)embedded and stored this run |
| `chunks_skipped` | chunks already up to date (content unchanged) |
| `wiped` | rows deleted when `wipe=true` |
| `embedder` | embedder used |

Ingestion is **synchronous** and can take tens of seconds; set a generous client
timeout for this call (not for chat).

**Errors**

| Status | Body | Cause |
|--------|------|-------|
| 400 | `{"error": "invalid JSON: ..."}` / `{"error": "not a STIX bundle: ..."}` | unparseable or non-STIX file |
| 401 | — | missing/invalid token |
| 502 | `{"error": "ingestion failed: ..."}` | embedding/DB failure during ingest |

---

## 5. Status code summary

| Code | Meaning |
|------|---------|
| 200 | OK (chat answer, or an open SSE stream) |
| 201 | Created (ingest completed) |
| 400 | Bad request (validation error; body has `error`) |
| 401 | Unauthorized (token missing/invalid) |
| 502 | Upstream failure (Ollama/embedding/DB); body has `error` |

---

## 6. Qt/QML integration guide

### 6.1 Recommended structure

Put all networking in a C++ `QObject` "backend" and expose it to QML. The
backend emits signals for stream events; QML binds to them and renders. This
keeps SSE parsing in C++ (where it belongs) and gives QML a clean, declarative
surface.

```
ChatClient (C++ QObject)
  Q_INVOKABLE void ask(const QString &question)        // -> POST /chat/stream/
  signals:
    void sourcesReceived(const QVariantList &sources);
    void tokenReceived(const QString &text);
    void answerFinished();
    void errorOccurred(const QString &detail);
```

### 6.2 Shared setup (auth header + base URL)

```cpp
QNetworkRequest ChatClient::makeRequest(const QString &path) const {
    QNetworkRequest req(QUrl(m_baseUrl + path));           // e.g. http://10.0.0.5:8000
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", "Token " + m_token.toUtf8());
    req.setRawHeader("Accept", "application/json");
    return req;
}
```

### 6.3 Streaming client (SSE) — the important part

`QNetworkAccessManager` does not parse SSE; read the reply incrementally on
`readyRead`, buffer bytes, and split on blank lines. Handle partial frames.

```cpp
void ChatClient::ask(const QString &question) {
    QJsonObject body{{"question", question}};
    // optional: body["attack_types"] = QJsonArray{"technique"}; body["k"] = 6;

    QNetworkRequest req = makeRequest("/api/attack/chat/stream/");
    req.setRawHeader("Accept", "text/event-stream");
    // Critical: disable the transfer timeout, or long generations get cut off.
    req.setTransferTimeout(0);

    m_buffer.clear();
    QNetworkReply *reply = m_nam->post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        m_buffer.append(reply->readAll());
        int sep;
        // Frames are separated by a blank line ("\n\n").
        while ((sep = m_buffer.indexOf("\n\n")) != -1) {
            const QByteArray frame = m_buffer.left(sep);
            m_buffer.remove(0, sep + 2);
            handleFrame(frame);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            // Pre-stream errors (400/502) arrive here as a JSON body.
            const auto doc = QJsonDocument::fromJson(reply->readAll());
            emit errorOccurred(doc.object().value("error").toString(
                                   reply->errorString()));
        }
        reply->deleteLater();
    });
}

void ChatClient::handleFrame(const QByteArray &frame) {
    QString event;
    QByteArray data;
    for (const QByteArray &line : frame.split('\n')) {
        if (line.startsWith("event:")) event = line.mid(6).trimmed();
        else if (line.startsWith("data:")) data = line.mid(5).trimmed();
    }
    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (event == "sources") {
        emit sourcesReceived(obj.value("sources").toArray().toVariantList());
    } else if (event == "token") {
        emit tokenReceived(obj.value("text").toString());
    } else if (event == "done") {
        emit answerFinished();
    } else if (event == "error") {
        emit errorOccurred(obj.value("detail").toString());
    }
}
```

### 6.4 Blocking client (non-streaming)

```cpp
void ChatClient::askBlocking(const QString &question) {
    QJsonObject body{{"question", question}};
    QNetworkReply *reply = m_nam->post(makeRequest("/api/attack/chat/"),
                                       QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() == QNetworkReply::NoError) {
            emit sourcesReceived(obj.value("sources").toArray().toVariantList());
            emit tokenReceived(obj.value("answer").toString());  // whole answer at once
            emit answerFinished();
        } else {
            emit errorOccurred(obj.value("error").toString(reply->errorString()));
        }
        reply->deleteLater();
    });
}
```

### 6.5 QML usage

```qml
Connections {
    target: chatClient
    function onSourcesReceived(sources) { citationModel.setSources(sources) }
    function onTokenReceived(text)      { answerArea.text += text }
    function onAnswerFinished()         { busy.running = false }
    function onErrorOccurred(detail)    { errorBanner.show(detail) }
}

TextField {
    onAccepted: {
        answerArea.text = ""
        busy.running = true
        chatClient.ask(text)     // streams into answerArea
    }
}
```

### 6.6 Rendering citations

The answer text contains inline ids like `[T0803]`. Match them against the
`sources` list (by `id`) and render each as a clickable link that opens
`source.url` (or a detail panel). A simple regex `\[[A-Z]\d{4}(?:\.\d{3})?\]`
catches technique/mitigation/group/software/campaign ids and sub-technique ids.

### 6.7 Gotchas

- **Disable the transfer timeout** on the streaming reply (`setTransferTimeout(0)`).
  Generation on a 6 GB card is slow; a default timeout will abort mid-answer.
- **Don't wait for `finished` to show tokens** — parse on `readyRead`. `finished`
  is only for end-of-stream cleanup and pre-stream error bodies.
- **Buffer partial frames.** A single `readyRead` may deliver half a frame or
  several frames; the `indexOf("\n\n")` loop above handles both.
- The backend must run under an **ASGI server** (uvicorn/daphne) for the stream
  to flush incrementally; otherwise tokens arrive all at once at the end.
- One in-flight generation per backend GPU: serialize requests or disable the
  send button while `busy.running` is true.

---

## 7. Quick reference

| Method | Path | Purpose | Response |
|--------|------|---------|----------|
| POST | `/api/attack/chat/` | grounded answer (blocking) | `{answer, sources}` |
| POST | `/api/attack/chat/stream/` | grounded answer (SSE) | `event: sources/token/done/error` |
| POST | `/api/attack/ingest/` | add a STIX bundle | `{documents, chunks_total, ...}` |

All requests: `Authorization: Token <token>`. Chat bodies: JSON with required
`question` and optional `k`, `attack_types`, `tactic`, `platform`, `history`.
