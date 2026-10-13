#include "ThreatGraphModel.h"

#include <QRandomGenerator>
#include <cmath>

// ThreatGraphModel — see header. Holds the current subgraph and runs a small
// Fruchterman–Reingold-style layout in step(); the QML Canvas/Repeater read
// posX/posY and edgeLines() each frame while settling() is true.

ThreatGraphModel::ThreatGraphModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> ThreatGraphModel::roleNames() const {
    // NOTE: "nodeId"/"posX"/"posY" — never "id"/"x"/"y" (QML reserves those).
    return {
        {IdRole, "nodeId"}, {TypeRole, "type"}, {NameRole, "name"},
        {XRole, "posX"}, {YRole, "posY"}, {DegreeRole, "degree"}, {SelectedRole, "selected"},
        {UrlHrefRole, "urlHref"}, {UrlKindRole, "urlKind"}, {UrlSafeRole, "urlSafe"},
        {UrlRawRole, "urlRaw"}, {PropsRole, "props"}
    };
}

QVariant ThreatGraphModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_nodes.size()) return {};
    const Node &n = m_nodes[index.row()];
    switch (role) {
    case IdRole:       return n.id;
    case TypeRole:     return n.type;
    case NameRole:     return n.name;
    case XRole:        return n.pos.x();
    case YRole:        return n.pos.y();
    case DegreeRole:   return n.degree;
    case SelectedRole: return n.id == m_selectedId;
    case UrlHrefRole:  return n.urlHref;
    case UrlKindRole:  return n.urlKind;
    case UrlSafeRole:  return n.urlSafe;
    case UrlRawRole:   return n.urlRaw;
    case PropsRole:    return n.props;
    default:           return {};
    }
}

ThreatGraphModel::Node ThreatGraphModel::parseNode(const QVariantMap &m) {
    Node n;
    n.id    = m.value(QStringLiteral("id")).toString();
    n.type  = m.value(QStringLiteral("type")).toString();
    n.name  = m.value(QStringLiteral("name")).toString();
    n.props = m.value(QStringLiteral("props")).toMap();
    const QVariantMap u = m.value(QStringLiteral("url")).toMap();
    n.urlHref = u.value(QStringLiteral("href")).toString();
    n.urlKind = u.value(QStringLiteral("kind")).toString();
    n.urlSafe = u.value(QStringLiteral("safe")).toBool();
    n.urlRaw  = u.value(QStringLiteral("raw")).toString();
    return n;
}

void ThreatGraphModel::setGraph(const QVariantList &nodes, const QVariantList &edges) {
    beginResetModel();
    m_nodes.clear();
    m_edges.clear();
    m_index.clear();
    for (const auto &v : nodes) {
        Node n = parseNode(v.toMap());
        if (n.id.isEmpty() || m_index.contains(n.id)) continue;
        m_index.insert(n.id, m_nodes.size());
        m_nodes.append(n);
    }
    for (const auto &v : edges) {
        const QVariantMap e = v.toMap();
        Edge ed;
        ed.source = e.value(QStringLiteral("source")).toString();
        ed.target = e.value(QStringLiteral("target")).toString();
        ed.type   = e.value(QStringLiteral("type")).toString();
        ed.props  = e.value(QStringLiteral("props")).toMap();
        ed.cooccurrence = ed.type == QLatin1String("CO_OCCURS_WITH");
        m_edges.append(ed);
    }
    recomputeDegrees();
    endResetModel();
    emit countChanged();
    // An edgeless set (e.g. the overview's top-N nodes) has nothing for the
    // physics sim to arrange, so lay it out as a static grid and do not animate.
    // A connected set gets seeded positions and a cooled physics pass.
    if (m_edges.isEmpty()) {
        gridLayout();
        setSettling(false);
    } else {
        seedPositions(0);
        reheat();
    }
    if (!m_nodes.isEmpty())
        emit dataChanged(index(0), index(m_nodes.size() - 1), {XRole, YRole});
}

void ThreatGraphModel::mergeGraph(const QVariantList &nodes, const QVariantList &edges) {
    const int firstNew = m_nodes.size();
    QVector<Node> added;
    for (const auto &v : nodes) {
        Node n = parseNode(v.toMap());
        if (n.id.isEmpty() || m_index.contains(n.id)) continue;
        m_index.insert(n.id, m_nodes.size() + added.size());
        added.append(n);
    }
    if (!added.isEmpty()) {
        beginInsertRows({}, firstNew, firstNew + added.size() - 1);
        m_nodes += added;
        endInsertRows();
    }
    for (const auto &v : edges) {
        const QVariantMap e = v.toMap();
        Edge ed;
        ed.source = e.value(QStringLiteral("source")).toString();
        ed.target = e.value(QStringLiteral("target")).toString();
        ed.type   = e.value(QStringLiteral("type")).toString();
        ed.props  = e.value(QStringLiteral("props")).toMap();
        ed.cooccurrence = ed.type == QLatin1String("CO_OCCURS_WITH");
        bool dup = false;
        for (const Edge &x : m_edges)
            if (x.source == ed.source && x.target == ed.target && x.type == ed.type) { dup = true; break; }
        if (!dup) m_edges.append(ed);
    }
    recomputeDegrees();
    emit countChanged();
    if (m_edges.isEmpty()) {
        gridLayout();
        setSettling(false);
        emit dataChanged(index(0), index(m_nodes.size() - 1), {XRole, YRole, DegreeRole});
    } else {
        seedPositions(firstNew);
        emit dataChanged(index(0), index(m_nodes.size() - 1), {DegreeRole});
        reheat();
    }
}

void ThreatGraphModel::clear() {
    beginResetModel();
    m_nodes.clear(); m_edges.clear(); m_index.clear(); m_selectedId.clear();
    endResetModel();
    emit countChanged();
    setSettling(false);
}

void ThreatGraphModel::recomputeDegrees() {
    for (Node &n : m_nodes) n.degree = 0;
    for (const Edge &e : m_edges) {
        auto s = m_index.constFind(e.source), t = m_index.constFind(e.target);
        if (s != m_index.constEnd()) ++m_nodes[*s].degree;
        if (t != m_index.constEnd()) ++m_nodes[*t].degree;
    }
}

void ThreatGraphModel::seedPositions(int firstNewRow) {
    auto *rng = QRandomGenerator::global();
    const QPointF c(m_w / 2.0, m_h / 2.0);
    for (int i = firstNewRow; i < m_nodes.size(); ++i) {
        Node &n = m_nodes[i];
        if (n.pinned) continue;
        // Place a newcomer near the centroid of its already-placed neighbours,
        // so an expand grows outward from where it was triggered; otherwise ring.
        QPointF acc; int cnt = 0;
        for (const Edge &e : m_edges) {
            QString other;
            if (e.source == n.id) other = e.target;
            else if (e.target == n.id) other = e.source;
            else continue;
            auto it = m_index.constFind(other);
            if (it != m_index.constEnd() && *it < i) { acc += m_nodes[*it].pos; ++cnt; }
        }
        if (i == 0 && firstNewRow == 0) {
            n.pos = c;   // first seed at centre
        } else if (cnt > 0) {
            n.pos = acc / cnt + QPointF(rng->bounded(40) - 20, rng->bounded(40) - 20);
        } else {
            constexpr double kPi = 3.14159265358979323846;
            const double ang = rng->bounded(10000) / 10000.0 * 2 * kPi;
            const double rad = qMin(m_w, m_h) * 0.3;
            n.pos = c + QPointF(std::cos(ang) * rad, std::sin(ang) * rad);
        }
        n.vel = QPointF();
    }
}

void ThreatGraphModel::gridLayout() {
    // Evenly tile unpinned nodes across the viewport. Used for an edgeless set,
    // where a force sim would just shove everything into the walls.
    const int n = m_nodes.size();
    if (n == 0) return;
    const int cols = qMax(1, int(std::ceil(std::sqrt(double(n) * m_w / qMax(1.0, m_h)))));
    const int rows = qMax(1, (n + cols - 1) / cols);
    const double cw = m_w / (cols + 1);
    const double ch = m_h / (rows + 1);
    int i = 0;
    for (Node &nd : m_nodes) {
        if (!nd.pinned) {
            const int r = i / cols, c = i % cols;
            nd.pos = QPointF(cw * (c + 1), ch * (r + 1));
            nd.vel = QPointF();
        }
        ++i;
    }
}

void ThreatGraphModel::setViewport(double w, double h) {
    if (w <= 0 || h <= 0) return;
    const bool firstReal = (m_w <= 0 || m_h <= 0);
    const double ratio = (m_w > 0) ? w / m_w : 1.0;
    m_w = w; m_h = h;
    // Re-flow to the new size: regrid an edgeless set; for a laid-out graph,
    // scale existing positions so nodes don't pile against the old bounds.
    if (m_nodes.isEmpty()) return;
    if (m_edges.isEmpty()) {
        gridLayout();
        if (!m_nodes.isEmpty()) emit dataChanged(index(0), index(m_nodes.size() - 1), {XRole, YRole});
    } else if (!firstReal && qAbs(ratio - 1.0) > 0.01) {
        for (Node &nd : m_nodes) nd.pos *= ratio;
        if (!m_nodes.isEmpty()) emit dataChanged(index(0), index(m_nodes.size() - 1), {XRole, YRole});
    }
}

void ThreatGraphModel::step(int iterations) {
    const int n = m_nodes.size();
    // Nothing to simulate for an empty or edgeless graph (grid handles those).
    if (n == 0 || m_edges.isEmpty()) { setSettling(false); return; }

    const double area = m_w * m_h;
    const double k = 0.75 * std::sqrt(area / double(n));   // ideal separation
    const double margin = 24.0;
    double maxMove = 0.0;

    for (int iter = 0; iter < iterations; ++iter) {
        ++m_cool;
        // Temperature cools 1 -> 0 over m_maxCool iterations; caps the per-step
        // move so the layout settles instead of bouncing off the walls forever.
        const double t = qMax(0.0, double(m_maxCool - m_cool) / double(m_maxCool));
        const double maxStep = 2.0 + 22.0 * t;
        QVector<QPointF> disp(n, QPointF(0, 0));

        // Repulsion (all pairs — n <= ~150, so O(n^2) is fine).
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                QPointF d = m_nodes[i].pos - m_nodes[j].pos;
                double dist = std::hypot(d.x(), d.y());
                if (dist < 0.01) { d = QPointF(0.1, 0.1); dist = 0.14; }
                const double f = (k * k) / dist;
                const QPointF u = d / dist * f;
                disp[i] += u; disp[j] -= u;
            }
        }
        // Attraction along edges.
        for (const Edge &e : m_edges) {
            auto s = m_index.constFind(e.source), t = m_index.constFind(e.target);
            if (s == m_index.constEnd() || t == m_index.constEnd()) continue;
            QPointF d = m_nodes[*s].pos - m_nodes[*t].pos;
            double dist = std::hypot(d.x(), d.y());
            if (dist < 0.01) continue;
            const double f = (dist * dist) / k;
            const QPointF u = d / dist * f;
            disp[*s] -= u; disp[*t] += u;
        }
        // Gentle pull to centre so disconnected nodes don't drift off-screen.
        const QPointF c(m_w / 2.0, m_h / 2.0);
        for (int i = 0; i < n; ++i)
            disp[i] += (c - m_nodes[i].pos) * 0.01;

        // Integrate with velocity damping + the cooling per-step clamp.
        for (int i = 0; i < n; ++i) {
            Node &nd = m_nodes[i];
            if (nd.pinned) { nd.vel = QPointF(); continue; }
            nd.vel = (nd.vel + disp[i]) * 0.55;
            double vlen = std::hypot(nd.vel.x(), nd.vel.y());
            if (vlen > maxStep) nd.vel *= maxStep / vlen;
            nd.pos += nd.vel;
            nd.pos.setX(qBound(margin, nd.pos.x(), m_w - margin));
            nd.pos.setY(qBound(margin, nd.pos.y(), m_h - margin));
            maxMove = qMax(maxMove, std::hypot(nd.vel.x(), nd.vel.y()));
        }
        if (m_cool >= m_maxCool) break;   // hard cap reached mid-batch
    }

    if (!m_nodes.isEmpty())
        emit dataChanged(index(0), index(n - 1), {XRole, YRole});
    // Stop when it has come to rest or the cooling budget is spent.
    if (maxMove < 0.5 || m_cool >= m_maxCool) setSettling(false);
}

void ThreatGraphModel::pin(const QString &id, double x, double y) {
    auto it = m_index.constFind(id);
    if (it == m_index.constEnd()) return;
    Node &n = m_nodes[*it];
    n.pos = QPointF(qBound(0.0, x, m_w), qBound(0.0, y, m_h));
    n.vel = QPointF();
    n.pinned = true;
    emit dataChanged(index(*it), index(*it), {XRole, YRole});
    reheat();
}

void ThreatGraphModel::release(const QString &id) {
    auto it = m_index.constFind(id);
    if (it != m_index.constEnd()) m_nodes[*it].pinned = false;
}

void ThreatGraphModel::reheat() {
    // Restart the cooling clock and run the sim — but only when there are edges
    // to arrange; an edgeless set stays in its static grid.
    m_cool = 0;
    if (m_edges.isEmpty()) { setSettling(false); return; }
    setSettling(true);
}

void ThreatGraphModel::setSettling(bool s) {
    if (m_settling == s) return;
    m_settling = s;
    emit settlingChanged();
}

QVariantList ThreatGraphModel::edgeLines() const {
    QVariantList out;
    out.reserve(m_edges.size());
    for (const Edge &e : m_edges) {
        auto s = m_index.constFind(e.source), t = m_index.constFind(e.target);
        if (s == m_index.constEnd() || t == m_index.constEnd()) continue;
        QVariantMap m;
        m[QStringLiteral("x1")] = m_nodes[*s].pos.x();
        m[QStringLiteral("y1")] = m_nodes[*s].pos.y();
        m[QStringLiteral("x2")] = m_nodes[*t].pos.x();
        m[QStringLiteral("y2")] = m_nodes[*t].pos.y();
        m[QStringLiteral("cooccurrence")] = e.cooccurrence;
        m[QStringLiteral("type")] = e.type;
        out.append(m);
    }
    return out;
}

QVariantMap ThreatGraphModel::get(int row) const {
    if (row < 0 || row >= m_nodes.size()) return {};
    const Node &n = m_nodes[row];
    QVariantMap m;
    m[QStringLiteral("id")]   = n.id;
    m[QStringLiteral("type")] = n.type;
    m[QStringLiteral("name")] = n.name;
    m[QStringLiteral("props")] = n.props;
    m[QStringLiteral("degree")] = n.degree;
    m[QStringLiteral("urlHref")] = n.urlHref;
    m[QStringLiteral("urlKind")] = n.urlKind;
    m[QStringLiteral("urlSafe")] = n.urlSafe;
    m[QStringLiteral("urlRaw")]  = n.urlRaw;
    return m;
}

int ThreatGraphModel::indexOfId(const QString &id) const {
    return m_index.value(id, -1);
}

void ThreatGraphModel::setSelectedId(const QString &id) {
    if (m_selectedId == id) return;
    const QString old = m_selectedId;
    m_selectedId = id;
    auto bump = [this](const QString &nid) {
        auto it = m_index.constFind(nid);
        if (it != m_index.constEnd()) emit dataChanged(index(*it), index(*it), {SelectedRole});
    };
    bump(old); bump(id);
    emit selectionChanged();
}
