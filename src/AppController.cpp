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
    , m_chat(new ChatClient(this))
{
    // Keep the inspector pointed at the newest detection until the operator
    // picks one; after that, leave their selection alone.
    connect(m_detections, &DetectionsModel::detectionAdded, this, [this]() {
        if (m_selectedDetectionIndex < 0) {
            m_selectedDetectionIndex = 0;
            emit selectedDetectionChanged();
        } else {
            // Indices shift by one on prepend, so follow the same detection.
            m_selectedDetectionIndex++;
            emit selectedDetectionChanged();
        }
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

    m_telemetry->loadSettings();
    m_chat->loadSettings();

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
