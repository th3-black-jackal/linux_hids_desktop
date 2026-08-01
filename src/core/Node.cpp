#include "Node.h"

namespace {

QString humanAge(const QDateTime &then)
{
    if (!then.isValid())
        return QStringLiteral("never");
    const qint64 secs = then.secsTo(QDateTime::currentDateTime());
    if (secs < 2)    return QStringLiteral("just now");
    if (secs < 60)   return QStringLiteral("%1 s ago").arg(secs);
    if (secs < 3600) return QStringLiteral("%1 min ago").arg(secs / 60);
    return QStringLiteral("%1 h ago").arg(secs / 3600);
}

} // namespace

Node::Node(QString host, QObject *parent)
    : QObject(parent)
    , m_host(std::move(host))
{
}

QString Node::state() const
{
    if (!m_online)
        return QStringLiteral("offline");

    // Stale detection: allow three missed windows before complaining, so
    // ordinary jitter doesn't flap the badge. Falls back to 15 s until the
    // period has been observed.
    if (m_lastEventAt.isValid()) {
        const double budget = (m_meanPeriodSecs > 0.0 ? m_meanPeriodSecs : 5.0) * 3.0;
        if (m_lastEventAt.secsTo(QDateTime::currentDateTime()) > qMax(15.0, budget))
            return QStringLiteral("stale");
    }
    return QStringLiteral("online");
}

QString Node::lastSeen() const { return humanAge(m_lastEventAt); }

QString Node::windowPeriod() const
{
    if (m_meanPeriodSecs <= 0.0)
        return QStringLiteral("—");
    return QStringLiteral("%1 s").arg(m_meanPeriodSecs, 0, 'f', 1);
}

void Node::applyStatus(bool online, const QString &reason)
{
    if (m_online == online && m_statusReason == reason)
        return;
    m_online = online;
    m_statusReason = reason;
    emit statsChanged();
}

void Node::applyEvent(const DetectionEvent &ev, bool isAlert)
{
    // An event proves the board is alive even if its retained status was
    // missed (e.g. the console subscribed between the board's connect and
    // its status publish).
    m_online = true;

    m_prevEventAt = m_lastEventAt;
    m_lastEventAt = ev.effectiveTime();

    if (m_prevEventAt.isValid() && m_lastEventAt.isValid()) {
        const double delta = m_prevEventAt.msecsTo(m_lastEventAt) / 1000.0;
        if (delta > 0.0 && delta < 3600.0) {
            m_meanPeriodSecs = (m_meanPeriodSecs <= 0.0)
                ? delta
                : (m_meanPeriodSecs * 0.8 + delta * 0.2);
        }
    }

    m_windows++;
    if (isAlert)
        m_alerts++;
    m_lastClass = ev.className;
    m_lastConfidence = ev.confidence;
    m_featureCount = ev.features.size();
    m_classCounts[ev.className]++;

    m_confidenceHistory.append(ev.confidence);
    if (m_confidenceHistory.size() > kHistoryLen)
        m_confidenceHistory.remove(0, m_confidenceHistory.size() - kHistoryLen);

    emit statsChanged();
}

void Node::refreshStats()
{
    // Ticked once a second so "last seen" and the stale check stay live.
    emit statsChanged();
}
