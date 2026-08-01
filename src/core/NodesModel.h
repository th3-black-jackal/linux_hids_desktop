#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "Node.h"

// Fleet inventory. Rows appear as boards are discovered from the broker;
// nothing here is configured by hand.
class NodesModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        StateRole,
        OnlineRole,
        StatusReasonRole,
        LastSeenRole,
        WindowsRole,
        AlertsRole,
        LastClassRole,
        LastConfidenceRole,
        WindowPeriodRole,
        FeatureCountRole,
    };
    Q_ENUM(Roles)

    explicit NodesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Returns the existing node for `host`, creating one if this is the
    // first time the console has heard of that board.
    Node *ensureNode(const QString &host);

    Q_INVOKABLE Node *nodeAt(int index) const;
    Q_INVOKABLE int indexForHost(const QString &host) const;

    int count() const { return m_nodes.size(); }
    int onlineCount() const;

signals:
    void countChanged();
    void nodeAdded(const QString &host);

private:
    void watch(Node *node);

    QVector<Node *> m_nodes;
};
