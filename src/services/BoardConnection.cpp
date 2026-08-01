#include "BoardConnection.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

BoardConnection::BoardConnection(QString host, quint16 port, QObject *parent)
    : QObject(parent)
    , m_host(std::move(host))
    , m_port(port)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &BoardConnection::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &BoardConnection::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &BoardConnection::onErrorOccurred);
    connect(m_socket, &QTcpSocket::readyRead, this, &BoardConnection::onReadyRead);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &BoardConnection::tryConnect);
}

QString BoardConnection::stateText() const
{
    switch (m_state) {
    case Connected:  return QStringLiteral("connected");
    case Connecting: return QStringLiteral("connecting");
    default:         return QStringLiteral("offline");
    }
}

void BoardConnection::setEndpoint(const QString &host, quint16 port)
{
    if (m_host == host && m_port == port)
        return;
    m_host = host;
    m_port = port;
    if (m_wanted) {
        stop();
        start();
    }
}

void BoardConnection::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged();
}

void BoardConnection::start()
{
    m_wanted = true;
    m_backoffMs = 1000;
    tryConnect();
}

void BoardConnection::stop()
{
    m_wanted = false;
    m_reconnectTimer->stop();
    m_socket->abort();
    m_buffer.clear();
    m_connectedSince = QDateTime();
    setState(Offline);
}

void BoardConnection::tryConnect()
{
    if (!m_wanted)
        return;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        return;

    setState(Connecting);
    m_socket->connectToHost(m_host, m_port);
}

void BoardConnection::onConnected()
{
    m_backoffMs = 1000;
    m_buffer.clear();
    m_connectedSince = QDateTime::currentDateTime();
    m_lastError.clear();
    setState(Connected);
    emit logged(QStringLiteral("connected to %1").arg(endpoint()));
    emit statsChanged();
}

void BoardConnection::onDisconnected()
{
    m_connectedSince = QDateTime();
    m_buffer.clear();
    setState(Offline);
    emit logged(QStringLiteral("disconnected from %1").arg(endpoint()));
    scheduleReconnect();
}

void BoardConnection::onErrorOccurred()
{
    m_lastError = m_socket->errorString();
    // Suppress the log line for the ordinary "board isn't up yet" retry loop;
    // it would otherwise fill the log at the reconnect interval forever.
    if (m_state == Connected)
        emit logged(QStringLiteral("%1: %2").arg(endpoint(), m_lastError));
    setState(Offline);
    scheduleReconnect();
}

void BoardConnection::scheduleReconnect()
{
    if (!m_wanted || m_reconnectTimer->isActive())
        return;
    m_reconnectTimer->start(m_backoffMs);
    m_backoffMs = qMin(m_backoffMs * 2, 30000);
}

void BoardConnection::onReadyRead()
{
    const QByteArray chunk = m_socket->readAll();
    m_bytesReceived += chunk.size();
    m_buffer += chunk;
    consumeBuffer();
    emit statsChanged();
}

void BoardConnection::consumeBuffer()
{
    for (;;) {
        if (m_buffer.size() < 4)
            return;

        // htonl() on the board, so the prefix is big-endian regardless of the
        // board's own byte order (ARM boards here are little-endian).
        quint32 len = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar *>(m_buffer.constData()));

        if (len > kMaxFrameBytes) {
            // Can't resynchronize a length-prefixed stream once it slips, so
            // drop the connection and let the reconnect path start clean.
            emit logged(QStringLiteral("%1: implausible frame length %2, resetting connection")
                            .arg(endpoint()).arg(len));
            m_framesDropped++;
            m_buffer.clear();
            m_socket->abort();
            setState(Offline);
            scheduleReconnect();
            return;
        }

        if (static_cast<quint32>(m_buffer.size()) < 4 + len)
            return; // wait for the rest of the frame

        const QByteArray payload = m_buffer.mid(4, static_cast<int>(len));
        m_buffer.remove(0, 4 + static_cast<int>(len));
        m_framesReceived++;

        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            m_framesDropped++;
            emit logged(QStringLiteral("%1: malformed JSON frame (%2)")
                            .arg(endpoint(), perr.errorString()));
            continue;
        }

        QString parseError;
        auto ev = DetectionEvent::parse(doc.object(), &parseError);
        if (!ev) {
            m_framesDropped++;
            emit logged(QStringLiteral("%1: %2").arg(endpoint(), parseError));
            continue;
        }

        ev->endpoint = endpoint();
        if (ev->host.isEmpty())
            ev->host = m_host;
        emit eventReceived(*ev);
    }
}

void BoardConnection::sampleThroughput()
{
    const qint64 delta = m_bytesReceived - m_bytesAtLastSample;
    m_bytesAtLastSample = m_bytesReceived;
    m_throughputBps = static_cast<double>(delta);
}
