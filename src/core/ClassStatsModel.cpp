#include "ClassStatsModel.h"

ClassStatsModel::ClassStatsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ClassStatsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant ClassStatsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &r = m_rows.at(index.row());
    const double share = m_total > 0 ? (double(r.count) / double(m_total)) * 100.0 : 0.0;

    switch (role) {
    case ClassIdxRole:       return r.classIdx;
    case ClassNameRole:      return r.className;
    case CountRole:          return r.count;
    case ShareRole:          return int(share);
    case ShareTextRole:      return QStringLiteral("%1 %").arg(share, 0, 'f', 1);
    case MeanConfidenceRole: return r.count > 0 ? QString::number(r.confidenceSum / r.count, 'f', 3)
                                                : QStringLiteral("—");
    case PeakConfidenceRole: return QString::number(r.peakConfidence, 'f', 3);
    case IsNormalRole:       return r.isNormal;
    default:                 return {};
    }
}

QHash<int, QByteArray> ClassStatsModel::roleNames() const
{
    return {
        { ClassIdxRole, "classIdx" },
        { ClassNameRole, "className" },
        { CountRole, "count" },
        { ShareRole, "share" },
        { ShareTextRole, "shareText" },
        { MeanConfidenceRole, "meanConfidence" },
        { PeakConfidenceRole, "peakConfidence" },
        { IsNormalRole, "isNormal" },
    };
}

void ClassStatsModel::record(int classIdx, const QString &className, double confidence, bool isNormal)
{
    m_total++;

    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        if (r.classIdx == classIdx && r.className == className) {
            r.count++;
            r.confidenceSum += confidence;
            r.peakConfidence = qMax(r.peakConfidence, confidence);
            // Every row's share moves when the total does.
            emit dataChanged(index(0), index(m_rows.size() - 1));
            emit statsChanged();
            return;
        }
    }

    Row r;
    r.classIdx = classIdx;
    r.className = className;
    r.count = 1;
    r.confidenceSum = confidence;
    r.peakConfidence = confidence;
    r.isNormal = isNormal;

    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
    m_rows.append(r);
    endInsertRows();

    // Inserting bumps m_total, which is the denominator for *every* row's
    // share — without this the pre-existing rows keep the share they had one
    // window ago and the column stops summing to 100%.
    if (m_rows.size() > 1)
        emit dataChanged(index(0), index(m_rows.size() - 2), { ShareRole, ShareTextRole });

    emit statsChanged();
}

void ClassStatsModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_total = 0;
    endResetModel();
    emit statsChanged();
}
