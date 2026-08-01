#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

#include "../core/DetectionEvent.h"

QT_BEGIN_NAMESPACE
class QTcpSocket;
class QTimer;
QT_END_NAMESPACE

// One outbound connection to a board running ironwatchd.
//
// Direction matters: the *board* is the TCP server. hids::TelemetryServer
// binds INADDR_ANY on Config::telemetryPort and accepts clients; this console
// is one of those clients and dials out to each configured board.
//
// Wire format, from TelemetryServer::broadcast():
//
//     [ uint32 big-endian payload length ][ payload bytes ]
//
// where payload is DetectionEvent::to_json(include_features=true), with no
// trailing newline and no delimiter. One frame per inference window
// (Config::windowSec), sent to every connected client. There is no
// handshake, no heartbeat and no client->server traffic at all.
class BoardConnection : public QObject {
    Q_OBJECT

public:
    enum State { Offline, Connecting, Connected };
    Q_ENUM(State)

    BoardConnection(QString host, quint16 port, QObject *parent = nullptr);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    QString endpoint() const { return QStringLiteral("%1:%2").arg(m_host).arg(m_port); }
    void setEndpoint(const QString &host, quint16 port);

    State state() const { return m_state; }
    QString stateText() const;
    QDateTime connectedSince() const { return m_connectedSince; }
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 framesReceived() const { return m_framesReceived; }
    qint64 framesDropped() const { return m_framesDropped; }
    QString lastError() const { return m_lastError; }

    // Bytes/sec over the last sampling tick, maintained by TelemetryClient.
    double throughputBps() const { return m_throughputBps; }
    void sampleThroughput();

    void start();
    void stop();

signals:
    void stateChanged();
    void statsChanged();
    void eventReceived(const DetectionEvent &ev);
    void logged(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred();
    void onReadyRead();
    void tryConnect();

private:
    void setState(State s);
    void consumeBuffer();
    void scheduleReconnect();

    // A single scored window with the full normalized feature vector is a few
    // KB at most; anything past this means the stream has desynchronized.
    static constexpr int kMaxFrameBytes = 8 * 1024 * 1024;

    QString  m_host;
    quint16  m_port = 0;
    QTcpSocket *m_socket = nullptr;
    QTimer   *m_reconnectTimer = nullptr;

    QByteArray m_buffer;
    State      m_state = Offline;
    bool       m_wanted = false;   // start() called and not stopped
    int        m_backoffMs = 1000;

    QDateTime m_connectedSince;
    qint64 m_bytesReceived = 0;
    qint64 m_framesReceived = 0;
    qint64 m_framesDropped = 0;
    qint64 m_bytesAtLastSample = 0;
    double m_throughputBps = 0.0;
    QString m_lastError;
};
