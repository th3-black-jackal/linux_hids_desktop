#pragma once

#include <QAbstractListModel>
#include <QVector>

// Service log tail shown on the Nodes screen (ironwatchd stdout/journal
// mirror, as forwarded over the telemetry channel).
class LogModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles { TimeRole = Qt::UserRole + 1, MessageRole };
    Q_ENUM(Roles)

    struct Entry {
        QString t, m;
    };

    explicit LogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void pushEntry(const Entry &entry);
    void seed(const QVector<Entry> &entries);

private:
    static constexpr int kMaxRows = 200;
    QVector<Entry> m_entries; // newest first
};
