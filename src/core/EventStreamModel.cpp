#include "EventStreamModel.h"

EventStreamModel::EventStreamModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EventStreamModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant EventStreamModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case TimeRole:           return e.time;
    case NodeRole:           return e.node;
    case SeverityRole:       return e.severity;
    case ClassNameRole:      return e.className;
    case ConfidenceTextRole: return e.confidenceText;
    case FeatureCountRole:   return e.featureCount;
    default:                 return {};
    }
}

QHash<int, QByteArray> EventStreamModel::roleNames() const
{
    return {
        { TimeRole, "time" },
        { NodeRole, "node" },
        { SeverityRole, "severity" },
        { ClassNameRole, "className" },
        { ConfidenceTextRole, "confidenceText" },
        { FeatureCountRole, "featureCount" },
    };
}

void EventStreamModel::pushEvent(const Entry &entry)
{
    beginInsertRows(QModelIndex(), 0, 0);
    m_entries.prepend(entry);
    endInsertRows();

    if (m_entries.size() > kMaxRows) {
        beginRemoveRows(QModelIndex(), kMaxRows, m_entries.size() - 1);
        m_entries.resize(kMaxRows);
        endRemoveRows();
    }
}

void EventStreamModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}
