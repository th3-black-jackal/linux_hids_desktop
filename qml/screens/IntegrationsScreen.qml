import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

Item {
    id: root

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: root.width
            topPadding: 18
            bottomPadding: 24
            leftPadding: 22
            rightPadding: 22
            spacing: 16

            RowLayout {
                width: parent.width - 44
                spacing: 12

                Column {
                    SectionLabel { text: "Console configuration" }
                    Text { text: "Integrations"; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 20 }
                }
                Item { Layout.fillWidth: true }
                AppButton { text: "Reconnect"; variant: "primary"; onClicked: App.telemetry.reconnect() }
            }

            Row {
                width: parent.width - 44
                spacing: 20

                // ── Broker connection ────────────────────────────────────
                BlueprintFrame {
                    width: (parent.width - 20) / 2
                    height: brokerCol.implicitHeight + 34

                    Column {
                        id: brokerCol
                        anchors.fill: parent
                        anchors.margins: 17
                        spacing: 10

                        Row {
                            width: parent.width
                            spacing: 10
                            Text { id: bTitle; text: "MQTT broker"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                            Item { width: Math.max(0, parent.width - bTitle.implicitWidth - bTag.implicitWidth - 10); height: 1 }
                            TagBadge {
                                id: bTag
                                readonly property var c: Theme.nodeStateColors(
                                    App.telemetry.brokerConnected ? "connected" : "offline")
                                text: App.telemetry.brokerState
                                bg: c.bg; fg: c.fg; borderColor: c.border
                                anchors.verticalCenter: bTitle.verticalCenter
                            }
                        }

                        Text {
                            width: parent.width
                            text: "Both the boards and this console dial out to the broker. Boards are "
                                + "not configured here — they announce themselves on "
                                + "ironwatch/+/status and appear automatically."
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Column {
                            width: parent.width
                            spacing: 12

                            Row {
                                width: parent.width
                                spacing: 14
                                FormField {
                                    id: hostField
                                    width: parent.width * 0.62 - 7
                                    label: "Broker address"
                                    value: App.telemetry.brokerHost
                                    onEdited: function(t) { App.telemetry.brokerHost = t }
                                }
                                FormField {
                                    id: portField
                                    width: parent.width * 0.38 - 7
                                    label: "Port (1883 plain / 8884 TLS)"
                                    value: String(App.telemetry.brokerPort)
                                    onEdited: function(t) {
                                        var p = parseInt(t)
                                        if (p > 0) App.telemetry.brokerPort = p
                                    }
                                }
                            }

                            Row {
                                width: parent.width
                                spacing: 14
                                FormField {
                                    width: parent.width * 0.5 - 7
                                    label: "Username"
                                    value: App.telemetry.brokerUser
                                    onEdited: function(t) { App.telemetry.brokerUser = t }
                                }
                                FormField {
                                    width: parent.width * 0.5 - 7
                                    label: "Password"
                                    isPassword: true
                                    value: App.telemetry.brokerPassword
                                    onEdited: function(t) { App.telemetry.brokerPassword = t }
                                }
                            }

                            FormField {
                                width: parent.width
                                label: "CA certificate (leave empty for a plaintext LAN broker)"
                                value: App.telemetry.caFile
                                onEdited: function(t) { App.telemetry.caFile = t }
                            }
                        }

                        Rectangle { width: parent.width; height: 1; color: Theme.divider }

                        Text {
                            width: parent.width
                            text: App.telemetry.tlsEnabled
                                ? ("TLS enabled — verifying the broker against " + App.telemetry.caFile)
                                : "No TLS. Credentials and telemetry cross the network in the clear; "
                                  + "acceptable on a trusted LAN, not for a cloud broker."
                            color: App.telemetry.tlsEnabled ? Theme.textMuted : Theme.text
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            width: parent.width
                            visible: App.telemetry.brokerError.length > 0 && !App.telemetry.brokerConnected
                            text: "Broker error: " + App.telemetry.brokerError
                            color: Theme.text
                            font.family: "ui-monospace, monospace"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // ── Topics + Wazuh topology ──────────────────────────────
                BlueprintFrame {
                    width: (parent.width - 20) / 2
                    height: topicCol.implicitHeight + 34

                    Column {
                        id: topicCol
                        anchors.fill: parent
                        anchors.margins: 17
                        spacing: 10

                        Text { text: "Subscriptions"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }

                        Text {
                            width: parent.width
                            text: "    ironwatch/+/status   QoS 1   retained — fleet discovery\n"
                                + "    ironwatch/+/alerts   QoS 1            — triage queue\n"
                                + "    ironwatch/+/events   QoS 0            — live stream"
                            color: Theme.text
                            font.family: "ui-monospace, monospace"
                            font.pixelSize: 11
                        }

                        Text {
                            width: parent.width
                            text: "Each scored window is published once, to events or alerts depending "
                                + "on its class, so nothing is counted twice. Alerts get QoS 1 because "
                                + "losing one loses an intrusion; normal windows get QoS 0 because the "
                                + "next one is a second away."
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Rectangle { width: parent.width; height: 1; color: Theme.divider }

                        Text { text: "Wazuh forwarding"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }

                        Text {
                            width: parent.width
                            text: "Unchanged by the broker: alerts still reach the SIEM from the board "
                                + "itself, never through this console.\n\n"
                                + "    ironwatchd → WazuhLogger → wazuh_log_path → agent → manager"
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Rectangle { width: parent.width; height: 1; color: Theme.divider }

                        Row {
                            width: parent.width
                            spacing: 20
                            Column {
                                spacing: 2
                                Text { text: "MESSAGES"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                Text { text: String(App.telemetry.framesReceived); color: Theme.text; font.pixelSize: 15 }
                            }
                            Column {
                                spacing: 2
                                Text { text: "UNREADABLE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                Text { text: String(App.telemetry.framesDropped); color: Theme.text; font.pixelSize: 15 }
                            }
                            Column {
                                spacing: 2
                                Text { text: "BOARDS"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                Text { text: App.telemetry.onlineCount + " / " + App.telemetry.boardCount; color: Theme.text; font.pixelSize: 15 }
                            }
                            Column {
                                spacing: 2
                                Text { text: "LAST EVENT"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                Text { text: App.telemetry.lastEventAge; color: Theme.text; font.pixelSize: 15 }
                            }
                        }
                    }
                }
            }

            // ── ATT&CK assistant backend ─────────────────────────────────
            BlueprintFrame {
                width: parent.width - 44
                height: chatCol.implicitHeight + 34

                Column {
                    id: chatCol
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 10
                        Text { id: cTitle; text: "Backend (SIEM database + ATT&CK assistant)"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Item { width: Math.max(0, parent.width - cTitle.implicitWidth - cTag.implicitWidth - 10); height: 1 }
                        TagBadge {
                            id: cTag
                            readonly property var c: Theme.nodeStateColors(
                                App.backend.configured ? "connected" : "offline")
                            text: App.backend.configured ? "configured" : "not configured"
                            bg: c.bg; fg: c.fg; borderColor: c.border
                            anchors.verticalCenter: cTitle.verticalCenter
                        }
                    }

                    Text {
                        width: parent.width
                        text: "One Django host serving two APIs: /api/siem/ is the stored event "
                            + "history behind the Events screen and the alerts loaded at startup, "
                            + "and /v1/ is the OpenAI-compatible LLM assistant. They authenticate "
                            + "differently — the SIEM API with `Token`, the chat API with `Bearer` "
                            + "— so the key below is separate, though the backend also accepts the "
                            + "token for both. Neither is on the MQTT path; the broker stays the "
                            + "only real-time source."
                        color: Theme.textMuted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        width: parent.width
                        spacing: 14

                        FormField {
                            width: parent.width * 0.42 - 10
                            label: "Base URL (e.g. http://10.0.0.5:8000)"
                            value: App.backend.baseUrl
                            onEdited: function(t) { App.backend.baseUrl = t }
                        }
                        FormField {
                            width: parent.width * 0.29 - 10
                            label: "SIEM token (Token …)"
                            isPassword: true
                            value: App.backend.token
                            onEdited: function(t) { App.backend.token = t }
                        }
                        FormField {
                            width: parent.width * 0.29 - 10
                            label: "Chat API key (Bearer …)"
                            isPassword: true
                            value: App.backend.apiKey
                            onEdited: function(t) { App.backend.apiKey = t }
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 14

                        FormField {
                            width: 160
                            label: "Model name"
                            value: App.chat.model
                            onEdited: function(t) { if (t.length > 0) App.chat.model = t }
                        }
                        FormField {
                            width: 110
                            label: "Chunks (k)"
                            value: String(App.chat.retrievalK)
                            onEdited: function(t) {
                                var v = parseInt(t)
                                if (v > 0) App.chat.retrievalK = v
                            }
                        }

                        Column {
                            spacing: 5
                            Text { text: "CONNECTION"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                            Row {
                                spacing: 8
                                height: 34
                                AppButton {
                                    text: App.chat.connectionState === "testing" ? "Testing…" : "Test"
                                    enabled: App.chat.connectionState !== "testing" && App.backend.configured
                                    // GET /v1/models — separates "wrong URL"
                                    // from "wrong key" without spending a
                                    // generation to find out.
                                    onClicked: App.chat.testConnection()
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: App.chat.connectionDetail
                                    color: App.chat.connectionState === "error" ? Theme.text : Theme.textMuted
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: streamCol.implicitHeight

                        Rectangle {
                            id: streamBox
                            width: 14; height: 14
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 2
                            color: App.chat.streaming ? Theme.accent : "transparent"
                            border.width: 1
                            border.color: streamMouse.containsMouse ? Theme.accent : Theme.divider
                        }

                        Column {
                            id: streamCol
                            anchors.left: streamBox.right
                            anchors.leftMargin: 9
                            anchors.right: parent.right
                            spacing: 3

                            Text {
                                text: "Stream tokens as they are generated"
                                color: Theme.text
                                font.family: Theme.fontBody
                                font.pixelSize: 13
                            }
                            Text {
                                width: parent.width
                                text: "POST /v1/chat/completions with stream:true — OpenAI chunk "
                                    + "streaming. Needs the "
                                    + "backend behind an ASGI server (uvicorn/daphne); under WSGI the "
                                    + "whole answer arrives at the end anyway. Turn this off to use "
                                    + "the blocking form of the same endpoint instead."
                                color: Theme.textMuted
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }

                        MouseArea {
                            id: streamMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.chat.streaming = !App.chat.streaming
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Text {
                        width: parent.width
                        text: {
                            if (!App.backend.configured)
                                return "No base URL set. The Events screen and the Assistant are "
                                     + "both inert until one is, and the alerts table starts empty "
                                     + "on every launch instead of loading its history."
                            if (!App.backend.hasToken)
                                return "No SIEM token set. The Events screen and the startup alert "
                                     + "history will come back 401 unless the backend allows "
                                     + "anonymous access."
                            if (App.backend.apiKeyIsToken)
                                return "No separate chat API key set, so the SIEM token is sent as "
                                     + "the bearer key. The backend accepts that; set an "
                                     + "sk-attack-… key here if it has been issued one."
                            if (!App.backend.secure)
                                return "Plain HTTP to " + App.backend.baseUrl + ". The token is a "
                                     + "bearer credential for the whole backend and crosses the "
                                     + "network in the clear — acceptable on a trusted LAN only."
                            return "HTTPS to " + App.backend.baseUrl + "."
                        }
                        color: App.backend.configured && App.backend.hasToken && App.backend.secure
                            ? Theme.textMuted : Theme.text
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        width: parent.width
                        visible: App.chat.lastError.length > 0
                        text: "Last assistant error: " + App.chat.lastError
                        color: Theme.text
                        font.family: "ui-monospace, monospace"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Row {
                        width: parent.width
                        spacing: 10

                        Text {
                            id: bfLabel
                            text: "Startup alert history"
                            color: Theme.text
                            font.family: Theme.fontHeading
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        TagBadge {
                            readonly property var c: Theme.nodeStateColors({
                                "done": "connected", "loading": "connecting",
                                "truncated": "stale", "error": "offline",
                                "skipped": "offline"
                            }[App.siem.backfillState] || "offline")
                            text: App.siem.backfillState
                            bg: c.bg; fg: c.fg; borderColor: c.border
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        AppButton {
                            text: "Reload history"
                            visible: App.siem.backfillState === "error"
                                     || App.siem.backfillState === "skipped"
                            onClicked: App.siem.retryBackfill()
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        width: parent.width
                        text: {
                            switch (App.siem.backfillState) {
                            case "loading":
                                return "Loading confirmed alerts from /api/siem/events/?type=alert — "
                                     + App.siem.backfillLoaded + " so far."
                            case "done":
                                return "Loaded " + App.siem.backfillLoaded + " confirmed alert"
                                     + (App.siem.backfillLoaded === 1 ? "" : "s")
                                     + " into the Detections queue at launch. MQTT adds new ones on "
                                     + "top; anything both sources deliver is shown once."
                            case "truncated":
                                return "Stopped at " + App.siem.backfillLoaded + " of "
                                     + App.siem.backfillTotal + " alerts — that is the console's "
                                     + "retention limit. Use the Events screen to reach older ones."
                            case "error":
                                return "The history could not be loaded; see the Nodes connection log. "
                                     + "The Detections queue holds only what MQTT has delivered since "
                                     + "launch."
                            case "skipped":
                                return "No backend configured, so the Detections queue starts empty "
                                     + "and fills from MQTT only."
                            default:
                                return ""
                            }
                        }
                        color: Theme.textMuted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // ── External web app ─────────────────────────────────────────
            BlueprintFrame {
                width: parent.width - 44
                height: webCol.implicitHeight + 34

                Column {
                    id: webCol
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 10

                    Row {
                        width: parent.width
                        spacing: 10
                        Text { id: wTitle; text: "Open event in web app"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Item { width: Math.max(0, parent.width - wTitle.implicitWidth - wTag.implicitWidth - 10); height: 1 }
                        TagBadge {
                            id: wTag
                            readonly property var c: Theme.nodeStateColors(
                                App.webLink.enabled ? "connected" : "offline")
                            text: App.webLink.enabled ? "enabled" : "disabled"
                            bg: c.bg; fg: c.fg; borderColor: c.border
                            anchors.verticalCenter: wTitle.verticalCenter
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Double-click a row on Detections or Events — or use the inspector "
                            + "button — to open that event in a browser. Plain selection does not "
                            + "open anything: selection also happens from the keyboard and when "
                            + "new alerts arrive, and a browser launching on every row change "
                            + "would make the table unusable."
                        color: Theme.textMuted
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    FormField {
                        width: parent.width
                        label: "URL template — clear this to disable the feature"
                        value: App.webLink.urlTemplate
                        onEdited: function(t) { App.webLink.urlTemplate = t }
                    }

                    Text {
                        width: parent.width
                        text: "Placeholders: {id} {uid} {host} {class} {classIdx} {confidence} "
                            + "{severity} {time}. Values are percent-encoded, and a query "
                            + "parameter whose placeholder resolves to nothing is dropped — so "
                            + "one template serves both a database row and a live MQTT detection, "
                            + "which has no server-side id."
                        color: Theme.textMuted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Text {
                        width: parent.width
                        text: "The web app's real deep-link format is not documented here, so the "
                            + "default passes the event in the query string: an unrecognised query "
                            + "parameter is ignored by a single-page app, where an unrecognised "
                            + "path is a 404. Change the template above once the format is known."
                        color: Theme.text
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }

                    Row {
                        width: parent.width
                        spacing: 10

                        AppButton {
                            text: "Restore default"
                            onClicked: App.webLink.resetTemplate()
                        }
                        Text {
                            width: parent.width - 140
                            text: App.webLink.lastError.length > 0
                                ? App.webLink.lastError
                                : (App.webLink.lastUrl.length > 0
                                    ? ("Last opened: " + App.webLink.lastUrl) : "")
                            color: App.webLink.lastError.length > 0 ? Theme.text : Theme.textMuted
                            font.family: "ui-monospace, monospace"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ── Discovered fleet ─────────────────────────────────────────
            BlueprintFrame {
                width: parent.width - 44
                height: fleetCol.implicitHeight + 34

                Column {
                    id: fleetCol
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 10

                    Text { text: "Discovered boards"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }

                    Row {
                        width: parent.width
                        Text { text: "BOARD"; width: 200; color: Theme.textMuted; font.pixelSize: 11 }
                        Text { text: "STATE"; width: 120; color: Theme.textMuted; font.pixelSize: 11 }
                        Text { text: "DETAIL"; width: Math.max(80, parent.width - 200 - 120 - 140); color: Theme.textMuted; font.pixelSize: 11 }
                        Text { text: "LAST SEEN"; width: 140; color: Theme.textMuted; font.pixelSize: 11 }
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Repeater {
                        model: App.nodesModel
                        delegate: Rectangle {
                            id: epRow
                            required property int index
                            required property string id
                            required property string state
                            required property string statusReason
                            required property string lastSeen

                            width: parent.width
                            height: 30
                            color: epMouse.containsMouse ? Theme.rowHover : "transparent"
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.cellBorder }

                            Text { x: 0; text: epRow.id; width: 200; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                            TagBadge {
                                x: 200
                                readonly property var c: Theme.nodeStateColors(epRow.state)
                                text: epRow.state
                                bg: c.bg; fg: c.fg; borderColor: c.border
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                x: 320
                                text: epRow.statusReason === "lwt"
                                    ? "lost — broker published last will"
                                    : (epRow.statusReason.length > 0 ? epRow.statusReason : "")
                                width: Math.max(80, epRow.width - 200 - 120 - 140)
                                color: Theme.textMuted
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text { x: epRow.width - 140; text: epRow.lastSeen; width: 140; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }

                            MouseArea {
                                id: epMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.selectNode(epRow.index)
                            }
                        }
                    }

                    EmptyState {
                        width: parent.width
                        visible: App.telemetry.boardCount === 0
                        title: App.telemetry.brokerConnected ? "No boards announced yet"
                                                             : "Not connected to the broker"
                        detail: App.telemetry.brokerConnected
                            ? "Boards appear here as soon as they publish their retained status."
                            : "Check the broker address and credentials above."
                    }
                }
            }
        }
    }
}
