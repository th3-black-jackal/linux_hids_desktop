#pragma once

// ThreatGraphModel — holds the current subgraph (nodes + edges) for the graph
// section, and owns the force-directed layout positions.
//
// The graph view is a QML Canvas that reads node x/y and edges from here; the
// model runs a light spring-electrical simulation so the layout logic stays in
// C++ (testable, and off the paint path). Nodes and edges are exposed both as
// a list model (for the Canvas / a Repeater of node chips) and as plain
// QVariantList getters for the Canvas edge pass.
//
// Node ids are stable (`Entity.id` from the schema), so merging an expand
// result keeps existing positions and only lays out the newcomers.

#include <QAbstractListModel>
#include <QHash>
#include <QPointF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class ThreatGraphModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString selectedId READ selectedId WRITE setSelectedId NOTIFY selectionChanged)
    Q_PROPERTY(bool settling READ settling NOTIFY settlingChanged)  // layout still moving

public:
    struct Node {
        QString id, type, name;
        QVariantMap props;
        QString urlHref, urlKind, urlRaw;
        bool urlSafe = false;
        QPointF pos;            // layout position in the model's coordinate box
        QPointF vel;            // simulation velocity
        bool pinned = false;    // user-dragged nodes stay put
        int degree = 0;
    };
    struct Edge {
        QString source, target, type;
        QVariantMap props;
        bool cooccurrence = false;   // dashed, statistical
    };

    // NOTE on role string names (set in roleNames() in the .cpp): expose these
    // to QML as "nodeId", "posX", "posY" — NOT "id"/"x"/"y", which collide with
    // QML's built-in `id` attribute and Item.x/Item.y in a delegate.
    enum Roles {
        IdRole = Qt::UserRole + 1, // -> "nodeId"
        TypeRole,                  // -> "type"
        NameRole,                  // -> "name"
        XRole,                     // -> "posX"
        YRole,                     // -> "posY"
        DegreeRole, SelectedRole,  // -> "degree", "selected"
        UrlHrefRole, UrlKindRole, UrlSafeRole, UrlRawRole, PropsRole
    };

    explicit ThreatGraphModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = {}) const override { return m_nodes.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replace the whole graph (new seed/overview) or merge (expand a node).
    void setGraph(const QVariantList &nodes, const QVariantList &edges);
    void mergeGraph(const QVariantList &nodes, const QVariantList &edges);
    void clear();

    // Edges for the Canvas paint pass: [{x1,y1,x2,y2,cooccurrence,type}].
    Q_INVOKABLE QVariantList edgeLines() const;
    Q_INVOKABLE QVariantMap get(int row) const;           // full node detail
    Q_INVOKABLE int indexOfId(const QString &id) const;

    // Layout control, called by the view.
    Q_INVOKABLE void setViewport(double w, double h);     // sim bounds
    Q_INVOKABLE void step(int iterations = 1);            // advance the sim
    Q_INVOKABLE void pin(const QString &id, double x, double y);  // drag
    Q_INVOKABLE void release(const QString &id);
    Q_INVOKABLE void reheat();                            // after a merge

    QString selectedId() const { return m_selectedId; }
    void setSelectedId(const QString &id);
    bool settling() const { return m_settling; }

signals:
    void countChanged();
    void selectionChanged();
    void settlingChanged();

private:
    static Node parseNode(const QVariantMap &m);
    void recomputeDegrees();
    void seedPositions(int firstNewRow);   // place newcomers near their neighbours
    void gridLayout();                     // static grid for an edgeless set
    void setSettling(bool s);

    QVector<Node> m_nodes;
    QVector<Edge> m_edges;
    QHash<QString, int> m_index;   // id -> row
    QString m_selectedId;
    double m_w = 1000, m_h = 700;
    bool m_settling = false;
    // Cooling: the sim runs at most m_maxCool iterations after a (re)heat, with
    // a temperature that decays to 0, so it always comes to rest instead of
    // animating (and repainting) forever.
    int m_cool = 0;
    int m_maxCool = 360;
};
