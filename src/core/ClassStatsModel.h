#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

// Observed distribution of the model's output classes.
//
// The console cannot read the model's metadata — CLASS_NAMES, n_in/n_out,
// the build version, training metrics and the alert thresholds all live on
// the board (model_meta.hpp / hids.conf) and are never transmitted. What the
// console *can* do is report what it has actually seen come off the wire,
// which is what this model holds: every distinct (classIdx, className) pair
// observed, with counts, share, and mean/peak confidence.
class ClassStatsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int distinctClasses READ rowCount NOTIFY statsChanged)
    Q_PROPERTY(qint64 totalWindows READ totalWindows NOTIFY statsChanged)

public:
    enum Roles {
        ClassIdxRole = Qt::UserRole + 1,
        ClassNameRole,
        CountRole,
        ShareRole,        // 0..100
        ShareTextRole,
        MeanConfidenceRole,
        PeakConfidenceRole,
        IsNormalRole,
    };
    Q_ENUM(Roles)

    explicit ClassStatsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void record(int classIdx, const QString &className, double confidence, bool isNormal);
    void clear();

    qint64 totalWindows() const { return m_total; }

signals:
    void statsChanged();

private:
    struct Row {
        int classIdx = -1;
        QString className;
        qint64 count = 0;
        double confidenceSum = 0.0;
        double peakConfidence = 0.0;
        bool isNormal = false;
    };

    QVector<Row> m_rows;
    qint64 m_total = 0;
};
