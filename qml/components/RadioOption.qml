import QtQuick
import Ironwatch
import IronwatchApp

// .radio — single radio row (dot + label). Parent owns the exclusive
// selection state; this just renders one option and reports clicks.
Item {
    id: root
    property bool checked: false
    property string text: ""
    signal clicked()

    implicitWidth: label.x + label.implicitWidth
    implicitHeight: 20

    Rectangle {
        id: dot
        width: 16; height: 16
        radius: 8
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        color: root.checked ? Theme.accent : "transparent"
        border.width: 1.5
        border.color: mouse.containsMouse ? Theme.accent : Theme.divider

        Rectangle {
            visible: root.checked
            anchors.centerIn: parent
            width: 8; height: 8
            radius: 4
            color: Theme.bg
        }
    }

    Text {
        id: label
        anchors.left: dot.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: Theme.text
        font.family: Theme.fontBody
        font.pixelSize: 14
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
