#include "MqttClient.h"

#include <QFile>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QMqttTopicFilter>
#include <QMqttTopicName>
#include <QSslConfiguration>
#include <QSslKey>
#include <QTimer>

namespace {

// QMqttClient reports the CONNACK return code plus a few transport-level
// conditions. The first five are configuration faults that will never
// resolve by retrying, which is worth distinguishing in the UI from "the
// broker isn't up yet".
QString describeError(int e)
{
    switch (e) {
    case QMqttClient::NoError:                return QString();
    case QMqttClient::InvalidProtocolVersion: return QStringLiteral("broker rejected the protocol version");
    case QMqttClient::IdRejected:             return QStringLiteral("broker rejected the client ID");
    case QMqttClient::ServerUnavailable:      return QStringLiteral("broker unavailable");
    case QMqttClient::BadUsernameOrPassword:  return QStringLiteral("bad username or password");
    case QMqttClient::NotAuthorized:          return QStringLiteral("not authorised");
    case QMqttClient::TransportInvalid:       return QStringLiteral("transport failed (host unreachable, or TLS rejected)");
    case QMqttClient::ProtocolViolation:      return QStringLiteral("protocol violation");
    case QMqttClient::Mqtt5SpecificError:     return QStringLiteral("MQTT 5 error");
    default:                                  return QStringLiteral("unknown error");
    }
}

bool isFatalError(int e)
{
    return e == QMqttClient::InvalidProtocolVersion
        || e == QMqttClient::IdRejected
        || e == QMqttClient::BadUsernameOrPassword
        || e == QMqttClient::NotAuthorized;
}

} // namespace

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttClient::attemptConnect);
}

MqttClient::~MqttClient() = default;

QString MqttClient::stateText() const
{
    switch (m_state) {
    case Connected:  return QStringLiteral("connected");
    case Connecting: return QStringLiteral("connecting");
    default:         return QStringLiteral("disconnected");
    }
}

void MqttClient::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged();
}

void MqttClient::start(const Options &opts)
{
    stop();

    m_opts = opts;
    if (m_opts.clientId.isEmpty())
        m_opts.clientId = QStringLiteral("ironwatch-console");

    m_client = new QMqttClient(this);
    m_client->setHostname(m_opts.host);
    m_client->setPort(static_cast<quint16>(m_opts.port));
    m_client->setClientId(m_opts.clientId);
    m_client->setKeepAlive(static_cast<quint16>(m_opts.keepaliveSec));

    // The console wants live state, not a replay of everything it missed
    // while closed. Board liveness still arrives immediately because the
    // status topic is retained on the broker.
    m_client->setCleanSession(true);

    if (!m_opts.username.isEmpty()) {
        m_client->setUsername(m_opts.username);
        m_client->setPassword(m_opts.password);
    }

    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::errorChanged, this,
            [this](QMqttClient::ClientError e) { onErrorChanged(static_cast<int>(e)); });

    // Topic-level delivery: one handler for every subscription. The topic is
    // carried on the signal, which is what TelemetryClient routes on.
    connect(m_client, &QMqttClient::messageReceived, this,
            [this](const QByteArray &message, const QMqttTopicName &topic) {
                onMessage(message, topic.name());
            });

    m_running = true;
    m_backoffMs = 1000;
    attemptConnect();
}

void MqttClient::attemptConnect()
{
    if (!m_running || !m_client)
        return;
    if (m_client->state() != QMqttClient::Disconnected)
        return;

    setState(Connecting);

    if (m_opts.caFile.isEmpty()) {
        m_client->connectToHost();
        return;
    }

    // ── TLS ──────────────────────────────────────────────────────────────
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();

    const QList<QSslCertificate> ca = QSslCertificate::fromPath(m_opts.caFile);
    if (ca.isEmpty()) {
        m_lastError = QStringLiteral("cannot read CA certificate %1").arg(m_opts.caFile);
        emit logged(m_lastError);
        setState(Disconnected);
        scheduleReconnect();
        return;
    }
    ssl.setCaCertificates(ca);

    // Mutual TLS, for a broker configured with require_certificate true.
    if (!m_opts.certFile.isEmpty() && !m_opts.keyFile.isEmpty()) {
        const QList<QSslCertificate> chain = QSslCertificate::fromPath(m_opts.certFile);
        if (!chain.isEmpty())
            ssl.setLocalCertificate(chain.first());

        QFile keyFile(m_opts.keyFile);
        if (keyFile.open(QIODevice::ReadOnly)) {
            // Try EC first (make-certs.sh issues prime256v1), then RSA.
            QSslKey key(&keyFile, QSsl::Ec);
            if (key.isNull()) {
                keyFile.seek(0);
                key = QSslKey(&keyFile, QSsl::Rsa);
            }
            if (!key.isNull())
                ssl.setPrivateKey(key);
            else
                emit logged(QStringLiteral("could not parse client key %1").arg(m_opts.keyFile));
        } else {
            emit logged(QStringLiteral("cannot open client key %1").arg(m_opts.keyFile));
        }
    }

    if (m_opts.tlsInsecure) {
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        emit logged(QStringLiteral("WARNING: TLS certificate verification disabled"));
    }

    m_client->connectToHostEncrypted(ssl);
}

void MqttClient::onConnected()
{
    m_backoffMs = 1000;
    m_lastError.clear();
    setState(Connected);
    emit logged(QStringLiteral("connected to broker %1:%2").arg(m_opts.host).arg(m_opts.port));

    // QtMqtt does not restore subscriptions across a reconnect, so this is
    // what makes reconnects transparent to the rest of the app.
    applySubscriptions();
}

void MqttClient::onDisconnected()
{
    setState(Disconnected);
    if (m_running) {
        emit logged(QStringLiteral("disconnected from broker, retrying"));
        scheduleReconnect();
    }
}

void MqttClient::onErrorChanged(int error)
{
    if (error == QMqttClient::NoError)
        return;

    m_lastError = describeError(error);
    emit logged(QStringLiteral("broker error: %1").arg(m_lastError));

    if (isFatalError(error)) {
        // Retrying a rejected credential forever just hammers the broker and
        // hides the real problem, so stop and leave the error visible.
        emit logged(QStringLiteral("not retrying — fix the broker settings and press Reconnect"));
        m_running = false;
        m_reconnectTimer->stop();
    }

    setState(Disconnected);
    if (m_running)
        scheduleReconnect();
}

void MqttClient::scheduleReconnect()
{
    if (!m_running || m_reconnectTimer->isActive())
        return;
    m_reconnectTimer->start(m_backoffMs);
    m_backoffMs = qMin(m_backoffMs * 2, 30000);
}

void MqttClient::applySubscriptions()
{
    if (!m_client || m_client->state() != QMqttClient::Connected)
        return;

    for (const Sub &s : std::as_const(m_subs)) {
        auto *sub = m_client->subscribe(QMqttTopicFilter(s.filter),
                                        static_cast<quint8>(s.qos));
        if (!sub) {
            // Almost always the broker ACL refusing the filter; without this
            // the console would sit connected and silent with no explanation.
            emit logged(QStringLiteral("subscribe failed: %1 (check the broker ACL)").arg(s.filter));
        }
    }
}

void MqttClient::subscribe(const QString &filter, int qos)
{
    for (const Sub &s : std::as_const(m_subs)) {
        if (s.filter == filter)
            return;
    }
    m_subs.append({ filter, qos });

    if (m_client && m_client->state() == QMqttClient::Connected)
        m_client->subscribe(QMqttTopicFilter(filter), static_cast<quint8>(qos));
}

void MqttClient::onMessage(const QByteArray &payload, const QString &topic)
{
    m_messagesReceived++;
    m_bytesReceived += payload.size();
    emit messageReceived(topic, payload);
    emit statsChanged();
}

void MqttClient::stop()
{
    m_running = false;
    m_reconnectTimer->stop();

    if (m_client) {
        m_client->disconnectFromHost();
        m_client->deleteLater();
        m_client = nullptr;
    }
    setState(Disconnected);
}
