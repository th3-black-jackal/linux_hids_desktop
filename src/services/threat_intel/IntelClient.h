#pragma once

// IntelClient — Qt client for the Threat Graph API (project
// GRAPH_API_DOCUMENTATION.md): the `cti-graph` microservice (`threatgraph`
// Django app) over the Neo4j CTI knowledge graph.
//
// It calls the NORMAL backend at `/api/cti-graph/v1/…` (Token auth, read-only,
// GET except the bulk indicator lookup and the admin query). The backend holds
// the Neo4j driver; the app never opens a Bolt connection and shares the one
// BackendSettings used by the SIEM and RAG clients.
//
// Wire shapes (GRAPH_API_DOCUMENTATION.md §3):
//   * Node references are already clean: {"id","type","name"} — NOT raw Neo4j
//     {element_id,labels,properties}. So no element_id mapping is needed.
//   * List envelope is {results, limit, offset, next_offset} (offset paging;
//     next_offset is null on the last page) — not {count,next,…}.
//   * The graph is assembled from `entities/neighbors/?id=`, which returns
//     rows {relation, direction, properties, node:{id,type,name}, detail}.
//     Each row is one edge between the queried id and `node`, oriented by
//     `direction` (out|in|both). `entities/?id=` gives a node's full
//     properties + a link-count summary, used to enrich the selected node.
//   * `path/?from=&to=` already returns schema ids
//     ({nodes:[{id,type,name}], relationships:[{type,from,to}]}).
//   * The API returns no per-node URL, so the Reference pane's "associated
//     URL" is derived client-side in associatedUrl() from the id prefix
//     (ioc type, CVE, technique id) and, when present, props.attack_id /
//     props.url — including the IOC-URL safety flag.
//
// Node ids contain ':' and (for url indicators) '/'; they are always passed as
// the `id`/`entity`/`from`/`to` QUERY parameter (never in the path), URL-encoded
// by QUrlQuery, per the doc's guidance.

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QPointer>
#include <QVector>
#include <QList>
#include <QPair>
#include <QUrl>
#include <functional>

#include "../BackendSettings.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QJsonDocument;
QT_END_NAMESPACE

class IntelClient : public QObject {
    Q_OBJECT
public:
    // cti-graph/v1 surface (GRAPH_API_DOCUMENTATION.md §8). If the deployment
    // changes THREATGRAPH SERVICE_ID/VERSION, change kApiRoot alone.
    static constexpr auto kApiRoot        = "/api/cti-graph/v1/";
    static constexpr auto kStats          = "/api/cti-graph/v1/stats/";
    static constexpr auto kSchema         = "/api/cti-graph/v1/schema/";
    static constexpr auto kSearch         = "/api/cti-graph/v1/search/";            // ?q=&type=&limit=
    static constexpr auto kActors         = "/api/cti-graph/v1/actors/";            // ?q=&country=&order= | <name>/
    static constexpr auto kMalware        = "/api/cti-graph/v1/malware/";           // ?q=&type=&platform=&feed= | <name>/
    static constexpr auto kVulnerabilities= "/api/cti-graph/v1/vulnerabilities/";   // ?q=&year= | <CVE>/
    static constexpr auto kIndicators     = "/api/cti-graph/v1/indicators/";        // ?type=&q=&min_reports=
    static constexpr auto kIndicatorLookup= "/api/cti-graph/v1/indicators/lookup/"; // ?value=  | POST {values:[…]}
    static constexpr auto kReports        = "/api/cti-graph/v1/reports/";           // ?q=&year=&source= | <name>/
    static constexpr auto kTechniques     = "/api/cti-graph/v1/techniques/";        // ?q=&tactic= | <T-id>/
    static constexpr auto kNews           = "/api/cti-graph/v1/news/";              // ?since=&entity=&q=
    static constexpr auto kEntities       = "/api/cti-graph/v1/entities/";          // ?id=
    static constexpr auto kNeighbors      = "/api/cti-graph/v1/entities/neighbors/";// ?id=&relation=&type=&direction=
    static constexpr auto kPath           = "/api/cti-graph/v1/path/";              // ?from=&to=&max_hops=&sourced_only=

    explicit IntelClient(BackendSettings *backend, QObject *parent = nullptr);

    // Graph assembly. Both emit graphReceived; the controller replaces on a
    // seed and merges on an expand. "Depth" is reached by expanding neighbours
    // one hop at a time (entities/neighbors/).
    Q_INVOKABLE void fetchNode(const QString &id);                     // GET entities/neighbors/?id=
    // Overview seeds: top nodes of a kind as starting points to expand from.
    // `label` is a node label (ThreatActor|Malware|Vulnerability); mapped to
    // the matching list endpoint. Emits graphReceived (nodes only, no edges).
    Q_INVOKABLE void browseNodes(const QString &label, const QString &q,
                                 int limit = 100, int offset = 0);

    // Full properties + link summary of one node (enriches the selection and
    // its associated URL). Emits detailReceived("entity", {...}).
    Q_INVOKABLE void fetchEntity(const QString &id);                   // GET entities/?id=

    Q_INVOKABLE void search(const QString &q);                         // GET search/?q=
    Q_INVOKABLE void shortestPath(const QString &fromId, const QString &toId,
                                  int maxHops = 4, bool sourcedOnly = false);

    // News section. `entityId` empty -> global recent feed; else articles that
    // mention it. `days` is converted to the API's `since=YYYY-MM-DD` filter.
    Q_INVOKABLE void fetchArticles(const QString &entityId, int days = 30);

    Q_INVOKABLE void fetchStats();                                     // GET stats/
    Q_INVOKABLE void cancelAll();

    // Client-side associated URL: {href, kind, safe, raw}. `safe:false` means
    // `raw` is a live IOC URL to defang and not auto-navigate. Works from a
    // bare {id,type,name} ref (deriving ioc type / CVE / technique id from the
    // id) and uses props.attack_id / props.url when the node is enriched.
    static QVariantMap associatedUrl(const QVariantMap &node);

    // id helpers (exposed for the controller/tests).
    static QString idPrefix(const QString &id);   // "malware:PlugX" -> "malware"
    static QString idValue(const QString &id);    // "malware:PlugX" -> "PlugX"
    static QString typeForPrefix(const QString &prefix);  // "malware" -> "Malware"

signals:
    // Model-shaped nodes/edges. `subjectId` is the node whose neighbourhood
    // this is (empty for an overview/browse result).
    void graphReceived(const QVariantList &nodes, const QVariantList &edges,
                       const QString &subjectId);
    void detailReceived(const QString &context, const QVariantMap &object);
    // Article rows {id,title,url,published,source,summary,mentions:[…]} + the
    // envelope's next_offset (empty when last page).
    void newsReceived(const QVariantList &articles, const QString &nextOffset);
    void searchResults(const QVariantList &rows);           // [{id,type,name}]
    void statsReceived(const QVariantMap &stats);
    void pathReceived(const QVariantList &nodes, const QVariantList &edges);
    // body `error` string + HTTP status (400/401/403/404/502).
    void errorOccurred(const QString &context, const QString &detail, int httpStatus);

private:
    // A {id,type,name} ref (+ optional extra props) -> model node
    // {id,type,name,props,url}. `type` falls back to the id prefix.
    static QVariantMap toModelNode(const QVariantMap &ref);

    QUrl url(const QString &path, const QList<QPair<QString, QString>> &query = {}) const;
    QNetworkReply *get(const QUrl &url, const QString &context);
    void handle(QNetworkReply *reply, const QString &context,
                const std::function<void(const QJsonDocument &)> &ok);

    BackendSettings *m_backend = nullptr;     // shared, not owned
    QNetworkAccessManager *m_nam = nullptr;
    QVector<QPointer<QNetworkReply>> m_inflight;
};
