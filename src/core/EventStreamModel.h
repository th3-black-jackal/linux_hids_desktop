#pragma once

#include <QAbstractListModel>
#include <QVector>

// Rolling live feed of *every* scored window, normal ones included — the
// service broadcasts one telemetry frame per Config::windowSec regardless of
// classification, and this is the unfiltered view of that stream. The
// Detections screen shows only the non-normal subset.
class EventStreamModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimeRole = Qt::UserRole + 1,
        NodeRole,
        SeverityRole,
        ClassNameRole,
        ConfidenceTextRole,
        FeatureCountRole,
    };
    Q_ENUM(Roles)

    struct Entry {
        QString time;
        QString node;
        QString severity;
        QString className;
        QString confidenceText;
        int featureCount = 0;
    };

    explicit EventStreamModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void pushEvent(const Entry &entry);
    void clear();

private:
    static constexpr int kMaxRows = 200;
    QVector<Entry> m_entries; // newest first
};
