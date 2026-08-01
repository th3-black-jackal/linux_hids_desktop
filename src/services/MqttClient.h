#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QMqttClient;
class QTimer;
QT_END_NAMESPACE

// MQTT subscriber for the console, built on the Qt MQTT module (QtMqtt).
//
// QtMqtt runs entirely on the Qt event loop — there is no background network
// thread — so unlike the libmosquitto implementation this replaced, no
// callback marshalling is needed anywhere and every signal below is already
// emitted on the object's own thread.
//
// The one thing QtMqtt does NOT do that libmosquitto did: **reconnect**.
// QMqttClient goes to Disconnected and stays there, so the retry loop below
// (exponential backoff, 1s → 30s) is ours. Without it a single broker
// restart would silently leave the console dead until someone noticed.
//
// Subscriptions are likewise not restored across a reconnect — QtMqtt's
// QMqttSubscription objects are invalidated on disconnect — so the filter
// list is remembered and re-issued on every successful connect.
class MqttClient : public QObject {
    Q_OBJECT

public:
    enum State { Disconnected, Connecting, Connected };
    Q_ENUM(State)

    struct Options {
        QString host = QStringLiteral("127.0.0.1");
        int port = 1883;
        QString clientId;
        QString username;
        QString password;
        int keepaliveSec = 30;

        QString caFile;      // set to enable TLS
        QString certFile;    // client cert (mutual TLS)
        QString keyFile;
        bool tlsInsecure = false;
    };

    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    State state() const { return m_state; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }
    qint64 messagesReceived() const { return m_messagesReceived; }
    qint64 bytesReceived() const { return m_bytesReceived; }

    void start(const Options &opts);
    void stop();

    // Remembered and re-issued on every reconnect; safe to call once at
    // startup and never again.
    void subscribe(const QString &filter, int qos);

signals:
    void stateChanged();
    void statsChanged();
    void messageReceived(const QString &topic, const QByteArray &payload);
    void logged(const QString &message);

private:
    void onConnected();
    void onDisconnected();
    void onErrorChanged(int error);
    void onMessage(const QByteArray &payload, const QString &topic);
    void applySubscriptions();
    void setState(State s);
    void scheduleReconnect();
    void attemptConnect();

    struct Sub { QString filter; int qos; };

    QMqttClient *m_client = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    Options m_opts;
    QVector<Sub> m_subs;
    State m_state = Disconnected;
    QString m_lastError;
    bool m_running = false;     // start() called and not stopped
    int m_backoffMs = 1000;
    qint64 m_messagesReceived = 0;
    qint64 m_bytesReceived = 0;
};
