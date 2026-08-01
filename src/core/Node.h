#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

#include "DetectionEvent.h"

// One board, as learned from the broker.
//
// Boards are no longer configured by hand: they appear when their retained
// `ironwatch/<host>/status` message arrives (or when their first event
// does), and their liveness comes from that same topic — "online" published
// on connect, "offline" published by the broker as the board's Last Will if
// it stops answering keepalives. There is no polling anywhere.
//
// Board model, kernel, CPU/RSS/temperature and service uptime are still not
// part of the payload; see app/README.md for the full gap list.
class Node : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString state READ state NOTIFY statsChanged)
    Q_PROPERTY(bool online READ online NOTIFY statsChanged)
    Q_PROPERTY(QString statusReason READ statusReason NOTIFY statsChanged)
    Q_PROPERTY(QString lastSeen READ lastSeen NOTIFY statsChanged)
    Q_PROPERTY(qint64 windows READ windows NOTIFY statsChanged)
    Q_PROPERTY(qint64 alerts READ alerts NOTIFY statsChanged)
    Q_PROPERTY(QString lastClass READ lastClass NOTIFY statsChanged)
    Q_PROPERTY(double lastConfidence READ lastConfidence NOTIFY statsChanged)
    Q_PROPERTY(QString windowPeriod READ windowPeriod NOTIFY statsChanged)
    Q_PROPERTY(int featureCount READ featureCount NOTIFY statsChanged)

public:
    explicit Node(QString host, QObject *parent = nullptr);

    QString id() const { return m_host; }
    QString host() const { return m_host; }

    // online | offline | stale
    //
    // "stale" is inferred, not reported: the board still claims online on
    // the status topic but no window has arrived for several of its own
    // window periods. That catches a service wedged after connecting —
    // which neither the status topic nor the broker would ever reveal,
    // because the board's MQTT client answers keepalives from its own
    // network thread even if the inference loop has stopped.
    QString state() const;
    bool online() const { return m_online; }
    QString statusReason() const { return m_statusReason; }
    QString lastSeen() const;
    qint64 windows() const { return m_windows; }
    qint64 alerts() const { return m_alerts; }
    QString lastClass() const { return m_lastClass; }
    double lastConfidence() const { return m_lastConfidence; }
    QString windowPeriod() const;
    double windowPeriodSecs() const { return m_meanPeriodSecs; }
    int featureCount() const { return m_featureCount; }

    const QVector<double> &confidenceHistory() const { return m_confidenceHistory; }
    const QHash<QString, qint64> &classCounts() const { return m_classCounts; }

    void applyStatus(bool online, const QString &reason);
    void applyEvent(const DetectionEvent &ev, bool isAlert);
    void refreshStats();

signals:
    void statsChanged();

private:
    static constexpr int kHistoryLen = 120;

    QString m_host;
    bool m_online = false;
    QString m_statusReason;

    qint64 m_windows = 0;
    qint64 m_alerts = 0;
    QString m_lastClass;
    double m_lastConfidence = 0.0;
    QDateTime m_lastEventAt;
    QDateTime m_prevEventAt;
    double m_meanPeriodSecs = 0.0;
    int m_featureCount = 0;

    QVector<double> m_confidenceHistory;
    QHash<QString, qint64> m_classCounts;
};
