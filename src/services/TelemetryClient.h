#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "../core/ClassStatsModel.h"
#include "../core/DetectionEvent.h"
#include "../core/DetectionsModel.h"
#include "../core/EventStreamModel.h"
#include "../core/LogModel.h"
#include "../core/NodesModel.h"
#include "MqttClient.h"
#include "SiemClient.h"

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

// Subscribes to the broker and fans incoming board messages out into the
// console's models.
//
// This replaces the per-board TCP dialling of the previous design. The
// console now holds exactly one connection — to the broker — and the fleet
// discovers itself from `ironwatch/+/status` (see deploy/mosquitto/TOPICS.md).
class TelemetryClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(NodesModel *nodes READ nodes CONSTANT)

    Q_PROPERTY(QString brokerState READ brokerState NOTIFY statsChanged)
    Q_PROPERTY(bool brokerConnected READ brokerConnected NOTIFY statsChanged)
    Q_PROPERTY(QString brokerError READ brokerError NOTIFY statsChanged)
    Q_PROPERTY(QString brokerEndpoint READ brokerEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(int boardCount READ boardCount NOTIFY statsChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY statsChanged)
    Q_PROPERTY(qint64 framesReceived READ framesReceived NOTIFY statsChanged)
    Q_PROPERTY(qint64 framesDropped READ framesDropped NOTIFY statsChanged)
    Q_PROPERTY(QString throughputText READ throughputText NOTIFY statsChanged)
    Q_PROPERTY(QString lastEventAge READ lastEventAge NOTIFY statsChanged)
    Q_PROPERTY(QString windowPeriodText READ windowPeriodText NOTIFY statsChanged)

    // Broker connection settings, persisted via QSettings.
    Q_PROPERTY(QString brokerHost READ brokerHost WRITE setBrokerHost NOTIFY settingsChanged)
    Q_PROPERTY(int brokerPort READ brokerPort WRITE setBrokerPort NOTIFY settingsChanged)
    Q_PROPERTY(QString brokerUser READ brokerUser WRITE setBrokerUser NOTIFY settingsChanged)
    Q_PROPERTY(QString brokerPassword READ brokerPassword WRITE setBrokerPassword NOTIFY settingsChanged)
    Q_PROPERTY(QString caFile READ caFile WRITE setCaFile NOTIFY settingsChanged)
    Q_PROPERTY(bool tlsEnabled READ tlsEnabled NOTIFY settingsChanged)

    // Console-side severity thresholds (see SeverityPolicy).
    Q_PROPERTY(double criticalAt READ criticalAt WRITE setCriticalAt NOTIFY policyChanged)
    Q_PROPERTY(double highAt READ highAt WRITE setHighAt NOTIFY policyChanged)
    Q_PROPERTY(double mediumAt READ mediumAt WRITE setMediumAt NOTIFY policyChanged)
    Q_PROPERTY(QString normalClassesText READ normalClassesText WRITE setNormalClassesText NOTIFY policyChanged)
    Q_PROPERTY(QString featureNamesPath READ featureNamesPath WRITE setFeatureNamesPath NOTIFY policyChanged)
    Q_PROPERTY(int featureNamesLoaded READ featureNamesLoaded NOTIFY policyChanged)

public:
    TelemetryClient(DetectionsModel *detections, EventStreamModel *stream,
                    LogModel *log, ClassStatsModel *classStats,
                    QObject *parent = nullptr);

    NodesModel *nodes() { return &m_nodes; }

    QString brokerState() const { return m_mqtt->stateText(); }
    bool brokerConnected() const { return m_mqtt->state() == MqttClient::Connected; }
    QString brokerError() const { return m_mqtt->lastError(); }
    QString brokerEndpoint() const;
    int boardCount() const { return m_nodes.count(); }
    int onlineCount() const { return m_nodes.onlineCount(); }
    qint64 framesReceived() const { return m_mqtt->messagesReceived(); }
    qint64 framesDropped() const { return m_framesDropped; }
    QString throughputText() const;
    QString lastEventAge() const;
    QString windowPeriodText() const;

    QString brokerHost() const { return m_brokerHost; }
    void setBrokerHost(const QString &v);
    int brokerPort() const { return m_brokerPort; }
    void setBrokerPort(int v);
    QString brokerUser() const { return m_brokerUser; }
    void setBrokerUser(const QString &v);
    QString brokerPassword() const { return m_brokerPassword; }
    void setBrokerPassword(const QString &v);
    QString caFile() const { return m_caFile; }
    void setCaFile(const QString &v);
    bool tlsEnabled() const { return !m_caFile.isEmpty(); }

    double criticalAt() const { return m_policy.criticalAt; }
    void setCriticalAt(double v);
    double highAt() const { return m_policy.highAt; }
    void setHighAt(double v);
    double mediumAt() const { return m_policy.mediumAt; }
    void setMediumAt(double v);
    QString normalClassesText() const { return m_policy.normalClasses.join(QLatin1String(", ")); }
    void setNormalClassesText(const QString &v);
    QString featureNamesPath() const { return m_featureNamesPath; }
    void setFeatureNamesPath(const QString &path);
    int featureNamesLoaded() const { return m_featureNames.size(); }

    // Applies the current broker settings and (re)connects.
    Q_INVOKABLE void reconnect();

    // Fills the triage queue from the SIEM database before (and alongside)
    // anything MQTT delivers. Called once at startup; the live feed is
    // unaffected and keeps adding new alerts as they arrive.
    void backfillFrom(SiemClient *siem);

    void loadSettings();
    void saveSettings();

signals:
    void statsChanged();
    void settingsChanged();
    void policyChanged();

private:
    void onMessage(const QString &topic, const QByteArray &payload);
    void handleStatus(const QString &host, const QByteArray &payload);
    void handleEvent(const QString &host, const QByteArray &payload, bool fromAlertsTopic);
    void onTick();
    void loadFeatureNames();
    void ingestSiemPage(const QVector<SiemEventsModel::Row> &page);

    NodesModel m_nodes;
    DetectionsModel *m_detections;
    EventStreamModel *m_stream;
    LogModel *m_log;
    ClassStatsModel *m_classStats;
    MqttClient *m_mqtt;

    SeverityPolicy m_policy;
    QStringList m_featureNames;
    QString m_featureNamesPath;

    QString m_brokerHost = QStringLiteral("127.0.0.1");
    int m_brokerPort = 1883;
    QString m_brokerUser = QStringLiteral("ironwatch-console");
    QString m_brokerPassword;
    QString m_caFile;

    SiemClient *m_siem = nullptr;
    QTimer *m_tick = nullptr;
    QDateTime m_lastEventAt;
    qint64 m_framesDropped = 0;
    qint64 m_bytesAtLastTick = 0;
    double m_throughputBps = 0.0;
    qint64 m_uidCounter = 0;
    bool m_loading = false;
};
