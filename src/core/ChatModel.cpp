#include "ChatModel.h"

#include <QJsonObject>
#include <QTime>

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_turns.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_turns.size())
        return {};

    const Turn &t = m_turns.at(index.row());
    switch (role) {
    case RoleRole:    return t.role;
    case TextRole:    return t.text;
    case SourcesRole: return t.sources;
    case StateRole:   return t.state;
    case ErrorRole:   return t.error;
    case TimeRole:    return t.time;
    default:          return {};
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        { RoleRole,    "role" },
        { TextRole,    "text" },
        { SourcesRole, "sources" },
        { StateRole,   "state" },
        { ErrorRole,   "error" },
        { TimeRole,    "time" },
    };
}

int ChatModel::appendUser(const QString &text)
{
    Turn t;
    t.role = QStringLiteral("user");
    t.text = text;
    t.state = QStringLiteral("done");
    t.time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));

    beginInsertRows(QModelIndex(), m_turns.size(), m_turns.size());
    m_turns.append(t);
    endInsertRows();
    emit countChanged();
    return m_turns.size() - 1;
}

int ChatModel::appendAssistantPlaceholder()
{
    Turn t;
    t.role = QStringLiteral("assistant");
    t.state = QStringLiteral("waiting");
    t.time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));

    beginInsertRows(QModelIndex(), m_turns.size(), m_turns.size());
    m_turns.append(t);
    endInsertRows();
    emit countChanged();
    return m_turns.size() - 1;
}

void ChatModel::touch(int row, const QVector<int> &roles)
{
    if (row < 0 || row >= m_turns.size())
        return;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, roles);
}

void ChatModel::appendToken(int row, const QString &text)
{
    if (row < 0 || row >= m_turns.size() || text.isEmpty())
        return;
    Turn &t = m_turns[row];
    t.text += text;
    const bool becameStreaming = (t.state != QStringLiteral("streaming"));
    t.state = QStringLiteral("streaming");
    touch(row, becameStreaming ? QVector<int>{ TextRole, StateRole }
                               : QVector<int>{ TextRole });
}

void ChatModel::setSources(int row, const QVariantList &sources)
{
    if (row < 0 || row >= m_turns.size())
        return;
    m_turns[row].sources = sources;
    touch(row, { SourcesRole });
}

void ChatModel::finish(int row)
{
    if (row < 0 || row >= m_turns.size())
        return;
    Turn &t = m_turns[row];
    // A `done` event with nothing before it is a real outcome, not a crash:
    // the model generated no tokens. Say so rather than leaving a blank row.
    if (t.text.isEmpty()) {
        t.state = QStringLiteral("error");
        t.error = QStringLiteral("the backend finished without producing an answer");
    } else {
        t.state = QStringLiteral("done");
    }
    touch(row, { StateRole, ErrorRole });
}

void ChatModel::fail(int row, const QString &detail)
{
    if (row < 0 || row >= m_turns.size())
        return;
    Turn &t = m_turns[row];
    t.state = QStringLiteral("error");
    t.error = detail;
    touch(row, { StateRole, ErrorRole });
}

void ChatModel::cancel(int row)
{
    if (row < 0 || row >= m_turns.size())
        return;
    Turn &t = m_turns[row];
    t.state = QStringLiteral("cancelled");
    touch(row, { StateRole });
}

QString ChatModel::textAt(int row) const
{
    if (row < 0 || row >= m_turns.size())
        return {};
    return m_turns.at(row).text;
}

QString ChatModel::roleAt(int row) const
{
    if (row < 0 || row >= m_turns.size())
        return {};
    return m_turns.at(row).role;
}

QJsonArray ChatModel::history(int maxTurns) const
{
    // Walk backwards collecting usable turns, then reverse: the backend wants
    // oldest-first, but the cap has to keep the *most recent* context.
    QVector<const Turn *> kept;
    for (int i = m_turns.size() - 1; i >= 0 && kept.size() < maxTurns; --i) {
        const Turn &t = m_turns.at(i);
        if (t.state != QStringLiteral("done") || t.text.isEmpty())
            continue;
        kept.append(&t);
    }

    QJsonArray out;
    for (int i = kept.size() - 1; i >= 0; --i) {
        QJsonObject o;
        o[QStringLiteral("role")] = kept.at(i)->role;
        o[QStringLiteral("content")] = kept.at(i)->text;
        out.append(o);
    }
    return out;
}

void ChatModel::clear()
{
    if (m_turns.isEmpty())
        return;
    beginResetModel();
    m_turns.clear();
    endResetModel();
    emit countChanged();
}
