import QtQuick
import QtQuick.Window
import QtQuick.Shapes
import Ironwatch
import IronwatchApp

import "screens"

Window {
    id: window
    flags: Qt.Window | Qt.FramelessWindowHint
    width: 1280
    height: 800
    minimumWidth: 1200
    minimumHeight: 760
    visible: true
    color: Theme.bg
    title: "Ironwatch — Edge HIDS Console"

    Rectangle {
        anchors.fill: parent
        color: Theme.bg

        Column {
            anchors.fill: parent
            spacing: 0

            // ── Title bar ──────────────────────────────────────────────
            Rectangle {
                id: titleBar
                width: parent.width
                height: 32
                color: Theme.neutral200
                border.width: 0

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.divider
                }

                MouseArea {
                    anchors.fill: parent
                    anchors.rightMargin: 46 * 4
                    onPressed: window.startSystemMove()
                    onDoubleClicked: window.visibility = (window.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Rectangle {
                        width: 14; height: 14
                        color: "transparent"
                        border.width: 1.5
                        border.color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 3
                            color: Theme.accent
                        }
                    }
                    Text {
                        text: "IRONWATCH"
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.text
                        font.family: Theme.fontHeading
                        font.pixelSize: 13
                        font.letterSpacing: 1.4
                    }
                    Text {
                        text: "— Edge HIDS Console"
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textMuted
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    TitleBarButton {
                        height: titleBar.height
                        onClicked: App.toggleTheme()
                        Shape {
                            visible: Theme.dark
                            anchors.centerIn: parent
                            width: 24; height: 24
                            scale: 14 / 24
                            transformOrigin: Item.Center
                            ShapePath {
                                strokeColor: Theme.text; strokeWidth: 2.4; fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                PathSvg { path: "M12,12 m-4,0 a4,4 0 1 0 8,0 a4,4 0 1 0 -8,0 M12,2 v2 M12,20 v2 M2,12 h2 M20,12 h2 M5,5 l1.5,1.5 M17.5,17.5 l1.5,1.5 M19,5 l-1.5,1.5 M6.5,17.5 l-1.5,1.5" }
                            }
                        }
                        Shape {
                            visible: !Theme.dark
                            anchors.centerIn: parent
                            width: 24; height: 24
                            scale: 14 / 24
                            transformOrigin: Item.Center
                            ShapePath {
                                strokeColor: Theme.text; strokeWidth: 2.4; fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg { path: "M20,14.5 A8.5,8.5 0 0 1 9.5,4 a8.5,8.5 0 1 0 10.5,10.5 Z" }
                            }
                        }
                    }
                    TitleBarButton {
                        height: titleBar.height
                        onClicked: window.showMinimized()
                        Rectangle { width: 10; height: 1; color: Theme.text }
                    }
                    TitleBarButton {
                        height: titleBar.height
                        onClicked: window.visibility = (window.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized
                        Rectangle { width: 9; height: 9; color: "transparent"; border.width: 1; border.color: Theme.text }
                    }
                    TitleBarButton {
                        height: titleBar.height
                        onClicked: window.close()
                        Shape {
                            anchors.centerIn: parent
                            width: 24; height: 24
                            scale: 11 / 24
                            transformOrigin: Item.Center
                            ShapePath {
                                strokeColor: Theme.text; strokeWidth: 3.5; fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                PathSvg { path: "M6,6 L18,18 M6,18 L18,6" }
                            }
                        }
                    }
                }
            }

            // ── Menu bar ───────────────────────────────────────────────
            Rectangle {
                id: menuBar
                width: parent.width
                height: 26
                color: Theme.bg

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.divider
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom

                    Repeater {
                        model: ["File", "Monitor", "Node", "Model", "Tools", "Help"]
                        delegate: Item {
                            required property string modelData
                            width: label.implicitWidth + 18
                            height: menuBar.height
                            Text {
                                id: label
                                anchors.centerIn: parent
                                text: parent.modelData
                                color: Theme.text
                                font.family: Theme.fontBody
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 7

                    Rectangle {
                        width: 7; height: 7
                        // Pulses only while at least one board is actually
                        // streaming; a steady dot means nothing is arriving.
                        color: App.telemetry.brokerConnected ? Theme.accent : Theme.neutral500
                        anchors.verticalCenter: parent.verticalCenter
                        SequentialAnimation on opacity {
                            running: App.telemetry.brokerConnected
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.25; duration: 1000; easing.type: Easing.InOutQuad }
                            NumberAnimation { from: 0.25; to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                        }
                    }
                    Text {
                        text: !App.telemetry.brokerConnected
                            ? "BROKER " + App.telemetry.brokerState.toUpperCase()
                            : (App.telemetry.onlineCount + " / " + App.telemetry.boardCount
                               + " BOARDS ONLINE · WINDOW " + App.telemetry.windowPeriodText)
                        color: Theme.textMuted
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.letterSpacing: 0.7
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Body ───────────────────────────────────────────────────
            Row {
                width: parent.width
                height: parent.height - titleBar.height - menuBar.height - statusBar.height
                spacing: 0

                // Sidebar
                Rectangle {
                    id: sidebar
                    width: 208
                    height: parent.height
                    color: Theme.bg

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 1
                        color: Theme.divider
                    }

                    Column {
                        anchors.fill: parent
                        anchors.topMargin: 10
                        anchors.bottomMargin: 10

                        SectionLabel { x: 12; text: "Workspace"; bottomPadding: 8 }

                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "overview"; text: "Overview"
                            active: App.screen === "overview"
                            onClicked: App.goOverview()
                        }
                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "detections"; text: "Detections"
                            active: App.screen === "detections"
                            badgeCount: App.detectionsModel.openTotal
                            onClicked: App.goDetections()
                        }
                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "nodes"; text: "Nodes"
                            active: App.screen === "nodes"
                            onClicked: App.goNodes()
                        }
                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "assistant"; text: "Assistant"
                            active: App.screen === "chat"
                            onClicked: App.goChat()
                        }
                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "model"; text: "Model & Rules"
                            active: App.screen === "model"
                            onClicked: App.goModel()
                        }
                        SidebarNavItem {
                            width: sidebar.width
                            iconKind: "integrations"; text: "Integrations"
                            active: App.screen === "integrations"
                            onClicked: App.goIntegrations()
                        }

                        Item { width: 1; height: 18 }
                        SectionLabel { x: 12; text: "Fleet" }
                        Item { width: 1; height: 8 }

                        Repeater {
                            model: App.nodesModel
                            delegate: Rectangle {
                                id: fleetRow
                                required property int index
                                required property string id
                                required property string state
                                required property var alerts

                                width: sidebar.width
                                height: 26
                                color: fm.containsMouse ? Theme.rowHover : "transparent"

                                Rectangle {
                                    id: dot
                                    width: 6; height: 6
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.nodeStateColors(fleetRow.state).dot
                                }
                                Text {
                                    anchors.left: dot.right
                                    anchors.leftMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: fleetRow.id
                                    color: Theme.text
                                    font.family: Theme.fontBody
                                    font.pixelSize: 13
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    // Alert count, not CPU — the board does not
                                    // report resource usage over telemetry.
                                    text: fleetRow.alerts > 0 ? String(fleetRow.alerts) : "—"
                                    color: Theme.textMuted
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    id: fm
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: App.selectNode(fleetRow.index)
                                }
                            }
                        }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        anchors.bottomMargin: 10
                        spacing: 2

                        Rectangle { width: parent.width; height: 1; color: Theme.divider }
                        Item { width: 1; height: 8 }
                        // The model build/version isn't transmitted, so there
                        // is nothing honest to show here — report what the
                        // stream does tell us instead.
                        SectionLabel { text: "Model output"; font.pixelSize: 11 }
                        Text {
                            width: parent.width
                            text: App.classStats.totalWindows > 0
                                ? (App.classStats.distinctClasses + " classes · "
                                   + App.classStats.totalWindows + " windows")
                                : "awaiting telemetry"
                            color: Theme.text
                            font.family: Theme.fontBody
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                    }
                }

                // Content
                Item {
                    id: content
                    width: parent.width - sidebar.width
                    height: parent.height

                    OverviewScreen { anchors.fill: parent; visible: App.screen === "overview" }
                    DetectionsScreen { anchors.fill: parent; visible: App.screen === "detections" }
                    NodesScreen { anchors.fill: parent; visible: App.screen === "nodes" }
                    ChatScreen { anchors.fill: parent; visible: App.screen === "chat" }
                    ModelScreen { anchors.fill: parent; visible: App.screen === "model" }
                    IntegrationsScreen { anchors.fill: parent; visible: App.screen === "integrations" }
                }
            }

            // ── Status bar ─────────────────────────────────────────────
            Rectangle {
                id: statusBar
                width: parent.width
                height: 24
                color: Theme.neutral200

                Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.divider }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 16

                    Row {
                        spacing: 6
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 6; height: 6
                            color: App.telemetry.brokerConnected ? Theme.accent : Theme.neutral500
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "Broker: " + App.telemetry.brokerState + " · boards " + App.telemetry.onlineCount + "/" + App.telemetry.boardCount
                            color: Theme.text; font.family: Theme.fontBody; font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    Text {
                        text: "Messages: " + App.telemetry.framesReceived + " · " + App.telemetry.throughputText
                        color: Theme.textMuted
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: App.telemetry.framesDropped > 0
                            ? (App.telemetry.framesDropped + " frames dropped")
                            : ("Last event: " + App.telemetry.lastEventAge)
                        color: App.telemetry.framesDropped > 0 ? Theme.text : Theme.textMuted
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    text: App.clock
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                }
            }
        }
    }
}
