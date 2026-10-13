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
// `--siem` does the same job for the read-only telemetry API: it runs the
// real SiemClient through a list query, a stats query, a detail fetch and
// the startup backfill, and reports what each returned.
//
//   iw-chat-probe --url http://10.0.0.5:8000 --token abc123 --siem
//
// Exit code is 0 only if every question produced a finished answer.

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

#include "services/BackendSettings.h"
#include "services/ChatClient.h"
#include "services/SiemClient.h"

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

// ── SIEM read API ───────────────────────────────────────────────────────

// Runs SiemClient through the four things the console actually does with
// /api/siem/, printing what each returned. Exit 0 only if the list query and
// the backfill both succeeded — those are the two the console depends on.
int probeSiem(BackendSettings *backend, int timeoutSec)
{
    SiemClient siem(backend);

    out << "backend : " << backend->baseUrl() << "/api/siem/\n"
        << "token   : " << (backend->hasToken() ? "set" : "NOT SET") << "\n\n"
        << Qt::flush;

    QObject::connect(&siem, &SiemClient::logged,
                     [](const QString &m) { out << "    " << m << "\n" << Qt::flush; });

    int failures = 0;

    // 1. A filtered list query, exactly as the Events screen issues it.
    {
        out << "── GET /api/siem/events/ (last 24 h, alerts only)\n" << Qt::flush;
        siem.filters()->setQuickRange(QStringLiteral("24h"));
        siem.filters()->setType(QStringLiteral("alert"));

        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&siem, &SiemClient::loadingChanged, [&]() {
            if (!siem.loading())
                loop.quit();
        });
        guard.start(timeoutSec * 1000);
        siem.refresh();
        if (siem.loading())
            loop.exec();

        if (!siem.lastError().isEmpty()) {
            out << "    ERROR    : " << siem.lastError() << "\n";
            failures++;
        } else {
            out << "    rows     : " << siem.events()->rowCount()
                << " of " << siem.events()->totalCount() << " matching\n"
                << "    has next : " << (siem.events()->hasMore() ? "yes" : "no") << "\n";
        }
        out << "\n" << Qt::flush;
    }

    // 2. Stats for the same filter, and 3. detail for the first row. Both
    // are best-effort: the console degrades gracefully without either.
    {
        QEventLoop loop;
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        loop.exec();
        out << "── GET /api/siem/events/stats/\n"
            << "    total    : " << siem.statsTotal() << "\n"
            << "    alerts   : " << siem.statsAlerts() << "\n"
            << "    classes  : " << siem.statsByClass().size() << "\n"
            << "    boards   : " << siem.statsByHost().size() << "\n\n" << Qt::flush;
    }

    if (siem.events()->rowCount() > 0) {
        const int id = siem.events()->idAt(0);
        out << "── GET /api/siem/events/" << id << "/\n" << Qt::flush;

        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&siem, &SiemClient::detailChanged, [&]() {
            if (!siem.detailLoading())
                loop.quit();
        });
        guard.start(timeoutSec * 1000);
        siem.fetchDetail(id);
        if (siem.detailLoading())
            loop.exec();

        if (!siem.detailError().isEmpty()) {
            out << "    ERROR    : " << siem.detailError() << "\n";
        } else {
            const QVariantMap d = siem.detail();
            const QVariant feats = d.value(QStringLiteral("features"));
            out << "    class    : " << d.value(QStringLiteral("class_name")).toString() << "\n"
                << "    features : "
                << (feats.isNull() || !feats.isValid()
                        ? QStringLiteral("null (producer stored none)")
                        : QStringLiteral("%1 floats").arg(feats.toList().size())) << "\n"
                << "    raw      : "
                << (d.contains(QStringLiteral("raw")) ? "present" : "absent") << "\n";
        }
        out << "\n" << Qt::flush;
    }

    // 4. A deliberately invalid filter. §6 says the 400 body names the
    // offending param; this checks the console decodes that into something
    // that can be shown next to the control, rather than a raw body.
    {
        out << "── GET /api/siem/events/?min_confidence=notanumber (expects 400)\n" << Qt::flush;
        siem.filters()->setMinConfidence(QStringLiteral("notanumber"));

        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&siem, &SiemClient::loadingChanged, [&]() {
            if (!siem.loading())
                loop.quit();
        });
        guard.start(timeoutSec * 1000);
        siem.refresh();
        if (siem.loading())
            loop.exec();

        if (siem.lastError().isEmpty()) {
            out << "    UNEXPECTED: the backend accepted it\n";
            failures++;
        } else {
            out << "    message  : " << siem.lastError() << "\n"
                << "    field    : " << (siem.lastErrorField().isEmpty()
                                             ? QStringLiteral("(none named)")
                                             : siem.lastErrorField()) << "\n";
        }
        siem.filters()->setMinConfidence(QString());
        out << "\n" << Qt::flush;
    }

    // 5. The startup backfill — the one the console runs unprompted, and the
    // one whose failure leaves the alerts table empty after a restart.
    {
        out << "── startup backfill (events/?type=alert, ascending, all pages)\n" << Qt::flush;
        int rows = 0;
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&siem, &SiemClient::backfillFinished, [&](int n) {
            rows = n;
            loop.quit();
        });
        guard.start(timeoutSec * 1000);

        int pages = 0;
        QString firstTime, lastTime;
        siem.backfillAlerts([&](const QVector<SiemEventsModel::Row> &page) {
            ++pages;
            if (firstTime.isEmpty() && !page.isEmpty())
                firstTime = page.first().eventTimeRaw;
            if (!page.isEmpty())
                lastTime = page.last().eventTimeRaw;
        }, 0);
        loop.exec();

        out << "    state    : " << siem.backfillState() << "\n"
            << "    pages    : " << pages << "\n"
            << "    rows     : " << rows << " of " << siem.backfillTotal() << "\n";
        if (!firstTime.isEmpty())
            out << "    oldest   : " << firstTime << "\n"
                << "    newest   : " << lastTime << "\n";
        if (siem.backfillState() != QLatin1String("done")
                && siem.backfillState() != QLatin1String("truncated"))
            failures++;
        out << "\n" << Qt::flush;
    }

    if (failures > 0)
        err << failures << " SIEM check(s) failed.\n";
    return failures == 0 ? 0 : 1;
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
    QCommandLineOption oTok(QStringList{"token"},
                            "DRF API token — the SIEM API's `Authorization: Token`.",
                            "token", "");
    QCommandLineOption oKey(QStringList{"key"},
                            "OpenAI bearer key for the chat API "
                            "(`Authorization: Bearer`). Defaults to --token, which "
                            "the backend also accepts.", "key", "");
    QCommandLineOption oAsk(QStringList{"ask"}, "Question to ask (repeatable).", "text");
    QCommandLineOption oK(QStringList{"k"}, "Chunks to retrieve.", "n", "6");
    QCommandLineOption oBlock(QStringList{"blocking"}, "Use /chat/ instead of /chat/stream/.");
    QCommandLineOption oScen(QStringList{"scenarios"},
                             "Run the iw_chat_stub.py scenario suite.");
    QCommandLineOption oTimeout(QStringList{"timeout"}, "Per-question timeout, seconds.", "sec", "120");
    QCommandLineOption oSiem(QStringList{"siem"},
                             "Probe the read-only /api/siem/ telemetry API instead.");
    p.addOptions({ oUrl, oTok, oKey, oAsk, oK, oBlock, oScen, oTimeout, oSiem });
    p.process(app);

    BackendSettings backend;
    backend.setBaseUrl(p.value(oUrl));
    backend.setToken(p.value(oTok));
    backend.setApiKey(p.value(oKey));

    if (p.isSet(oSiem))
        return probeSiem(&backend, p.value(oTimeout).toInt());

    ChatClient client(&backend);
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
        << "key     : " << (backend.effectiveApiKey().isEmpty()
                                ? QStringLiteral("NOT SET")
                                : (backend.apiKeyIsToken()
                                       ? QStringLiteral("using the backend token")
                                       : QStringLiteral("set"))) << "\n"
        << "mode    : " << (client.streaming() ? "streaming (SSE)" : "blocking") << "\n\n"
        << Qt::flush;

    {
        // §3.2 — the cheapest way to tell "wrong URL" from "wrong key",
        // before spending a generation finding out.
        QEventLoop loop;
        QTimer guard;
        guard.setSingleShot(true);
        QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&client, &ChatClient::connectionChanged, [&]() {
            if (client.connectionState() != QLatin1String("testing"))
                loop.quit();
        });
        guard.start(20000);
        client.testConnection();
        if (client.connectionState() == QLatin1String("testing"))
            loop.exec();
        out << "models  : " << client.connectionState() << " — "
            << client.connectionDetail() << "\n\n" << Qt::flush;
    }

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
