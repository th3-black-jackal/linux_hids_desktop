#pragma once

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QNetworkRequest;
class QUrl;
QT_END_NAMESPACE

// Where the Django backend is, and the token to reach it.
//
// Two separate APIs live on that host — `/api/attack/` (the RAG assistant)
// and `/api/siem/` (the read-only telemetry store) — and both documents
// specify the same base URL form and the same `Authorization: Token …`
// scheme. Configuring them twice would only let the two copies drift, so
// ChatClient and SiemClient share this one object.
//
// If a deployment ever splits them onto different hosts, this is the single
// place that has to grow a second pair of fields.
class BackendSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY changed)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY changed)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY changed)
    Q_PROPERTY(QString effectiveApiKey READ effectiveApiKey NOTIFY changed)
    Q_PROPERTY(bool apiKeyIsToken READ apiKeyIsToken NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool hasToken READ hasToken NOTIFY changed)
    Q_PROPERTY(bool secure READ secure NOTIFY changed)
    Q_PROPERTY(bool generating READ generating NOTIFY generatingChanged)

public:
    explicit BackendSettings(QObject *parent = nullptr);

    QString baseUrl() const { return m_baseUrl; }
    void setBaseUrl(const QString &v);
    QString token() const { return m_token; }
    void setToken(const QString &v);

    // The chat API is OpenAI-compatible and authenticates with
    // `Authorization: Bearer <key>`, where the key is one of the server's
    // configured `ATTACK_RAG_OPENAI_API_KEYS` (`sk-attack-…`) — *or*, per
    // OPENAI_API_DOCUMENTATION.md §2, a valid backend token. So this can be
    // left empty on a deployment that accepts the SIEM token for both, and
    // set when the operator has been issued a separate `sk-` key.
    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString &v);
    QString effectiveApiKey() const { return m_apiKey.isEmpty() ? m_token : m_apiKey; }
    bool apiKeyIsToken() const { return m_apiKey.isEmpty() && !m_token.isEmpty(); }

    bool configured() const { return !m_baseUrl.isEmpty(); }
    bool hasToken() const { return !m_token.isEmpty(); }
    bool secure() const;

    // Absolute URL for an API path ("/api/siem/events/").
    QUrl url(const QString &path) const;
    // With `Authorization: Token …` — the SIEM read API (§2 of
    // SIEM_API_DOCUMENTATION.md).
    QNetworkRequest request(const QUrl &url) const;
    // With `Authorization: Bearer …` — the OpenAI-compatible chat API.
    QNetworkRequest bearerRequest(const QUrl &url) const;

    void load();
    void save();

    // §6.7 of API_DOCUMENTATION.md: one in-flight generation per backend GPU.
    // The console now has two things that can ask — the Assistant screen and
    // the right-click explain popup — so the constraint has to be enforced
    // across both, not inside one client. Whoever holds the slot finishes
    // first; the other is told why instead of being silently queued behind a
    // request it cannot see.
    bool generating() const { return m_generating; }
    bool tryAcquireGeneration();
    void releaseGeneration();

signals:
    void changed();
    void generatingChanged();

private:
    QString m_baseUrl;
    QString m_token;
    QString m_apiKey;
    bool m_loading = false;
    bool m_generating = false;
};
