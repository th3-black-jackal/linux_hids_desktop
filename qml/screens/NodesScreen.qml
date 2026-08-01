import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

Item {
    id: root
    readonly property var node: App.selectedNode

    readonly property var confidenceSeries: {
        var pts = []
        var hist = App.selectedNodeConfidence
        if (!hist || hist.length < 2) return pts
        for (var i = 0; i < hist.length; i++) {
            pts.push({
                x: (i / (hist.length - 1)) * 600,
                y: 150 - (Math.max(0, Math.min(1, hist[i])) * 140) - 5
            })
        }
        return pts
    }

    EmptyState {
        anchors.fill: parent
        anchors.margins: 40
        visible: !root.node
        title: "No board selected"
        detail: "Boards appear automatically once they publish to the broker."
        actionText: "Open Integrations"
        onActionClicked: App.goIntegrations()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        visible: root.node

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
                    SectionLabel { text: "Target board" }
                    Text { text: root.node ? root.node.id : ""; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 20 }
                }
                Text {
                    text: root.node ? ("via broker " + App.telemetry.brokerEndpoint) : ""
                    color: Theme.textMuted
                    font.family: "ui-monospace, monospace"
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignBottom
                }
                Item { Layout.fillWidth: true }
                AppButton { text: "Reconnect broker"; onClicked: App.telemetry.reconnect() }
            }

            // The three service components from the brief. Only the telemetry
            // uplink is directly observable from here; the inference and Wazuh
            // logger states are inferred from the fact that scored windows are
            // arriving at all, and labelled as such.
            Grid {
                width: parent.width - 44
                columns: 3
                columnSpacing: 16
                rowSpacing: 16

                StatCard {
                    width: (parent.width - 32) / 3
                    kicker: "Telemetry uplink"
                    value: root.node ? root.node.state : "—"
                    valuePixelSize: 26
                    meta: root.node
                        ? (root.node.windows + " windows · " + root.node.alerts + " alerts · "
                           + (root.node.statusReason === "lwt" ? "lost (last will)" : "via broker"))
                        : ""
                }
                StatCard {
                    width: (parent.width - 32) / 3
                    kicker: "ironwatchd — inference"
                    value: root.node && root.node.windows > 0 ? "scoring" : "no data"
                    valuePixelSize: 26
                    meta: root.node && root.node.windows > 0
                        ? ("window " + root.node.windowPeriod + " · " + root.node.featureCount + " features · inferred from stream")
                        : "no scored windows received"
                }
                StatCard {
                    width: (parent.width - 32) / 3
                    kicker: "Wazuh logger"
                    value: "not visible"
                    valuePixelSize: 26
                    meta: "board writes wazuh_log_path for a local agent to tail; not sent over telemetry"
                }
            }

            Row {
                width: parent.width - 44
                spacing: 18

                BlueprintFrame {
                    width: (parent.width - 18) * 0.583
                    height: 220

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Row {
                            spacing: 10
                            Text { id: envTitle; text: "Confidence history"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                            Text {
                                text: "last " + (App.selectedNodeConfidence ? App.selectedNodeConfidence.length : 0) + " windows"
                                color: Theme.textMuted; font.pixelSize: 11
                                anchors.verticalCenter: envTitle.verticalCenter
                            }
                        }

                        Item {
                            width: parent.width
                            height: 150

                            LineChart {
                                anchors.fill: parent
                                visible: root.confidenceSeries.length > 1
                                thresholdPixelY: 150 - (App.telemetry.mediumAt * 140) - 5
                                thresholdColor: Theme.accent700
                                series: [{ points: root.confidenceSeries, color: Theme.accent, lineWidth: 1.6 }]
                            }

                            EmptyState {
                                anchors.fill: parent
                                visible: root.confidenceSeries.length <= 1
                                title: "No windows yet"
                                detail: "CPU, memory and temperature are not part of the telemetry protocol; "
                                      + "this charts model confidence instead."
                            }
                        }

                        Row {
                            spacing: 16
                            Row { spacing: 6
                                Rectangle { width: 14; height: 2; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "confidence"; color: Theme.text; font.pixelSize: 11 } }
                            Row { spacing: 6
                                Rectangle { width: 14; height: 2; color: Theme.accent700; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "alert threshold"; color: Theme.text; font.pixelSize: 11 } }
                        }
                    }
                }

                BlueprintFrame {
                    width: (parent.width - 18) * 0.417
                    height: 220

                    Column {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text { text: "Connection log"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }

                        Item {
                            width: parent.width
                            height: parent.height - 30

                            ListView {
                                id: logView
                                anchors.fill: parent
                                clip: true
                                model: App.serviceLog
                                delegate: Row {
                                    id: logRow
                                    required property string t
                                    required property string m
                                    width: ListView.view.width
                                    spacing: 10
                                    Text { text: logRow.t; width: 58; color: Theme.textMuted; font.family: "ui-monospace, monospace"; font.pixelSize: 12 }
                                    Text { text: logRow.m; width: Math.max(40, logRow.width - 68); color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
                                }
                            }

                            EmptyState {
                                anchors.fill: parent
                                visible: logView.count === 0
                                title: "No log entries"
                                detail: "This is the console's own connection log. The board's service log stays on the board."
                            }
                        }
                    }
                }
            }

            Text { text: "Fleet inventory"; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 16 }

            Column {
                width: parent.width - 44
                spacing: 0

                Row {
                    width: parent.width
                    height: 28
                    Text { text: "BOARD"; width: 150; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "DETAIL"; width: 170; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "LINK"; width: 110; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "LAST CLASS"; width: 90; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "WINDOW"; width: 80; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "WINDOWS"; width: 90; rightPadding: 12; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "ALERTS"; width: 80; rightPadding: 12; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11 }
                    Text { text: "LAST SEEN"; width: 110; color: Theme.textMuted; font.pixelSize: 11 }
                }
                Rectangle { width: parent.width; height: 1; color: Theme.divider }

                Repeater {
                    model: App.nodesModel
                    delegate: Rectangle {
                        id: invRow
                        required property int index
                        required property string id
                        required property string state
                        required property string statusReason
                        required property string lastClass
                        required property string windowPeriod
                        required property var windows
                        required property var alerts
                        required property string lastSeen

                        width: parent.width
                        height: 32
                        color: invMouse.containsMouse ? Theme.rowHover
                             : (invRow.index === App.selectedNodeIndex ? Theme.rowSelected : "transparent")

                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.cellBorder }

                        Text { x: 0; text: invRow.id; width: 150; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                        Text { x: 150; text: invRow.statusReason === "lwt" ? "lost (last will)" : invRow.statusReason; width: 170; color: Theme.textMuted; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                        TagBadge {
                            x: 320
                            text: invRow.state
                            bg: Theme.nodeStateColors(invRow.state).bg
                            fg: Theme.nodeStateColors(invRow.state).fg
                            borderColor: Theme.nodeStateColors(invRow.state).border
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text { x: 430; text: invRow.lastClass; width: 90; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                        Text { x: 520; text: invRow.windowPeriod; width: 80; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { x: 600; text: String(invRow.windows); width: 90; rightPadding: 12; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { x: 690; text: String(invRow.alerts); width: 80; rightPadding: 12; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        Text { x: 770; text: invRow.lastSeen; width: 110; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }

                        MouseArea {
                            id: invMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.selectNode(invRow.index)
                        }
                    }
                }
            }

            Text {
                width: parent.width - 44
                text: "Board model, kernel, service uptime, CPU, RSS and temperature are not carried by "
                    + "the telemetry protocol — the service broadcasts only scored windows. Adding them "
                    + "would mean extending DetectionEvent::to_json or adding a second frame type."
                color: Theme.textMutedFaint
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
