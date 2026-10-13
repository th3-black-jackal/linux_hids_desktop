#include "BackendSettings.h"

#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

namespace {

// Accepts what an operator would actually type — "10.0.0.5",
// "10.0.0.5:8000", "http://host:8000/" — and normalizes it to a
// scheme-qualified origin with no trailing slash, since every API path is
// appended verbatim.
QString normalize(QString v)
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

BackendSettings::BackendSettings(QObject *parent)
    : QObject(parent)
{
}

bool BackendSettings::secure() const
{
    return m_baseUrl.startsWith(QLatin1String("https://"), Qt::CaseInsensitive);
}

QUrl BackendSettings::url(const QString &path) const
{
    return QUrl(m_baseUrl + path);
}

QNetworkRequest BackendSettings::request(const QUrl &u) const
{
    QNetworkRequest req(u);
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", "Token " + m_token.toUtf8());
    req.setRawHeader("Accept", "application/json");
    return req;
}

QNetworkRequest BackendSettings::bearerRequest(const QUrl &u) const
{
    // Two APIs on one host with two auth schemes: the SIEM API wants DRF's
    // `Token`, the OpenAI-compatible chat API wants `Bearer`. Sending the
    // wrong one is a 401 with nothing to distinguish it from a bad
    // credential, so the scheme is chosen at the call site rather than
    // guessed here.
    QNetworkRequest req(u);
    const QString key = effectiveApiKey();
    if (!key.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    req.setRawHeader("Accept", "application/json");
    return req;
}

void BackendSettings::load()
{
    m_loading = true;

    QSettings s;
    m_baseUrl = s.value(QStringLiteral("backend/baseUrl")).toString();
    m_token   = s.value(QStringLiteral("backend/token")).toString();
    m_apiKey  = s.value(QStringLiteral("backend/apiKey")).toString();

    // Migration: these lived under assistant/* when the RAG chat was the
    // only thing on this host. Carry them forward once so an existing
    // install does not silently come up unconfigured.
    if (m_baseUrl.isEmpty() && m_token.isEmpty()) {
        m_baseUrl = s.value(QStringLiteral("assistant/baseUrl")).toString();
        m_token   = s.value(QStringLiteral("assistant/token")).toString();
        if (!m_baseUrl.isEmpty() || !m_token.isEmpty()) {
            m_loading = false;
            save();
            m_loading = true;
        }
    }

    m_loading = false;
    emit changed();
}

void BackendSettings::save()
{
    if (m_loading)
        return;

    QSettings s;
    s.setValue(QStringLiteral("backend/baseUrl"), m_baseUrl);
    // NOTE: stored in the clear (registry on Windows, plain INI elsewhere),
    // exactly like the broker password. A DRF token is a bearer credential
    // for the *whole* backend — both APIs — so this is the more valuable of
    // the two secrets this console holds. Platform keychain before this
    // leaves a lab.
    s.setValue(QStringLiteral("backend/token"), m_token);
    s.setValue(QStringLiteral("backend/apiKey"), m_apiKey);
}

void BackendSettings::setBaseUrl(const QString &v)
{
    const QString n = normalize(v);
    if (m_baseUrl == n)
        return;
    m_baseUrl = n;
    save();
    emit changed();
}

void BackendSettings::setToken(const QString &v)
{
    const QString n = v.trimmed();
    if (m_token == n)
        return;
    m_token = n;
    save();
    emit changed();
}

void BackendSettings::setApiKey(const QString &v)
{
    const QString n = v.trimmed();
    if (m_apiKey == n)
        return;
    m_apiKey = n;
    save();
    emit changed();
}

bool BackendSettings::tryAcquireGeneration()
{
    if (m_generating)
        return false;
    m_generating = true;
    emit generatingChanged();
    return true;
}

void BackendSettings::releaseGeneration()
{
    if (!m_generating)
        return;
    m_generating = false;
    emit generatingChanged();
}
