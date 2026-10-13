#include "SiemEventsModel.h"

#include <QJsonArray>

SiemEventsModel::SiemEventsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SiemEventsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant SiemEventsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &r = m_rows.at(index.row());
    switch (role) {
    case NRole:
        return QString::number(index.row() + 1).rightJustified(2, QLatin1Char('0'));
    case IdRole:        return r.id;
    case TimeRole:
        // §7.5: timestamps are UTC; show local time, but every filter still
        // goes out as UTC.
        return r.eventTime.isValid()
            ? r.eventTime.toLocalTime().toString(QStringLiteral("MM-dd HH:mm:ss"))
            : QStringLiteral("—");
    case TimeRawRole:   return r.eventTimeRaw;
    case ReceivedAtRole: return r.receivedAtRaw;
    case SeverityRole:  return r.severity;
    case NodeRole:      return r.host;
    case ClassNameRole: return r.className;
    case ClassIdxRole:  return r.classIdx;
    case ConfidenceRole: return r.confidence;
    case ConfidenceTextRole: return QString::number(r.confidence, 'f', 3);
    case AlertRole:     return r.alert;
    case SourceTopicRole: return r.sourceTopic;
    default:            return {};
    }
}

QHash<int, QByteArray> SiemEventsModel::roleNames() const
{
    return {
        { NRole,              "n" },
        { IdRole,             "eventId" },
        { TimeRole,           "time" },
        { TimeRawRole,        "timeRaw" },
        { ReceivedAtRole,     "receivedAt" },
        { SeverityRole,       "severity" },
        { NodeRole,           "node" },
        { ClassNameRole,      "className" },
        { ClassIdxRole,       "classIdx" },
        { ConfidenceRole,     "confidence" },
        { ConfidenceTextRole, "confidenceText" },
        { AlertRole,          "alert" },
        { SourceTopicRole,    "sourceTopic" },
    };
}

bool SiemEventsModel::parseRow(const QJsonObject &o, const SeverityPolicy &policy, Row *out)
{
    Row r;
    r.id            = o.value(QStringLiteral("id")).toInt(-1);
    r.host          = o.value(QStringLiteral("host")).toString();
    r.eventTimeRaw  = o.value(QStringLiteral("event_time")).toString();
    r.receivedAtRaw = o.value(QStringLiteral("received_at")).toString();
    r.classIdx      = o.value(QStringLiteral("class_idx")).toInt(-1);
    r.className     = o.value(QStringLiteral("class_name")).toString();
    r.confidence    = o.value(QStringLiteral("confidence")).toDouble();
    r.alert         = o.value(QStringLiteral("alert")).toBool();
    r.sourceTopic   = o.value(QStringLiteral("source_topic")).toString();

    if (r.id < 0 && r.host.isEmpty())
        return false;

    r.eventTime = QDateTime::fromString(r.eventTimeRaw, Qt::ISODate);

    // Severity via the same policy the live feed uses, so the two views
    // cannot disagree about the same event.
    DetectionEvent ev;
    ev.className  = r.className;
    ev.classIdx   = r.classIdx;
    ev.confidence = r.confidence;
    ev.host       = r.host;
    r.severity    = policy.severityFor(ev);

    *out = r;
    return true;
}

void SiemEventsModel::applyPage(const QJsonObject &envelope, bool append)
{
    m_nextUrl = envelope.value(QStringLiteral("next")).toString();
    m_totalCount = envelope.value(QStringLiteral("count")).toInt();

    const QJsonArray results = envelope.value(QStringLiteral("results")).toArray();

    QVector<Row> parsed;
    parsed.reserve(results.size());
    for (const QJsonValue &v : results) {
        Row r;
        if (parseRow(v.toObject(), m_policy, &r))
            parsed.append(r);
    }

    if (!append) {
        beginResetModel();
        m_rows = parsed;
        endResetModel();
    } else if (!parsed.isEmpty()) {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + parsed.size() - 1);
        m_rows += parsed;
        endInsertRows();
    }

    emit countChanged();
}

void SiemEventsModel::reset()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
    m_nextUrl.clear();
    m_totalCount = 0;
    emit countChanged();
}

int SiemEventsModel::idAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return -1;
    return m_rows.at(row).id;
}

QString SiemEventsModel::hostAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    return m_rows.at(row).host;
}
