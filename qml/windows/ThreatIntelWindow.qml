import QtQuick
import QtQuick.Window
import Ironwatch
import IronwatchApp
// NOTE: no `import QtWebEngine` here — WebEngine is optional. The embedded
// browser lives in ReferencePaneWeb.qml, loaded only when the module is
// present (Intel.webEngineAvailable); otherwise ReferencePaneExternal.qml
// opens references in the system browser. See THREAT_INTEL.md.

import "../components"

// Threat Intel — part of the console, opened from the sidebar (see
// THREAT_INTEL.md "Wiring"). Three linked sections bound to the `Intel`
// singleton: a node/edge graph, a locked-down WebEngineView reference pane,
// and a news feed. Selecting a graph node drives the other two.
Window {
    id: win
    flags: Qt.Window | Qt.FramelessWindowHint
    width: 1360
    height: 860
    minimumWidth: 1100
    minimumHeight: 680
    visible: false
    color: Theme.bg
    title: "Threat Intel"

    // Entity label -> a stable accent from the severity palette, so kinds are
    // visually separable without inventing a new colour system.
    function kindColor(type) {
        switch (type) {
        case "ThreatActor":   return Theme.severityColors("critical")
        case "Malware":       return Theme.severityColors("high")
        case "Vulnerability": return Theme.severityColors("medium")
        case "Indicator":     return Theme.severityColors("low")
        case "Technique":     return Theme.severityColors("info")
        default:              return { bg: Theme.neutral200, fg: Theme.text, border: Theme.divider }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        Column {
            anchors.fill: parent
            spacing: 0

            // ── Title bar ──────────────────────────────────────────────
            Rectangle {
                width: parent.width; height: 32; color: Theme.neutral200
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
                MouseArea { anchors.fill: parent; anchors.rightMargin: 46; onPressed: win.startSystemMove() }
                Row {
                    anchors.left: parent.left; anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter; spacing: 8
                    Rectangle { width: 14; height: 14; color: "transparent"; border.width: 1.5; border.color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "THREAT INTEL"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 12; font.letterSpacing: 1.5; anchors.verticalCenter: parent.verticalCenter }
                }
                TitleBarButton {
                    anchors.right: parent.right; height: parent.height
                    onClicked: win.hide()
                    Text {   // TitleBarButton takes its icon as a child, not a `glyph` property
                        anchors.centerIn: parent
                        text: "×"; color: Theme.text
                        font.family: Theme.fontBody; font.pixelSize: 18
                    }
                }
            }

            // ── Toolbar: seed search + depth ───────────────────────────
            Rectangle {
                width: parent.width; height: 44; color: Theme.bg
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
                Row {
                    anchors.left: parent.left; anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter; spacing: 10

                    SectionLabel { text: "Seed"; anchors.verticalCenter: parent.verticalCenter }
                    Rectangle {
                        width: 320; height: 28; color: Theme.surface; border.width: 1
                        border.color: seedInput.activeFocus ? Theme.accent : Theme.divider
                        anchors.verticalCenter: parent.verticalCenter
                        TextInput {
                            id: seedInput
                            anchors.fill: parent; anchors.margins: 6
                            verticalAlignment: TextInput.AlignVCenter
                            color: Theme.text; font.family: Theme.fontBody; font.pixelSize: 13
                            clip: true
                            onTextEdited: Intel.search(text)
                            onAccepted: if (Intel.searchResults.length) Intel.seedFrom(Intel.searchResults[0].id)
                            Text {
                                anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                                visible: !seedInput.text && !seedInput.activeFocus
                                text: "actor, malware, CVE, hash…"; color: Theme.textMuted; font.pixelSize: 13
                            }
                        }
                        // Typeahead dropdown
                        Rectangle {
                            visible: seedInput.activeFocus && Intel.searchResults.length > 0
                            anchors.top: parent.bottom; width: parent.width
                            height: Math.min(Intel.searchResults.length, 8) * 26
                            color: Theme.surface; border.width: 1; border.color: Theme.divider; z: 50
                            ListView {
                                anchors.fill: parent; clip: true
                                model: Intel.searchResults
                                delegate: Rectangle {
                                    required property var modelData
                                    width: ListView.view.width; height: 26
                                    color: ma.containsMouse ? Theme.rowHover : "transparent"
                                    Row {
                                        anchors.fill: parent; anchors.leftMargin: 8; spacing: 8
                                        Text { text: modelData.type; color: Theme.textMuted; font.pixelSize: 10; width: 86; anchors.verticalCenter: parent.verticalCenter }
                                        Text { text: modelData.name; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                    }
                                    MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true
                                        onClicked: { seedInput.text = modelData.name; Intel.seedFrom(modelData.id) } }
                                }
                            }
                        }
                    }
                    AppButton { text: "Overview"; anchors.verticalCenter: parent.verticalCenter; onClicked: Intel.loadOverview() }
                    Text {
                        visible: Intel.busy; text: "loading…"; color: Theme.textMuted
                        font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Body: graph | (reference / news) ───────────────────────
            Item {
                width: parent.width
                height: parent.height - 32 - 44

                // Backend not configured / empty → guidance.
                EmptyState {
                    anchors.centerIn: parent; width: 560
                    visible: Intel.phase === "idle" && Intel.graph.count === 0
                    title: Intel.backendConfigured ? "Explore the threat graph"
                                                   : "Backend not configured"
                    detail: Intel.backendConfigured
                        ? "Search for an actor, malware family, CVE or indicator above, or load an overview. "
                          + "The graph is served by the backend's /api/graph/ endpoints (see THREAT_INTEL.md)."
                        : "Set the backend host and token in the console's Integrations screen first; "
                          + "Threat Intel uses the same connection as the SIEM and assistant."
                    actionText: Intel.backendConfigured ? "Load overview" : ""
                    onActionClicked: Intel.loadOverview()
                }

                Row {
                    anchors.fill: parent
                    visible: Intel.graph.count > 0 || Intel.phase === "error"
                    spacing: 0

                    // ========== Section 1: GRAPH ==========
                    Item {
                        id: graphPane
                        width: parent.width * 0.58
                        height: parent.height
                        clip: true

                        onWidthChanged: Intel.graph.setViewport(width, height)
                        onHeightChanged: Intel.graph.setViewport(width, height)
                        Component.onCompleted: Intel.graph.setViewport(width, height)

                        // Edges, painted behind the node bubbles.
                        Canvas {
                            id: edgeCanvas
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                const lines = Intel.graph.edgeLines()
                                for (let i = 0; i < lines.length; ++i) {
                                    const e = lines[i]
                                    ctx.beginPath()
                                    ctx.lineWidth = 1
                                    ctx.strokeStyle = Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b,
                                                              e.cooccurrence ? 0.18 : 0.33)
                                    if (e.cooccurrence) ctx.setLineDash([4, 3]); else ctx.setLineDash([])
                                    ctx.moveTo(e.x1, e.y1); ctx.lineTo(e.x2, e.y2)
                                    ctx.stroke()
                                }
                            }
                        }

                        // Layout driver: step the sim while it is settling.
                        Timer {
                            interval: 16; repeat: true; running: Intel.graph.settling
                            onTriggered: { Intel.graph.step(2); edgeCanvas.requestPaint() }
                        }
                        Connections {
                            target: Intel.graph
                            function onCountChanged() { edgeCanvas.requestPaint() }
                        }

                        // Node bubbles.
                        Repeater {
                            model: Intel.graph
                            delegate: Rectangle {
                                id: bubble
                                // Role names avoid QML's built-in `id` and Item.x/y.
                                required property int index
                                required property string nodeId
                                required property string type
                                required property string name
                                required property real posX
                                required property real posY
                                required property int degree
                                required property bool selected

                                property var pal: win.kindColor(type)
                                width: Math.max(16, Math.min(40, 14 + degree * 2))
                                height: width; radius: width / 2
                                x: posX - width / 2
                                y: posY - height / 2
                                color: pal.bg
                                border.width: selected ? 2 : 1
                                border.color: selected ? Theme.accent : pal.border

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.bottom; anchors.topMargin: 2
                                    text: bubble.name; color: Theme.text
                                    font.family: Theme.fontBody; font.pixelSize: 10
                                    width: 120; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter
                                    visible: bubble.degree > 1 || bubble.selected
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    drag.target: parent
                                    onPressed: Intel.graph.pin(bubble.nodeId, bubble.posX, bubble.posY)
                                    onReleased: {
                                        // commit dragged position, keep it pinned where dropped
                                        Intel.graph.pin(bubble.nodeId, parent.x + parent.width / 2,
                                                                       parent.y + parent.height / 2)
                                    }
                                    onPositionChanged: if (drag.active) edgeCanvas.requestPaint()
                                    onClicked: Intel.select(bubble.nodeId)
                                    onDoubleClicked: Intel.expand(bubble.nodeId)
                                }
                            }
                        }

                        // Legend
                        Row {
                            anchors.bottom: parent.bottom; anchors.left: parent.left
                            anchors.margins: 10; spacing: 10
                            Repeater {
                                model: ["ThreatActor", "Malware", "Vulnerability", "Indicator", "Technique"]
                                delegate: Row {
                                    required property string modelData
                                    spacing: 4
                                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter
                                        color: win.kindColor(modelData).bg; border.width: 1; border.color: win.kindColor(modelData).border }
                                    Text { text: modelData; color: Theme.textMuted; font.pixelSize: 9 }
                                }
                            }
                        }
                    }

                    Rectangle { width: 1; height: parent.height; color: Theme.divider }

                    // ========== Sections 2 & 3: REFERENCE over NEWS ==========
                    Column {
                        width: parent.width * 0.42 - 1
                        height: parent.height

                        // ----- Section 2: REFERENCE -----
                        // The pane is loaded from a separate file so the window
                        // never hard-imports QtWebEngine. When the module is
                        // present we embed a locked-down browser; otherwise we
                        // open references in the system browser. Both read the
                        // same Intel.referenceUrl* state.
                        Item {
                            width: parent.width
                            height: parent.height * 0.52
                            Column {
                                anchors.fill: parent
                                SectionLabel { text: "Reference"; leftPadding: 12; topPadding: 8; bottomPadding: 4 }
                                Loader {
                                    width: parent.width
                                    height: parent.height - 26
                                    source: Intel.webEngineAvailable
                                        ? "ReferencePaneWeb.qml"
                                        : "ReferencePaneExternal.qml"
                                }
                            }
                        }

                        Rectangle { width: parent.width; height: 1; color: Theme.divider }

                        // ----- Section 3: NEWS feed -----
                        Item {
                            width: parent.width
                            height: parent.height * 0.48 - 1
                            Column {
                                anchors.fill: parent
                                Row {
                                    width: parent.width; height: 26; spacing: 8
                                    SectionLabel { text: "News"; leftPadding: 12; topPadding: 8; bottomPadding: 4; anchors.verticalCenter: parent.verticalCenter }
                                    // Lookback window — the graph_api /articles/ feed filters by `days`.
                                    Repeater {
                                        model: [7, 30, 90]
                                        delegate: Text {
                                            required property int modelData
                                            text: modelData + "d"
                                            anchors.verticalCenter: parent.verticalCenter
                                            font.pixelSize: 10
                                            color: Intel.newsDays === modelData ? Theme.accent : Theme.textMuted
                                            MouseArea { anchors.fill: parent; anchors.margins: -4; onClicked: Intel.newsDays = modelData }
                                        }
                                    }
                                    Item { width: parent.width - 220; height: 1 }
                                    Text {
                                        text: Intel.news.total + " articles"; color: Theme.textMuted
                                        font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                ListView {
                                    id: newsList
                                    width: parent.width; height: parent.height - 26
                                    clip: true; model: Intel.news; spacing: 0
                                    delegate: Rectangle {
                                        required property int index
                                        required property string title
                                        required property string url
                                        required property string published
                                        required property string source
                                        required property string summary
                                        width: ListView.view.width
                                        height: col.implicitHeight + 16
                                        color: mouse.containsMouse ? Theme.rowHover : (index % 2 ? "transparent" : Qt.rgba(Theme.text.r,Theme.text.g,Theme.text.b,0.02))
                                        Column {
                                            id: col
                                            anchors.left: parent.left; anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 2
                                            Text { text: title; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 12; width: parent.width; elide: Text.ElideRight }
                                            Row { spacing: 6
                                                Text { text: published; color: Theme.accent; font.pixelSize: 10 }
                                                Text { text: "·"; color: Theme.textMuted; font.pixelSize: 10 }
                                                Text { text: source; color: Theme.textMuted; font.pixelSize: 10 }
                                            }
                                            Text { text: summary; color: Theme.textMuted; font.pixelSize: 11; width: parent.width; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight }
                                        }
                                        MouseArea {
                                            id: mouse; anchors.fill: parent; hoverEnabled: true
                                            // Article links are safe → show in the reference pane.
                                            onClicked: if (url.length) Intel.showUrl(url)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Error strip.
                Rectangle {
                    visible: Intel.phase === "error"
                    anchors.bottom: parent.bottom; width: parent.width; height: 28
                    color: Theme.severityColors("high").bg
                    Text { anchors.centerIn: parent; text: Intel.lastError; color: Theme.severityColors("high").fg; font.pixelSize: 11 }
                }
            }
        }
    }
}
