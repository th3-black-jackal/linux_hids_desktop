#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include "../core/ChatModel.h"
#include "BackendSettings.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
QT_END_NAMESPACE

// Talks to the ATT&CK RAG backend through its **OpenAI-compatible** surface
// (OPENAI_API_DOCUMENTATION.md): POST /v1/chat/completions, bearer auth,
// OpenAI chunk streaming.
//
// The backend is a *separate machine* from the broker and from the boards;
// nothing here touches MQTT. The console holds no state the backend needs
// beyond the conversation itself, so this class is: build a request, stream
// the reply into ChatModel, surface failures.
//
// `stream:true` is the default because generation on the backend's GPU is
// slow enough that a blocking call looks like a hang.
//
// Three things differ from the native `/api/attack/chat/` surface this
// replaced, and each one costs code here:
//
//   * **Auth is `Bearer`, not `Token`.** The SIEM API still wants `Token`, so
//     the scheme is picked per call site (BackendSettings::bearerRequest).
//   * **There is no `sources` event.** The retrieved items arrive inside the
//     answer as a markdown `**Sources:** [T0803](url) · …` footer. It is
//     parsed back out into the same `sources` shape the UI already consumes,
//     so the "only link what was actually retrieved" rule survives — but
//     `type` and `score` are simply gone from this surface and are not
//     invented.
//   * **Mid-stream failures are content**, not a distinct event: a delta
//     containing `[error: …]` on an otherwise 200 stream.
class ChatClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(ChatModel *transcript READ transcript CONSTANT)

    // Where the backend is lives in BackendSettings (shared with SiemClient);
    // only the assistant-specific knobs are stored here.
    Q_PROPERTY(int retrievalK READ retrievalK WRITE setRetrievalK NOTIFY settingsChanged)
    Q_PROPERTY(bool streaming READ streaming WRITE setStreaming NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY settingsChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY settingsChanged)
    Q_PROPERTY(QString endpointText READ endpointText NOTIFY settingsChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // unknown | testing | ok | error
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionDetail READ connectionDetail NOTIFY connectionChanged)

    // Question text handed over from elsewhere in the console (the Detections
    // inspector), for the composer to pick up. It is a draft, not a sent
    // message — the operator gets to edit it first.
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)

public:
    explicit ChatClient(BackendSettings *backend, QObject *parent = nullptr);
    ~ChatClient() override;

    ChatModel *transcript() { return &m_transcript; }

    int retrievalK() const { return m_k; }
    void setRetrievalK(int v);
    bool streaming() const { return m_streaming; }
    void setStreaming(bool v);
    // §1: a single model is served and the name is cosmetic, but it is
    // configurable server-side, so it is configurable here too.
    QString model() const { return m_model; }
    void setModel(const QString &v);

    bool configured() const { return m_backend->configured(); }
    QString endpointText() const;

    // Out of line: QPointer<QNetworkReply>'s comparison operators
    // instantiate a static_cast that needs the complete type, and this
    // header only forward-declares it.
    bool busy() const;
    QString lastError() const { return m_lastError; }
    QString connectionState() const { return m_connectionState; }
    QString connectionDetail() const { return m_connectionDetail; }

    QString draft() const { return m_draft; }
    void setDraft(const QString &v);

    Q_INVOKABLE void ask(const QString &question);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retryLast();
    Q_INVOKABLE void newConversation();

    // GET /v1/models (§3.2) — what OpenAI clients call to validate a
    // connection, and the cheapest way to tell "wrong key" from "wrong URL"
    // without starting a generation.
    Q_INVOKABLE void testConnection();

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();
    void busyChanged();
    void lastErrorChanged();
    void draftChanged();
    void connectionChanged();

    // The surface API_DOCUMENTATION.md §6.1 suggests. ChatClient already
    // folds these into `transcript`, but they stay public because the view
    // needs the *edges* (a token arrived → scroll to bottom) and not just
    // the resulting state.
    void sourcesReceived(const QVariantList &sources);
    // The raw delta, for consumers that care about the *edge* (scroll to
    // bottom, a progress dot). It still contains the Sources footer.
    void tokenReceived(const QString &text);
    // The answer as it should be *displayed*: footer lifted out, protocol
    // markers removed. Anything rendering the answer wants this one — a
    // consumer that concatenates tokenReceived ends up showing the raw
    // markdown footer, which is exactly the bug this signal exists to
    // prevent.
    void answerTextChanged(const QString &visibleText);
    void answerFinished();
    void errorOccurred(const QString &detail);

private:
    QNetworkRequest makeRequest(bool sse) const;
    QByteArray requestBody(const QString &question) const;
    void consume();
    void handleFrame(const QByteArray &frame);
    void applyDelta(const QString &delta);
    void publishAnswer();
    void finishReply();
    QString describeHttpError(QNetworkReply *reply, const QByteArray &body) const;
    void setError(const QString &detail);
    void clearReply();

    // Splits a raw answer into the visible body and the `**Sources:**`
    // footer, hiding a partially-received marker so it never flashes on
    // screen mid-stream.
    static QString splitSourcesFooter(const QString &raw, QString *footer);
    // "[T0803](https://…) · [M0810](https://…)" -> the `sources` shape the UI
    // already renders. `type` and `score` are absent from this surface.
    static QVariantList parseSourcesFooter(const QString &footer);

    ChatModel m_transcript;
    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;

    QByteArray m_buffer;
    int m_answerRow = -1;
    // The answer exactly as the backend sent it, footer included. The model
    // holds only the visible part.
    QString m_rawAnswer;
    QVariantList m_sources;
    bool m_sawDone = false;      // the [DONE] sentinel
    bool m_sawStop = false;      // a chunk with finish_reason
    bool m_sawError = false;
    bool m_cancelled = false;
    bool m_httpOk = true;
    QString m_lastQuestion;
    QString m_lastError;
    QString m_draft;

    BackendSettings *m_backend;
    int m_k = 6;
    bool m_streaming = true;
    QString m_model = QStringLiteral("attack-rag");
    bool m_loading = false;

    QPointer<QNetworkReply> m_testReply;
    QString m_connectionState = QStringLiteral("unknown");
    QString m_connectionDetail;

    // §4: earlier user/assistant messages become history for follow-ups, and
    // the server keeps "the most recent few". Six matches what the native
    // surface documented and is plenty of context for a triage question.
    static constexpr int kHistoryTurns = 6;
};
