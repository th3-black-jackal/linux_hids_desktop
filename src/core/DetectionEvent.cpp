#include "DetectionEvent.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

// Returns the first present key from `names`, or an undefined value.
QJsonValue pick(const QJsonObject &o, std::initializer_list<const char *> names)
{
    for (const char *n : names) {
        const auto it = o.constFind(QLatin1String(n));
        if (it != o.constEnd() && !it->isNull() && !it->isUndefined())
            return *it;
    }
    return QJsonValue(QJsonValue::Undefined);
}

// The service emits ISO-8601; be liberal about the exact flavour.
QDateTime parseTimestamp(const QString &raw)
{
    if (raw.isEmpty())
        return {};
    QDateTime dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(raw, Qt::ISODate);
    if (!dt.isValid())
        dt = QDateTime::fromString(raw, Qt::RFC2822Date);
    return dt;
}

} // namespace

std::optional<DetectionEvent> DetectionEvent::parse(const QJsonObject &obj, QString *error)
{
    // Tolerate the event being wrapped one level deep, e.g. {"event":{...}}.
    QJsonObject o = obj;
    for (const char *wrapper : { "event", "detection", "data" }) {
        if (o.size() == 1 && o.contains(QLatin1String(wrapper))
            && o.value(QLatin1String(wrapper)).isObject()) {
            o = o.value(QLatin1String(wrapper)).toObject();
            break;
        }
    }

    DetectionEvent ev;
    ev.receivedAt = QDateTime::currentDateTime();

    const QJsonValue vTs    = pick(o, { "timestamp", "ts", "time", "datetime" });
    const QJsonValue vIdx   = pick(o, { "classIdx", "class_idx", "classIndex", "class_index", "idx", "class_id" });
    const QJsonValue vName  = pick(o, { "className", "class_name", "class", "label", "prediction" });
    const QJsonValue vConf  = pick(o, { "confidence", "conf", "score", "probability", "prob" });
    const QJsonValue vHost  = pick(o, { "host", "hostname", "node", "agent" });
    const QJsonValue vFeat  = pick(o, { "features", "feature_vector", "featureVector", "feats", "x" });

    if (vTs.isString()) {
        ev.timestampRaw = vTs.toString();
        ev.timestamp = parseTimestamp(ev.timestampRaw);
    } else if (vTs.isDouble()) {
        // Some to_json implementations emit epoch seconds instead.
        const double secs = vTs.toDouble();
        ev.timestamp = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(secs * 1000.0));
        ev.timestampRaw = ev.timestamp.toString(Qt::ISODate);
    }

    if (vIdx.isDouble())
        ev.classIdx = vIdx.toInt(-1);
    if (vName.isString())
        ev.className = vName.toString();
    if (vConf.isDouble())
        ev.confidence = vConf.toDouble();
    if (vHost.isString())
        ev.host = vHost.toString();

    if (vFeat.isArray()) {
        const QJsonArray arr = vFeat.toArray();
        ev.features.reserve(arr.size());
        for (const QJsonValue &v : arr)
            ev.features.append(v.toDouble());
    }

    // A frame with neither a class index nor a class name is not something
    // this console knows how to display; surface it rather than swallow it.
    if (!ev.isValid()) {
        if (error) {
            *error = QStringLiteral("frame has no recognizable class field (keys: %1)")
                         .arg(o.keys().join(QLatin1String(", ")));
        }
        return std::nullopt;
    }

    if (ev.className.isEmpty())
        ev.className = QStringLiteral("class %1").arg(ev.classIdx);

    return ev;
}

bool SeverityPolicy::isNormal(const QString &className) const
{
    for (const QString &n : normalClasses) {
        if (className.compare(n, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QString SeverityPolicy::severityFor(const DetectionEvent &ev) const
{
    if (isNormal(ev.className))
        return QStringLiteral("info");
    if (ev.confidence >= criticalAt)
        return QStringLiteral("critical");
    if (ev.confidence >= highAt)
        return QStringLiteral("high");
    if (ev.confidence >= mediumAt)
        return QStringLiteral("medium");
    return QStringLiteral("low");
}
