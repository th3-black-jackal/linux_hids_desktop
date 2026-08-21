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
                        Text { id: cTitle; text: "ATT&CK assistant"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Item { width: Math.max(0, parent.width - cTitle.implicitWidth - cTag.implicitWidth - 10); height: 1 }
                        TagBadge {
                            id: cTag
                            readonly property var c: Theme.nodeStateColors(
                                App.chat.configured ? "connected" : "offline")
                            text: App.chat.configured ? "configured" : "not configured"
                            bg: c.bg; fg: c.fg; borderColor: c.border
                            anchors.verticalCenter: cTitle.verticalCenter
                        }
                    }

                    Text {
                        width: parent.width
                        text: "A separate Django + Ollama service that answers questions about "
                            + "alerts, grounded in retrieved MITRE ATT&CK entries. It is not on "
                            + "the MQTT path — nothing from the boards reaches it unless an "
                            + "operator types it into the Assistant screen."
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
                            value: App.chat.baseUrl
                            onEdited: function(t) { App.chat.baseUrl = t }
                        }
                        FormField {
                            width: parent.width * 0.42 - 10
                            label: "API token (Authorization: Token …)"
                            isPassword: true
                            value: App.chat.token
                            onEdited: function(t) { App.chat.token = t }
                        }
                        FormField {
                            width: parent.width * 0.16 - 8
                            label: "Chunks (k)"
                            value: String(App.chat.retrievalK)
                            onEdited: function(t) {
                                var v = parseInt(t)
                                if (v > 0) App.chat.retrievalK = v
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
                                text: "POST /api/attack/chat/stream/ — Server-Sent Events. Needs the "
                                    + "backend behind an ASGI server (uvicorn/daphne); under WSGI the "
                                    + "whole answer arrives at the end anyway. Turn this off to use "
                                    + "the blocking /api/attack/chat/ endpoint instead."
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
                            if (!App.chat.configured)
                                return "No base URL set — the Assistant screen is inert until one is."
                            if (!App.chat.hasToken)
                                return "No API token set. The backend requires an authenticated user, "
                                     + "so requests will come back 401 unless it has been configured "
                                     + "to allow anonymous access."
                            if (!App.chat.secure)
                                return "Plain HTTP to " + App.chat.endpointText + ". The token is a "
                                     + "bearer credential for the whole backend and crosses the "
                                     + "network in the clear — acceptable on a trusted LAN only."
                            return "HTTPS to " + App.chat.endpointText + "."
                        }
                        color: App.chat.configured && App.chat.hasToken && App.chat.secure
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
