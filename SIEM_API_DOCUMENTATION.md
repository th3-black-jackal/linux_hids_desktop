# SIEM Read API — Documentation (Desktop application)

Reference for the Qt/QML desktop app to read and filter HIDS telemetry served by
the `siem_api` Django app: detection events, alerts, board status/liveness,
nodes, and quarantined messages. All endpoints are **read-only (GET)**.

> This is the data/telemetry API. The RAG chat endpoints (`/api/attack/chat/…`)
> are documented separately.

---

## 1. Conventions

| Item | Value |
|------|-------|
| Base URL | `http://<backend-host>:8000` (per deployment) |
| API root | `/api/siem/` |
| Response body | `application/json` |
| Encoding | UTF-8; all timestamps ISO-8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`) |
| Auth | required on every request (see §2) |

Native desktop client, so there is no CORS concern. Use **HTTPS** in production.

---

## 2. Authentication

Endpoints require an authenticated user. The recommended scheme for the desktop
app is Token authentication:

```
Authorization: Token <your-token>
```

Missing/invalid credentials return **401 Unauthorized**. (The exact scheme is a
project-level Django setting; confirm token issuance with the backend owner.)

---

## 3. Data models

### 3.1 Event (list form)

Returned by the events list. Lean by design — no feature vector or raw payload.

```json
{
  "id": 42,
  "host": "rpi3-sensor-01",
  "event_time": "2026-07-31T14:31:10Z",
  "received_at": "2026-07-31T14:31:11Z",
  "class_idx": 8,
  "class_name": "xmrig",
  "confidence": 0.9736,
  "alert": true,
  "source_topic": "alerts,events"
}
```

| Field | Type | Notes |
|-------|------|-------|
| `id` | int | event id (use for the detail endpoint) |
| `host` | string | board hostname (authoritative identity) |
| `event_time` | string | when the board classified the window |
| `received_at` | string | when the backend stored it (may differ from `event_time`) |
| `class_idx` | int | 0–8, see §3.5 |
| `class_name` | string | see §3.5 |
| `confidence` | number | softmax probability, 0.0–1.0 |
| `alert` | bool | `true` = debouncer-confirmed detection |
| `source_topic` | string | `events`, `alerts`, or `alerts,events` (seen on both) |

### 3.2 Event (detail form)

The detail endpoint adds two fields:

| Field | Type | Notes |
|-------|------|-------|
| `features` | number[] \| null | 31 normalized floats in `[0,1]`, or `null` if the producer disabled them |
| `raw` | object | the exact original MQTT JSON payload (audit/replay) |

Feature index → name mapping is fixed (FEATURE_ORDER); request it from the
backend team if you need to label individual features in a detail view.

### 3.3 Status record

```json
{"id": 7, "host": "rpi3-sensor-01", "state": "offline", "reason": "lwt", "received_at": "2026-07-31T14:35:02Z"}
```

`state` ∈ {`online`, `offline`}. `reason` ∈ {`clean`, `lwt`, `""`}. `lwt` (last
will) means the board dropped without a clean disconnect — the "board died"
signal.

### 3.4 Node

```json
{"host": "rpi3-sensor-01", "first_seen": "2026-07-31T09:00:00Z", "last_seen": "2026-07-31T14:35:02Z", "last_state": "offline", "last_reason": "lwt"}
```

One row per board; `last_state`/`last_reason` give current liveness at a glance.

### 3.5 Class index → name

| idx | name | idx | name | idx | name |
|-----|------|-----|------|-----|------|
| 0 | `normal` | 3 | `bashlite` | 6 | `backdoor` |
| 1 | `ransomware` | 4 | `httpbackdoor` | 7 | `bdvl` |
| 2 | `thetick` | 5 | `beurk` | 8 | `xmrig` |

---

## 4. Endpoints

### 4.1 `GET /api/siem/events/` — list events

The primary feed: all detection events, newest first, paginated. Supports every
filter in §5.

**Response `200 OK`** — standard paginated envelope:

```json
{
  "count": 128,
  "next": "http://backend:8000/api/siem/events/?page=2",
  "previous": null,
  "results": [ { /* Event (list form) */ } ]
}
```

`next`/`previous` are absolute URLs (or `null`). Follow `next` for the next page.

### 4.2 `GET /api/siem/events/<id>/` — event detail

Returns one Event in detail form (§3.2), including `features` and `raw`.
**404** if the id doesn't exist.

### 4.3 `GET /api/siem/events/stats/` — aggregate counts

Applies the **same filters** as the list, then aggregates. Ideal for dashboard
widgets.

```json
{
  "total": 128,
  "alerts": 17,
  "by_class": [{"class_name": "normal", "count": 90}, {"class_name": "xmrig", "count": 20}],
  "by_host":  [{"host": "rpi3-sensor-01", "count": 128}]
}
```

### 4.4 `GET /api/siem/status/` — status history

Board liveness transitions, newest first (paginated). Filters: time params
(§5.1, on `received_at`), `host`, `state`.

### 4.5 `GET /api/siem/nodes/` — boards

All boards with liveness. Filter: `state`. Sortable by `last_seen`, `first_seen`,
`host`.

### 4.6 `GET /api/siem/nodes/<host>/` — one board

Single Node by hostname. **404** if unknown.

### 4.7 `GET /api/siem/quarantine/` — failed messages

Messages that couldn't be ingested (bad JSON, missing fields, unknown topic).
Filters: time params (on `received_at`), `leaf`. Fields: `id`, `received_at`,
`topic`, `host`, `leaf`, `reason`, `payload`.

---

## 5. Filter reference

Combine any of these as query-string params. Invalid values → **400** with a
per-field message (see §6).

### 5.1 Time (all list endpoints)

Applies to `event_time` on events, `received_at` on status/quarantine.

| Param | Example | Meaning |
|-------|---------|---------|
| `since` | `2026-07-31T14:00:00Z` | time ≥ value |
| `until` | `2026-07-31T15:00:00Z` | time ≤ value |
| `minutes` | `30` | last 30 minutes |
| `hours` | `24` | last 24 hours |
| `days` | `7` | last 7 days |
| `date` | `2026-07-31` | one UTC calendar day |

### 5.2 Events — type & attributes

| Param | Example | Meaning |
|-------|---------|---------|
| `type` | `xmrig` / `alert` / `event` | class name, confirmed alerts only, or all |
| `class_name` | `ransomware` | exact class name |
| `class` | `8` | class index 0–8 |
| `alert` | `true` / `false` | confirmed or not (`true/1/yes/on`, `false/0/no/off`) |
| `host` | `rpi3-sensor-01,bbb-sensor-02` | one or more boards (comma-separated) |
| `source_topic` | `alerts` | seen on this topic |
| `min_confidence` | `0.9` | confidence ≥ value |

### 5.3 Status / nodes / quarantine

`status`: `host`, `state`. `nodes`: `state`. `quarantine`: `leaf`.

### 5.4 Sorting & pagination

| Param | Example | Notes |
|-------|---------|-------|
| `ordering` | `-event_time` | events: `event_time`, `received_at`, `confidence`; prefix `-` for desc |
| `page` | `2` | 1-based |
| `page_size` | `100` | default 50, max 500 |

---

## 6. Errors

| Status | Body | Cause |
|--------|------|-------|
| 400 | `{"<field>": ["<message>"]}` | invalid filter value (e.g. `{"hours": ["must be a number, got 'abc'"]}`) |
| 401 | — | missing/invalid token |
| 404 | `{"detail": "Not found."}` | unknown event id / node host |

Always check the status before parsing; on 400 the body names the offending
param, which you can surface next to the corresponding filter control.

---

## 7. Qt/QML integration

### 7.1 Building filtered requests

Use `QUrlQuery` so params are encoded correctly; map UI controls straight onto
the query keys.

```cpp
QUrl SiemClient::eventsUrl(const QVariantMap &filters, int page) const {
    QUrl url(m_baseUrl + "/api/siem/events/");
    QUrlQuery q;
    for (auto it = filters.constBegin(); it != filters.constEnd(); ++it)
        if (!it.value().toString().isEmpty())
            q.addQueryItem(it.key(), it.value().toString());
    if (page > 1) q.addQueryItem("page", QString::number(page));
    url.setQuery(q);
    return url;
}

QNetworkRequest SiemClient::request(const QUrl &url) const {
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Token " + m_token.toUtf8());
    return req;
}
```

Filter/control → param mapping:

| UI control | Param(s) |
|------------|----------|
| Date picker (single day) | `date` |
| From/To datetime pickers | `since`, `until` |
| Quick range buttons | `hours=1`, `hours=24`, `days=7` |
| Type dropdown | `type` (class name, or `alert`) |
| Host selector | `host` |
| Confidence slider | `min_confidence` |
| Column header click | `ordering` |

### 7.2 Events table via QAbstractListModel

The idiomatic pattern: a C++ model exposing roles, bound to a QML `TableView`/
`ListView`. Parse the paginated envelope, append `results`, keep `next` for
lazy loading.

```cpp
void EventsModel::fetch(const QUrl &url, bool append) {
    QNetworkReply *reply = m_nam->get(m_client->request(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, append]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(QJsonDocument::fromJson(reply->readAll())
                                   .toJson());   // 400/401 body
            reply->deleteLater();
            return;
        }
        const QJsonObject env = QJsonDocument::fromJson(reply->readAll()).object();
        m_nextUrl = env.value("next").toString();            // "" when last page
        const QJsonArray rows = env.value("results").toArray();

        if (!append) { beginResetModel(); m_events.clear(); endResetModel(); }
        beginInsertRows(QModelIndex(), m_events.size(),
                        m_events.size() + rows.size() - 1);
        for (const auto &v : rows) m_events.append(v.toObject());
        endInsertRows();
        emit countChanged(env.value("count").toInt());
        reply->deleteLater();
    });
}

// Apply a new filter set: reset to page 1.
void EventsModel::setFilters(const QVariantMap &f) {
    m_filters = f;
    fetch(m_client->eventsUrl(f, 1), /*append=*/false);
}

// Infinite scroll: called when the view nears the bottom.
void EventsModel::loadMore() {
    if (!m_nextUrl.isEmpty()) fetch(QUrl(m_nextUrl), /*append=*/true);
}
```

Expose roles (`host`, `eventTime`, `className`, `confidence`, `alert`) via
`roleNames()` and bind them in the QML delegate. Highlight rows where
`alert === true`.

### 7.3 Near-real-time updates (polling)

There is no push feed yet; poll for new rows using `since` = the newest
`received_at` you've seen:

```cpp
// every N seconds:
QVariantMap f = m_filters;
f["since"] = m_lastReceivedAt;   // ISO-8601 of most recent row seen
f["ordering"] = "event_time";
fetch(m_client->eventsUrl(f, 1), /*append=*/true);
```

Poll `events/?type=alert&since=…` on a shorter interval for an alerts banner.
(If you want true push, the backend can add an SSE/WebSocket alert feed later —
ask the backend team.)

### 7.4 Dashboard widgets

Drive charts from `events/stats/` with the current filter: `by_class` → a bar
chart of detections per class, `by_host` → per-board counts, `alerts`/`total` →
headline tiles. Refresh it whenever the filter set changes.

### 7.5 Gotchas

- **Always send the auth header** — 401 otherwise.
- **Follow `next`, don't compute page URLs by hand** — it already carries the
  active filters.
- **Reset to page 1 when filters change**; only use `append=true` for `loadMore`
  / polling.
- `features`/`raw` are **only** on the detail endpoint — don't expect them in the
  list.
- Timestamps are UTC; convert to local time for display but send UTC in filters.

---

## 8. Quick reference

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/siem/events/` | list + filter detection events |
| GET | `/api/siem/events/<id>/` | event detail (features + raw) |
| GET | `/api/siem/events/stats/` | aggregate counts for a filter |
| GET | `/api/siem/status/` | board status history |
| GET | `/api/siem/nodes/` | boards + liveness |
| GET | `/api/siem/nodes/<host>/` | one board |
| GET | `/api/siem/quarantine/` | failed-ingest messages |

All requests: `Authorization: Token <token>`. Filters: time
(`since/until/minutes/hours/days/date`), type (`type/class_name/class/alert/host/
source_topic/min_confidence`), `ordering`, `page`, `page_size`.
