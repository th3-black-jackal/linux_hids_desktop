#include "ThreatIntelController.h"

#include "../AppController.h"              // to borrow the shared BackendSettings
#include "threat_intel/IntelClient.h"
#include "BackendSettings.h"

// ThreatIntelController — see header. Wires IntelClient -> the two models and
// the current selection, and turns a node selection into the Reference pane's
// URL and the News feed's filter.

ThreatIntelController::ThreatIntelController(QObject *appController, QObject *parent)
    : QObject(parent) {
    if (auto *app = qobject_cast<AppController *>(appController))
        m_backend = app->backend();

    m_client = new IntelClient(m_backend, this);

    connect(m_client, &IntelClient::graphReceived, this,
            [this](const QVariantList &nodes, const QVariantList &edges, const QString &subjectId) {
        if (m_seeding) { m_graph.setGraph(nodes, edges); m_seeding = false; }
        else           { m_graph.mergeGraph(nodes, edges); }
        setPhase(QStringLiteral("ready"));
        // After a seed, select the subject so the other two panes populate.
        if (!subjectId.isEmpty() && m_selected.value(QStringLiteral("id")).toString().isEmpty())
            select(subjectId);
    });

    connect(m_client, &IntelClient::pathReceived, this,
            [this](const QVariantList &nodes, const QVariantList &edges) {
        m_graph.mergeGraph(nodes, edges);
        setPhase(QStringLiteral("ready"));
    });

    connect(m_client, &IntelClient::newsReceived, this,
            [this](const QVariantList &articles, const QString &nextOffset) {
        // Offset-paged envelope; `total` isn't returned, so expose the count we
        // have. (News feed replaces on each entity/window change — no append.)
        m_news.setArticles(articles, nextOffset, articles.size(), /*append=*/false);
    });

    connect(m_client, &IntelClient::searchResults, this,
            [this](const QVariantList &rows) { m_search = rows; emit searchChanged(); });

    connect(m_client, &IntelClient::detailReceived, this,
            [this](const QString &ctx, const QVariantMap &obj) {
        // entities/?id= enrichment of the selected node: {entity:{…}, links:[…]}.
        // Fold the full properties onto the selection and recompute the
        // reference URL, so malware/actor (attack_id) and article (url) links
        // resolve even though the graph node ref was bare {id,type,name}.
        if (ctx != QLatin1String("entity") || m_selected.isEmpty()) return;
        const QVariantMap entity = obj.value(QStringLiteral("entity")).toMap();
        if (entity.value(QStringLiteral("id")).toString()
                != m_selected.value(QStringLiteral("id")).toString())
            return;   // a later selection superseded this reply

        QVariantMap enriched = m_selected;
        QVariantMap props = enriched.value(QStringLiteral("props")).toMap();
        for (auto it = entity.constBegin(); it != entity.constEnd(); ++it)
            if (it.key() != QStringLiteral("id") && it.key() != QStringLiteral("type")
                && it.key() != QStringLiteral("name"))
                props.insert(it.key(), it.value());
        enriched[QStringLiteral("props")] = props;
        enriched.insert(QStringLiteral("links"), obj.value(QStringLiteral("links")));
        m_selected = enriched;

        const QVariantMap u = IntelClient::associatedUrl(enriched);
        const bool safe = u.value(QStringLiteral("safe")).toBool();
        const QString href = u.value(QStringLiteral("href")).toString();
        if (safe && !href.isEmpty()) { m_refSafe = true; m_refUrl = href; m_refRaw.clear(); }
        emit selectionChanged();
    });

    connect(m_client, &IntelClient::errorOccurred, this,
            [this](const QString &ctx, const QString &detail, int status) {
        setPhase(QStringLiteral("error"),
                 QStringLiteral("%1: %2%3").arg(ctx, detail,
                     status ? QStringLiteral(" (HTTP %1)").arg(status) : QString()));
    });
}

bool ThreatIntelController::backendConfigured() const {
    return m_backend && m_backend->configured();
}

void ThreatIntelController::loadOverview(const QVariantMap &filters) {
    if (!backendConfigured()) { setPhase(QStringLiteral("error"), QStringLiteral("backend not configured")); return; }
    m_seeding = true;
    setPhase(QStringLiteral("loading"));
    m_client->fetchStats();
    const QString label = filters.value(QStringLiteral("label"), QStringLiteral("Malware")).toString();
    const QString q     = filters.value(QStringLiteral("q")).toString();
    const int limit     = filters.value(QStringLiteral("limit"), 48).toInt();
    m_client->browseNodes(label, q, limit, 0);
}

void ThreatIntelController::seedFrom(const QString &nodeId) {
    if (nodeId.isEmpty()) return;
    m_seeding = true;
    m_selected.clear();
    setPhase(QStringLiteral("loading"));
    m_graph.clear();
    m_client->fetchNode(nodeId);
}

void ThreatIntelController::expand(const QString &nodeId) {
    if (nodeId.isEmpty()) return;
    m_seeding = false;
    setPhase(QStringLiteral("loading"));
    m_client->fetchNode(nodeId);
}

void ThreatIntelController::select(const QString &nodeId) {
    const int row = m_graph.indexOfId(nodeId);
    if (row < 0) return;
    m_selected = m_graph.get(row);
    m_graph.setSelectedId(nodeId);

    const bool safe = m_selected.value(QStringLiteral("urlSafe")).toBool();
    const QString href = m_selected.value(QStringLiteral("urlHref")).toString();
    const QString raw  = m_selected.value(QStringLiteral("urlRaw")).toString();
    if (safe && !href.isEmpty()) {
        m_refSafe = true;  m_refUrl = href; m_refRaw.clear();
    } else {
        m_refSafe = false; m_refUrl.clear(); m_refRaw = raw;   // raw is the live IOC URL
    }
    emit selectionChanged();

    // Enrich the bare {id,type,name} node with full properties (attack_id, url,
    // aliases, link summary) so the reference URL and detail panel fill in.
    m_client->fetchEntity(nodeId);

    // News for this entity in the current window.
    m_client->fetchArticles(nodeId, m_newsDays);
}

void ThreatIntelController::search(const QString &q) {
    m_client->search(q);
}

void ThreatIntelController::setNewsDays(int days) {
    days = qBound(1, days, 365);
    if (days == m_newsDays) return;
    m_newsDays = days;
    emit newsDaysChanged();
    const QString sel = m_selected.value(QStringLiteral("id")).toString();
    m_client->fetchArticles(sel, m_newsDays);   // empty sel -> global recent feed
}

void ThreatIntelController::showUrl(const QString &url) {
    if (url.isEmpty()) return;
    m_refSafe = true;        // article/reference links are safe to display
    m_refUrl = url;
    m_refRaw.clear();
    emit selectionChanged();
}

void ThreatIntelController::openRawReference() {
    if (m_refRaw.isEmpty()) return;
    m_refUrl = m_refRaw;   // the real IOC URL; operator explicitly opted in
    m_refSafe = true;
    emit selectionChanged();
}

void ThreatIntelController::refresh() {
    const QString sel = m_selected.value(QStringLiteral("id")).toString();
    if (!sel.isEmpty()) seedFrom(sel);
    else loadOverview();
}

void ThreatIntelController::setPhase(const QString &p, const QString &err) {
    m_lastError = err;
    if (m_phase != p) { m_phase = p; }
    emit phaseChanged();
}

QString ThreatIntelController::defang(const QString &url) {
    if (url.isEmpty()) return url;
    QString s = url;
    s.replace(QStringLiteral("http://"),  QStringLiteral("hxxp://"));
    s.replace(QStringLiteral("https://"), QStringLiteral("hxxps://"));
    s.replace(QLatin1Char('.'), QStringLiteral("[.]"));
    return s;
}
