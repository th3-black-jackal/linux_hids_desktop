#include "TelemetryClient.h"

#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTextStream>
#include <QTime>
#include <QTimer>

namespace {

// ironwatch/<host>/<leaf> — returns false for anything else.
bool splitTopic(const QString &topic, QString *host, QString *leaf)
{
    const QStringList parts = topic.split(QLatin1Char('/'));
    if (parts.size() != 3 || parts.at(0) != QLatin1String("ironwatch"))
        return false;
    *host = parts.at(1);
    *leaf = parts.at(2);
    return !host->isEmpty();
}

} // namespace

TelemetryClient::TelemetryClient(DetectionsModel *detections, EventStreamModel *stream,
                                 LogModel *log, ClassStatsModel *classStats, QObject *parent)
    : QObject(parent)
    , m_detections(detections)
    , m_stream(stream)
    , m_log(log)
    , m_classStats(classStats)
    , m_mqtt(new MqttClient(this))
{
    connect(&m_nodes, &NodesModel::countChanged, this, &TelemetryClient::statsChanged);
    connect(m_mqtt, &MqttClient::stateChanged, this, &TelemetryClient::statsChanged);
    connect(m_mqtt, &MqttClient::messageReceived, this, &TelemetryClient::onMessage);
    connect(m_mqtt, &MqttClient::logged, this, [this](const QString &msg) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), msg });
    });

    connect(&m_nodes, &NodesModel::nodeAdded, this, [this](const QString &host) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("board discovered: %1").arg(host) });
    });

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &TelemetryClient::onTick);
    m_tick->start();
}

QString TelemetryClient::brokerEndpoint() const
{
    return QStringLiteral("%1:%2%3")
        .arg(m_brokerHost)
        .arg(m_brokerPort)
        .arg(tlsEnabled() ? QStringLiteral(" (TLS)") : QString());
}

void TelemetryClient::loadSettings()
{
    // Setters persist as a side effect; without this guard a setter firing
    // mid-load would write back a half-read configuration.
    m_loading = true;

    QSettings s;
    m_brokerHost     = s.value(QStringLiteral("broker/host"), m_brokerHost).toString();
    m_brokerPort     = s.value(QStringLiteral("broker/port"), m_brokerPort).toInt();
    m_brokerUser     = s.value(QStringLiteral("broker/user"), m_brokerUser).toString();
    m_brokerPassword = s.value(QStringLiteral("broker/password")).toString();
    m_caFile         = s.value(QStringLiteral("broker/caFile")).toString();

    m_policy.criticalAt = s.value(QStringLiteral("severity/criticalAt"), 0.90).toDouble();
    m_policy.highAt     = s.value(QStringLiteral("severity/highAt"), 0.75).toDouble();
    m_policy.mediumAt   = s.value(QStringLiteral("severity/mediumAt"), 0.50).toDouble();

    // Stored as a QStringList: the INI backend parses a comma-containing
    // value as a list, so a joined string round-trips back as empty.
    const QStringList normals = s.value(
        QStringLiteral("severity/normalClasses"),
        QStringList{ "normal", "benign", "none", "clean", "idle" }).toStringList();
    if (!normals.isEmpty())
        m_policy.normalClasses = normals;

    m_featureNamesPath = s.value(QStringLiteral("model/featureNamesPath")).toString();
    loadFeatureNames();

    m_loading = false;
    emit settingsChanged();
    emit policyChanged();

    reconnect();
}

void TelemetryClient::saveSettings()
{
    if (m_loading)
        return;

    QSettings s;
    s.setValue(QStringLiteral("broker/host"), m_brokerHost);
    s.setValue(QStringLiteral("broker/port"), m_brokerPort);
    s.setValue(QStringLiteral("broker/user"), m_brokerUser);
    // NOTE: QSettings stores this in the clear (registry / plain INI). For a
    // deployment that matters, move it to the platform keychain or issue the
    // console a client certificate instead of a password.
    s.setValue(QStringLiteral("broker/password"), m_brokerPassword);
    s.setValue(QStringLiteral("broker/caFile"), m_caFile);

    s.setValue(QStringLiteral("severity/criticalAt"), m_policy.criticalAt);
    s.setValue(QStringLiteral("severity/highAt"), m_policy.highAt);
    s.setValue(QStringLiteral("severity/mediumAt"), m_policy.mediumAt);
    s.setValue(QStringLiteral("severity/normalClasses"), m_policy.normalClasses);
    s.setValue(QStringLiteral("model/featureNamesPath"), m_featureNamesPath);
}

void TelemetryClient::reconnect()
{
    MqttClient::Options o;
    o.host = m_brokerHost;
    o.port = m_brokerPort;
    o.username = m_brokerUser;
    o.password = m_brokerPassword;
    o.caFile = m_caFile;
    o.clientId = QStringLiteral("ironwatch-console-%1").arg(QCoreApplication::applicationPid());

    m_mqtt->start(o);

    // Subscribed in priority order. Status first so the fleet is populated
    // (from retained messages) before any traffic arrives; alerts at QoS 1
    // because losing one is losing an intrusion; events at QoS 0 because a
    // dropped normal window is replaced a second later.
    m_mqtt->subscribe(QStringLiteral("ironwatch/+/status"), 1);
    m_mqtt->subscribe(QStringLiteral("ironwatch/+/alerts"), 1);
    m_mqtt->subscribe(QStringLiteral("ironwatch/+/events"), 0);
}

void TelemetryClient::onMessage(const QString &topic, const QByteArray &payload)
{
    QString host, leaf;
    if (!splitTopic(topic, &host, &leaf)) {
        m_framesDropped++;
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("unrecognized topic: %1").arg(topic) });
        return;
    }

    if (leaf == QLatin1String("status"))
        handleStatus(host, payload);
    else if (leaf == QLatin1String("alerts"))
        handleEvent(host, payload, true);
    else if (leaf == QLatin1String("events"))
        handleEvent(host, payload, false);

    emit statsChanged();
}

void TelemetryClient::handleStatus(const QString &host, const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    const QJsonObject o = doc.object();

    const QString state = o.value(QStringLiteral("state")).toString();
    const QString reason = o.value(QStringLiteral("reason")).toString();
    const bool online = (state == QLatin1String("online"));

    Node *node = m_nodes.ensureNode(host);
    node->applyStatus(online, reason);

    // "lwt" means the broker published the will because the board stopped
    // answering — a crash or a dead link, as distinct from a clean stop.
    const QString detail = online
        ? QStringLiteral("online")
        : (reason == QLatin1String("lwt")
               ? QStringLiteral("OFFLINE (lost — last will)")
               : QStringLiteral("offline (%1)").arg(reason.isEmpty() ? QStringLiteral("clean") : reason));

    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("%1: %2").arg(host, detail) });
}

void TelemetryClient::handleEvent(const QString &host, const QByteArray &payload, bool fromAlertsTopic)
{
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        m_framesDropped++;
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("%1: malformed JSON (%2)").arg(host, perr.errorString()) });
        return;
    }

    QString parseError;
    auto ev = DetectionEvent::parse(doc.object(), &parseError);
    if (!ev) {
        m_framesDropped++;
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("%1: %2").arg(host, parseError) });
        return;
    }

    // The topic is authoritative for identity: the broker ACL guarantees a
    // board can only publish under its own hostname, whereas the `host`
    // field inside the payload is just something the board typed. Trusting
    // the topic means a compromised board cannot attribute its events to a
    // different one.
    ev->endpoint = host;
    ev->host = host;

    m_lastEventAt = QDateTime::currentDateTime();

    const bool isNormal = m_policy.isNormal(ev->className);
    const QString severity = m_policy.severityFor(*ev);

    // The board already decided this by choosing the topic; disagreement
    // means the console's "normal classes" list is out of step with the
    // board's model, which is worth surfacing rather than silently
    // reclassifying.
    if (fromAlertsTopic == isNormal) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("%1: '%2' arrived on %3 but console classes it %4 — "
                                          "check the normal-class list on Model & Rules")
                               .arg(host, ev->className,
                                    fromAlertsTopic ? QStringLiteral("alerts") : QStringLiteral("events"),
                                    isNormal ? QStringLiteral("normal") : QStringLiteral("non-normal")) });
    }

    Node *node = m_nodes.ensureNode(host);
    node->applyEvent(*ev, !isNormal);

    m_classStats->record(ev->classIdx, ev->className, ev->confidence, isNormal);

    const QDateTime t = ev->effectiveTime();
    EventStreamModel::Entry e;
    e.time = t.isValid() ? t.toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("—");
    e.node = host;
    e.severity = severity;
    e.className = ev->className;
    e.confidenceText = QString::number(ev->confidence, 'f', 3);
    e.featureCount = ev->features.size();
    m_stream->pushEvent(e);

    if (!isNormal) {
        const QString uid = QStringLiteral("IW-%1-%2")
            .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd")))
            .arg(++m_uidCounter, 4, 10, QChar('0'));
        m_detections->addDetection(
            new Detection(*ev, uid, severity, m_featureNames),
            DetectionsModel::dedupKeyFor(host, ev->timestampRaw, ev->classIdx));
    }
}

// ── SIEM backfill ────────────────────────────────────────────────────────

void TelemetryClient::backfillFrom(SiemClient *siem)
{
    m_siem = siem;
    // The database rows must be graded by the same policy as the live ones,
    // and loadSettings() has already run by the time this is called.
    m_siem->setPolicy(m_policy);

    connect(m_siem, &SiemClient::logged, this, [this](const QString &msg) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), msg });
    });

    m_siem->backfillAlerts(
        [this](const QVector<SiemEventsModel::Row> &page) { ingestSiemPage(page); },
        /*maxRows=*/0);
}

void TelemetryClient::ingestSiemPage(const QVector<SiemEventsModel::Row> &page)
{
    QVector<Detection *> batch;
    QVector<QString> keys;
    batch.reserve(page.size());
    keys.reserve(page.size());

    for (const SiemEventsModel::Row &row : page) {
        DetectionEvent ev;
        ev.timestampRaw = row.eventTimeRaw;
        ev.timestamp    = row.eventTime;
        ev.receivedAt   = QDateTime::fromString(row.receivedAtRaw, Qt::ISODate);
        ev.classIdx     = row.classIdx;
        ev.className    = row.className;
        ev.confidence   = row.confidence;
        ev.host         = row.host;
        ev.endpoint     = row.host;
        // The list endpoint carries no feature vector by design (§3.1, §7.5);
        // it is only on events/<id>/. Leaving this empty is the honest state
        // — the inspector shows 0 features rather than a fabricated vector.

        // A uid that names its origin: IW-DB-42 is row 42 of the SIEM
        // database and can be looked up with GET /api/siem/events/42/, where
        // the live IW-<date>-<n> form is console-local and has no
        // server-side identity at all.
        const QString uid = row.id >= 0 ? QStringLiteral("IW-DB-%1").arg(row.id)
                                        : QStringLiteral("IW-DB-?");

        batch.append(new Detection(ev, uid, row.severity, m_featureNames));
        keys.append(DetectionsModel::dedupKeyFor(row.host, row.eventTimeRaw, row.classIdx));
    }

    // Backfill fills the triage queue and nothing else. The node registry,
    // the class-distribution panel and the Overview event stream are all
    // explicitly *live* views — replaying a week of history into them would
    // make "last event 3 s ago" and "N windows observed" lie about the
    // running system. m_lastEventAt is likewise left alone.
    m_detections->addBatch(batch, keys);
}

void TelemetryClient::onTick()
{
    const qint64 bytes = m_mqtt->bytesReceived();
    m_throughputBps = static_cast<double>(bytes - m_bytesAtLastTick);
    m_bytesAtLastTick = bytes;

    for (int i = 0; i < m_nodes.count(); ++i)
        m_nodes.nodeAt(i)->refreshStats();

    emit statsChanged();
}

QString TelemetryClient::throughputText() const
{
    if (m_throughputBps >= 1024.0)
        return QStringLiteral("%1 KB/s").arg(m_throughputBps / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 B/s").arg(m_throughputBps, 0, 'f', 0);
}

QString TelemetryClient::lastEventAge() const
{
    if (!m_lastEventAt.isValid())
        return QStringLiteral("no data yet");
    const qint64 secs = m_lastEventAt.secsTo(QDateTime::currentDateTime());
    if (secs < 2)    return QStringLiteral("just now");
    if (secs < 60)   return QStringLiteral("%1 s ago").arg(secs);
    if (secs < 3600) return QStringLiteral("%1 min ago").arg(secs / 60);
    return QStringLiteral("%1 h ago").arg(secs / 3600);
}

QString TelemetryClient::windowPeriodText() const
{
    double sum = 0.0;
    int n = 0;
    for (int i = 0; i < m_nodes.count(); ++i) {
        const double p = m_nodes.nodeAt(i)->windowPeriodSecs();
        if (p > 0.0) { sum += p; ++n; }
    }
    if (n == 0)
        return QStringLiteral("\u2014");
    return QStringLiteral("%1 s").arg(sum / n, 0, 'f', 1);
}

// ── setters ──────────────────────────────────────────────────────────────

void TelemetryClient::setBrokerHost(const QString &v)
{
    if (m_brokerHost == v) return;
    m_brokerHost = v; saveSettings(); emit settingsChanged();
}

void TelemetryClient::setBrokerPort(int v)
{
    if (m_brokerPort == v || v <= 0 || v > 65535) return;
    m_brokerPort = v; saveSettings(); emit settingsChanged();
}

void TelemetryClient::setBrokerUser(const QString &v)
{
    if (m_brokerUser == v) return;
    m_brokerUser = v; saveSettings(); emit settingsChanged();
}

void TelemetryClient::setBrokerPassword(const QString &v)
{
    if (m_brokerPassword == v) return;
    m_brokerPassword = v; saveSettings(); emit settingsChanged();
}

void TelemetryClient::setCaFile(const QString &v)
{
    if (m_caFile == v) return;
    m_caFile = v; saveSettings(); emit settingsChanged();
}

void TelemetryClient::setCriticalAt(double v)
{
    if (qFuzzyCompare(m_policy.criticalAt, v)) return;
    m_policy.criticalAt = v; saveSettings(); emit policyChanged();
}

void TelemetryClient::setHighAt(double v)
{
    if (qFuzzyCompare(m_policy.highAt, v)) return;
    m_policy.highAt = v; saveSettings(); emit policyChanged();
}

void TelemetryClient::setMediumAt(double v)
{
    if (qFuzzyCompare(m_policy.mediumAt, v)) return;
    m_policy.mediumAt = v; saveSettings(); emit policyChanged();
}

void TelemetryClient::setNormalClassesText(const QString &v)
{
    QStringList parts;
    for (const QString &p : v.split(QLatin1Char(','), Qt::SkipEmptyParts))
        parts << p.trimmed();
    if (parts == m_policy.normalClasses)
        return;
    m_policy.normalClasses = parts;
    saveSettings();
    emit policyChanged();
}

void TelemetryClient::setFeatureNamesPath(const QString &path)
{
    if (m_featureNamesPath == path)
        return;
    m_featureNamesPath = path;
    loadFeatureNames();
    saveSettings();
    emit policyChanged();
}

void TelemetryClient::loadFeatureNames()
{
    m_featureNames.clear();
    if (m_featureNamesPath.isEmpty())
        return;

    QFile f(m_featureNamesPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                           QStringLiteral("feature names: cannot open %1").arg(m_featureNamesPath) });
        return;
    }

    // One name per line in FEATURE_ORDER order; blanks and #-comments are
    // skipped so the file can be pasted out of feature_spec.hpp.
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        QString line = ts.readLine().trimmed();
        const int hash = line.indexOf(QLatin1Char('#'));
        if (hash >= 0)
            line = line.left(hash).trimmed();
        if (!line.isEmpty())
            m_featureNames << line;
    }

    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("feature names: loaded %1 from %2")
                           .arg(m_featureNames.size()).arg(m_featureNamesPath) });
}
