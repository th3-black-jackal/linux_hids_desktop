#include "NodesModel.h"

NodesModel::NodesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int NodesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_nodes.size();
}

QVariant NodesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return {};

    const Node *n = m_nodes.at(index.row());
    switch (role) {
    case IdRole:             return n->id();
    case StateRole:          return n->state();
    case OnlineRole:         return n->online();
    case StatusReasonRole:   return n->statusReason();
    case LastSeenRole:       return n->lastSeen();
    case WindowsRole:        return n->windows();
    case AlertsRole:         return n->alerts();
    case LastClassRole:      return n->lastClass();
    case LastConfidenceRole: return n->lastConfidence();
    case WindowPeriodRole:   return n->windowPeriod();
    case FeatureCountRole:   return n->featureCount();
    default:                 return {};
    }
}

QHash<int, QByteArray> NodesModel::roleNames() const
{
    return {
        { IdRole, "id" },
        { StateRole, "state" },
        { OnlineRole, "online" },
        { StatusReasonRole, "statusReason" },
        { LastSeenRole, "lastSeen" },
        { WindowsRole, "windows" },
        { AlertsRole, "alerts" },
        { LastClassRole, "lastClass" },
        { LastConfidenceRole, "lastConfidence" },
        { WindowPeriodRole, "windowPeriod" },
        { FeatureCountRole, "featureCount" },
    };
}

void NodesModel::watch(Node *node)
{
    connect(node, &Node::statsChanged, this, [this, node]() {
        const int row = m_nodes.indexOf(node);
        if (row < 0)
            return;
        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
        emit countChanged(); // onlineCount may have moved
    });
}

Node *NodesModel::ensureNode(const QString &host)
{
    const int existing = indexForHost(host);
    if (existing >= 0)
        return m_nodes.at(existing);

    auto *node = new Node(host, this);
    watch(node);

    beginInsertRows(QModelIndex(), m_nodes.size(), m_nodes.size());
    m_nodes.append(node);
    endInsertRows();

    emit countChanged();
    emit nodeAdded(host);
    return node;
}

Node *NodesModel::nodeAt(int index) const
{
    if (index < 0 || index >= m_nodes.size())
        return nullptr;
    return m_nodes.at(index);
}

int NodesModel::indexForHost(const QString &host) const
{
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes.at(i)->host() == host)
            return i;
    }
    return -1;
}

int NodesModel::onlineCount() const
{
    int n = 0;
    for (const Node *node : m_nodes) {
        if (node->state() == QLatin1String("online"))
            ++n;
    }
    return n;
}
