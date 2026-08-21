#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QString>
#include <QVariantList>
#include <QVector>

// The chat transcript: alternating operator questions and backend answers.
//
// An assistant turn is created empty the moment a question is sent and then
// filled in as SSE frames arrive, so the UI has a row to stream into. That is
// why `state` exists — a row can be waiting, streaming, finished or failed,
// and the three look different on screen.
//
// Only `history()` leaves the console: it rebuilds the last few completed
// turns in the shape the backend documents (§3.2), skipping anything that
// failed or is still in flight.
class ChatModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        RoleRole = Qt::UserRole + 1, // "user" | "assistant"
        TextRole,
        SourcesRole,   // QVariantList of source objects, assistant rows only
        StateRole,     // "waiting" | "streaming" | "done" | "error" | "cancelled"
        ErrorRole,
        TimeRole,
    };
    Q_ENUM(Roles)

    struct Turn {
        QString role;
        QString text;
        QVariantList sources;
        QString state;
        QString error;
        QString time;
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int appendUser(const QString &text);
    int appendAssistantPlaceholder();

    void appendToken(int row, const QString &text);
    void setSources(int row, const QVariantList &sources);
    void finish(int row);
    void fail(int row, const QString &detail);
    void cancel(int row);

    QString textAt(int row) const;
    QString roleAt(int row) const;

    // The last `maxTurns` completed turns, oldest first, as the backend's
    // history array. Unfinished or failed answers are left out — sending a
    // truncated answer back as context would poison the next question.
    QJsonArray history(int maxTurns) const;

    Q_INVOKABLE void clear();

signals:
    void countChanged();

private:
    void touch(int row, const QVector<int> &roles);

    QVector<Turn> m_turns; // oldest first
};
