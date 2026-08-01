#include "Detection.h"

#include <QVariantMap>
#include <algorithm>
#include <cmath>

Detection::Detection(const DetectionEvent &ev, QString uid, QString severity,
                     QStringList featureNames, QObject *parent)
    : QObject(parent)
    , m_ev(ev)
    , m_uid(std::move(uid))
    , m_severity(std::move(severity))
{
    // Rank by absolute deviation: the vector is already normalized by
    // Model::normalize() on the board, so magnitude is "how far from the
    // training mean", which is the useful thing to eyeball during triage.
    struct Entry { int idx; double value; };
    QVector<Entry> entries;
    entries.reserve(m_ev.features.size());
    for (int i = 0; i < m_ev.features.size(); ++i)
        entries.append({ i, m_ev.features.at(i) });

    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return std::abs(a.value) > std::abs(b.value);
    });

    const double maxAbs = entries.isEmpty() ? 0.0 : std::abs(entries.first().value);

    const int shown = static_cast<int>(std::min<qsizetype>(entries.size(), 12));
    for (int i = 0; i < shown; ++i) {
        const Entry &e = entries.at(i);
        const QString name = (e.idx < featureNames.size() && !featureNames.at(e.idx).isEmpty())
            ? featureNames.at(e.idx)
            : QStringLiteral("f[%1]").arg(e.idx);
        m_features.append(QVariantMap{
            { QStringLiteral("name"), name },
            { QStringLiteral("index"), e.idx },
            { QStringLiteral("value"), e.value },
            { QStringLiteral("valueText"), QString::number(e.value, 'f', 3) },
            { QStringLiteral("pct"), maxAbs > 0.0 ? int(std::abs(e.value) / maxAbs * 100.0) : 0 },
        });
    }
}

QString Detection::time() const
{
    const QDateTime t = m_ev.effectiveTime();
    return t.isValid() ? t.toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("—");
}

QString Detection::confidenceText() const
{
    return QString::number(m_ev.confidence, 'f', 3);
}

void Detection::setAcknowledged(bool v)
{
    if (m_acked == v)
        return;
    m_acked = v;
    emit acknowledgedChanged();
}

void Detection::setDisposition(const QString &v)
{
    if (m_disposition == v)
        return;
    m_disposition = v;
    emit dispositionChanged();
}
