# SIEM read API — desktop console

The console now has **two sources of the same events**: the MQTT broker for
what is happening now, and the `siem_api` database for what has already
happened. This is how they fit together, and where they deliberately do not.

The wire contract is `SIEM_API_DOCUMENTATION.md` in this directory.

## 1. Two sources, one queue

```
boards ──MQTT──► broker ─────────────► console          live, from now on
                             │
backend DB ──GET /api/siem/──┘                          history, at launch
```

Both feed **one** table — the Detections triage queue — and it de-duplicates.
Everything else stays single-sourced:

| Surface | Fed by | Why |
|---|---|---|
| **Detections** (triage queue) | database at launch, **then** MQTT live | An alerts queue is about outstanding work, and work does not stop existing because the console was restarted. |
| **Events** (filter window) | database only, on demand | It is a query tool. Its row counts mean "matching rows in the database". |
| Nodes / fleet liveness | MQTT only | Liveness is a *now* question. Retained status + Last Will already answer it the instant the broker connects. |
| Overview event stream | MQTT only | Explicitly a live feed. |
| Class distribution, "last event N s ago", throughput | MQTT only | Replaying a week of history into these would make them lie about the running system. |

That split is the single most important design decision here, and it is why
the sidebar can say "2 classes · 2 windows" while the Detections badge says
201. The first is what this console has *watched*; the second is what the
fleet has *found*.

## 2. Startup backfill

```
AppController ctor
  └─ backend->load()               base URL + token from QSettings
  └─ telemetry->loadSettings()     → MQTT connect + subscribe
  └─ telemetry->backfillFrom(siem)
        └─ SiemClient::backfillAlerts()
              ├─ probe   GET events/?type=alert&page_size=1     → count
              ├─ pages   GET events/?type=alert&ordering=event_time&page_size=500
              └─ sink    TelemetryClient::ingestSiemPage() → DetectionsModel::addBatch()
```

Four decisions worth knowing:

**`type=alert`, which is the *backend's* definition.** That flag is the board
debouncer's confirmed detection (§3.1). The live path uses a different rule —
the console's own `SeverityPolicy` over the class name. They usually agree;
where they don't, the Events screen shows everything and the Detections queue
shows confirmed alerts plus whatever MQTT classed as non-normal. Set the
"normal classes" list on Model & Rules to match the board if you want them
identical.

**Ascending order.** `DetectionsModel` prepends, so feeding it oldest-first is
what leaves the newest row at the top.

**A count probe before paging.** Without it, a history longer than the console
retains gets walked from the oldest end and stops at the cap — throwing away
exactly the newest alerts, the ones an operator opens the console to see. The
probe learns `count` first so the walk starts at the right offset and keeps
the newest N. When that happens the state is `truncated` and the Integrations
screen says so; older rows are still reachable from the Events screen.

**Whole pages at a time.** `addBatch()` inserts a page under one model reset.
The per-row path does one reset each, which is fine at telemetry rates and
quadratic at backfill rates.

### De-duplication

While the console is starting, the same alert can arrive twice: once from the
database, once live from the broker. The key is `host|event_time|class_idx` —
the only fields both sources carry. A repeat is dropped.

The key is forgotten when its row is trimmed past the retention cap, so the
set cannot grow without bound on a long-running console.

Verified: replaying a row the backfill had already loaded, plus one genuinely
new alert, over MQTT against a console holding 200 backfilled alerts →
**201**, not 202.

### uids tell you where a row came from

| Form | Source | Meaning |
|---|---|---|
| `IW-DB-42` | database | Row 42; look it up with `GET /api/siem/events/42/`. |
| `IW-20260822-0002` | live MQTT | Console-local. The wire assigns no event id, so this has no server-side identity. |

### Backfilled rows have no feature vector

The list endpoint does not carry one (§3.1, §7.5) — it is only on
`events/<id>/`. Backfilled detections therefore show `FEATURES 0`, which is
the honest state rather than a fabricated vector. The full vector is one click
away on the **Events** screen, which does fetch the detail endpoint.

## 3. The Events screen

The Detections table design, over the database, with a filter bar. Every
control maps onto exactly one query param (§5):

| Control | Param |
|---|---|
| Range: 1 h / 24 h / 7 d / All time | `hours=1`, `hours=24`, `days=7`, none |
| Type: All / Alerts only | `type=alert` |
| Board | `host` (comma-separated) |
| Class name | `class_name` |
| Min confidence | `min_confidence` |
| Since / Until | `since`, `until` |
| Day | `date` |
| Source topic | `source_topic` |
| TIME / CONFIDENCE column headers | `ordering`, `-` toggling |

**Filtering happens on the backend.** Nothing is re-filtered client-side,
which is what lets the header count mean "matching rows in the database"
rather than "rows that happen to be loaded". A quick range and an explicit
since/until are mutually exclusive — the API would otherwise apply both and
silently intersect them.

Scrolling near the bottom follows the envelope's `next` (§7.5: follow it,
don't compute page URLs — it already carries the active filters). The
`MATCHING` row comes from `events/stats/` with the same filter.

Clicking a row fetches `events/<id>/` for `features` and the original MQTT
`raw` payload. `features` can legitimately be `null` if the producer disabled
them (§3.2); that is reported as a fact about the row, not a fetch failure.

A 400 names the offending param (§6), and the message is shown as
`min_confidence: must be a number, got 'notanumber'` — the field that has to
change, not a raw body.

The screen is read-only, because `/api/siem/` is. Acknowledgement and triage
state live in the console and are **not** persisted to the backend.

## 4. Configure it

**Integrations → Backend**. One base URL and one token serve both
`/api/siem/` and `/api/attack/`: both documents specify the same host form and
the same `Authorization: Token …` scheme, and configuring them separately
would only let the two copies drift. `BackendSettings` is the single place
that would grow a second pair of fields if a deployment ever split them.

Existing installs that had `assistant/baseUrl` and `assistant/token` are
migrated to `backend/*` on first launch.

The same panel reports the startup backfill: `loading` → `done`, or
`truncated` / `error` / `skipped`, with a **Reload history** button for the
last two. `skipped` is not a failure — a console pointed only at a broker is a
valid deployment; it just starts with an empty queue.

## 5. Verify

Without the GUI, using the real `SiemClient`:

```bash
./build/iw-chat-probe --url http://10.0.0.5:8000 --token <tok> --siem
```

It runs a filtered list query, `stats/`, a detail fetch, a deliberately
invalid filter, and the startup backfill, and reports what each returned —
including the backfill's oldest and newest timestamps, which is how you check
truncation kept the right end.

Without a backend at all:

```bash
python3 tools/iw_siem_stub.py --port 8000 --token devtoken
python3 tools/iw_siem_stub.py --events 6000 --alert-rate 1.0   # force truncation
```

## 6. Failure modes worth knowing

**Detections is empty after a restart, and the log says `backfill: no backend
configured`.** Expected without a base URL. MQTT alone cannot show you
anything from before it connected.

**`401 Unauthorized`.** Missing or rejected token. Note the same token gates
the assistant, so if one is 401 the other will be too.

**The backfill says `truncated`.** More confirmed alerts are stored than the
console retains (5000). It loaded the newest 5000; use the Events screen for
older ones.

**The Events count and the Detections count disagree.** They are measuring
different things: Events counts rows matching a filter in the database,
Detections counts what is in the triage queue — confirmed alerts from history
plus anything MQTT has classed non-normal since launch, capped at 5000.

**A board appears in Events but not in the fleet list.** The fleet comes from
the broker's retained status topic. A board that has not published status
since the broker last restarted is in the database but not on the fleet list.
`GET /api/siem/nodes/` would fix this; it is not wired up (see below).

## 7. If you extend it

- **`GET /api/siem/nodes/` and `/status/`** (§4.5, §4.4) are implemented in
  the stub and reachable, but not wired into the Nodes screen. Doing so would
  mean deciding what "online" means when the database and the retained MQTT
  status disagree — a real question, not a plumbing one, and the retained
  topic is the more current answer.
- **`GET /api/siem/quarantine/`** (§4.7) lists messages the backend could not
  ingest. That is the server-side twin of the console's own `UNREADABLE`
  counter and would make a good addition to Integrations.
- **Polling** (§7.3) is deliberately *not* implemented. The doc offers it
  because a browser client has no other option; this console has MQTT, which
  is push, lower-latency and already connected. Adding a poll on top would
  duplicate every live row and make the de-duplication load-bearing rather
  than a startup-window safety net.
- **Token storage**: `QSettings`, in the clear, same as the broker password.
  One token now unlocks both the event history and the LLM — worth moving to
  the platform keychain before this leaves a lab.
