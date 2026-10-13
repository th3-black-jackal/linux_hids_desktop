#pragma once

// NewsModel — the News section: a list of Article rows from the backend
// graph_api `/api/graph/articles/?entity=&days=` ({count, results}). That feed
// filters only by entity + a days window (no cursor), so the view uses a
// 7/30/90-day selector; the append/nextUrl params below are kept generic (and
// simply unused for this feed) so the model also suits a future paged source.

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>

class NewsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY totalChanged)     // server's `count`
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1, TitleRole, UrlRole, PublishedRole,
        SourceRole, SummaryRole, MentionsRole   // MentionsRole: QVariantList of {id,type,name}
    };

    explicit NewsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = {}) const override { return m_rows.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // append=false resets (new filter/node); append=true continues the feed.
    void setArticles(const QVariantList &articles, const QString &nextUrl,
                     int total, bool append);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;
    QString nextUrl() const { return m_nextUrl; }
    bool canLoadMore() const { return !m_nextUrl.isEmpty(); }
    int total() const { return m_total; }

signals:
    void countChanged();
    void totalChanged();

private:
    QVector<QVariantMap> m_rows;
    QString m_nextUrl;
    int m_total = 0;
};
