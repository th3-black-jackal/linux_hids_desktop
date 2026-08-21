# MQTT integration — desktop console

The console is **already wired to MQTT**; there is no code to write. This
is the runbook for building it, pointing it at a broker, and confirming
data is flowing.

## 1. What is already in place

```
main.cpp
  └─ AppController                      ctor
       └─ TelemetryClient               ctor  → creates MqttClient
            └─ loadSettings()                 → QSettings, then reconnect()
                 └─ reconnect()               → MqttClient::start(opts)
                                              → subscribe ×3
```

```
MqttClient::start()
    new QMqttClient → signals → [QSslConfiguration] → connectToHost[Encrypted]
                                                              │
  Qt event loop (no background thread) ───────────────────────┘
    QMqttClient::messageReceived(payload, topic)
      └─ MqttClient::messageReceived
      └─ TelemetryClient::onMessage        splits ironwatch/<host>/<leaf>
           ├─ handleStatus  → NodesModel::ensureNode → Node::applyStatus
           └─ handleEvent   → DetectionEvent::parse
                              ├─ Node::applyEvent
                              ├─ ClassStatsModel::record
                              ├─ EventStreamModel::pushEvent
                              └─ DetectionsModel::addDetection   (non-normal only)
```

QML binds to `App.telemetry.*` and to the models. Nothing in QML knows MQTT
exists.

QtMqtt runs entirely on the Qt event loop, so there is no threading rule to
observe — signals arrive on the object's own thread and models can be
touched directly. (The previous libmosquitto implementation needed every
callback marshalled with `QMetaObject::invokeMethod`; that machinery is
gone.)

**What QtMqtt does not do for you**, and `MqttClient` therefore implements:

- **Reconnect.** `QMqttClient` does not retry. `scheduleReconnect()` does,
  with exponential backoff 1 s → 30 s.
- **Re-subscribe.** `QMqttSubscription` objects are invalidated on
  disconnect; `applySubscriptions()` re-issues every filter on each
  successful connect.
- **Give up on hopeless errors.** `BadUsernameOrPassword`, `NotAuthorized`,
  `IdRejected` and `InvalidProtocolVersion` stop the retry loop — retrying
  a rejected credential forever only hides the real problem.

## 2. Build

### Getting QtMqtt

QtMqtt is an **add-on module**; a default Qt install does not include it.

*Official Qt installer* (simplest): run the Qt Maintenance Tool, and under
your Qt version tick **Additional Libraries → Qt Mqtt**. Nothing else to do.

*From source*, against a distro Qt:

```bash
sudo apt install qt6-base-private-dev        # QtMqtt needs Qt private headers
git clone --depth 1 -b v6.4.2 https://github.com/qt/qtmqtt.git
cmake -S qtmqtt -B qtmqtt/build \
      -DCMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake \
      -DCMAKE_INSTALL_PREFIX=/opt/qtmqtt
cmake --build qtmqtt/build -j$(nproc) && cmake --install qtmqtt/build
```

Match the branch tag to your Qt version. The private-headers package is the
non-obvious requirement — without it CMake fails with
`Imported target "Qt::CorePrivate" includes non-existent path`.

### Linux

```bash
cmake -S app -B build -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build
```

If QtMqtt was installed to a separate prefix, point CMake at it:

```bash
cmake -S app -B build -DCMAKE_PREFIX_PATH=/path/to/Qt6 \
      -DQt6Mqtt_DIR=/opt/qtmqtt/lib/x86_64-linux-gnu/cmake/Qt6Mqtt
```

### Windows (your deployment target)

Install Qt Mqtt via the Maintenance Tool (Additional Libraries → Qt Mqtt),
then build normally — no third-party dependency and no DLL to copy by hand:

```powershell
cmake -S app -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.7.0/msvc2019_64
cmake --build build --config Release
```

For a distributable build run `windeployqt`, which pulls in `Qt6Mqtt.dll`
along with the rest of the Qt runtime and the QML plugins:

```powershell
C:\Qt\6.7.0\msvc2019_64\bin\windeployqt.exe --qmldir app\qml build\Release\IronwatchConsole.exe
```

## 3. Point it at a broker

All runtime configuration is on the **Integrations** screen — there is no
config file to edit:

| Field | LAN | Cloud |
|---|---|---|
| Broker address | broker's LAN IP | `mqtt.yourdomain` |
| Port | `1883` | `8884` |
| Username | `ironwatch-console` | `ironwatch-console` |
| Password | from `setup-lan.sh` | as provisioned |
| CA certificate | *(empty)* | path to `ca.crt` |

Setting the CA path is what switches TLS on; leaving it empty means
plaintext. Settings persist in `QSettings`, so this is a one-time step.

Then press **Reconnect**. The badge next to "MQTT broker" shows
`connecting` → `connected`, and boards appear in "Discovered boards" on
their own.

> **Boards are never configured here.** They announce themselves on the
> retained `ironwatch/<host>/status` topic. If a board is missing, the
> problem is on the board or in the broker ACL, not in the console.

## 4. Verify

Confirm the broker is reachable and the console's credentials work,
independently of the app:

```bash
mosquitto_sub -h <broker> -p 1883 -u ironwatch-console -P <pw> -t 'ironwatch/#' -v
```

- **`Connection Refused: not authorised`** → wrong credentials, or the
  broker has `allow_anonymous false` and no matching password entry.
- **Connects but silent** → no board is publishing, or the console user
  lacks a `topic read` grant in the ACL.
- **Messages appear here but not in the console** → a console-side problem;
  check the Nodes screen's connection log.

Inside the app, the signals that it is working:

- title bar: `N / M BOARDS ONLINE`
- status bar: `Broker: connected · boards N/M`
- Integrations → MESSAGES climbing, UNREADABLE at 0

`UNREADABLE` counts payloads that arrived but could not be parsed — almost
always a JSON key mismatch (see the `to_json()` caveat in `README.md`). The
Nodes screen connection log names the specific field.

## 5. Testing without hardware

**Python driver** (`tools/iw_test_publisher.py`) — no build, easy to edit:

```bash
pip install paho-mqtt

# continuous telemetry
python3 tools/iw_test_publisher.py stream --host <broker> \
        --board rpi4-01:pw --board bbb-02:pw2

# 13 edge cases, each printing what the console should do
python3 tools/iw_test_publisher.py scenario --board rpi4-01:pw

# is the fault the console, or the broker/ACL?
python3 tools/iw_test_publisher.py verify --console ironwatch-console:pw
```

Start with `verify` when something is wrong — it splits "broker/ACL
problem" from "console problem" in one command. See `tools/README.md`.

**C++ harness** (`iw-fakeboard-mqtt`) — links the *real* board-side
publisher, so it exercises the code that will run on the Pi:

```bash
cmake -S board -B build-board && cmake --build build-board
./build-board/iw-fakeboard-mqtt --host <broker> --port 1883 \
    --user rpi4-01 --pass <pw> --board rpi4-01 --window-sec 2 --alert-every 5
```

For both, `--user` must equal the board name, or the broker ACL silently
discards everything (see below).

## 6. Failure modes worth knowing

**A board looks healthy but nothing arrives.** MQTT 3.1.1 has no way to
tell a publisher its message was refused — the broker returns `PUBACK` and
then discards. The usual cause is `mqtt_user != Config::host`, because the
ACL grants `ironwatch/<username>/#`. `MqttPublisher` warns at startup; the
broker does not log the denial at default log levels.

**Board shows `stale`.** The board's status still says online and the MQTT
keepalive is being answered, but no scored window has arrived for three of
its window periods. libmosquitto's network thread keeps answering pings
even if the inference loop has wedged, so this is the only way that
failure is visible.

**Board shows `offline — lost (last will)`.** The broker published the
board's Will because it stopped responding: crash, power loss, or dead
link. A clean `stop()` instead shows `reason: shutdown`. On a socket close
(process killed) the Will fires within seconds; on true power loss it
waits out the keepalive, ~45 s with the default 30 s setting.

**Console reconnects but sees nothing until the next window.** Expected.
The console uses `cleanSession=true` — it wants live state, not a replay.
Board liveness still arrives immediately because the status topic is
retained.

**Console stops retrying after a credential error.** Deliberate.
`BadUsernameOrPassword` / `NotAuthorized` / `IdRejected` will not fix
themselves, so the loop stops and the error stays on the Integrations
screen. Fix the settings and press **Reconnect**.

**`transport failed (host unreachable, or TLS rejected)`.** QtMqtt reports
an unreachable broker and a rejected TLS handshake as the same
`TransportInvalid` error, so if the address is definitely right, suspect
the CA path or a hostname that does not match the server certificate's
SAN.

## 7. If you extend it

- **New topic**: subscribe in `TelemetryClient::reconnect()`, then branch
  on the leaf in `onMessage()`. Subscriptions are re-issued automatically
  on every reconnect.
- **Publishing from the console** (a command channel): the ACL currently
  grants the console read-only, deliberately. It would need a `topic write`
  grant, a subscriber on the board, and — importantly — a think about
  authentication, since anything that can command a board is a much more
  valuable credential than one that can only watch.
- **Broker password storage**: `QSettings` keeps it in the clear (registry
  on Windows). Before this is anything but a lab tool, move it to the
  platform keychain or issue the console a client certificate and drop the
  password entirely.
