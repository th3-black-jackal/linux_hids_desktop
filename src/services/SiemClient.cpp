#include "SiemClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {

// The paginated envelope can carry up to page_size rows; a backfill of a
// long history is many round trips, so ask for the documented maximum.
constexpr int kBackfillPageSize = 500;

// Matches DetectionsModel::kMaxRetained — there is no point pulling more
// history than the triage queue will keep.
constexpr int kDefaultBackfillMax = 5000;

} // namespace

SiemClient::SiemClient(BackendSettings *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_backend, &BackendSettings::changed, this, &SiemClient::configuredChanged);
    m_events.setPolicy(m_policy);
}

SiemClient::~SiemClient()
{
    // Abort in-flight GETs on the way out, with their handlers detached —
    // abort() emits finished() synchronously and there is nothing useful for
    // it to do against a half-destroyed object.
    for (QPointer<QNetworkReply> *r : { &m_listReply, &m_statsReply, &m_backfillReply, &m_detailReply }) {
        if (*r) {
            (*r)->disconnect(this);
            (*r)->abort();
        }
    }
}

bool SiemClient::loading() const
{
    return !m_listReply.isNull();
}

bool SiemClient::detailLoading() const
{
    return !m_detailReply.isNull();
}

void SiemClient::setPolicy(const SeverityPolicy &p)
{
    m_policy = p;
    m_events.setPolicy(p);
}

// ── URLs ────────────────────────────────────────────────────────────────

QUrl SiemClient::eventsUrl(const QVariantMap &filters, int page) const
{
    QUrl url = m_backend->url(QStringLiteral("/api/siem/events/"));
    QUrlQuery q;
    for (auto it = filters.constBegin(); it != filters.constEnd(); ++it) {
        const QString v = it.value().toString();
        if (!v.isEmpty())
            q.addQueryItem(it.key(), v);
    }
    if (page > 1)
        q.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(q);
    return url;
}

// ── errors ──────────────────────────────────────────────────────────────

QString SiemClient::describeError(QNetworkReply *reply, const QByteArray &body, QString *field) const
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject o = QJsonDocument::fromJson(body).object();

    if (status == 401)
        return QStringLiteral("401 Unauthorized — the API token is missing or not "
                              "accepted by the backend.");
    if (status == 404)
        return QStringLiteral("404 — %1")
            .arg(o.value(QStringLiteral("detail")).toString(QStringLiteral("not found")));

    if (status == 400) {
        // §6: {"<field>": ["<message>"]} — name the control the operator
        // needs to fix rather than showing them a raw body.
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            QString msg;
            if (it.value().isArray() && !it.value().toArray().isEmpty())
                msg = it.value().toArray().first().toString();
            else
                msg = it.value().toString();
            if (!msg.isEmpty()) {
                if (field)
                    *field = it.key();
                return QStringLiteral("%1: %2").arg(it.key(), msg);
            }
        }
        return QStringLiteral("400 — the backend rejected a filter value.");
    }

    if (status > 0)
        return QStringLiteral("HTTP %1 — %2").arg(status).arg(reply->errorString());
    return reply->errorString();
}

void SiemClient::setError(const QString &detail, const QString &field)
{
    m_lastError = detail;
    m_lastErrorField = field;
    emit lastErrorChanged();
    if (!detail.isEmpty())
        emit logged(QStringLiteral("siem: %1").arg(detail));
}

// ── events list ─────────────────────────────────────────────────────────

void SiemClient::refresh()
{
    if (!m_backend->configured()) {
        setError(QStringLiteral("No backend configured. Set the base URL on the "
                                "Integrations screen."));
        return;
    }
    // §7.5: reset to page 1 whenever the filters change.
    fetchPage(eventsUrl(m_filters.toQuery(), 1), /*append=*/false);
    fetchStats();
}

void SiemClient::loadMore()
{
    // §7.5: follow `next`, don't compute page URLs by hand — it already
    // carries the active filters.
    if (m_events.nextUrl().isEmpty() || m_listReply)
        return;
    fetchPage(QUrl(m_events.nextUrl()), /*append=*/true);
}

void SiemClient::fetchPage(const QUrl &url, bool append)
{
    if (m_listReply) {
        m_listReply->disconnect(this);
        m_listReply->abort();
        m_listReply = nullptr;
    }

    setError(QString());

    m_listReply = m_nam->get(m_backend->request(url));
    emit loadingChanged();

    connect(m_listReply, &QNetworkReply::finished, this, [this, append]() {
        QNetworkReply *reply = m_listReply;
        if (!reply)
            return;
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            QString field;
            setError(describeError(reply, body, &field), field);
            if (!append)
                m_events.reset();
        } else {
            m_events.applyPage(QJsonDocument::fromJson(body).object(), append);
        }

        reply->deleteLater();
        m_listReply = nullptr;
        emit loadingChanged();
    });
}

void SiemClient::fetchStats()
{
    if (m_statsReply) {
        m_statsReply->disconnect(this);
        m_statsReply->abort();
        m_statsReply = nullptr;
    }

    // §4.3: the same filters as the list, then aggregated. Paging params are
    // meaningless here and are dropped.
    QVariantMap q = m_filters.toQuery();
    q.remove(QStringLiteral("page_size"));
    q.remove(QStringLiteral("ordering"));

    QUrl url = m_backend->url(QStringLiteral("/api/siem/events/stats/"));
    QUrlQuery uq;
    for (auto it = q.constBegin(); it != q.constEnd(); ++it) {
        const QString v = it.value().toString();
        if (!v.isEmpty())
            uq.addQueryItem(it.key(), v);
    }
    url.setQuery(uq);

    m_statsReply = m_nam->get(m_backend->request(url));
    connect(m_statsReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_statsReply;
        if (!reply)
            return;
        const QByteArray body = reply->readAll();

        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            m_statsTotal  = o.value(QStringLiteral("total")).toInt();
            m_statsAlerts = o.value(QStringLiteral("alerts")).toInt();
            m_statsByClass = o.value(QStringLiteral("by_class")).toArray().toVariantList();
            m_statsByHost  = o.value(QStringLiteral("by_host")).toArray().toVariantList();
            emit statsChanged();
        }
        // A stats failure is deliberately not surfaced as *the* error: the
        // table is the screen's job and it may well have loaded fine. The
        // list request will have reported anything that matters.

        reply->deleteLater();
        m_statsReply = nullptr;
    });
}

// ── detail ──────────────────────────────────────────────────────────────

void SiemClient::fetchDetail(int eventId)
{
    if (eventId < 0 || !m_backend->configured())
        return;

    if (m_detailReply) {
        m_detailReply->disconnect(this);
        m_detailReply->abort();
        m_detailReply = nullptr;
    }

    m_detailId = eventId;
    m_detail.clear();
    m_detailError.clear();
    emit detailChanged();

    const QUrl url = m_backend->url(QStringLiteral("/api/siem/events/%1/").arg(eventId));
    m_detailReply = m_nam->get(m_backend->request(url));
    emit detailChanged();

    connect(m_detailReply, &QNetworkReply::finished, this, [this, eventId]() {
        QNetworkReply *reply = m_detailReply;
        if (!reply)
            return;
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            QString field;
            m_detailError = describeError(reply, body, &field);
        } else if (m_detailId == eventId) {
            m_detail = QJsonDocument::fromJson(body).object().toVariantMap();
        }

        reply->deleteLater();
        m_detailReply = nullptr;
        emit detailChanged();
    });
}

void SiemClient::clearDetail()
{
    if (m_detailReply) {
        m_detailReply->disconnect(this);
        m_detailReply->abort();
        m_detailReply = nullptr;
    }
    m_detailId = -1;
    m_detail.clear();
    m_detailError.clear();
    emit detailChanged();
}

// ── backfill ────────────────────────────────────────────────────────────

QVariantMap SiemClient::backfillQuery() const
{
    // `type=alert` is the *backend's* definition of an alert — the board
    // debouncer's confirmed detections (§3.1) — not the console's
    // class-name rule. Ascending order matters: DetectionsModel prepends, so
    // feeding it oldest-first is what leaves the newest row at the top.
    QVariantMap q;
    q[QStringLiteral("type")] = QStringLiteral("alert");
    q[QStringLiteral("ordering")] = QStringLiteral("event_time");
    q[QStringLiteral("page_size")] = kBackfillPageSize;
    return q;
}

void SiemClient::backfillAlerts(PageSink sink, int maxRows)
{
    m_sink = std::move(sink);
    m_backfillMax = maxRows > 0 ? maxRows : kDefaultBackfillMax;
    m_backfillLoaded = 0;
    m_backfillTotal = 0;

    if (!m_backend->configured()) {
        // Not an error: a console pointed only at a broker is a valid
        // deployment. Say so once and move on — MQTT still fills the table.
        m_backfillState = QStringLiteral("skipped");
        emit backfillChanged();
        emit logged(QStringLiteral("backfill: no backend configured — "
                                   "alerts will come from MQTT only"));
        emit backfillFinished(0);
        return;
    }

    m_backfillState = QStringLiteral("loading");
    emit backfillChanged();
    emit logged(QStringLiteral("backfill: fetching confirmed alerts from the SIEM database"));

    probeBackfillCount();
}

void SiemClient::probeBackfillCount()
{
    // One cheap request for the envelope's `count` before paging starts.
    //
    // Without it, a history longer than the console retains gets walked from
    // the *oldest* end and stops at the cap — which throws away precisely
    // the newest alerts, the ones an operator opens the console to see.
    // Knowing the count first means the walk can start at the right offset
    // and keep the newest N instead.
    QVariantMap q = backfillQuery();
    q[QStringLiteral("page_size")] = 1;

    QNetworkReply *probe = m_nam->get(m_backend->request(eventsUrl(q, 1)));
    m_backfillReply = probe;

    connect(probe, &QNetworkReply::finished, this, [this, probe]() {
        const QByteArray body = probe->readAll();
        probe->deleteLater();
        m_backfillReply = nullptr;

        if (probe->error() != QNetworkReply::NoError) {
            QString field;
            const QString detail = describeError(probe, body, &field);
            m_backfillState = QStringLiteral("error");
            emit backfillChanged();
            emit logged(QStringLiteral("backfill failed: %1").arg(detail));
            emit backfillFinished(0);
            return;
        }

        m_backfillTotal = QJsonDocument::fromJson(body).object()
                              .value(QStringLiteral("count")).toInt();
        emit backfillChanged();

        int startPage = 1;
        if (m_backfillTotal > m_backfillMax) {
            // Skip whole pages off the old end. The remainder inside the
            // first fetched page is harmless: DetectionsModel trims from the
            // oldest end anyway, so a few extra rows simply fall off.
            const int skip = m_backfillTotal - m_backfillMax;
            startPage = skip / kBackfillPageSize + 1;
            m_backfillState = QStringLiteral("truncated");
            emit backfillChanged();
            emit logged(QStringLiteral("backfill: %1 alerts stored, loading the newest %2 "
                                       "(the console's retention limit) — use the Events "
                                       "screen to reach older ones")
                            .arg(m_backfillTotal).arg(m_backfillMax));
        }

        fetchBackfillPage(eventsUrl(backfillQuery(), startPage));
    });
}

void SiemClient::retryBackfill()
{
    if (!m_sink || m_backfillReply)
        return;
    backfillAlerts(m_sink, m_backfillMax);
}

void SiemClient::fetchBackfillPage(const QUrl &url)
{
    m_backfillReply = m_nam->get(m_backend->request(url));

    connect(m_backfillReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_backfillReply;
        if (!reply)
            return;
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        m_backfillReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            QString field;
            const QString detail = describeError(reply, body, &field);
            m_backfillState = QStringLiteral("error");
            emit backfillChanged();
            // Deliberately not routed through setError(): the Events screen's
            // error line is about the operator's filter, and a failed startup
            // backfill is neither caused by nor fixable from there.
            emit logged(QStringLiteral("backfill failed: %1").arg(detail));
            emit backfillFinished(m_backfillLoaded);
            return;
        }

        const QJsonObject env = QJsonDocument::fromJson(body).object();

        const QJsonArray results = env.value(QStringLiteral("results")).toArray();
        QVector<SiemEventsModel::Row> page;
        page.reserve(results.size());
        for (const QJsonValue &v : results) {
            SiemEventsModel::Row row;
            if (!SiemEventsModel::parseRow(v.toObject(), m_policy, &row))
                continue;
            page.append(row);
        }
        m_backfillLoaded += page.size();
        if (m_sink && !page.isEmpty())
            m_sink(page);
        emit backfillChanged();

        const QString next = env.value(QStringLiteral("next")).toString();

        if (!next.isEmpty()) {
            fetchBackfillPage(QUrl(next));
            return;
        }

        // "truncated" was decided by the count probe and stays; only a run
        // that reached the end without one is plain "done".
        if (m_backfillState != QLatin1String("truncated"))
            m_backfillState = QStringLiteral("done");
        emit backfillChanged();
        emit logged(QStringLiteral("backfill: loaded %1 alert%2 from the SIEM database")
                        .arg(m_backfillLoaded)
                        .arg(m_backfillLoaded == 1 ? QString() : QStringLiteral("s")));
        emit backfillFinished(m_backfillLoaded);
    });
}

void SiemClient::cancel()
{
    if (m_listReply) {
        m_listReply->disconnect(this);
        m_listReply->abort();
        m_listReply = nullptr;
        emit loadingChanged();
    }
}
