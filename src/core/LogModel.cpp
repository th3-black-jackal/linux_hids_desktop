#include "LogModel.h"

LogModel::LogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case TimeRole: return e.t;
    case MessageRole: return e.m;
    default: return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {
        { TimeRole, "t" },
        { MessageRole, "m" },
    };
}

void LogModel::pushEntry(const Entry &entry)
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

void LogModel::seed(const QVector<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}
