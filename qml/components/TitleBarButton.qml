import QtQuick
import Ironwatch
import IronwatchApp

Rectangle {
    id: root
    width: 46
    height: 32
    color: mouse.containsMouse ? Theme.rowHover : "transparent"
    signal clicked()

    // No custom default property: Rectangle already exposes `data` as its
    // default, so icon content declared inside a TitleBarButton {...}
    // usage becomes a normal (fixed-size-parent) child automatically.

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
