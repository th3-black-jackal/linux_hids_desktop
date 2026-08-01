#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "Detection.h"

// The triage queue: non-normal scored windows, newest first, with a filtered
// view driven by the severity segmented control and the free-text field.
// Selection is tracked as an index into the unfiltered list so it survives
// filter changes.
class DetectionsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString severityFilter READ severityFilter WRITE setSeverityFilter NOTIFY filterChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    Q_PROPERTY(int total READ total NOTIFY countsChanged)
    Q_PROPERTY(int openTotal READ openTotal NOTIFY countsChanged)
    Q_PROPERTY(int openCritical READ openCritical NOTIFY countsChanged)
    Q_PROPERTY(int openHigh READ openHigh NOTIFY countsChanged)
    Q_PROPERTY(int openMedium READ openMedium NOTIFY countsChanged)

public:
    enum Roles {
        NRole = Qt::UserRole + 1,
        UidRole,
        TimeRole,
        SeverityRole,
        NodeRole,
        ClassNameRole,
        ClassIdxRole,
        ConfidenceTextRole,
        StateRole,
        OriginalIndexRole,
    };
    Q_ENUM(Roles)

    explicit DetectionsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Prepends (newest first) and takes ownership. Trims the oldest beyond
    // kMaxRetained so a long-running console doesn't grow without bound.
    void addDetection(Detection *detection);

    Q_INVOKABLE Detection *detectionAt(int originalIndex) const;
    Q_INVOKABLE void toggleAcknowledge(int originalIndex);
    Q_INVOKABLE void acknowledgeAll();

    QString severityFilter() const { return m_severityFilter; }
    void setSeverityFilter(const QString &sev);
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    int total() const { return m_all.size(); }
    int openTotal() const { return m_openTotal; }
    int openCritical() const { return m_openCritical; }
    int openHigh() const { return m_openHigh; }
    int openMedium() const { return m_openMedium; }

signals:
    void filterChanged();
    void countsChanged();
    void detectionAdded();

private:
    void applyFilter();
    void recomputeCounts();
    void watch(Detection *d);

    static constexpr int kMaxRetained = 5000;

    QVector<Detection *> m_all;
    QVector<int> m_filtered;
    QString m_severityFilter = QStringLiteral("all");
    QString m_searchText;

    int m_openTotal = 0, m_openCritical = 0, m_openHigh = 0, m_openMedium = 0;
};
