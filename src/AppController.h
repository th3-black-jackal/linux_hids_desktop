#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Q_PROPERTY declarations below need the complete type for each of these
// (moc's generated metatype registration inspects the pointee type), so
// forward declarations aren't enough here — unlike plain member pointers.
#include "core/ClassStatsModel.h"
#include "core/DetectionsModel.h"
#include "core/EventStreamModel.h"
#include "core/LogModel.h"
#include "core/NodesModel.h"
#include "services/AlertExplainer.h"
#include "services/BackendSettings.h"
#include "services/EventWebLink.h"
#include "services/ChatClient.h"
#include "services/SiemClient.h"
#include "services/TelemetryClient.h"

class Detection;
class Node;
class QTimer;

// Root of the app's object graph: owns every model/service, and holds the
// UI-only state the screens key off (current screen, theme, selection, the
// title-bar clock). Exposed to QML as the "App" singleton from main.cpp.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QString clock READ clock NOTIFY clockChanged)

    Q_PROPERTY(int selectedNodeIndex READ selectedNodeIndex NOTIFY selectedNodeChanged)
    Q_PROPERTY(QObject *selectedNode READ selectedNode NOTIFY selectedNodeChanged)
    Q_PROPERTY(QVariantList selectedNodeConfidence READ selectedNodeConfidence NOTIFY selectedNodeChanged)
    Q_PROPERTY(int selectedDetectionIndex READ selectedDetectionIndex NOTIFY selectedDetectionChanged)
    Q_PROPERTY(QObject *selectedDetection READ selectedDetection NOTIFY selectedDetectionChanged)

    Q_PROPERTY(NodesModel *nodesModel READ nodesModel CONSTANT)
    Q_PROPERTY(DetectionsModel *detectionsModel READ detectionsModel CONSTANT)
    Q_PROPERTY(EventStreamModel *eventStream READ eventStream CONSTANT)
    Q_PROPERTY(LogModel *serviceLog READ serviceLog CONSTANT)
    Q_PROPERTY(ClassStatsModel *classStats READ classStats CONSTANT)
    Q_PROPERTY(TelemetryClient *telemetry READ telemetry CONSTANT)
    Q_PROPERTY(ChatClient *chat READ chat CONSTANT)
    Q_PROPERTY(SiemClient *siem READ siem CONSTANT)
    Q_PROPERTY(BackendSettings *backend READ backend CONSTANT)
    Q_PROPERTY(AlertExplainer *explainer READ explainer CONSTANT)
    Q_PROPERTY(EventWebLink *webLink READ webLink CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);

    QString screen() const { return m_screen; }
    QString theme() const { return m_theme; }
    QString clock() const { return m_clock; }

    int selectedNodeIndex() const { return m_selectedNodeIndex; }
    QObject *selectedNode() const;
    QVariantList selectedNodeConfidence() const;
    int selectedDetectionIndex() const { return m_selectedDetectionIndex; }
    QObject *selectedDetection() const;

    NodesModel *nodesModel() const { return m_telemetry->nodes(); }
    DetectionsModel *detectionsModel() const { return m_detections; }
    EventStreamModel *eventStream() const { return m_stream; }
    LogModel *serviceLog() const { return m_log; }
    ClassStatsModel *classStats() const { return m_classStats; }
    TelemetryClient *telemetry() const { return m_telemetry; }
    ChatClient *chat() const { return m_chat; }
    SiemClient *siem() const { return m_siem; }
    BackendSettings *backend() const { return m_backend; }
    AlertExplainer *explainer() const { return m_explainer; }
    EventWebLink *webLink() const { return m_webLink; }

    Q_INVOKABLE void goOverview();
    Q_INVOKABLE void goDetections();
    Q_INVOKABLE void goNodes();
    Q_INVOKABLE void goModel();
    Q_INVOKABLE void goIntegrations();
    Q_INVOKABLE void goChat();
    Q_INVOKABLE void goEvents();

    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void selectNode(int index);
    Q_INVOKABLE void selectDetection(int originalIndex);

    Q_INVOKABLE void toggleAcknowledgeSelected();
    Q_INVOKABLE void markBenignSelected();
    Q_INVOKABLE void acknowledgeAll();
    Q_INVOKABLE void copySelectedAsJson();

    // Opens the assistant with a question drafted from the selected
    // detection. The draft is composed only of fields the event actually
    // carries; the operator edits and sends it.
    Q_INVOKABLE void askAboutSelectedDetection();

    // Right-click on a row in the Detections table: opens the explain popup
    // for that detection without disturbing the Assistant conversation.
    Q_INVOKABLE void explainDetection(int originalIndex);
    // The same, from the Events table, whose rows are database rows rather
    // than Detection objects.
    Q_INVOKABLE void explainEventRow(const QString &host, const QString &className,
                                     int classIdx, const QString &confidenceText,
                                     const QString &timestampRaw, const QString &severity,
                                     int eventId);
    // Moves whatever the popup is asking into the full Assistant screen.
    Q_INVOKABLE void openExplainInAssistant();

    // Opens one event in the external web app (see EventWebLink). Bound to
    // double-click and to an explicit button, *not* to plain selection —
    // selection also happens from the keyboard and programmatically, and
    // launching a browser on every row change would make the table unusable.
    Q_INVOKABLE void openDetectionInWeb(int originalIndex);
    Q_INVOKABLE void openEventRowInWeb(int eventId, const QString &host,
                                       const QString &className, int classIdx,
                                       const QString &confidenceText,
                                       const QString &timestampRaw, const QString &severity);

signals:
    void screenChanged();
    void themeChanged();
    void clockChanged();
    void selectedNodeChanged();
    void selectedDetectionChanged();

private:
    void setScreen(const QString &s);

    QString m_screen = QStringLiteral("overview");
    QString m_theme = QStringLiteral("light");
    QString m_clock;

    int m_selectedNodeIndex = 0;
    int m_selectedDetectionIndex = -1;
    // False until the operator clicks a row: before that the inspector
    // tracks the newest detection instead of a fixed one.
    bool m_userSelectedDetection = false;
    int m_lastDetectionTotal = 0;

    DetectionsModel *m_detections;
    EventStreamModel *m_stream;
    LogModel *m_log;
    ClassStatsModel *m_classStats;
    TelemetryClient *m_telemetry;
    BackendSettings *m_backend;
    ChatClient *m_chat;
    SiemClient *m_siem;
    AlertExplainer *m_explainer;
    EventWebLink *m_webLink;

    QTimer *m_clockTimer;
};
