import QtQuick
import QtQuick.Controls
import Ironwatch
import IronwatchApp

Item {
    id: root

    // Confidence trace of the selected board, newest last, mapped into
    // LineChart's 600x150 viewBox. Confidence is 0..1 so it maps directly —
    // no synthetic scaling. (The prototype's Series.js random walk is gone;
    // every point here is a real scored window.)
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

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: root.width
            topPadding: 20
            bottomPadding: 24
            leftPadding: 22
            rightPadding: 22
            spacing: 18

            // Header
            Row {
                width: parent.width - 44
                spacing: 14

                Column {
                    id: titleCol
                    anchors.verticalCenter: parent.verticalCenter
                    SectionLabel { text: "Live monitor" }
                    Text { text: "Fleet overview"; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 20 }
                }

                Item { width: Math.max(0, parent.width - titleCol.implicitWidth - headerRight.implicitWidth); height: 1 }

                Row {
                    id: headerRight
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    AppButton { text: "Reconnect broker"; anchors.verticalCenter: parent.verticalCenter; onClicked: App.telemetry.reconnect() }
                    AppButton { text: "Triage queue"; variant: "primary"; anchors.verticalCenter: parent.verticalCenter; onClicked: App.goDetections() }
                }
            }

            // KPI row
            Grid {
                width: parent.width - 44
                columns: 4
                columnSpacing: 16
                rowSpacing: 16

                StatCard {
                    width: (parent.width - 48) / 4
                    kicker: "Open detections"
                    value: String(App.detectionsModel.openTotal)
                    meta: App.detectionsModel.openCritical + " critical · " + App.detectionsModel.openHigh
                          + " high · " + App.detectionsModel.openMedium + " medium"
                }
                StatCard {
                    width: (parent.width - 48) / 4
                    kicker: "Boards online"
                    value: App.telemetry.onlineCount + " / " + App.telemetry.boardCount
                    meta: App.telemetry.boardCount === 0
                        ? "none announced yet"
                        : "last event " + App.telemetry.lastEventAge
                }
                StatCard {
                    width: (parent.width - 48) / 4
                    kicker: "Windows scored"
                    value: String(App.telemetry.framesReceived)
                    // The board sends one frame per Config::windowSec; the
                    // period is inferred from arrival times, never reported.
                    meta: "window ≈ " + App.telemetry.windowPeriodText + " · " + App.telemetry.throughputText
                }
                StatCard {
                    width: (parent.width - 48) / 4
                    kicker: "Frames dropped"
                    value: String(App.telemetry.framesDropped)
                    meta: App.telemetry.framesDropped > 0
                        ? "unparseable or desynchronized"
                        : "all frames decoded"
                }
            }

            // Two-column body
            Row {
                width: parent.width - 44
                spacing: 18

                Column {
                    width: (parent.width - 18) * 0.608
                    spacing: 18

                    BlueprintFrame {
                        width: parent.width
                        height: 200

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6

                            Row {
                                spacing: 10
                                Text { id: scoreTitle; text: "Confidence — scored windows"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                                Text {
                                    text: App.selectedNode
                                        ? (App.selectedNode.id + " · alert ≥ " + App.telemetry.mediumAt.toFixed(2))
                                        : "no board selected"
                                    color: Theme.textMuted; font.family: Theme.fontBody; font.pixelSize: 11
                                    anchors.verticalCenter: scoreTitle.verticalCenter
                                }
                            }

                            Item {
                                width: parent.width
                                height: 132

                                LineChart {
                                    anchors.fill: parent
                                    visible: root.confidenceSeries.length > 1
                                    // Alert line = the console-side "medium"
                                    // threshold, mapped into viewBox space.
                                    thresholdPixelY: 150 - (App.telemetry.mediumAt * 140) - 5
                                    thresholdColor: Theme.accent700
                                    series: [{ points: root.confidenceSeries, color: Theme.accent, lineWidth: 1.6 }]
                                }

                                EmptyState {
                                    anchors.fill: parent
                                    visible: root.confidenceSeries.length <= 1
                                    title: "No scored windows yet"
                                    detail: App.telemetry.brokerConnected
                                        ? "Waiting for a board to publish its first window."
                                        : "Not connected to the broker."
                                }
                            }

                            Row {
                                width: parent.width
                                visible: root.confidenceSeries.length > 1
                                Text { text: "oldest"; color: Theme.textMuted; font.pixelSize: 11; width: parent.width / 2 }
                                Text { text: "now"; color: Theme.textMuted; font.pixelSize: 11; width: parent.width / 2; horizontalAlignment: Text.AlignRight }
                            }
                        }
                    }

                    BlueprintFrame {
                        width: parent.width
                        height: 300

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6

                            Row {
                                id: streamHeader
                                width: parent.width
                                spacing: 10
                                Text { id: streamTitle; text: "Event stream"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                                Text { id: streamSub; text: "every scored window, normal included"; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: streamTitle.verticalCenter }
                                Item { width: Math.max(0, streamHeader.width - streamTitle.implicitWidth - streamSub.implicitWidth - streamRate.implicitWidth - 20); height: 1 }
                                Text { id: streamRate; text: App.telemetry.throughputText; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: streamTitle.verticalCenter }
                            }

                            Row {
                                width: parent.width
                                Text { text: "TIME"; width: 84; color: Theme.textMuted; font.pixelSize: 11 }
                                Text { text: "BOARD"; width: 130; color: Theme.textMuted; font.pixelSize: 11 }
                                Text { text: "SEVERITY"; width: 104; color: Theme.textMuted; font.pixelSize: 11 }
                                Text { text: "CLASS"; width: Math.max(40, parent.width - 84 - 130 - 104 - 78); color: Theme.textMuted; font.pixelSize: 11 }
                                Text { text: "CONF"; width: 78; color: Theme.textMuted; font.pixelSize: 11; horizontalAlignment: Text.AlignRight }
                            }
                            Rectangle { width: parent.width; height: 1; color: Theme.divider }

                            Item {
                                width: parent.width
                                height: 210

                                ListView {
                                    id: streamView
                                    anchors.fill: parent
                                    clip: true
                                    model: App.eventStream
                                    delegate: Row {
                                        id: streamRow
                                        required property string time
                                        required property string node
                                        required property string severity
                                        required property string className
                                        required property string confidenceText
                                        width: ListView.view.width
                                        height: 24

                                        Text { text: streamRow.time; width: 84; color: Theme.textMuted; font.family: "ui-monospace, monospace"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: streamRow.node; width: 130; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                                        TagBadge {
                                            width: 96
                                            text: streamRow.severity
                                            bg: Theme.severityColors(streamRow.severity).bg
                                            fg: Theme.severityColors(streamRow.severity).fg
                                            borderColor: Theme.severityColors(streamRow.severity).border
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Item { width: 8; height: 1 }
                                        Text { text: streamRow.className; width: Math.max(40, streamRow.width - 84 - 130 - 104 - 78); color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: streamRow.confidenceText; width: 78; color: Theme.text; font.pixelSize: 12; horizontalAlignment: Text.AlignRight; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                }

                                EmptyState {
                                    anchors.fill: parent
                                    visible: streamView.count === 0
                                    title: "No telemetry received"
                                    detail: "Frames appear here as the board scores each window."
                                }
                            }
                        }
                    }
                }

                Column {
                    width: (parent.width - 18) * 0.392
                    spacing: 16

                    BlueprintFrame {
                        width: parent.width
                        visible: App.telemetry.boardCount === 0
                        height: 170
                        EmptyState {
                            anchors.fill: parent
                            title: App.telemetry.brokerConnected ? "No boards announced yet" : "Broker not connected"
                            detail: App.telemetry.brokerConnected
                                ? "Boards appear automatically once they publish their retained status."
                                : "Check the broker address and credentials."
                            actionText: "Open Integrations"
                            onActionClicked: App.goIntegrations()
                        }
                    }

                    Repeater {
                        model: App.nodesModel
                        delegate: BlueprintFrame {
                            id: nodeCard
                            required property int index
                            required property string id
                            required property string state
                            required property string lastSeen
                            required property var windows
                            required property var alerts
                            required property string lastClass
                            required property double lastConfidence
                            required property string windowPeriod
                            required property int featureCount

                            width: parent.width
                            height: nodeCol.implicitHeight + 24

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.selectNode(nodeCard.index)
                            }

                            Column {
                                id: nodeCol
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Row {
                                    id: nodeCardHeader
                                    width: parent.width
                                    Text { id: nodeCardId; text: nodeCard.id; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                                    Item {
                                        width: Math.max(0, nodeCardHeader.width - nodeCardId.implicitWidth - nodeCardTag.implicitWidth)
                                        height: 1
                                    }
                                    TagBadge {
                                        id: nodeCardTag
                                        text: nodeCard.state
                                        bg: Theme.nodeStateColors(nodeCard.state).bg
                                        fg: Theme.nodeStateColors(nodeCard.state).fg
                                        borderColor: Theme.nodeStateColors(nodeCard.state).border
                                        anchors.verticalCenter: nodeCardId.verticalCenter
                                    }
                                }


                                Grid {
                                    width: parent.width
                                    columns: 3
                                    columnSpacing: 10

                                    Column {
                                        width: (nodeCol.width - 20) / 3
                                        spacing: 2
                                        Text { text: "WINDOWS"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                        Text { text: String(nodeCard.windows); color: Theme.text; font.pixelSize: 15 }
                                    }
                                    Column {
                                        width: (nodeCol.width - 20) / 3
                                        spacing: 2
                                        Text { text: "ALERTS"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                        Text { text: String(nodeCard.alerts); color: Theme.text; font.pixelSize: 15 }
                                    }
                                    Column {
                                        width: (nodeCol.width - 20) / 3
                                        spacing: 2
                                        Text { text: "LAST CONF"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                        Text {
                                            text: nodeCard.windows > 0 ? nodeCard.lastConfidence.toFixed(3) : "—"
                                            color: Theme.text; font.pixelSize: 15
                                        }
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: nodeCard.windows > 0
                                        ? ("last " + nodeCard.lastClass + " · " + nodeCard.lastSeen
                                           + " · window " + nodeCard.windowPeriod + " · " + nodeCard.featureCount + " features")
                                        : "announced, no windows received yet"
                                    color: Theme.textMutedFaint
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
