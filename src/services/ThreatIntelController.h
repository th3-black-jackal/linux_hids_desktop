#pragma once

// ThreatIntelController — the `Intel` singleton the Threat Intel window binds
// to. Ties the IntelClient (backend cti-graph service, /api/cti-graph/v1/…) to
// the two models and the current selection, and turns a node selection into the
// other two sections' content:
//   select(node) -> graph highlights it
//                -> Reference pane loads the node's associated URL (safety-gated)
//                -> News feed filters to articles mentioning node (news/?entity=)
//
// The API returns no computed URL, so associatedUrl() is derived client-side;
// the graph model's nodes already carry {urlHref,urlSafe,urlRaw}, and select()
// reads them off the chosen node, then calls entities/?id= to enrich the bare
// {id,type,name} ref with full properties (attack_id, url) and recompute the
// URL. seedFrom/expand both call entities/neighbors/?id= (one hop) and the
// controller replaces vs. merges into the graph model; loadOverview lists top
// nodes of a label (plus stats/) as starting points to expand.
//
// Mirrors DrIronwatchController's shape (phase/busy/error + models exposed as
// CONSTANT properties). It borrows the app's BackendSettings so it shares the
// one host/token config; it owns nothing from the live telemetry graph.

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "threat_intel/ThreatGraphModel.h"
#include "threat_intel/NewsModel.h"

class IntelClient;
class BackendSettings;

class ThreatIntelController : public QObject {
    Q_OBJECT
    Q_PROPERTY(ThreatGraphModel *graph READ graph CONSTANT)
    Q_PROPERTY(NewsModel *news READ news CONSTANT)

    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)      // idle|loading|ready|error
    Q_PROPERTY(bool busy READ busy NOTIFY phaseChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY phaseChanged)

    // The current selection and the URL the Reference pane should show.
    Q_PROPERTY(QVariantMap selectedNode READ selectedNode NOTIFY selectionChanged)
    Q_PROPERTY(QString referenceUrl READ referenceUrl NOTIFY selectionChanged)  // "" until safe
    Q_PROPERTY(bool referenceUrlSafe READ referenceUrlSafe NOTIFY selectionChanged)
    Q_PROPERTY(QString referenceUrlRaw READ referenceUrlRaw NOTIFY selectionChanged) // defanged IOC
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
    Q_PROPERTY(bool backendConfigured READ backendConfigured NOTIFY phaseChanged)
    Q_PROPERTY(int newsDays READ newsDays WRITE setNewsDays NOTIFY newsDaysChanged)  // feed lookback window
    // True only when the build linked Qt WebEngine; QML uses it to choose the
    // embedded browser pane vs. the system-browser fallback.
    Q_PROPERTY(bool webEngineAvailable READ webEngineAvailable CONSTANT)

public:
    explicit ThreatIntelController(QObject *appController, QObject *parent = nullptr);

    ThreatGraphModel *graph() { return &m_graph; }
    NewsModel *news() { return &m_news; }
    QString phase() const { return m_phase; }
    bool busy() const { return m_phase == QLatin1String("loading"); }
    QString lastError() const { return m_lastError; }
    QVariantMap selectedNode() const { return m_selected; }
    QString referenceUrl() const { return m_refSafe ? m_refUrl : QString(); }
    bool referenceUrlSafe() const { return m_refSafe; }
    // Defanged for display (hxxp://evil[.]example); the real URL stays in
    // m_refRaw and is only loaded if openRawReference() is called.
    QString referenceUrlRaw() const { return defang(m_refRaw); }
    QVariantList searchResults() const { return m_search; }
    bool backendConfigured() const;

    // QML entry points.
    Q_INVOKABLE void loadOverview(const QVariantMap &filters = {});   // initial graph
    Q_INVOKABLE void seedFrom(const QString &nodeId);                 // centre + depth
    Q_INVOKABLE void expand(const QString &nodeId);                   // click-to-expand
    Q_INVOKABLE void select(const QString &nodeId);                   // drive panes
    Q_INVOKABLE void search(const QString &q);
    Q_INVOKABLE void openRawReference();   // operator opted into a dangerous IOC URL
    Q_INVOKABLE void refresh();

    int newsDays() const { return m_newsDays; }
    void setNewsDays(int days);            // re-queries /articles/?days= for the current entity

    bool webEngineAvailable() const {
#ifdef IRONWATCH_HAS_WEBENGINE
        return true;
#else
        return false;
#endif
    }

    // Show an arbitrary safe URL in the Reference pane (e.g. a clicked news
    // article), independent of the current graph selection.
    Q_INVOKABLE void showUrl(const QString &url);

signals:
    void phaseChanged();
    void selectionChanged();
    void searchChanged();
    void newsDaysChanged();

private:
    void setPhase(const QString &p, const QString &err = {});
    static QString defang(const QString &url);   // http->hxxp, "."->"[.]" for display

    ThreatGraphModel m_graph;
    NewsModel m_news;
    IntelClient *m_client = nullptr;
    BackendSettings *m_backend = nullptr;   // shared, not owned

    QString m_phase = QStringLiteral("idle");
    QString m_lastError;
    QVariantMap m_selected;
    QString m_refUrl, m_refRaw;
    bool m_refSafe = false;
    QVariantList m_search;
    int m_newsDays = 30;
    bool m_seeding = false;   // next graphReceived replaces (seed) vs merges (expand)
};
