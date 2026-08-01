import QtQuick
import Ironwatch
import IronwatchApp

Rectangle {
    id: root
    property string iconKind: "overview"
    property string text: ""
    property bool active: false
    property int badgeCount: -1
    signal clicked()

    implicitHeight: 34
    color: mouse.containsMouse && !active ? Theme.rowHover : "transparent"

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: root.active ? Theme.accent : "transparent"
    }

    NavIcon {
        id: icon
        kind: root.iconKind
        color: Theme.text
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
    }

    Text {
        anchors.left: icon.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: Theme.text
        font.family: Theme.fontBody
        font.pixelSize: 13
    }

    TagBadge {
        visible: root.badgeCount >= 0
        text: String(root.badgeCount)
        bg: Theme.accent900
        fg: Theme.bg
        borderColor: Theme.accent900
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
