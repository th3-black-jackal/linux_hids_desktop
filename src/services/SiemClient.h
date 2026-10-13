#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <functional>

#include "../core/EventFilters.h"
#include "../core/SiemEventsModel.h"
#include "BackendSettings.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QUrl;
QT_END_NAMESPACE

// Read-only client for `/api/siem/` — the stored telemetry (see
// SIEM_API_DOCUMENTATION.md). Every endpoint is a GET; nothing here can
// change anything on the backend.
//
// Two jobs, deliberately kept apart:
//
//   * **The Events screen.** `refresh()` / `loadMore()` drive `events`,
//     filtered by `EventFilters`, paged by following the envelope's `next`.
//   * **Startup backfill.** `backfillAlerts()` walks the whole alert history
//     oldest-first and hands each row to a callback, which TelemetryClient
//     uses to fill the live alerts table before MQTT has said anything.
//
// The two use separate replies so an operator changing a filter cannot
// cancel a backfill that is still running, or vice versa.
class SiemClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(SiemEventsModel *events READ events CONSTANT)
    Q_PROPERTY(EventFilters *filters READ filters CONSTANT)

    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastErrorField READ lastErrorField NOTIFY lastErrorChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY configuredChanged)

    // events/stats/ for the current filter (§4.3).
    Q_PROPERTY(int statsTotal READ statsTotal NOTIFY statsChanged)
    Q_PROPERTY(int statsAlerts READ statsAlerts NOTIFY statsChanged)
    Q_PROPERTY(QVariantList statsByClass READ statsByClass NOTIFY statsChanged)
    Q_PROPERTY(QVariantList statsByHost READ statsByHost NOTIFY statsChanged)

    // Backfill progress, surfaced so the Detections screen can say where its
    // history came from instead of silently filling up.
    Q_PROPERTY(QString backfillState READ backfillState NOTIFY backfillChanged)
    Q_PROPERTY(int backfillLoaded READ backfillLoaded NOTIFY backfillChanged)
    Q_PROPERTY(int backfillTotal READ backfillTotal NOTIFY backfillChanged)

    // Detail (§4.2) for the row selected on the Events screen.
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)
    Q_PROPERTY(int detailId READ detailId NOTIFY detailChanged)
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailChanged)
    Q_PROPERTY(QString detailError READ detailError NOTIFY detailChanged)

public:
    // Called once per backfilled *page*, rows oldest first. Page-level
    // rather than row-level so the sink can insert a whole page under one
    // model reset — a 5000-row history is otherwise 5000 of them.
    using PageSink = std::function<void(const QVector<SiemEventsModel::Row> &)>;

    explicit SiemClient(BackendSettings *backend, QObject *parent = nullptr);
    ~SiemClient() override;

    SiemEventsModel *events() { return &m_events; }
    EventFilters *filters() { return &m_filters; }

    bool loading() const;
    QString lastError() const { return m_lastError; }
    QString lastErrorField() const { return m_lastErrorField; }
    bool configured() const { return m_backend->configured(); }

    int statsTotal() const { return m_statsTotal; }
    int statsAlerts() const { return m_statsAlerts; }
    QVariantList statsByClass() const { return m_statsByClass; }
    QVariantList statsByHost() const { return m_statsByHost; }

    QString backfillState() const { return m_backfillState; }
    int backfillLoaded() const { return m_backfillLoaded; }
    int backfillTotal() const { return m_backfillTotal; }

    QVariantMap detail() const { return m_detail; }
    int detailId() const { return m_detailId; }
    bool detailLoading() const;
    QString detailError() const { return m_detailError; }

    void setPolicy(const SeverityPolicy &p);

    // Events screen.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void fetchDetail(int eventId);
    Q_INVOKABLE void clearDetail();

    // Startup backfill. `sink` receives each page, oldest first; `maxRows`
    // bounds an unbounded history (0 = the model's own retention cap).
    void backfillAlerts(PageSink sink, int maxRows);
    Q_INVOKABLE void retryBackfill();

    QUrl eventsUrl(const QVariantMap &filters, int page) const;

signals:
    void loadingChanged();
    void lastErrorChanged();
    void configuredChanged();
    void statsChanged();
    void backfillChanged();
    void detailChanged();
    void backfillFinished(int rows);
    void logged(const QString &message);

private:
    void fetchPage(const QUrl &url, bool append);
    void fetchStats();
    void fetchBackfillPage(const QUrl &url);
    void probeBackfillCount();
    QVariantMap backfillQuery() const;
    // Decodes a failed reply into a human sentence, and the offending filter
    // field when the backend named one (§6).
    QString describeError(QNetworkReply *reply, const QByteArray &body, QString *field) const;
    void setError(const QString &detail, const QString &field = QString());

    BackendSettings *m_backend;
    QNetworkAccessManager *m_nam;

    SiemEventsModel m_events;
    EventFilters m_filters;
    SeverityPolicy m_policy;

    QPointer<QNetworkReply> m_listReply;
    QPointer<QNetworkReply> m_statsReply;
    QPointer<QNetworkReply> m_backfillReply;
    QPointer<QNetworkReply> m_detailReply;

    QString m_lastError;
    QString m_lastErrorField;

    int m_statsTotal = 0;
    int m_statsAlerts = 0;
    QVariantList m_statsByClass;
    QVariantList m_statsByHost;

    PageSink m_sink;
    int m_backfillMax = 0;
    int m_backfillLoaded = 0;
    int m_backfillTotal = 0;
    QString m_backfillState = QStringLiteral("idle"); // idle|loading|done|truncated|error|skipped

    QVariantMap m_detail;
    int m_detailId = -1;
    QString m_detailError;
};
