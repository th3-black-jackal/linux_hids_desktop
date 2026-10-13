#include "EventWebLink.h"

#include <QDesktopServices>
#include <QSettings>
#include <QUrlQuery>

namespace {

// Percent-encodes a value for use inside a query parameter. Class names come
// off the wire and are not guaranteed to be tame — the unicode scenario in
// the test publisher sends a non-ASCII one — so nothing is interpolated raw.
QString enc(const QString &v)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(v));
}

} // namespace

EventWebLink::EventWebLink(QObject *parent)
    : QObject(parent)
    , m_template(defaultTemplate())
{
}

QString EventWebLink::defaultTemplate()
{
    return QStringLiteral("https://orchicyb-ai-attack.lovable.app/"
                          "?event={id}&host={host}&class={class}&time={time}");
}

void EventWebLink::loadSettings()
{
    m_loading = true;
    QSettings s;
    m_template = s.value(QStringLiteral("weblink/template"), defaultTemplate()).toString();
    m_loading = false;
    emit changed();
}

void EventWebLink::saveSettings()
{
    if (m_loading)
        return;
    QSettings s;
    s.setValue(QStringLiteral("weblink/template"), m_template);
}

void EventWebLink::setUrlTemplate(const QString &v)
{
    const QString n = v.trimmed();
    if (m_template == n)
        return;
    m_template = n;
    saveSettings();
    emit changed();
}

void EventWebLink::resetTemplate()
{
    setUrlTemplate(defaultTemplate());
}

QString EventWebLink::buildUrl(const QString &id, const QString &uid, const QString &host,
                               const QString &className, int classIdx,
                               const QString &confidenceText, const QString &timestampRaw,
                               const QString &severity) const
{
    if (m_template.isEmpty())
        return {};

    QString url = m_template;
    url.replace(QLatin1String("{id}"),         enc(id));
    url.replace(QLatin1String("{uid}"),        enc(uid));
    url.replace(QLatin1String("{host}"),       enc(host));
    url.replace(QLatin1String("{class}"),      enc(className));
    url.replace(QLatin1String("{classIdx}"),   classIdx >= 0 ? QString::number(classIdx) : QString());
    url.replace(QLatin1String("{confidence}"), enc(confidenceText));
    url.replace(QLatin1String("{time}"),       enc(timestampRaw));
    url.replace(QLatin1String("{severity}"),   enc(severity));

    // Drop query parameters whose placeholder resolved to nothing, so the one
    // template serves both a database row (which has an id) and a live
    // detection (which has none) without sending "?event=" either way.
    QUrl u(url);
    if (u.hasQuery()) {
        const QUrlQuery in(u);
        QUrlQuery out;
        for (const auto &item : in.queryItems(QUrl::FullyEncoded)) {
            if (!item.second.isEmpty())
                out.addQueryItem(item.first, item.second);
        }
        if (out.isEmpty())
            u.setQuery(QString());
        else
            u.setQuery(out);
    }
    return u.toString(QUrl::FullyEncoded);
}

void EventWebLink::openEvent(const QString &id, const QString &uid, const QString &host,
                             const QString &className, int classIdx,
                             const QString &confidenceText, const QString &timestampRaw,
                             const QString &severity)
{
    m_lastError.clear();

    const QString built = buildUrl(id, uid, host, className, classIdx,
                                   confidenceText, timestampRaw, severity);
    if (built.isEmpty()) {
        m_lastError = QStringLiteral("No web app URL configured.");
        emit lastUrlChanged();
        emit logged(QStringLiteral("web link: %1").arg(m_lastError));
        return;
    }

    const QUrl url(built);
    // Hand only http(s) to the desktop. The values are percent-encoded so
    // they cannot inject a scheme, but a mistyped template could — and
    // handing an arbitrary scheme to the OS opener is not something a
    // security tool should do on a keystroke.
    if (!url.isValid() || (url.scheme() != QLatin1String("http")
                           && url.scheme() != QLatin1String("https"))) {
        m_lastError = QStringLiteral("Refusing to open \"%1\" — the template must "
                                     "produce an http or https URL.").arg(built);
        emit lastUrlChanged();
        emit logged(QStringLiteral("web link: %1").arg(m_lastError));
        return;
    }

    m_lastUrl = built;
    emit lastUrlChanged();
    emit logged(QStringLiteral("web link: opening %1").arg(uid.isEmpty() ? built : uid));

    if (!QDesktopServices::openUrl(url)) {
        // Happens on a machine with no registered browser, and silently
        // otherwise — so it is worth reporting rather than looking like a
        // click that did nothing.
        m_lastError = QStringLiteral("The desktop could not open a browser for this URL.");
        emit lastUrlChanged();
        emit logged(QStringLiteral("web link: %1").arg(m_lastError));
    }
}
