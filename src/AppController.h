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

    Q_INVOKABLE void goOverview();
    Q_INVOKABLE void goDetections();
    Q_INVOKABLE void goNodes();
    Q_INVOKABLE void goModel();
    Q_INVOKABLE void goIntegrations();

    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void selectNode(int index);
    Q_INVOKABLE void selectDetection(int originalIndex);

    Q_INVOKABLE void toggleAcknowledgeSelected();
    Q_INVOKABLE void markBenignSelected();
    Q_INVOKABLE void acknowledgeAll();
    Q_INVOKABLE void copySelectedAsJson();

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

    DetectionsModel *m_detections;
    EventStreamModel *m_stream;
    LogModel *m_log;
    ClassStatsModel *m_classStats;
    TelemetryClient *m_telemetry;

    QTimer *m_clockTimer;
};
