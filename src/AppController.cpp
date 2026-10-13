#include "AppController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include <QTimer>

#include "core/Detection.h"
#include "core/Node.h"

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_detections(new DetectionsModel(this))
    , m_stream(new EventStreamModel(this))
    , m_log(new LogModel(this))
    , m_classStats(new ClassStatsModel(this))
    , m_telemetry(new TelemetryClient(m_detections, m_stream, m_log, m_classStats, this))
    , m_backend(new BackendSettings(this))
    , m_chat(new ChatClient(m_backend, this))
    , m_siem(new SiemClient(m_backend, this))
    , m_explainer(new AlertExplainer(m_backend, this))
    , m_webLink(new EventWebLink(this))
{
    connect(m_webLink, &EventWebLink::logged, this, [this](const QString &msg) {
        m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), msg });
    });

    // Keep the inspector pointed at the newest detection until the operator
    // picks one; after that, follow *their* row as new arrivals shift it
    // down. Which of the two applies is what m_userSelectedDetection tracks:
    // without it, the first detection ever added became sticky, and a
    // startup backfill (which arrives oldest-first, in bulk) would leave the
    // inspector pinned to the oldest alert in the database.
    connect(m_detections, &DetectionsModel::detectionAdded, this, [this]() {
        if (!m_userSelectedDetection) {
            m_selectedDetectionIndex = 0;
        } else if (m_selectedDetectionIndex >= 0) {
            // Prepends shift every existing index down by however many rows
            // arrived; recover the offset from the model rather than
            // assuming one at a time, since a backfill page lands at once.
            const int shift = m_detections->total() - m_lastDetectionTotal;
            if (shift > 0)
                m_selectedDetectionIndex += shift;
        }
        m_lastDetectionTotal = m_detections->total();
        emit selectedDetectionChanged();
    });

    // The selected board's counters and confidence history change on every
    // incoming window, not just when the selection moves — without this the
    // Nodes/Overview charts would freeze at whatever the board looked like
    // when it was first selected.
    connect(m_telemetry->nodes(), &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
        if (m_selectedNodeIndex >= topLeft.row() && m_selectedNodeIndex <= bottomRight.row())
            emit selectedNodeChanged();
    });

    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("console started") });

    m_backend->load();
    m_telemetry->loadSettings();
    m_chat->loadSettings();
    m_explainer->loadSettings();
    m_webLink->loadSettings();

    // The database is the console's memory: MQTT only ever shows what has
    // happened since it connected, so without this the alerts table starts
    // empty after every restart. The live subscription above is untouched —
    // it keeps adding new alerts on top, and DetectionsModel drops any that
    // both sources deliver.
    m_telemetry->backfillFrom(m_siem);

    // Boards are discovered from the broker's retained status topic, so
    // there is nothing to configure per board — only the broker itself.
    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("subscribing to ironwatch/+/{status,alerts,events}") });

    m_clock = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_clockTimer = new QTimer(this);
    m_clockTimer->setInterval(1000);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        m_clock = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
        emit clockChanged();
    });
    m_clockTimer->start();
}

QObject *AppController::selectedNode() const
{
    return m_telemetry->nodes()->nodeAt(m_selectedNodeIndex);
}

QVariantList AppController::selectedNodeConfidence() const
{
    QVariantList out;
    const Node *n = m_telemetry->nodes()->nodeAt(m_selectedNodeIndex);
    if (!n)
        return out;
    for (double v : n->confidenceHistory())
        out.append(v);
    return out;
}

QObject *AppController::selectedDetection() const
{
    return m_detections->detectionAt(m_selectedDetectionIndex);
}

void AppController::setScreen(const QString &s)
{
    if (m_screen == s)
        return;
    m_screen = s;
    emit screenChanged();
}

void AppController::goOverview() { setScreen(QStringLiteral("overview")); }
void AppController::goDetections() { setScreen(QStringLiteral("detections")); }
void AppController::goNodes() { setScreen(QStringLiteral("nodes")); }
void AppController::goModel() { setScreen(QStringLiteral("model")); }
void AppController::goIntegrations() { setScreen(QStringLiteral("integrations")); }
void AppController::goChat() { setScreen(QStringLiteral("chat")); }
void AppController::goEvents() { setScreen(QStringLiteral("events")); }

void AppController::toggleTheme()
{
    m_theme = (m_theme == QStringLiteral("dark")) ? QStringLiteral("light") : QStringLiteral("dark");
    emit themeChanged();
}

void AppController::selectNode(int index)
{
    if (index < 0 || index >= m_telemetry->nodes()->count())
        return;
    m_selectedNodeIndex = index;
    emit selectedNodeChanged();
    goNodes();
}

void AppController::selectDetection(int originalIndex)
{
    if (!m_detections->detectionAt(originalIndex))
        return;
    m_selectedDetectionIndex = originalIndex;
    m_userSelectedDetection = true;
    emit selectedDetectionChanged();
}

void AppController::toggleAcknowledgeSelected()
{
    m_detections->toggleAcknowledge(m_selectedDetectionIndex);
}

void AppController::acknowledgeAll()
{
    m_detections->acknowledgeAll();
}

void AppController::markBenignSelected()
{
    Detection *d = m_detections->detectionAt(m_selectedDetectionIndex);
    if (!d)
        return;
    d->setDisposition(QStringLiteral("benign"));
    d->setAcknowledged(true);
    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("triage: %1 marked benign").arg(d->uid()) });
}

void AppController::copySelectedAsJson()
{
    Detection *d = m_detections->detectionAt(m_selectedDetectionIndex);
    if (!d)
        return;

    // Reconstructs the event as received, for pasting into a ticket or diffing
    // against the board's Wazuh log line.
    const DetectionEvent &ev = d->event();
    QJsonObject o;
    o[QStringLiteral("timestamp")] = ev.timestampRaw;
    o[QStringLiteral("classIdx")]  = ev.classIdx;
    o[QStringLiteral("className")] = ev.className;
    o[QStringLiteral("confidence")] = ev.confidence;
    o[QStringLiteral("host")] = ev.host;
    QJsonArray feats;
    for (double f : ev.features)
        feats.append(f);
    o[QStringLiteral("features")] = feats;

    QGuiApplication::clipboard()->setText(
        QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented)));

    m_log->pushEntry({ QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                       QStringLiteral("copied %1 to clipboard").arg(d->uid()) });
}

void AppController::askAboutSelectedDetection()
{
    Detection *d = m_detections->detectionAt(m_selectedDetectionIndex);
    if (!d) {
        goChat();
        return;
    }

    // Every value below is on the wire. The class name is the *model's* own
    // label, so the question says so — the backend must not be led into
    // treating it as an ATT&CK identifier it already knows.
    const QString q =
        QStringLiteral("A host-based IDS on board \"%1\" classified a system-call window as "
                       "\"%2\" (the model's own class label, index %3) with %4 confidence at %5. "
                       "Which MITRE ATT&CK techniques would match that behaviour on a Linux "
                       "edge device, and what mitigations apply?")
            .arg(d->node(), d->className())
            .arg(d->classIdx())
            .arg(d->confidenceText(), d->timestampRaw());

    m_chat->setDraft(q);
    goChat();
}

void AppController::explainDetection(int originalIndex)
{
    Detection *d = m_detections->detectionAt(originalIndex);
    if (!d)
        return;

    // Right-click selects too: the popup and the inspector should never be
    // describing two different rows.
    selectDetection(originalIndex);

    m_explainer->explain(d->uid(), d->node(), d->className(), d->classIdx(),
                         d->confidenceText(), d->timestampRaw(), d->severity());
}

void AppController::explainEventRow(const QString &host, const QString &className,
                                    int classIdx, const QString &confidenceText,
                                    const QString &timestampRaw, const QString &severity,
                                    int eventId)
{
    // Database rows have a server-side id, so the popup can name it the same
    // way a backfilled detection does.
    m_explainer->explain(QStringLiteral("IW-DB-%1").arg(eventId), host, className,
                         classIdx, confidenceText, timestampRaw, severity);
}

void AppController::openExplainInAssistant()
{
    const QString q = m_explainer->questionForHandoff();
    if (q.isEmpty())
        return;
    // The popup's own request is cancelled rather than left racing the
    // Assistant for the backend's single generation slot.
    m_explainer->close();
    m_chat->setDraft(q);
    goChat();
}

void AppController::openDetectionInWeb(int originalIndex)
{
    Detection *d = m_detections->detectionAt(originalIndex);
    if (!d)
        return;

    selectDetection(originalIndex);

    // A backfilled row's uid is IW-DB-<id>, which is the only place a live
    // triage row can carry a server-side event id — a detection that arrived
    // over MQTT has none, and the link goes out identified by host and
    // timestamp instead.
    QString id;
    const QString uid = d->uid();
    if (uid.startsWith(QLatin1String("IW-DB-")))
        id = uid.mid(6);

    m_webLink->openEvent(id, uid, d->node(), d->className(), d->classIdx(),
                         d->confidenceText(), d->timestampRaw(), d->severity());
}

void AppController::openEventRowInWeb(int eventId, const QString &host,
                                      const QString &className, int classIdx,
                                      const QString &confidenceText,
                                      const QString &timestampRaw, const QString &severity)
{
    m_webLink->openEvent(eventId >= 0 ? QString::number(eventId) : QString(),
                         eventId >= 0 ? QStringLiteral("IW-DB-%1").arg(eventId) : QString(),
                         host, className, classIdx, confidenceText, timestampRaw, severity);
}
