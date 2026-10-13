#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "DetectionEvent.h"

// Rows from `GET /api/siem/events/` — the database view, as opposed to the
// live MQTT feed that fills DetectionsModel.
//
// Roles deliberately mirror DetectionsModel's so the Events screen can reuse
// the alerts table verbatim, with two additions the database knows and the
// wire does not: `alert` (the board debouncer's own verdict) and
// `sourceTopic` (which topic the row was seen on).
//
// Severity is computed the same console-side way as for live events, from
// (class_name, confidence) via SeverityPolicy — so a row looks identical
// whether it arrived over MQTT or came back from the database.
class SiemEventsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY countChanged)

public:
    enum Roles {
        NRole = Qt::UserRole + 1,
        IdRole,
        TimeRole,
        TimeRawRole,
        ReceivedAtRole,
        SeverityRole,
        NodeRole,
        ClassNameRole,
        ClassIdxRole,
        ConfidenceRole,
        ConfidenceTextRole,
        AlertRole,
        SourceTopicRole,
    };
    Q_ENUM(Roles)

    struct Row {
        int id = -1;
        QString host;
        QString eventTimeRaw;
        QDateTime eventTime;
        QString receivedAtRaw;
        int classIdx = -1;
        QString className;
        double confidence = 0.0;
        bool alert = false;
        QString sourceTopic;
        QString severity;
    };

    explicit SiemEventsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalCount() const { return m_totalCount; }
    bool hasMore() const { return !m_nextUrl.isEmpty(); }
    QString nextUrl() const { return m_nextUrl; }

    void setPolicy(const SeverityPolicy &p) { m_policy = p; }

    // Replaces or appends one page of the paginated envelope (§4.1).
    void applyPage(const QJsonObject &envelope, bool append);
    void reset();

    Q_INVOKABLE int idAt(int row) const;
    Q_INVOKABLE QString hostAt(int row) const;

    // Parses one list-form Event object (§3.1) into a Row. Returns false if
    // the object carries no usable identity.
    static bool parseRow(const QJsonObject &o, const SeverityPolicy &policy, Row *out);

signals:
    void countChanged();

private:
    QVector<Row> m_rows;
    QString m_nextUrl;
    int m_totalCount = 0;
    SeverityPolicy m_policy;
};
