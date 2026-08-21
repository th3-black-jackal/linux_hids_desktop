#include "ChatClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace {

// Accepts what an operator would actually type into the field — "10.0.0.5",
// "10.0.0.5:8000", "http://host:8000/" — and normalizes it to a scheme-
// qualified origin with no trailing slash, because every path is appended
// verbatim.
QString normalizeBaseUrl(QString v)
{
    v = v.trimmed();
    if (v.isEmpty())
        return {};
    if (!v.contains(QLatin1String("://")))
        v.prepend(QLatin1String("http://"));
    while (v.endsWith(QLatin1Char('/')))
        v.chop(1);
    return v;
}

} // namespace

ChatClient::ChatClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
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
    }
}

bool ChatClient::busy() const
{
    return !m_reply.isNull();
}

QString ChatClient::endpointText() const
{
    if (m_baseUrl.isEmpty())
        return QStringLiteral("not configured");
    return m_baseUrl + (m_streaming ? QStringLiteral("/api/attack/chat/stream/")
                                    : QStringLiteral("/api/attack/chat/"));
}

// ── settings ────────────────────────────────────────────────────────────

void ChatClient::loadSettings()
{
    m_loading = true;

    QSettings s;
    m_baseUrl   = s.value(QStringLiteral("assistant/baseUrl")).toString();
    m_token     = s.value(QStringLiteral("assistant/token")).toString();
    m_k         = s.value(QStringLiteral("assistant/k"), 6).toInt();
    m_streaming = s.value(QStringLiteral("assistant/streaming"), true).toBool();

    m_loading = false;
    emit settingsChanged();
}

void ChatClient::saveSettings()
{
    if (m_loading)
        return;

    QSettings s;
    s.setValue(QStringLiteral("assistant/baseUrl"), m_baseUrl);
    // Same caveat as the broker password: QSettings stores this in the clear
    // (registry on Windows, plain INI elsewhere). A DRF token is a bearer
    // credential for the whole backend — before this ships anywhere real it
    // belongs in the platform keychain.
    s.setValue(QStringLiteral("assistant/token"), m_token);
    s.setValue(QStringLiteral("assistant/k"), m_k);
    s.setValue(QStringLiteral("assistant/streaming"), m_streaming);
}

void ChatClient::setBaseUrl(const QString &v)
{
    const QString n = normalizeBaseUrl(v);
    if (m_baseUrl == n)
        return;
    m_baseUrl = n;
    saveSettings();
    emit settingsChanged();
}

void ChatClient::setToken(const QString &v)
{
    const QString n = v.trimmed();
    if (m_token == n)
        return;
    m_token = n;
    saveSettings();
    emit settingsChanged();
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

void ChatClient::setDraft(const QString &v)
{
    // Always re-emit: asking about two different alerts in a row must move
    // the composer both times, even if the second question is identical.
    m_draft = v;
    emit draftChanged();
}

// ── requests ────────────────────────────────────────────────────────────

QNetworkRequest ChatClient::makeRequest(const QString &path, bool sse) const
{
    QNetworkRequest req(QUrl(m_baseUrl + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", "Token " + m_token.toUtf8());
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
    QJsonObject body;
    body[QStringLiteral("question")] = question;
    body[QStringLiteral("k")] = m_k;
    // `attack_types`, `tactic` and `platform` are deliberately not sent: the
    // console has no honest value for them. A detection carries a model class
    // name, not an ATT&CK tactic, and guessing a mapping here would silently
    // narrow retrieval on a wrong assumption.
    const QJsonArray hist = m_transcript.history(kHistoryTurns);
    if (!hist.isEmpty())
        body[QStringLiteral("history")] = hist;

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void ChatClient::ask(const QString &question)
{
    const QString q = question.trimmed();
    if (q.isEmpty())
        return;

    if (m_baseUrl.isEmpty()) {
        setError(QStringLiteral("No backend configured. Set the assistant base "
                                "URL on the Integrations screen."));
        return;
    }

    // One in-flight generation per backend GPU (§6.7). The UI disables the
    // send button while busy; this is the guard for every other path in.
    if (m_reply)
        return;

    m_lastQuestion = q;
    m_lastError.clear();
    emit lastErrorChanged();

    // History is read before the new turn is appended, so the question is not
    // also duplicated into its own context.
    const QByteArray payload = requestBody(q);

    m_transcript.appendUser(q);
    m_answerRow = m_transcript.appendAssistantPlaceholder();

    m_buffer.clear();
    m_sawDone = false;
    m_sawError = false;
    m_cancelled = false;
    m_httpOk = true;

    if (m_streaming) {
        QNetworkRequest req = makeRequest(QStringLiteral("/api/attack/chat/stream/"), true);
        m_reply = m_nam->post(req, payload);
        connect(m_reply, &QNetworkReply::readyRead, this, &ChatClient::consume);
    } else {
        QNetworkRequest req = makeRequest(QStringLiteral("/api/attack/chat/"), false);
        m_reply = m_nam->post(req, payload);
    }
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
    QByteArray event;
    QByteArray data;

    // Multi-line `data:` is legal SSE and concatenates with newlines. The
    // backend sends one line today, but a longer token containing a newline
    // would arrive this way.
    const QList<QByteArray> lines = frame.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith(':'))
            continue; // comment / keep-alive heartbeat
        if (line.startsWith("event:")) {
            event = line.mid(6).trimmed();
        } else if (line.startsWith("data:")) {
            if (!data.isEmpty())
                data.append('\n');
            data.append(line.mid(5).trimmed());
        }
    }

    if (event.isEmpty())
        return;

    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (event == "sources") {
        const QVariantList sources = obj.value(QStringLiteral("sources")).toArray().toVariantList();
        m_transcript.setSources(m_answerRow, sources);
        emit sourcesReceived(sources);
    } else if (event == "token") {
        const QString text = obj.value(QStringLiteral("text")).toString();
        m_transcript.appendToken(m_answerRow, text);
        emit tokenReceived(text);
    } else if (event == "done") {
        m_sawDone = true;
        const bool empty = m_transcript.textAt(m_answerRow).isEmpty();
        m_transcript.finish(m_answerRow);
        // `done` with nothing before it is a failed generation wearing a
        // success event. Report it as the failure it is, so callers watching
        // the signals agree with what the transcript shows.
        if (empty)
            setError(QStringLiteral("The backend finished without producing an answer."));
        else
            emit answerFinished();
    } else if (event == "error") {
        // A failure *during* generation: the response was 200 and the stream
        // ends after this frame.
        m_sawError = true;
        const QString detail = obj.value(QStringLiteral("detail")).toString(
            QStringLiteral("the backend reported an error"));
        m_transcript.fail(m_answerRow, detail);
        setError(detail);
    }
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

    if (!m_streaming) {
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        if (err == QNetworkReply::NoError) {
            const QVariantList sources =
                obj.value(QStringLiteral("sources")).toArray().toVariantList();
            m_transcript.setSources(m_answerRow, sources);
            emit sourcesReceived(sources);

            const QString answer = obj.value(QStringLiteral("answer")).toString();
            m_transcript.appendToken(m_answerRow, answer);
            emit tokenReceived(answer);

            m_transcript.finish(m_answerRow);
            if (answer.isEmpty())
                setError(QStringLiteral("The backend returned 200 with no answer."));
            else
                emit answerFinished();
        } else {
            const QString detail = obj.value(QStringLiteral("error")).toString(reply->errorString());
            m_transcript.fail(m_answerRow, detail);
            setError(QStringLiteral("HTTP %1 — %2").arg(status).arg(detail));
        }
        clearReply();
        return;
    }

    // Streaming path. Once the response is a 200 this is a stream, and a
    // transport error on it means "the stream broke", not "the request
    // failed" — the difference matters because there is already a partial
    // answer on screen. Pre-stream failures are the other branch.
    if (m_httpOk && status == 200) {
        // Trailing frame with no terminating blank line — flush it rather
        // than dropping the last token of every answer that ends abruptly.
        if (!m_buffer.trimmed().isEmpty()) {
            const QByteArray tail = m_buffer;
            m_buffer.clear();
            handleFrame(tail);
        }

        if (!m_sawDone && !m_sawError) {
            // Well-formed frames, then the connection just stopped: the
            // backend process died, the model was unloaded, or a proxy timed
            // the stream out. The answer on screen is incomplete and must
            // not be presented as finished.
            QString detail =
                QStringLiteral("The backend closed the stream before it finished. "
                               "The answer above is incomplete.");
            if (err != QNetworkReply::NoError)
                detail += QStringLiteral(" (%1)").arg(reply->errorString());
            m_transcript.fail(m_answerRow, detail);
            setError(detail);
        }

        clearReply();
        return;
    }

    // Pre-stream failures land here with a JSON body, per §4.2 — as does an
    // unreachable host, which never got a status code at all.
    const QJsonObject obj = QJsonDocument::fromJson(body).object();
    QString detail = obj.value(QStringLiteral("error")).toString();
    if (detail.isEmpty())
        detail = obj.value(QStringLiteral("detail")).toString();
    if (detail.isEmpty())
        detail = reply->errorString();
    if (status == 401)
        detail = QStringLiteral("401 Unauthorized — the API token is missing or "
                                "not accepted by the backend.");
    else if (status > 0)
        detail = QStringLiteral("HTTP %1 — %2").arg(status).arg(detail);

    m_transcript.fail(m_answerRow, detail);
    setError(detail);
    clearReply();
}

void ChatClient::clearReply()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
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
