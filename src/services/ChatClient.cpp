#include "ChatClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>

ChatClient::ChatClient(BackendSettings *backend, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_backend(backend)
{
    // "configured" and endpointText both derive from the shared settings.
    connect(m_backend, &BackendSettings::changed, this, &ChatClient::settingsChanged);
}

ChatClient::~ChatClient()
{
    // The reply is parented to the QNAM, but abort explicitly so a
    // half-finished generation doesn't keep the socket open through teardown.
    // Disconnect first: abort() emits finished() synchronously, and running
    // finishReply() against a half-destroyed object (and a closed device)
    // achieves nothing but a warning.
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_backend->releaseGeneration();
    }
}

bool ChatClient::busy() const
{
    return !m_reply.isNull();
}

QString ChatClient::endpointText() const
{
    if (!m_backend->configured())
        return QStringLiteral("not configured");
    return m_backend->baseUrl() + QStringLiteral("/v1/chat/completions")
           + (m_streaming ? QStringLiteral("  (stream:true)") : QString());
}

// ── settings ────────────────────────────────────────────────────────────

void ChatClient::loadSettings()
{
    m_loading = true;

    QSettings s;
    m_k         = s.value(QStringLiteral("assistant/k"), 6).toInt();
    m_streaming = s.value(QStringLiteral("assistant/streaming"), true).toBool();
    m_model     = s.value(QStringLiteral("assistant/model"),
                          QStringLiteral("attack-rag")).toString();

    m_loading = false;
    emit settingsChanged();
}

void ChatClient::saveSettings()
{
    if (m_loading)
        return;

    QSettings s;
    s.setValue(QStringLiteral("assistant/k"), m_k);
    s.setValue(QStringLiteral("assistant/streaming"), m_streaming);
    s.setValue(QStringLiteral("assistant/model"), m_model);
}

void ChatClient::setRetrievalK(int v)
{
    if (v < 1 || v > 50 || m_k == v)
        return;
    m_k = v;
    saveSettings();
    emit settingsChanged();
}

void ChatClient::setStreaming(bool v)
{
    if (m_streaming == v)
        return;
    m_streaming = v;
    saveSettings();
    emit settingsChanged();
}

void ChatClient::setModel(const QString &v)
{
    const QString n = v.trimmed();
    if (n.isEmpty() || m_model == n)
        return;
    m_model = n;
    saveSettings();
    emit settingsChanged();
}

void ChatClient::setDraft(const QString &v)
{
    // Always re-emit: asking about two different alerts in a row must move
    // the composer both times, even if the second question is identical.
    m_draft = v;
    emit draftChanged();
}

// ── requests ────────────────────────────────────────────────────────────

QNetworkRequest ChatClient::makeRequest(bool sse) const
{
    // Bearer, not Token — this endpoint is the OpenAI-compatible one (§2).
    QNetworkRequest req = m_backend->bearerRequest(
        m_backend->url(QStringLiteral("/v1/chat/completions")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", sse ? "text/event-stream" : "application/json");

    if (sse) {
        // Without this a long generation is cut off mid-answer by the
        // transfer timeout: an SSE stream is idle between tokens by design,
        // which is exactly what the timeout is looking for.
        req.setTransferTimeout(0);
        // And nothing about a token stream survives being cached or
        // aggregated by an intermediary.
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    }
    return req;
}

QByteArray ChatClient::requestBody(const QString &question) const
{
    // §4: the last `user` message is the retrieval question; earlier
    // user/assistant messages are history. No `system` message is sent —
    // the server ignores them and its own grounding prompt always governs,
    // so sending one would only imply an override that does not exist.
    QJsonArray messages = m_transcript.openAiMessages(kHistoryTurns);
    QJsonObject user;
    user[QStringLiteral("role")] = QStringLiteral("user");
    user[QStringLiteral("content")] = question;
    messages.append(user);

    QJsonObject body;
    body[QStringLiteral("model")] = m_model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("stream")] = m_streaming;
    body[QStringLiteral("k")] = m_k;   // §3.1 extension
    // `attack_types` is deliberately not sent: the console has no honest
    // value for it. A detection carries a model class name, not an ATT&CK
    // type, and guessing a mapping here would silently narrow retrieval on
    // an assumption nothing supports.
    //
    // Sampling fields (temperature, top_p, max_tokens, …) are omitted rather
    // than sent-and-ignored (§7) — sending them would suggest the console
    // controls generation when it does not.

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void ChatClient::ask(const QString &question)
{
    const QString q = question.trimmed();
    if (q.isEmpty())
        return;

    if (!m_backend->configured()) {
        setError(QStringLiteral("No backend configured. Set the base URL on the "
                                "Integrations screen."));
        return;
    }

    if (m_reply)
        return;

    // One in-flight generation per backend GPU (§6.7), across *every* client
    // sharing this backend — the Assistant screen and the explain popup both
    // ask, and the backend can only answer one at a time.
    if (!m_backend->tryAcquireGeneration()) {
        setError(QStringLiteral("The backend is already generating an answer. "
                                "Wait for it to finish, or stop it."));
        return;
    }

    m_lastQuestion = q;
    m_lastError.clear();
    emit lastErrorChanged();

    // History is read before the new turn is appended, so the question is not
    // also duplicated into its own context.
    const QByteArray payload = requestBody(q);

    m_transcript.appendUser(q);
    m_answerRow = m_transcript.appendAssistantPlaceholder();

    m_buffer.clear();
    m_rawAnswer.clear();
    m_sources.clear();
    m_sawDone = false;
    m_sawStop = false;
    m_sawError = false;
    m_cancelled = false;
    m_httpOk = true;

    m_reply = m_nam->post(makeRequest(m_streaming), payload);
    if (m_streaming)
        connect(m_reply, &QNetworkReply::readyRead, this, &ChatClient::consume);
    connect(m_reply, &QNetworkReply::finished, this, &ChatClient::finishReply);

    emit busyChanged();
}

void ChatClient::cancel()
{
    if (!m_reply)
        return;
    m_cancelled = true;
    m_reply->abort(); // -> finished()
}

void ChatClient::retryLast()
{
    if (m_reply || m_lastQuestion.isEmpty())
        return;
    ask(m_lastQuestion);
}

void ChatClient::newConversation()
{
    cancel();
    m_transcript.clear();
    m_lastQuestion.clear();
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        emit lastErrorChanged();
    }
}

// ── SSE ─────────────────────────────────────────────────────────────────

void ChatClient::consume()
{
    if (!m_reply)
        return;

    // A pre-stream failure (400/401/502) arrives as a normal JSON body on a
    // non-200 response. Don't try to read it as event frames — let
    // finishReply() pull the whole body and report the `error` field.
    const QVariant status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (status.isValid() && status.toInt() != 200) {
        m_httpOk = false;
        return;
    }

    m_buffer.append(m_reply->readAll());

    // Frames are separated by a blank line. The spec allows CRLF line
    // endings, and the example trace uses bare LF; normalizing means one
    // separator to search for either way. (JSON escapes any real CR inside
    // `data`, so no payload byte is harmed by this.)
    m_buffer.replace("\r\n", "\n");

    int sep;
    while ((sep = m_buffer.indexOf("\n\n")) != -1) {
        const QByteArray frame = m_buffer.left(sep);
        m_buffer.remove(0, sep + 2);
        handleFrame(frame);
    }
}

void ChatClient::handleFrame(const QByteArray &frame)
{
    // OpenAI chunks carry no `event:` line — every frame is one or more
    // `data:` lines, and the stream ends with the literal `data: [DONE]`.
    QByteArray data;
    for (const QByteArray &line : frame.split('\n')) {
        if (line.startsWith(':'))
            continue; // comment / keep-alive heartbeat
        if (line.startsWith("data:")) {
            if (!data.isEmpty())
                data.append('\n');
            data.append(line.mid(5).trimmed());
        }
    }
    if (data.isEmpty())
        return;

    if (data == "[DONE]") {
        m_sawDone = true;
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty())
        return;

    const QJsonObject choice = choices.first().toObject();

    // The first chunk carries `delta.role` and no content; content chunks
    // carry `delta.content`; the last carries `finish_reason`.
    const QString delta = choice.value(QStringLiteral("delta")).toObject()
                              .value(QStringLiteral("content")).toString();
    if (!delta.isEmpty())
        applyDelta(delta);

    if (!choice.value(QStringLiteral("finish_reason")).isNull()
            && choice.value(QStringLiteral("finish_reason")).isString())
        m_sawStop = true;
}

void ChatClient::applyDelta(const QString &delta)
{
    m_rawAnswer += delta;
    publishAnswer();
    emit tokenReceived(delta);
}

void ChatClient::publishAnswer()
{
    QString footer;
    const QString visible = splitSourcesFooter(m_rawAnswer, &footer);

    m_transcript.setAnswerText(m_answerRow, visible);
    emit answerTextChanged(visible);

    if (!footer.isEmpty()) {
        const QVariantList parsed = parseSourcesFooter(footer);
        if (parsed != m_sources) {
            m_sources = parsed;
            m_transcript.setSources(m_answerRow, m_sources);
            emit sourcesReceived(m_sources);
        }
    }
}

// ── the Sources footer ──────────────────────────────────────────────────

QString ChatClient::splitSourcesFooter(const QString &raw, QString *footer)
{
    static const QString marker = QStringLiteral("**Sources:**");
    static const QRegularExpression errMarker(
        QStringLiteral("\\s*\\[error:[^\\]]*\\]\\s*$"));

    // §5's `[error: …]` marker is protocol, not prose. It is reported as the
    // turn's error; leaving it in the body too would say the same thing
    // twice, in a place that looks like the model wrote it.
    QString body = raw;
    body.remove(errMarker);

    const int at = body.indexOf(marker);
    if (at >= 0) {
        if (footer)
            *footer = body.mid(at + marker.size());
        // Trim the blank line that separates body from footer, so removing
        // the footer does not leave a trailing gap.
        return body.left(at).trimmed();
    }

    if (footer)
        footer->clear();

    // The marker may still be arriving one delta at a time. Hide any tail
    // that is a prefix of it, or the operator watches "**Sou" appear and
    // then vanish.
    const int maxPartial = qMin(marker.size() - 1, body.size());
    for (int n = maxPartial; n > 0; --n) {
        if (QStringView(body).right(n) == QStringView(marker).left(n))
            return body.left(body.size() - n).trimmed();
    }
    return body;
}

QVariantList ChatClient::parseSourcesFooter(const QString &footer)
{
    // "[T0803](https://attack.mitre.org/techniques/T0803) · [M0810](…)"
    //
    // Only `id` and `url` exist on this surface. The native API also carried
    // `type` and `score`; they are not reconstructed, and the UI must not
    // pretend otherwise.
    static const QRegularExpression re(QStringLiteral("\\[([^\\]\\s]+)\\]\\(([^)\\s]+)\\)"));

    QVariantList out;
    QSet<QString> seen;
    auto it = re.globalMatch(footer);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString id = m.captured(1);
        if (seen.contains(id))
            continue;
        seen.insert(id);

        QVariantMap o;
        o[QStringLiteral("id")] = id;
        o[QStringLiteral("url")] = m.captured(2);
        o[QStringLiteral("title")] = id;
        out.append(o);
    }
    return out;
}

// ── completion ──────────────────────────────────────────────────────────

void ChatClient::finishReply()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError err = reply->error();
    const QByteArray body = reply->readAll();

    if (m_cancelled) {
        m_transcript.cancel(m_answerRow);
        clearReply();
        return;
    }

    // ── blocking (§3.1) ─────────────────────────────────────────────────
    if (!m_streaming) {
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        if (err == QNetworkReply::NoError) {
            const QJsonArray choices = obj.value(QStringLiteral("choices")).toArray();
            const QString answer = choices.isEmpty()
                ? QString()
                : choices.first().toObject().value(QStringLiteral("message"))
                      .toObject().value(QStringLiteral("content")).toString();

            m_rawAnswer = answer;
            publishAnswer();
            emit tokenReceived(answer);

            m_transcript.finish(m_answerRow);
            if (answer.isEmpty())
                setError(QStringLiteral("The backend returned 200 with no answer."));
            else
                emit answerFinished();
        } else {
            m_transcript.fail(m_answerRow, describeHttpError(reply, body));
            setError(describeHttpError(reply, body));
        }
        clearReply();
        return;
    }

    // ── streaming (§3.1) ────────────────────────────────────────────────
    //
    // Once the response is a 200 this is a stream, and a transport error on
    // it means "the stream broke", not "the request failed" — the difference
    // matters because there is already a partial answer on screen.
    if (m_httpOk && status == 200) {
        if (!m_buffer.trimmed().isEmpty()) {
            const QByteArray tail = m_buffer;
            m_buffer.clear();
            handleFrame(tail);
        }

        // §5: a failure *after* the stream started is emitted as a content
        // delta containing `[error: …]`, followed by the stop chunk and
        // [DONE]. The HTTP status is 200 by then, so this marker is the only
        // signal that the answer on screen is a failure rather than a result.
        static const QRegularExpression errRe(QStringLiteral("\\[error:\\s*([^\\]]*)\\]"));
        const QRegularExpressionMatch em = errRe.match(m_rawAnswer);
        if (em.hasMatch()) {
            m_sawError = true;
            const QString detail = em.captured(1).trimmed();
            m_transcript.fail(m_answerRow,
                              detail.isEmpty()
                                  ? QStringLiteral("The backend reported an error mid-stream.")
                                  : detail);
            setError(detail.isEmpty()
                         ? QStringLiteral("The backend reported an error mid-stream.")
                         : detail);
            clearReply();
            return;
        }

        if (m_sawDone || m_sawStop) {
            const bool empty = m_transcript.textAt(m_answerRow).isEmpty();
            m_transcript.finish(m_answerRow);
            if (empty)
                setError(QStringLiteral("The backend finished without producing an answer."));
            else
                emit answerFinished();
            clearReply();
            return;
        }

        // Well-formed chunks, then the connection just stopped without
        // [DONE] or a stop chunk: the backend died, the model was unloaded,
        // or a proxy timed the stream out. The partial answer stays on
        // screen, marked incomplete — presenting it as finished would be the
        // worst possible outcome for a security tool.
        QString detail = QStringLiteral("The backend closed the stream before it finished. "
                                        "The answer above is incomplete.");
        if (err != QNetworkReply::NoError)
            detail += QStringLiteral(" (%1)").arg(reply->errorString());
        m_transcript.fail(m_answerRow, detail);
        setError(detail);
        clearReply();
        return;
    }

    // Pre-stream failures, and an unreachable host that never got a status.
    const QString detail = describeHttpError(reply, body);
    m_transcript.fail(m_answerRow, detail);
    setError(detail);
    clearReply();
}

// §5: {"error": {"message": …, "type": …, "param": …, "code": …}} — an
// envelope, where the native surface used a bare string. Both the message
// and the type matter: "upstream_error" is the backend's problem,
// "invalid_request_error" is ours.
QString ChatClient::describeHttpError(QNetworkReply *reply, const QByteArray &body) const
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    const QJsonValue errVal = root.value(QStringLiteral("error"));

    QString message, type;
    if (errVal.isObject()) {
        const QJsonObject e = errVal.toObject();
        message = e.value(QStringLiteral("message")).toString();
        type = e.value(QStringLiteral("type")).toString();
    } else if (errVal.isString()) {
        message = errVal.toString();
    }
    if (message.isEmpty())
        message = root.value(QStringLiteral("detail")).toString();
    if (message.isEmpty())
        message = reply->errorString();

    if (status == 401)
        return QStringLiteral("401 Unauthorized — the API key is missing or not accepted. "
                              "This endpoint expects an OpenAI-style bearer key.");
    if (status <= 0)
        return message;

    return type.isEmpty()
        ? QStringLiteral("HTTP %1 — %2").arg(status).arg(message)
        : QStringLiteral("HTTP %1 (%2) — %3").arg(status).arg(type, message);
}

// ── connection test (§3.2) ──────────────────────────────────────────────

void ChatClient::testConnection()
{
    if (m_testReply)
        return;

    if (!m_backend->configured()) {
        m_connectionState = QStringLiteral("error");
        m_connectionDetail = QStringLiteral("No base URL set.");
        emit connectionChanged();
        return;
    }

    m_connectionState = QStringLiteral("testing");
    m_connectionDetail.clear();
    emit connectionChanged();

    m_testReply = m_nam->get(m_backend->bearerRequest(
        m_backend->url(QStringLiteral("/v1/models"))));

    connect(m_testReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_testReply;
        if (!reply)
            return;
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            m_connectionState = QStringLiteral("error");
            m_connectionDetail = describeHttpError(reply, body);
        } else {
            QStringList ids;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object()
                                           .value(QStringLiteral("data")).toArray())
                ids << v.toObject().value(QStringLiteral("id")).toString();

            m_connectionState = QStringLiteral("ok");
            m_connectionDetail = ids.isEmpty()
                ? QStringLiteral("connected, but the server lists no models")
                : QStringLiteral("connected — model%1: %2")
                      .arg(ids.size() == 1 ? QString() : QStringLiteral("s"), ids.join(QLatin1String(", ")));

            // A served model that isn't the one configured here is worth
            // saying out loud: the name is cosmetic (§7) but a mismatch
            // usually means the URL points somewhere unexpected.
            if (!ids.isEmpty() && !ids.contains(m_model))
                m_connectionDetail += QStringLiteral("  (this console is set to \"%1\")").arg(m_model);
        }

        reply->deleteLater();
        m_testReply = nullptr;
        emit connectionChanged();
    });
}

void ChatClient::clearReply()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
        m_backend->releaseGeneration();
    }
    m_answerRow = -1;
    m_buffer.clear();
    emit busyChanged();
}

void ChatClient::setError(const QString &detail)
{
    m_lastError = detail;
    emit lastErrorChanged();
    emit errorOccurred(detail);
}
