import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

Item {
    id: root

    readonly property var sel: App.selectedDetection
    readonly property string selSeverity: (sel && sel.severity) ? String(sel.severity) : "info"
    // NB: the Theme singleton is not yet resolvable while this root object's
    // own property bindings are being initialized, so the colour lookup has
    // to live on the item that consumes it (as it does in every delegate)
    // rather than in a root-level property here.

    Column {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            width: parent.width
            height: 58
            color: Theme.bg

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 10

                Column {
                    SectionLabel { text: "Triage" }
                    Text { text: "Detections"; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 18 }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 240
                    Layout.preferredHeight: 34
                    color: Theme.surface
                    border.width: 1
                    border.color: filterInput.activeFocus ? Theme.accent : Theme.divider

                    TextInput {
                        id: filterInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.text
                        font.family: Theme.fontBody
                        font.pixelSize: 13
                        selectByMouse: true
                        onTextChanged: App.detectionsModel.searchText = text

                        Text {
                            visible: filterInput.text.length === 0
                            text: "Filter by class, board, ID…"
                            color: Theme.textMuted
                            font: filterInput.font
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                SegmentedControl {
                    options: ["All", "Critical", "High", "Unacked"]
                    currentIndex: {
                        switch (App.detectionsModel.severityFilter) {
                        case "critical": return 1
                        case "high": return 2
                        case "open": return 3
                        default: return 0
                        }
                    }
                    onActivated: function(index) {
                        App.detectionsModel.severityFilter = ["all", "critical", "high", "open"][index]
                    }
                }

                AppButton { text: "Ack all"; onClicked: App.acknowledgeAll() }
            }
        }

        Row {
            width: parent.width
            height: parent.height - 58

            // Table
            Item {
                width: parent.width - 392
                height: parent.height

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Row {
                        width: parent.width
                        height: 30
                        Text { text: "#"; width: 44; leftPadding: 22; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "TIME"; width: 96; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "SEVERITY"; width: 104; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "BOARD"; width: 140; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "CLASS"; width: Math.max(60, parent.width - 44 - 96 - 104 - 140 - 90 - 100); color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "CONFIDENCE"; width: 90; rightPadding: 12; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "STATE"; width: 100; color: Theme.textMuted; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Item {
                        width: parent.width
                        height: parent.height - 31

                        ListView {
                            id: detView
                            anchors.fill: parent
                            clip: true
                            model: App.detectionsModel
                            delegate: Rectangle {
                                id: row
                                required property int index
                                required property string n
                                required property string time
                                required property string severity
                                required property string node
                                required property string className
                                required property string confidenceText
                                required property string state
                                required property int originalIndex

                                width: ListView.view.width
                                height: 32
                                color: row.originalIndex === App.selectedDetectionIndex
                                    ? Theme.rowSelected
                                    : (rowMouse.containsMouse ? Theme.rowHover : "transparent")

                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.cellBorder }

                                Text { x: 0; text: row.n; width: 44; leftPadding: 22; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { x: 44; text: row.time; width: 96; color: Theme.textMuted; font.family: "ui-monospace, monospace"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                TagBadge {
                                    x: 44 + 96
                                    width: 96
                                    text: row.severity
                                    bg: Theme.severityColors(row.severity).bg
                                    fg: Theme.severityColors(row.severity).fg
                                    borderColor: Theme.severityColors(row.severity).border
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text { x: 44 + 96 + 104; text: row.node; width: 140; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    x: 44 + 96 + 104 + 140
                                    text: row.className
                                    width: Math.max(60, row.width - 44 - 96 - 104 - 140 - 90 - 100)
                                    color: Theme.text
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    x: row.width - 100 - 90
                                    text: row.confidenceText
                                    width: 90
                                    rightPadding: 12
                                    horizontalAlignment: Text.AlignRight
                                    color: Theme.text
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    x: row.width - 100
                                    text: row.state
                                    width: 100
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: App.selectDetection(row.originalIndex)
                                }
                            }
                        }

                        EmptyState {
                            anchors.fill: parent
                            visible: detView.count === 0
                            title: App.detectionsModel.total === 0
                                ? "No detections yet"
                                : "No detections match this filter"
                            detail: App.detectionsModel.total === 0
                                ? "Windows classified as a non-normal class land here. "
                                  + "Normal windows stay in the Overview event stream."
                                : ""
                        }
                    }
                }
            }

            // Inspector
            Rectangle {
                width: 392
                height: parent.height
                color: "transparent"

                Rectangle { anchors.left: parent.left; width: 1; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.divider }

                EmptyState {
                    anchors.fill: parent
                    anchors.margins: 20
                    visible: !root.sel
                    title: "Nothing selected"
                    detail: "Pick a detection to see its class, confidence and the normalized feature vector that produced it."
                }

                // Built lazily: there is nothing to inspect until a detection
                // is selected, and deferring creation also keeps these
                // bindings off the initial component-load pass, where the
                // Theme singleton is not yet resolvable.
                Loader {
                    anchors.fill: parent
                    anchors.leftMargin: 21
                    anchors.rightMargin: 20
                    anchors.topMargin: 16
                    anchors.bottomMargin: 22
                    active: root.sel !== null
                    visible: active
                    sourceComponent: inspectorComponent
                }

                Component {
                    id: inspectorComponent

                    ScrollView {
                    contentWidth: availableWidth
                    clip: true

                    Column {
                        width: 392 - 41
                        spacing: 16

                        Column {
                            width: parent.width
                            spacing: 6
                            Row {
                                spacing: 8
                                TagBadge {
                                    readonly property var c: Theme.severityColors(root.selSeverity)
                                    text: root.sel ? root.sel.severity : ""
                                    bg: c.bg
                                    fg: c.fg
                                    borderColor: c.border
                                }
                                Text { text: root.sel ? root.sel.uid : ""; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            }
                            Text {
                                width: parent.width
                                text: root.sel ? root.sel.className : ""
                                color: Theme.text
                                font.family: Theme.fontHeading
                                font.weight: Font.DemiBold
                                font.pixelSize: 18
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: root.sel
                                    ? ("Model output class " + root.sel.classIdx
                                       + " at confidence " + root.sel.confidenceText + ".")
                                    : ""
                                color: Theme.textMuted
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                        }

                        Column {
                            width: parent.width
                            spacing: 10

                            Rectangle { width: parent.width; height: 1; color: Theme.divider }

                            Grid {
                                width: parent.width
                                columns: 2
                                columnSpacing: 14
                                rowSpacing: 10

                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "BOARD"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: root.sel ? root.sel.node : ""; color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "SOURCE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: root.sel ? ("ironwatch/" + root.sel.endpoint + "/alerts") : ""; color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "BOARD TIMESTAMP"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: root.sel ? root.sel.timestampRaw : ""; color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "CLASS INDEX"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { text: root.sel ? String(root.sel.classIdx) : ""; color: Theme.text; font.pixelSize: 13 }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "CONFIDENCE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { text: root.sel ? root.sel.confidenceText : ""; color: Theme.text; font.pixelSize: 13 }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "FEATURES"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { text: root.sel ? String(root.sel.featureCount) : ""; color: Theme.text; font.pixelSize: 13 }
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: Theme.divider }
                        }

                        Column {
                            width: parent.width
                            spacing: 8
                            Text { text: "FEATURE VECTOR"; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
                            Text {
                                width: parent.width
                                text: "Normalized inputs, ranked by absolute deviation. This is the "
                                    + "vector the model scored, not an attribution — the service does "
                                    + "not report per-feature contributions."
                                color: Theme.textMuted
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            Repeater {
                                model: root.sel ? root.sel.features : []
                                delegate: Row {
                                    id: featRow
                                    required property var modelData
                                    width: parent.width
                                    height: 18
                                    spacing: 8

                                    Text {
                                        width: 132
                                        text: featRow.modelData.name
                                        color: Theme.text
                                        font.family: "ui-monospace, monospace"
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Rectangle {
                                        width: Math.max(10, featRow.width - 132 - 52 - 16)
                                        height: 8
                                        color: Theme.neutral300
                                        anchors.verticalCenter: parent.verticalCenter
                                        Rectangle {
                                            width: parent.width * featRow.modelData.pct / 100
                                            height: 8
                                            // Sign matters: below-mean vs above-mean.
                                            color: featRow.modelData.value < 0 ? Theme.accent700 : Theme.accent
                                        }
                                    }
                                    Text {
                                        width: 52
                                        text: featRow.modelData.valueText
                                        horizontalAlignment: Text.AlignRight
                                        color: Theme.textMuted
                                        font.pixelSize: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                visible: root.sel && root.sel.featureCount > 12
                                text: "Showing the 12 largest of " + (root.sel ? root.sel.featureCount : 0) + "."
                                color: Theme.textMutedFaint
                                font.pixelSize: 11
                            }
                        }

                        Grid {
                            width: parent.width
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8

                            AppButton {
                                width: (parent.width - 8) / 2
                                text: root.sel && root.sel.acknowledged ? "Acknowledged" : "Acknowledge"
                                variant: "primary"
                                onClicked: App.toggleAcknowledgeSelected()
                            }
                            AppButton {
                                width: (parent.width - 8) / 2
                                text: "Mark benign"
                                onClicked: App.markBenignSelected()
                            }
                            AppButton {
                                width: (parent.width - 8) / 2
                                text: "Copy JSON"
                                onClicked: App.copySelectedAsJson()
                            }
                            AppButton {
                                width: (parent.width - 8) / 2
                                text: "View board"
                                onClicked: App.goNodes()
                            }
                            AppButton {
                                width: parent.width
                                text: "Ask the ATT&CK assistant"
                                // Drafts a question from this detection and
                                // opens the Assistant; it is not sent until
                                // the operator presses Send.
                                onClicked: App.askAboutSelectedDetection()
                            }
                        }

                        Text {
                            width: parent.width
                            text: "Escalation and quarantine are decided on the board (alert_streak / "
                                + "min_confidence in hids.conf) and written to its Wazuh log. The console "
                                + "is read-only over this protocol — there is no command channel back."
                            color: Theme.textMutedFaint
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    }
                }
            }
        }
    }
}
