#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include "../core/ChatModel.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
QT_END_NAMESPACE

// Talks to the ATT&CK RAG backend (Django + Ollama) documented in
// API_DOCUMENTATION.md.
//
// The backend is a *separate machine* from the broker and from the boards;
// nothing here touches MQTT. The console holds no state the backend needs
// beyond the conversation itself, so this class is: build a request, stream
// the reply into ChatModel, surface failures.
//
// Streaming (POST /api/attack/chat/stream/) is the default because generation
// on the backend's GPU is slow enough that a blocking call looks like a hang.
// The blocking endpoint stays available for the case the doc warns about —
// a backend served by WSGI rather than ASGI, where the "stream" arrives in
// one lump at the end anyway.
class ChatClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(ChatModel *transcript READ transcript CONSTANT)

    // Deployment settings, persisted via QSettings (see the storage caveat
    // on the token, below).
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY settingsChanged)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY settingsChanged)
    Q_PROPERTY(int retrievalK READ retrievalK WRITE setRetrievalK NOTIFY settingsChanged)
    Q_PROPERTY(bool streaming READ streaming WRITE setStreaming NOTIFY settingsChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY settingsChanged)
    Q_PROPERTY(bool hasToken READ hasToken NOTIFY settingsChanged)
    Q_PROPERTY(QString endpointText READ endpointText NOTIFY settingsChanged)
    Q_PROPERTY(bool secure READ secure NOTIFY settingsChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // Question text handed over from elsewhere in the console (the Detections
    // inspector), for the composer to pick up. It is a draft, not a sent
    // message — the operator gets to edit it first.
    Q_PROPERTY(QString draft READ draft WRITE setDraft NOTIFY draftChanged)

public:
    explicit ChatClient(QObject *parent = nullptr);
    ~ChatClient() override;

    ChatModel *transcript() { return &m_transcript; }

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &v);
    QString token() const { return m_token; }
    void setToken(const QString &v);
    int retrievalK() const { return m_k; }
    void setRetrievalK(int v);
    bool streaming() const { return m_streaming; }
    void setStreaming(bool v);

    bool configured() const { return !m_baseUrl.isEmpty(); }
    bool hasToken() const { return !m_token.isEmpty(); }
    QString endpointText() const;
    bool secure() const { return m_baseUrl.startsWith(QLatin1String("https://"), Qt::CaseInsensitive); }

    // Out of line: QPointer<QNetworkReply>'s comparison operators
    // instantiate a static_cast that needs the complete type, and this
    // header only forward-declares it.
    bool busy() const;
    QString lastError() const { return m_lastError; }

    QString draft() const { return m_draft; }
    void setDraft(const QString &v);

    Q_INVOKABLE void ask(const QString &question);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retryLast();
    Q_INVOKABLE void newConversation();

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();
    void busyChanged();
    void lastErrorChanged();
    void draftChanged();

    // The surface API_DOCUMENTATION.md §6.1 suggests. ChatClient already
    // folds these into `transcript`, but they stay public because the view
    // needs the *edges* (a token arrived → scroll to bottom) and not just
    // the resulting state.
    void sourcesReceived(const QVariantList &sources);
    void tokenReceived(const QString &text);
    void answerFinished();
    void errorOccurred(const QString &detail);

private:
    QNetworkRequest makeRequest(const QString &path, bool sse) const;
    QByteArray requestBody(const QString &question) const;
    void startStreaming(const QString &question);
    void startBlocking(const QString &question);
    void consume();
    void handleFrame(const QByteArray &frame);
    void finishReply();
    void setError(const QString &detail);
    void clearReply();

    ChatModel m_transcript;
    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;

    QByteArray m_buffer;
    int m_answerRow = -1;
    bool m_sawDone = false;
    bool m_sawError = false;
    bool m_cancelled = false;
    bool m_httpOk = true;
    QString m_lastQuestion;
    QString m_lastError;
    QString m_draft;

    QString m_baseUrl;
    QString m_token;
    int m_k = 6;
    bool m_streaming = true;
    bool m_loading = false;

    // The backend keeps at most the last 6 turns (§3.2); sending more is
    // wasted bytes.
    static constexpr int kHistoryTurns = 6;
};
