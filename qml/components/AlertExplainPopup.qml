import QtQuick
import QtQuick.Controls
import Ironwatch
import IronwatchApp

import "../util/Citations.js" as Citations

// The right-click "explain this alert" panel: a small, dismissible widget
// that floats beside the alerts table while one answer streams into it.
//
// Deliberately not a modal dialog. An operator right-clicks a row to get
// context on the row, so the table has to stay visible and usable underneath
// — including right-clicking a *different* row, which replaces the answer.
//
// `anchorY` is the y of the clicked row in this item's coordinate space; the
// panel opens next to it and is clamped so it never leaves the viewport.
Item {
    id: root

    // Where the table's right edge is, so the panel can sit just inside it.
    property real anchorY: 0
    property int panelWidth: 380
    property int margin: 12

    visible: App.explainer.active
    z: 100

    // Click-away dismissal, over the table only — the panel itself is above
    // this, so clicks inside it are unaffected.
    MouseArea {
        anchors.fill: parent
        enabled: App.explainer.active
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onPressed: function(mouse) {
            App.explainer.close()
            // Let the click through to the table so dismissing and selecting
            // are one gesture rather than two.
            mouse.accepted = false
        }
    }

    Rectangle {
        id: panel
        width: root.panelWidth
        // Grows with the answer, but never taller than the space it has.
        height: Math.min(root.height - 2 * root.margin, col.implicitHeight + 26)
        x: root.width - width - root.margin
        y: Math.max(root.margin,
                    Math.min(root.height - height - root.margin,
                             root.anchorY - 40))

        color: Theme.bg
        border.width: 1
        border.color: Theme.accent

        // A hairline drop shadow, done as offset rectangles rather than a
        // blur: the design system is flat and square, and this only needs to
        // lift the panel off the table rows behind it.
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.leftMargin: 3
            anchors.topMargin: 3
            color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
        }

        // Keeps table hover/click from reaching rows behind the panel.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            onWheel: function(wheel) { wheel.accepted = true }
        }

        Column {
            id: col
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 13
            spacing: 9

            // ── Header ───────────────────────────────────────────────────
            Item {
                width: parent.width
                height: 18

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "ATT&CK ASSISTANT"
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 10
                    font.letterSpacing: 1.0
                }

                Text {
                    id: closeBtn
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✕"
                    color: closeMouse.containsMouse ? Theme.text : Theme.textMuted
                    font.pixelSize: 13
                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: App.explainer.close()
                    }
                }
            }

            // ── What this is about ───────────────────────────────────────
            Row {
                width: parent.width
                spacing: 8

                TagBadge {
                    readonly property var c: Theme.severityColors(App.explainer.severity)
                    text: App.explainer.severity
                    bg: c.bg; fg: c.fg; borderColor: c.border
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    width: parent.width - 100
                    text: App.explainer.subject
                    color: Theme.text
                    font.family: Theme.fontHeading
                    font.weight: Font.DemiBold
                    font.pixelSize: 15
                    elide: Text.ElideRight
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Text {
                width: parent.width
                text: App.explainer.subjectDetail
                color: Theme.textMuted
                font.family: "ui-monospace, monospace"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Rectangle { width: parent.width; height: 1; color: Theme.divider }

            // ── Waiting ──────────────────────────────────────────────────
            Row {
                width: parent.width
                spacing: 8
                visible: App.explainer.state === "waiting"

                Rectangle {
                    width: 7; height: 7
                    color: Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                    SequentialAnimation on opacity {
                        running: App.explainer.state === "waiting"
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.2; duration: 700 }
                        NumberAnimation { from: 0.2; to: 1.0; duration: 700 }
                    }
                }
                Text {
                    text: "asking the backend…"
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // ── The answer ───────────────────────────────────────────────
            ScrollView {
                width: parent.width
                height: Math.min(260, answerText.implicitHeight + 4)
                visible: App.explainer.answer.length > 0
                clip: true

                Text {
                    id: answerText
                    width: panel.width - 26
                    textFormat: Text.RichText
                    // Same rule as the Assistant screen: a citation becomes a
                    // link only when the backend actually retrieved it.
                    text: Citations.render(App.explainer.answer, App.explainer.sources,
                                           Theme.accent700, Theme.textMuted)
                    color: Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        cursorShape: parent.hoveredLink.length > 0
                            ? Qt.PointingHandCursor : Qt.ArrowCursor
                    }
                }
            }

            // ── Sources ──────────────────────────────────────────────────
            Flow {
                width: parent.width
                spacing: 5
                visible: App.explainer.sources.length > 0

                Repeater {
                    model: App.explainer.sources
                    delegate: TagBadge {
                        id: chip
                        required property var modelData
                        text: modelData.id
                        bg: chipMouse.containsMouse ? Theme.rowSelected : "transparent"
                        fg: modelData.url && modelData.url.length > 0
                            ? Theme.accent700 : Theme.textMuted
                        borderColor: Theme.divider

                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: chip.modelData.url && chip.modelData.url.length > 0
                                ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: if (chip.modelData.url && chip.modelData.url.length > 0)
                                           Qt.openUrlExternally(chip.modelData.url)
                            ToolTip.visible: containsMouse
                            ToolTip.delay: 400
                            ToolTip.text: chip.modelData.title
                        }
                    }
                }
            }

            // Same honesty note as the Assistant screen, compressed.
            Text {
                width: parent.width
                readonly property var g: App.explainer.state === "done"
                    ? Citations.groundedCount(App.explainer.answer, App.explainer.sources)
                    : { cited: 0, grounded: 0 }
                visible: g.cited > g.grounded
                text: (g.cited - g.grounded) + " citation"
                      + ((g.cited - g.grounded) === 1 ? "" : "s")
                      + " not in the retrieved sources — not linked."
                color: Theme.textMutedFaint
                font.family: Theme.fontBody
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }

            // ── Failure ──────────────────────────────────────────────────
            Text {
                width: parent.width
                visible: App.explainer.state === "error"
                text: App.explainer.error
                color: Theme.text
                font.family: "ui-monospace, monospace"
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                visible: App.explainer.state === "cancelled"
                text: "Stopped before the answer finished."
                color: Theme.textMuted
                font.family: Theme.fontBody
                font.pixelSize: 11
            }

            Rectangle { width: parent.width; height: 1; color: Theme.divider }

            // ── Actions ──────────────────────────────────────────────────
            Row {
                width: parent.width
                spacing: 6

                AppButton {
                    text: "Stop"
                    visible: App.explainer.busy
                    onClicked: App.explainer.cancel()
                }
                AppButton {
                    text: "Retry"
                    visible: !App.explainer.busy
                             && (App.explainer.state === "error"
                                 || App.explainer.state === "cancelled")
                    onClicked: App.explainer.retry()
                }
                AppButton {
                    text: "Open in Assistant"
                    variant: "primary"
                    onClicked: App.openExplainInAssistant()
                }
            }

            Text {
                width: parent.width
                text: "Generated by an LLM and grounded in retrieved ATT&CK entries. "
                    + "A research aid, not a verdict — check the sources."
                color: Theme.textMutedFaint
                font.family: Theme.fontBody
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
        }
    }
}
