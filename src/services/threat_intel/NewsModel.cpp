#include "NewsModel.h"

// NewsModel — see header. A flat list of article rows for the News section.

NewsModel::NewsModel(QObject *parent) : QAbstractListModel(parent) {}

QHash<int, QByteArray> NewsModel::roleNames() const {
    return {
        {IdRole, "articleId"}, {TitleRole, "title"}, {UrlRole, "url"},
        {PublishedRole, "published"}, {SourceRole, "source"},
        {SummaryRole, "summary"}, {MentionsRole, "mentions"}
    };
}

QVariant NewsModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= m_rows.size()) return {};
    const QVariantMap &a = m_rows[index.row()];
    switch (role) {
    case IdRole:        return a.value(QStringLiteral("id"));
    case TitleRole:     return a.value(QStringLiteral("title"));
    case UrlRole:       return a.value(QStringLiteral("url"));
    case PublishedRole: return a.value(QStringLiteral("published"));
    case SourceRole:    return a.value(QStringLiteral("source"));
    case SummaryRole:   return a.value(QStringLiteral("summary"));
    case MentionsRole:  return a.value(QStringLiteral("mentions"));
    default:            return {};
    }
}

void NewsModel::setArticles(const QVariantList &articles, const QString &nextUrl,
                            int total, bool append) {
    if (!append) {
        beginResetModel();
        m_rows.clear();
        for (const auto &v : articles) m_rows.append(v.toMap());
        endResetModel();
    } else if (!articles.isEmpty()) {
        beginInsertRows({}, m_rows.size(), m_rows.size() + articles.size() - 1);
        for (const auto &v : articles) m_rows.append(v.toMap());
        endInsertRows();
    }
    m_nextUrl = nextUrl;
    if (m_total != total) { m_total = total; emit totalChanged(); }
    emit countChanged();
}

void NewsModel::clear() {
    beginResetModel();
    m_rows.clear();
    m_nextUrl.clear();
    endResetModel();
    if (m_total != 0) { m_total = 0; emit totalChanged(); }
    emit countChanged();
}

QVariantMap NewsModel::get(int row) const {
    if (row < 0 || row >= m_rows.size()) return {};
    return m_rows[row];
}
