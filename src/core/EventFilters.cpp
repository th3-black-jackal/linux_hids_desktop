#include "EventFilters.h"

EventFilters::EventFilters(QObject *parent)
    : QObject(parent)
{
}

void EventFilters::set(QString *slot, const QString &value)
{
    const QString v = value.trimmed();
    if (*slot == v)
        return;
    *slot = v;
    emit changed();
}

void EventFilters::setQuickRange(const QString &v)
{
    if (m_quickRange == v)
        return;
    m_quickRange = v;
    // A quick range and an explicit window are mutually exclusive: sending
    // `hours=24` alongside `since=` would apply both and silently intersect
    // them, which is not what either control looks like it does.
    if (!v.isEmpty()) {
        m_since.clear();
        m_until.clear();
        m_date.clear();
    }
    emit changed();
}

void EventFilters::setSince(const QString &v)
{
    if (!v.trimmed().isEmpty())
        m_quickRange.clear();
    set(&m_since, v);
}

void EventFilters::setUntil(const QString &v)
{
    if (!v.trimmed().isEmpty())
        m_quickRange.clear();
    set(&m_until, v);
}

void EventFilters::setDate(const QString &v)
{
    if (!v.trimmed().isEmpty()) {
        m_quickRange.clear();
        m_since.clear();
        m_until.clear();
    }
    set(&m_date, v);
}

void EventFilters::setType(const QString &v)        { set(&m_type, v); }
void EventFilters::setClassName(const QString &v)   { set(&m_className, v); }
void EventFilters::setHost(const QString &v)        { set(&m_host, v); }
void EventFilters::setSourceTopic(const QString &v) { set(&m_sourceTopic, v); }
void EventFilters::setMinConfidence(const QString &v) { set(&m_minConfidence, v); }
void EventFilters::setOrdering(const QString &v)    { set(&m_ordering, v); }

void EventFilters::setPageSize(int v)
{
    // §5.4: default 50, max 500. Clamping here rather than letting the
    // backend 400 — this one is a console-side control, not operator input.
    v = qBound(1, v, 500);
    if (m_pageSize == v)
        return;
    m_pageSize = v;
    emit changed();
}

bool EventFilters::isDefault() const
{
    return m_quickRange == QLatin1String("24h")
        && m_since.isEmpty() && m_until.isEmpty() && m_date.isEmpty()
        && m_type.isEmpty() && m_className.isEmpty() && m_host.isEmpty()
        && m_sourceTopic.isEmpty() && m_minConfidence.isEmpty()
        && m_ordering == QLatin1String("-event_time");
}

QVariantMap EventFilters::toQuery() const
{
    QVariantMap q;

    if (m_quickRange == QLatin1String("1h"))       q[QStringLiteral("hours")] = 1;
    else if (m_quickRange == QLatin1String("24h")) q[QStringLiteral("hours")] = 24;
    else if (m_quickRange == QLatin1String("7d"))  q[QStringLiteral("days")] = 7;

    if (!m_date.isEmpty())  q[QStringLiteral("date")] = m_date;
    if (!m_since.isEmpty()) q[QStringLiteral("since")] = m_since;
    if (!m_until.isEmpty()) q[QStringLiteral("until")] = m_until;

    if (!m_type.isEmpty())          q[QStringLiteral("type")] = m_type;
    if (!m_className.isEmpty())     q[QStringLiteral("class_name")] = m_className;
    if (!m_host.isEmpty())          q[QStringLiteral("host")] = m_host;
    if (!m_sourceTopic.isEmpty())   q[QStringLiteral("source_topic")] = m_sourceTopic;
    if (!m_minConfidence.isEmpty()) q[QStringLiteral("min_confidence")] = m_minConfidence;

    if (!m_ordering.isEmpty()) q[QStringLiteral("ordering")] = m_ordering;
    q[QStringLiteral("page_size")] = m_pageSize;

    return q;
}

QString EventFilters::summary() const
{
    QStringList parts;

    if (m_quickRange == QLatin1String("1h"))       parts << QStringLiteral("last hour");
    else if (m_quickRange == QLatin1String("24h")) parts << QStringLiteral("last 24 h");
    else if (m_quickRange == QLatin1String("7d"))  parts << QStringLiteral("last 7 days");
    else if (!m_date.isEmpty())                    parts << m_date;
    else if (!m_since.isEmpty() || !m_until.isEmpty())
        parts << QStringLiteral("%1 → %2")
                     .arg(m_since.isEmpty() ? QStringLiteral("start") : m_since,
                          m_until.isEmpty() ? QStringLiteral("now") : m_until);
    else
        parts << QStringLiteral("all time");

    if (m_type == QLatin1String("alert"))      parts << QStringLiteral("alerts only");
    else if (m_type == QLatin1String("event")) parts << QStringLiteral("all events");
    else if (!m_type.isEmpty())                parts << m_type;

    if (!m_className.isEmpty())     parts << QStringLiteral("class %1").arg(m_className);
    if (!m_host.isEmpty())          parts << m_host;
    if (!m_sourceTopic.isEmpty())   parts << QStringLiteral("topic %1").arg(m_sourceTopic);
    if (!m_minConfidence.isEmpty()) parts << QStringLiteral("conf ≥ %1").arg(m_minConfidence);

    return parts.join(QStringLiteral(" · "));
}

void EventFilters::toggleOrdering(const QString &field)
{
    // First click on a new column sorts descending — for a time-ordered feed
    // "newest first" is what an operator means by "sort by time".
    if (m_ordering == field)
        m_ordering = QStringLiteral("-") + field;
    else if (m_ordering == QStringLiteral("-") + field)
        m_ordering = field;
    else
        m_ordering = QStringLiteral("-") + field;
    emit changed();
}

QString EventFilters::orderingFor(const QString &field) const
{
    if (m_ordering == field)
        return QStringLiteral("asc");
    if (m_ordering == QStringLiteral("-") + field)
        return QStringLiteral("desc");
    return QString();
}

void EventFilters::clear()
{
    m_quickRange = QStringLiteral("24h");
    m_since.clear();
    m_until.clear();
    m_date.clear();
    m_type.clear();
    m_className.clear();
    m_host.clear();
    m_sourceTopic.clear();
    m_minConfidence.clear();
    m_ordering = QStringLiteral("-event_time");
    emit changed();
}
