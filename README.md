# HIDS Console (Qt/QML)



## Transport: MQTT via Mosquitto

The console no longer dials boards directly. It holds **one** connection —
to the broker — and subscribes:

| Filter | QoS | Purpose |
|---|---|---|
| `hids/+/status` | 1 | fleet discovery (retained) + online/offline |
| `hids/+/alerts` | 1 | triage queue |
| `hids/+/events` | 0 | live stream |

Boards are **not configured in the console**. They announce themselves on
the retained status topic and appear on their own; the Integrations screen
configures only the broker. See `deploy/mosquitto/TOPICS.md`.

Board identity comes from the **topic**, not the payload's `host` field:
the broker ACL guarantees a board can only publish under its own hostname,
whereas the payload is just something the board typed. Trusting the topic
means a compromised board cannot attribute events to another one.

MQTT is via the **Qt MQTT module (QtMqtt)**. It runs on the Qt event loop
with no background network thread, so nothing needs marshalling between
threads. Two things it does *not* do, which `MqttClient` therefore
implements itself:

- **Reconnect.** `QMqttClient` goes to `Disconnected` and stays there. A
  retry loop with exponential backoff (1 s → 30 s) is ours; without it a
  single broker restart would silently leave the console dead.
- **Restore subscriptions.** `QMqttSubscription` objects are invalidated on
  disconnect, so the filter list is remembered and re-issued on every
  successful connect.

Credential failures (bad password, not authorised, ID rejected) stop the
retry loop instead of hammering the broker forever — they will never
resolve on their own, and the error stays visible on the Integrations
screen.

QtMqtt is an **add-on module**, not part of a default Qt install, and is
licensed **GPLv3 or commercial** — unlike LGPL Qt Core. Fine for an
internal or open-source tool; a closed-source binary needs the commercial
licence. The board side still uses libmosquitto, which is unaffected.

**Cadence**: one message per `Config::windowSec` per board — *every*
window, normal classifications included. The Overview stream shows
everything; the Detections queue holds only the non-normal subset.

**Payload fields** (from `InferenceEngine::predict`): `timestamp`
(ISO-8601), `classIdx`, `className`, `confidence`, `host`, and `features`
(the **normalized** vector, since `to_json` is called with
`include_features=true`). The payload is byte-identical to what the old
TCP path carried — only the transport changed.

> ⚠️ **Unconfirmed:** `detection_event.hpp` wasn't among the files provided,
> so the literal JSON key spellings `to_json()` emits are inferred from the
> C++ member names. `DetectionEvent::parse()` accepts those plus common
> snake_case/short aliases and logs anything it can't interpret rather than
> silently showing nothing. If your `to_json()` uses different names, add
> them to the alias lists in `src/core/DetectionEvent.cpp` — that's the only
> place they appear. Sending me `detection_event.hpp` would let this be
> tightened to exact keys.

## Verified end-to-end

Run against a live Mosquitto 2.0.18 with two simulated boards publishing
through it (see `board/README.md` for the harness, which links the real
board-side publisher):

- both boards discovered automatically from retained status, no console
  configuration;
- **broker killed mid-run and restarted**: the console retried at 2 s, 4 s
  and 8 s, reconnected, re-subscribed, and both boards returned to online
  from their retained status — the reconnect path QtMqtt does not provide;
- per-board window periods correctly inferred (1.0 s and 2.0 s, matching
  how the harnesses were launched);
- interleaved event stream, QoS-split routing, triage queue populated;
- a `SIGKILL`ed board surfaced as `offline — lost (last will)` in a
  **freshly launched** console that had never seen it alive, purely from
  the retained Will;
- all five screens, zero QML warnings.

Not exercised: real hardware, a real Wazuh deployment, TLS/mutual-TLS
against a cloud broker (the config is written but only the plaintext LAN
path was run), or a Windows build.

## Build

```
cmake -S app -B build -DCMAKE_PREFIX_PATH=<path-to-Qt6>
cmake --build build
```

Requires Qt 6.4+ **including the Qt MQTT module**. See
`MQTT_INTEGRATION.md` for how to obtain it, the per-platform build
(including Windows), broker configuration and troubleshooting.

## What the protocol does *not* carry

The design mocked up a richer console than this service can feed. Rather
than fabricate the difference, the affected panels either show a real
equivalent or say plainly that the data isn't available:

| Design element | Status |
|---|---|
| Per-node CPU / RSS / temperature | **Not sent.** Node cards show windows / alerts / last-confidence; the "resource envelope" chart is now confidence history. |
| Board model, kernel, IP, service uptime | **Not sent.** Inventory shows the board name, link state and observed window period instead. |
| Rule ID, process name, PID/UID, syscall trace | **Not sent.** The inspector shows class + confidence + the real feature vector. |
| Wazuh delivery status / "escalate to Wazuh" | **Not reachable.** The board writes `wazuh_log_path` for a local agent to tail; the console has no Wazuh connection, and the broker does not change this. The Integrations screen documents that topology instead of offering a fake connection test. |
| Model build/version, TPR/FPR, build history | **Not sent** (lives in `model_meta.hpp`). Replaced with the observed class distribution — what has actually come off the wire. |
| Deploy model to fleet, kill process, restart service | **No control channel exists.** Telemetry is one-way; these actions were removed rather than left as buttons that do nothing. |
| Anomaly score vs. 0.72 threshold | Reframed: the model is a **classifier**, so the console charts `confidence` and maps (class, confidence) → severity itself. |

Severity is a **console-side** mapping (`SeverityPolicy`), editable on the
Model screen. The board's own alerting (`alert_streak` / `min_confidence`,
applied by the `Debouncer` before writing to the Wazuh log) is independent
and not visible over telemetry — set the console's "medium" threshold to
match `min_confidence` if you want them to agree.

Feature names: `FEATURE_ORDER` isn't transmitted, so features display as
`f[0]`, `f[1]`… Point the Model screen's "feature names file" at a text
file with one name per line to label them.

## Layout

```
app/
  src/
    main.cpp                    — registers App + Theme singletons, loads Main.qml
    AppController.{h,cpp}       — root object graph; screen/theme/selection state
    ThemeProvider.{h,cpp}       — design tokens (C++ singleton, exposed as "Theme")
    core/
      DetectionEvent.{h,cpp}    — the wire event + defensive JSON parse + SeverityPolicy
      Node/NodesModel           — boards discovered from the broker
      Detection/DetectionsModel — triage queue over non-normal windows
      EventStreamModel          — unfiltered live feed
      ClassStatsModel           — observed class distribution
      LogModel                  — console-side connection log
    services/
      MqttClient.{h,cpp}        — QtMqtt wrapper + reconnect/re-subscribe logic
      TelemetryClient.{h,cpp}   — subscriptions, board registry, event fan-out
  qml/
    Main.qml, components/, screens/
```
