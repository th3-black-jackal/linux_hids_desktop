#include "IntelClient.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

// IntelClient — see IntelClient.h. HTTP + adaptation of the cti-graph/v1 wire
// shapes to the view models. Node refs are already {id,type,name}, so this is
// mostly plumbing; the one piece of real logic is associatedUrl().

IntelClient::IntelClient(BackendSettings *backend, QObject *parent)
    : QObject(parent), m_backend(backend), m_nam(new QNetworkAccessManager(this)) {}

// ── id helpers ─────────────────────────────────────────────────────────────

QString IntelClient::idPrefix(const QString &id) {
    const int c = id.indexOf(QLatin1Char(':'));
    return c < 0 ? QString() : id.left(c);
}

QString IntelClient::idValue(const QString &id) {
    const int c = id.indexOf(QLatin1Char(':'));
    return c < 0 ? id : id.mid(c + 1);
}

QString IntelClient::typeForPrefix(const QString &p) {
    if (p == QLatin1String("actor"))       return QStringLiteral("ThreatActor");
    if (p == QLatin1String("malware"))      return QStringLiteral("Malware");
    if (p == QLatin1String("cve"))          return QStringLiteral("Vulnerability");
    if (p == QLatin1String("technique"))    return QStringLiteral("Technique");
    if (p == QLatin1String("malwaretype"))  return QStringLiteral("MalwareType");
    if (p == QLatin1String("report"))       return QStringLiteral("Report");
    if (p == QLatin1String("article"))      return QStringLiteral("Article");
    // Indicator id prefixes double as the ioc_type.
    if (p == QLatin1String("ipv4") || p == QLatin1String("domain") ||
        p == QLatin1String("url")  || p == QLatin1String("email")  ||
        p == QLatin1String("md5")  || p == QLatin1String("sha1")   ||
        p == QLatin1String("sha256"))
        return QStringLiteral("Indicator");
    return QString();
}

// ── URL + request plumbing ───────────────────────────────────────────────

QUrl IntelClient::url(const QString &path, const QList<QPair<QString, QString>> &query) const {
    QUrl u = m_backend ? m_backend->url(path) : QUrl(path);
    if (!query.isEmpty()) {
        QUrlQuery q;   // QUrlQuery percent-encodes ids (':', '/') correctly
        for (const auto &kv : query)
            if (!kv.second.isEmpty()) q.addQueryItem(kv.first, kv.second);
        u.setQuery(q);
    }
    return u;
}

QNetworkReply *IntelClient::get(const QUrl &u, const QString &context) {
    if (!m_backend) { emit errorOccurred(context, QStringLiteral("backend not configured"), 0); return nullptr; }
    QNetworkRequest req = m_backend->request(u);
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->get(req);
    m_inflight.append(reply);
    return reply;
}

void IntelClient::handle(QNetworkReply *reply, const QString &context,
                         const std::function<void(const QJsonDocument &)> &ok) {
    if (!reply) return;
    connect(reply, &QNetworkReply::finished, this, [this, reply, context, ok]() {
        m_inflight.removeAll(reply);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            // Error body is {"error": "..."} across this API (doc §2).
            QString detail = QJsonDocument::fromJson(body).object()
                                 .value(QStringLiteral("error")).toString();
            if (detail.isEmpty()) detail = reply->errorString();
            emit errorOccurred(context, detail, status);
            return;
        }
        ok(QJsonDocument::fromJson(body));
    });
}

// ── Shape adaptation ──────────────────────────────────────────────────────

QVariantMap IntelClient::toModelNode(const QVariantMap &ref) {
    QVariantMap n;
    const QString id = ref.value(QStringLiteral("id")).toString();
    n[QStringLiteral("id")]   = id;
    n[QStringLiteral("name")] = ref.contains(QStringLiteral("name"))
                                    ? ref.value(QStringLiteral("name"))
                                    : idValue(id);
    QString type = ref.value(QStringLiteral("type")).toString();
    if (type.isEmpty()) type = typeForPrefix(idPrefix(id));
    n[QStringLiteral("type")] = type;
    // Carry any extra fields the list/profile endpoints included (attack_id,
    // url, aliases, reports…) so associatedUrl and the detail panel can use them.
    QVariantMap props = ref;
    props.remove(QStringLiteral("id"));
    props.remove(QStringLiteral("type"));
    props.remove(QStringLiteral("name"));
    n[QStringLiteral("props")] = props;
    n[QStringLiteral("url")]   = associatedUrl(n);
    return n;
}

// ── Endpoints ─────────────────────────────────────────────────────────────

void IntelClient::fetchNode(const QString &id) {
    // entities/neighbors/?id=  → rows {relation, direction, properties, node, detail}.
    const QUrl u = url(QString::fromLatin1(kNeighbors), {
        {QStringLiteral("id"), id},
        {QStringLiteral("direction"), QStringLiteral("both")},
        {QStringLiteral("limit"), QStringLiteral("200")},
    });
    handle(get(u, QStringLiteral("neighbors")), QStringLiteral("neighbors"),
           [this, id](const QJsonDocument &doc) {
        const QJsonObject o = doc.object();
        QVariantList nodes, edges;

        // The queried node itself (synthesised from its id; enriched later on
        // selection via fetchEntity).
        QVariantMap center;
        center[QStringLiteral("id")] = id;
        nodes.append(toModelNode(center));

        for (const auto &v : o.value(QStringLiteral("results")).toArray()) {
            const QVariantMap row = v.toObject().toVariantMap();
            const QVariantMap ref = row.value(QStringLiteral("node")).toMap();
            if (ref.value(QStringLiteral("id")).toString().isEmpty()) continue;
            nodes.append(toModelNode(ref));

            const QString dir = row.value(QStringLiteral("direction")).toString();
            QVariantMap e;
            // "in" means neighbour -> center; otherwise center -> neighbour.
            if (dir == QLatin1String("in")) {
                e[QStringLiteral("source")] = ref.value(QStringLiteral("id"));
                e[QStringLiteral("target")] = id;
            } else {
                e[QStringLiteral("source")] = id;
                e[QStringLiteral("target")] = ref.value(QStringLiteral("id"));
            }
            e[QStringLiteral("type")]  = row.value(QStringLiteral("relation"));
            e[QStringLiteral("props")] = row.value(QStringLiteral("properties"));
            edges.append(e);
        }
        emit graphReceived(nodes, edges, id);
    });
}

void IntelClient::browseNodes(const QString &label, const QString &q, int limit, int offset) {
    // Map a node label to its list endpoint; overview seeds, nodes only.
    QString path = QString::fromLatin1(kMalware);
    if (label == QLatin1String("ThreatActor"))        path = QString::fromLatin1(kActors);
    else if (label == QLatin1String("Vulnerability")) path = QString::fromLatin1(kVulnerabilities);
    else if (label == QLatin1String("Technique"))     path = QString::fromLatin1(kTechniques);

    const QUrl u = url(path, {
        {QStringLiteral("q"), q},
        {QStringLiteral("limit"), QString::number(limit)},
        {QStringLiteral("offset"), QString::number(offset)},
    });
    handle(get(u, QStringLiteral("browse")), QStringLiteral("browse"),
           [this, label](const QJsonDocument &doc) {
        QVariantList nodes;
        for (const auto &v : doc.object().value(QStringLiteral("results")).toArray()) {
            QVariantMap row = v.toObject().toVariantMap();
            // List rows carry `name` but not always `type`/`id`; fill them in.
            if (!row.contains(QStringLiteral("type"))) row[QStringLiteral("type")] = label;
            if (!row.contains(QStringLiteral("id")) && row.contains(QStringLiteral("name"))) {
                // Build the id from the label's prefix + name (matches SCHEMA.md).
                const QString pfx = label == QLatin1String("ThreatActor") ? QStringLiteral("actor")
                                  : label == QLatin1String("Vulnerability") ? QStringLiteral("cve")
                                  : label == QLatin1String("Technique") ? QStringLiteral("technique")
                                  : QStringLiteral("malware");
                row[QStringLiteral("id")] = pfx + QLatin1Char(':') + row.value(QStringLiteral("name")).toString();
            }
            nodes.append(toModelNode(row));
        }
        emit graphReceived(nodes, {}, QString());   // nodes only; expand for edges
    });
}

void IntelClient::fetchEntity(const QString &id) {
    const QUrl u = url(QString::fromLatin1(kEntities), {{QStringLiteral("id"), id}});
    handle(get(u, QStringLiteral("entity")), QStringLiteral("entity"),
           [this](const QJsonDocument &doc) {
        emit detailReceived(QStringLiteral("entity"), doc.object().toVariantMap());
    });
}

void IntelClient::search(const QString &q) {
    if (q.trimmed().length() < 2) { emit searchResults({}); return; }
    const QUrl u = url(QString::fromLatin1(kSearch), {{QStringLiteral("q"), q}});
    handle(get(u, QStringLiteral("search")), QStringLiteral("search"),
           [this](const QJsonDocument &doc) {
        QVariantList rows;
        for (const auto &v : doc.object().value(QStringLiteral("results")).toArray()) {
            const QVariantMap r = v.toObject().toVariantMap();
            QVariantMap row;
            row[QStringLiteral("id")]   = r.value(QStringLiteral("id"));
            row[QStringLiteral("type")] = r.value(QStringLiteral("type"));
            row[QStringLiteral("name")] = r.value(QStringLiteral("name"));
            rows.append(row);
        }
        emit searchResults(rows);
    });
}

void IntelClient::shortestPath(const QString &fromId, const QString &toId,
                               int maxHops, bool sourcedOnly) {
    const QUrl u = url(QString::fromLatin1(kPath), {
        {QStringLiteral("from"), fromId},
        {QStringLiteral("to"), toId},
        {QStringLiteral("max_hops"), QString::number(qBound(1, maxHops, 6))},
        {QStringLiteral("sourced_only"), sourcedOnly ? QStringLiteral("true") : QString()},
    });
    handle(get(u, QStringLiteral("path")), QStringLiteral("path"),
           [this](const QJsonDocument &doc) {
        const QJsonObject o = doc.object();
        QVariantList nodes, edges;
        for (const auto &v : o.value(QStringLiteral("nodes")).toArray())
            nodes.append(toModelNode(v.toObject().toVariantMap()));
        for (const auto &v : o.value(QStringLiteral("relationships")).toArray()) {
            const QVariantMap r = v.toObject().toVariantMap();
            QVariantMap e;
            e[QStringLiteral("source")] = r.value(QStringLiteral("from"));  // already schema ids
            e[QStringLiteral("target")] = r.value(QStringLiteral("to"));
            e[QStringLiteral("type")]   = r.value(QStringLiteral("type"));
            edges.append(e);
        }
        emit pathReceived(nodes, edges);
    });
}

void IntelClient::fetchArticles(const QString &entityId, int days) {
    QList<QPair<QString, QString>> q;
    if (!entityId.isEmpty()) q.append({QStringLiteral("entity"), entityId});
    if (days > 0)
        q.append({QStringLiteral("since"),
                  QDate::currentDate().addDays(-days).toString(Qt::ISODate)});
    handle(get(url(QString::fromLatin1(kNews), q), QStringLiteral("news")),
           QStringLiteral("news"), [this](const QJsonDocument &doc) {
        const QJsonObject env = doc.object();
        QVariantList articles;
        for (const auto &v : env.value(QStringLiteral("results")).toArray()) {
            const QVariantMap o = v.toObject().toVariantMap();
            QVariantMap a;
            a[QStringLiteral("id")]        = o.value(QStringLiteral("id"));
            a[QStringLiteral("title")]     = o.value(QStringLiteral("title"));
            a[QStringLiteral("url")]       = o.value(QStringLiteral("url"));
            a[QStringLiteral("published")] = o.value(QStringLiteral("published"));
            a[QStringLiteral("source")]    = o.value(QStringLiteral("source"));
            a[QStringLiteral("summary")]   = o.value(QStringLiteral("description"));
            a[QStringLiteral("mentions")]  = o.value(QStringLiteral("mentions"));
            articles.append(a);
        }
        const QJsonValue nx = env.value(QStringLiteral("next_offset"));
        emit newsReceived(articles, nx.isNull() ? QString() : QString::number(nx.toInt()));
    });
}

void IntelClient::fetchStats() {
    handle(get(url(QString::fromLatin1(kStats)), QStringLiteral("stats")),
           QStringLiteral("stats"), [this](const QJsonDocument &doc) {
        emit statsReceived(doc.object().toVariantMap());
    });
}

void IntelClient::cancelAll() {
    for (const auto &r : m_inflight)
        if (r) r->abort();
    m_inflight.clear();
}

// ── Associated URL (client-side) ───────────────────────────────────────────

QVariantMap IntelClient::associatedUrl(const QVariantMap &node) {
    const QString id   = node.value(QStringLiteral("id")).toString();
    const QString type = node.value(QStringLiteral("type")).toString();
    const QVariantMap p = node.value(QStringLiteral("props")).toMap();
    const QString name = node.value(QStringLiteral("name")).toString();
    const QString prefix = idPrefix(id);

    auto make = [](const QString &href, const QString &kind, bool safe,
                   const QString &raw = QString()) {
        QVariantMap u;
        u[QStringLiteral("href")] = href;
        u[QStringLiteral("kind")] = kind;
        u[QStringLiteral("safe")] = safe;
        u[QStringLiteral("raw")]  = raw;
        return u;
    };

    const QString attack = p.value(QStringLiteral("attack_id")).toString();

    if (type == QLatin1String("Article"))
        return make(p.value(QStringLiteral("url")).toString(), QStringLiteral("article"), true);

    if (type == QLatin1String("ThreatActor"))
        return attack.isEmpty()
            ? make(QString(), QStringLiteral("reference"), true)
            : make(QStringLiteral("https://attack.mitre.org/groups/") + attack,
                   QStringLiteral("reference"), true);

    if (type == QLatin1String("Malware")) {
        if (!attack.isEmpty())
            return make(QStringLiteral("https://attack.mitre.org/software/") + attack,
                        QStringLiteral("reference"), true);
        const QString pu = p.value(QStringLiteral("url")).toString();
        return make(pu, QStringLiteral("reference"), true);   // may be empty until enriched
    }

    if (type == QLatin1String("Technique")) {
        // attack_id is the id value itself (technique:T1059.001).
        QString tid = attack.isEmpty() ? idValue(id) : attack;
        tid.replace(QLatin1Char('.'), QLatin1Char('/'));      // T1059.001 -> T1059/001
        return make(QStringLiteral("https://attack.mitre.org/techniques/") + tid,
                    QStringLiteral("reference"), true);
    }

    if (type == QLatin1String("Vulnerability")) {
        const QString cve = name.isEmpty() ? idValue(id) : name;
        return make(QStringLiteral("https://nvd.nist.gov/vuln/detail/") + cve,
                    QStringLiteral("reference"), true);
    }

    if (type == QLatin1String("Indicator")) {
        // ioc type is the id prefix; value is the id value (or name).
        const QString ioc = prefix;
        const QString val = name.isEmpty() ? idValue(id) : name;
        if (ioc == QLatin1String("md5") || ioc == QLatin1String("sha1") || ioc == QLatin1String("sha256"))
            return make(QStringLiteral("https://www.virustotal.com/gui/file/") + val,
                        QStringLiteral("reputation"), true);
        if (ioc == QLatin1String("ipv4"))
            return make(QStringLiteral("https://www.virustotal.com/gui/ip-address/") + val,
                        QStringLiteral("reputation"), true);
        if (ioc == QLatin1String("domain"))
            return make(QStringLiteral("https://www.virustotal.com/gui/domain/") + val,
                        QStringLiteral("reputation"), true);
        if (ioc == QLatin1String("url"))
            return make(QString(), QStringLiteral("reputation"), false, val);  // live IOC: gated
        return make(QString(), QStringLiteral("reputation"), false, val);      // email, etc.
    }

    // Report, MalwareType, Date, Description: no URL.
    return make(QString(), QString(), true);
}
