// iw-fakeboard — a stand-in for hids::TelemetryServer.
//
// Speaks the exact wire protocol the real service speaks, so the console can
// be exercised without a Raspberry Pi on the desk, and so the framing code in
// BoardConnection is tested against an independent implementation of the same
// format rather than against itself:
//
//     [ uint32 big-endian payload length ][ DetectionEvent JSON ]
//
// It mirrors TelemetryServer's behaviour: binds INADDR_ANY, accepts any number
// of clients, broadcasts one frame per window to all of them, drops clients
// whose sends fail.
//
//     iw-fakeboard [--port N] [--window-sec N] [--host NAME]
//                  [--features N] [--alert-every N] [--split-frames]
//
// --split-frames deliberately dribbles each frame out in small chunks across
// separate writes, which is the case a naive reader gets wrong; the console
// must reassemble it identically.

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QtEndian>

namespace {

const char *kClasses[] = { "normal", "dos", "portscan", "bruteforce", "exfiltration" };
constexpr int kClassCount = 5;

QByteArray buildFrame(const QByteArray &payload)
{
    QByteArray frame;
    frame.resize(4);
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                          reinterpret_cast<uchar *>(frame.data()));
    frame.append(payload);
    return frame;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("iw-fakeboard"));

    QCommandLineParser p;
    p.setApplicationDescription("Fake Ironwatch board: serves the telemetry protocol.");
    p.addHelpOption();
    QCommandLineOption optPort({ "p", "port" }, "Listen port.", "port", "9542");
    QCommandLineOption optWindow({ "w", "window-sec" }, "Seconds per window.", "sec", "2");
    QCommandLineOption optHost({ "n", "host" }, "Value for the event's host field.", "name", "rpi4-lab01");
    QCommandLineOption optFeatures({ "f", "features" }, "Feature vector length.", "n", "24");
    QCommandLineOption optAlert({ "a", "alert-every" }, "Emit a non-normal class every Nth window.", "n", "5");
    QCommandLineOption optSplit("split-frames", "Send each frame in several small writes.");
    p.addOptions({ optPort, optWindow, optHost, optFeatures, optAlert, optSplit });
    p.process(app);

    const quint16 port = static_cast<quint16>(p.value(optPort).toUShort());
    const int windowSec = qMax(1, p.value(optWindow).toInt());
    const QString host = p.value(optHost);
    const int nFeatures = qMax(1, p.value(optFeatures).toInt());
    const int alertEvery = qMax(0, p.value(optAlert).toInt());
    const bool split = p.isSet(optSplit);

    QTextStream err(stderr);

    auto *server = new QTcpServer(&app);
    if (!server->listen(QHostAddress::Any, port)) {
        err << "[fakeboard] listen failed on port " << port << ": "
            << server->errorString() << "\n";
        return 1;
    }
    err << "[fakeboard] listening on port " << port << ", window=" << windowSec << "s\n";
    err.flush();

    auto clients = new QVector<QTcpSocket *>();

    QObject::connect(server, &QTcpServer::newConnection, [server, clients, &err]() {
        while (server->hasPendingConnections()) {
            QTcpSocket *s = server->nextPendingConnection();
            clients->append(s);
            err << "[fakeboard] client connected (" << clients->size() << " total)\n";
            err.flush();
            QObject::connect(s, &QTcpSocket::disconnected, [s, clients, &err]() {
                clients->removeAll(s);
                s->deleteLater();
                err << "[fakeboard] client disconnected (" << clients->size() << " left)\n";
                err.flush();
            });
        }
    });

    int windowNo = 0;
    auto *timer = new QTimer(&app);
    timer->setInterval(windowSec * 1000);
    QObject::connect(timer, &QTimer::timeout, [&, clients]() {
        windowNo++;

        const bool alert = alertEvery > 0 && (windowNo % alertEvery == 0);
        const int classIdx = alert
            ? 1 + int(QRandomGenerator::global()->bounded(kClassCount - 1))
            : 0;
        const double confidence = alert
            ? 0.60 + QRandomGenerator::global()->bounded(400) / 1000.0
            : 0.80 + QRandomGenerator::global()->bounded(200) / 1000.0;

        QJsonArray features;
        for (int i = 0; i < nFeatures; ++i) {
            // Normalized-looking values; alerts get a few strong outliers so
            // the console's feature ranking has something to sort.
            double v = (QRandomGenerator::global()->bounded(2000) - 1000) / 1000.0;
            if (alert && i % 7 == 0)
                v *= 4.0;
            features.append(v);
        }

        QJsonObject o;
        o["timestamp"]  = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        o["classIdx"]   = classIdx;
        o["className"]  = QString::fromLatin1(kClasses[classIdx]);
        o["confidence"] = confidence;
        o["host"]       = host;
        o["features"]   = features;

        const QByteArray frame =
            buildFrame(QJsonDocument(o).toJson(QJsonDocument::Compact));

        for (QTcpSocket *s : std::as_const(*clients)) {
            if (s->state() != QAbstractSocket::ConnectedState)
                continue;
            if (!split) {
                s->write(frame);
            } else {
                // Chop into ~7-byte writes so the length prefix itself lands
                // across multiple reads on the far side.
                for (int off = 0; off < frame.size(); off += 7) {
                    s->write(frame.constData() + off, qMin(7, frame.size() - off));
                    s->flush();
                }
            }
            s->flush();
        }

        err << "[fakeboard] window " << windowNo << " -> " << kClasses[classIdx]
            << " conf=" << QString::number(confidence, 'f', 3)
            << " (" << frame.size() << " bytes, " << clients->size() << " clients)\n";
        err.flush();
    });
    timer->start();

    return app.exec();
}
