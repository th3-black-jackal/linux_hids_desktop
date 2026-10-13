#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "BackendSettings.h"
#include "ChatClient.h"

// Backs the right-click "explain this alert" popup.
//
// It owns its **own** ChatClient rather than borrowing the Assistant
// screen's. Two reasons, both about not surprising the operator:
//
//   * A popup answer is a single throwaway question about one row. Pushing it
//     into the Assistant transcript would rewrite a conversation the operator
//     is in the middle of, and — because the backend keeps the last 6 turns
//     as context — would silently steer their next question too.
//   * The popup sends no `history`, so each explanation is grounded only in
//     the alert it is about.
//
// The two clients still cannot both generate at once: they share
// BackendSettings' generation slot, which is where §6.7's one-at-a-time rule
// actually lives.
//
// The answer is exposed as flat properties rather than a transcript model —
// a popup shows one answer, and a list model for a single row would only be
// ceremony.
class AlertExplainer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString subject READ subject NOTIFY changed)
    Q_PROPERTY(QString subjectDetail READ subjectDetail NOTIFY changed)
    Q_PROPERTY(QString severity READ severity NOTIFY changed)

    Q_PROPERTY(QString answer READ answer NOTIFY answerChanged)
    Q_PROPERTY(QVariantList sources READ sources NOTIFY answerChanged)
    // idle | waiting | streaming | done | error | cancelled
    Q_PROPERTY(QString state READ state NOTIFY answerChanged)
    Q_PROPERTY(QString error READ error NOTIFY answerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY answerChanged)

    Q_PROPERTY(QString question READ question NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)

public:
    AlertExplainer(BackendSettings *backend, QObject *parent = nullptr);

    bool active() const { return m_active; }
    QString subject() const { return m_subject; }
    QString subjectDetail() const { return m_subjectDetail; }
    QString severity() const { return m_severity; }

    QString answer() const { return m_answer; }
    QVariantList sources() const { return m_sources; }
    QString state() const { return m_state; }
    QString error() const { return m_error; }
    bool busy() const { return m_chat->busy(); }

    QString question() const { return m_question; }
    bool configured() const { return m_backend->configured(); }

    // Opens the popup for one alert and starts the query. Every argument is a
    // field the event actually carries; nothing is inferred.
    Q_INVOKABLE void explain(const QString &uid, const QString &host,
                             const QString &className, int classIdx,
                             const QString &confidenceText, const QString &timestampRaw,
                             const QString &severity);

    Q_INVOKABLE void close();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void cancel();

    // Hands the same question to the full Assistant screen, for an operator
    // who wants to follow up rather than read one answer.
    Q_INVOKABLE QString questionForHandoff() const { return m_question; }

    void loadSettings() { m_chat->loadSettings(); }

signals:
    void changed();
    void answerChanged();

private:
    void setState(const QString &s);

    BackendSettings *m_backend;
    ChatClient *m_chat;

    bool m_active = false;
    QString m_subject;
    QString m_subjectDetail;
    QString m_severity;
    QString m_question;

    QString m_answer;
    QVariantList m_sources;
    QString m_state = QStringLiteral("idle");
    QString m_error;
};
