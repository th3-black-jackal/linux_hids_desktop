// iw-chat-probe — drives ChatClient from the command line, with no GUI.
//
// Two uses:
//
//   * Point it at the real backend to separate "the backend/token is wrong"
//     from "the console is wrong", the same way `mosquitto_sub` does for the
//     broker. It links the *actual* ChatClient, so a green run here means the
//     console's request shape, auth header and SSE parser all work against
//     that deployment.
//
//   * Point it at tools/iw_chat_stub.py to exercise the failure paths that
//     are hard to provoke on a real backend (mid-stream error, truncated
//     stream, 502, an answer citing an id that was never retrieved).
//
//   iw-chat-probe --url http://10.0.0.5:8000 --token abc123 \
//                 --ask "How do adversaries block PLC command messages?"
//   iw-chat-probe --url http://127.0.0.1:8055 --token devtoken --scenarios
//
// Exit code is 0 only if every question produced a finished answer.

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

#include "services/ChatClient.h"

namespace {

QTextStream out(stdout);
QTextStream err(stderr);

struct Result {
    QString question;
    bool finished = false;
    QString error;
    int tokens = 0;
    int sources = 0;
    int chars = 0;
    qint64 firstTokenMs = -1;
    qint64 totalMs = 0;
};

// Runs one question to completion (or failure) on the event loop.
Result runOne(ChatClient *client, const QString &question, int timeoutSec)
{
    Result r;
    r.question = question;

    QEventLoop loop;
    QElapsedTimer clock;

    QList<QMetaObject::Connection> conns;
    conns << QObject::connect(client, &ChatClient::tokenReceived, [&](const QString &t) {
        if (r.firstTokenMs < 0)
            r.firstTokenMs = clock.elapsed();
        r.tokens++;
        r.chars += t.size();
        out << "." << Qt::flush;
    });
    conns << QObject::connect(client, &ChatClient::sourcesReceived, [&](const QVariantList &s) {
        r.sources = s.size();
        out << "[" << s.size() << " sources]" << Qt::flush;
    });
    conns << QObject::connect(client, &ChatClient::answerFinished, [&]() {
        r.finished = true;
    });
    conns << QObject::connect(client, &ChatClient::errorOccurred, [&](const QString &d) {
        r.error = d;
    });
    // busyChanged going false is the single reliable end-of-request edge —
    // it fires for success, failure, cancellation and transport errors alike.
    conns << QObject::connect(client, &ChatClient::busyChanged, [&]() {
        if (!client->busy())
            loop.quit();
    });

    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, [&]() {
        r.error = QStringLiteral("probe timeout after %1 s").arg(timeoutSec);
        client->cancel();
        loop.quit();
    });
    guard.start(timeoutSec * 1000);

    clock.start();
    client->ask(question);
    if (client->busy())
        loop.exec();
    r.totalMs = clock.elapsed();

    for (const auto &c : conns)
        QObject::disconnect(c);
    return r;
}

void report(const Result &r, const QString &answer)
{
    out << "\n";
    out << "    sources        : " << r.sources << "\n";
    out << "    token events   : " << r.tokens << "\n";
    out << "    answer chars   : " << r.chars << "\n";
    if (r.firstTokenMs >= 0)
        out << "    first token    : " << r.firstTokenMs << " ms\n";
    out << "    total          : " << r.totalMs << " ms\n";
    if (!r.error.isEmpty())
        out << "    ERROR          : " << r.error << "\n";
    out << "    result         : " << (r.finished ? "finished" : "did not finish") << "\n";
    if (!answer.isEmpty()) {
        const QString head = answer.size() > 220 ? answer.left(220) + QStringLiteral(" …") : answer;
        out << "    answer         : " << QString(head).replace(QLatin1Char('\n'),
                                                                QLatin1String("\n                     "))
            << "\n";
    }
    out << Qt::flush;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // A distinct application name so the probe never reads or writes the
    // console's own saved settings.
    app.setOrganizationName(QStringLiteral("Ironwatch"));
    app.setApplicationName(QStringLiteral("iw-chat-probe"));

    QCommandLineParser p;
    p.setApplicationDescription("Drives the console's ChatClient against an ATT&CK RAG backend.");
    p.addHelpOption();
    QCommandLineOption oUrl(QStringList{"url"}, "Backend base URL.", "url", "http://127.0.0.1:8000");
    QCommandLineOption oTok(QStringList{"token"}, "DRF API token.", "token", "");
    QCommandLineOption oAsk(QStringList{"ask"}, "Question to ask (repeatable).", "text");
    QCommandLineOption oK(QStringList{"k"}, "Chunks to retrieve.", "n", "6");
    QCommandLineOption oBlock(QStringList{"blocking"}, "Use /chat/ instead of /chat/stream/.");
    QCommandLineOption oScen(QStringList{"scenarios"},
                             "Run the iw_chat_stub.py scenario suite.");
    QCommandLineOption oTimeout(QStringList{"timeout"}, "Per-question timeout, seconds.", "sec", "120");
    p.addOptions({ oUrl, oTok, oAsk, oK, oBlock, oScen, oTimeout });
    p.process(app);

    ChatClient client;
    client.setBaseUrl(p.value(oUrl));
    client.setToken(p.value(oTok));
    client.setRetrievalK(p.value(oK).toInt());
    client.setStreaming(!p.isSet(oBlock));

    QStringList questions = p.values(oAsk);
    if (p.isSet(oScen)) {
        // The stub picks its behaviour from a keyword in the question, so the
        // suite is just a list of questions. Each name is documented in
        // tools/iw_chat_stub.py.
        questions = QStringList{
            "normal: how do adversaries block PLC command messages?",
            "fast: same question, three chunks",
            "unicode: mixed scripts across chunk boundaries",
            "ungrounded: an answer citing an id that was never retrieved",
            "empty: a done event with no tokens",
            "midstream: an error event partway through generation",
            "truncated: the connection drops with no done event",
            "err400: a pre-stream validation failure",
            "err502: a pre-stream upstream failure",
        };
    }
    if (questions.isEmpty())
        questions << QStringLiteral("What is Block Command Message?");

    out << "backend : " << client.endpointText() << "\n"
        << "token   : " << (client.hasToken() ? "set" : "NOT SET") << "\n"
        << "mode    : " << (client.streaming() ? "streaming (SSE)" : "blocking") << "\n\n"
        << Qt::flush;

    const int timeout = p.value(oTimeout).toInt();
    int failures = 0;

    for (const QString &q : questions) {
        out << "── " << q << "\n    " << Qt::flush;
        const Result r = runOne(&client, q, timeout);
        // The assistant turn is the last row; read the accumulated text back
        // out of the model, which is what the UI renders.
        const QString answer = client.transcript()->textAt(
            client.transcript()->rowCount() - 1);
        report(r, answer);
        out << "\n" << Qt::flush;
        if (!r.finished)
            failures++;
    }

    if (p.isSet(oScen)) {
        // Most of the suite is *supposed* to fail; the run is only
        // meaningful as a transcript to read, so don't grade it.
        out << "scenario suite complete — " << (questions.size() - failures)
            << " of " << questions.size() << " produced a finished answer "
            << "(the failure scenarios are expected to fail).\n";
        return 0;
    }

    if (failures > 0)
        err << failures << " of " << questions.size() << " question(s) did not finish.\n";
    return failures == 0 ? 0 : 1;
}
