import QtQuick
import Ironwatch
import IronwatchApp

// .tag — small pill used for severity/state/build labels. Caller supplies
// resolved colors (see Theme.severityColors / Theme.nodeStateColors) so
// this stays a dumb presentational piece.
Rectangle {
    id: root
    property string text: ""
    property color bg: "transparent"
    property color fg: Theme.text
    property color borderColor: Theme.divider

    implicitWidth: label.implicitWidth + 20
    implicitHeight: label.implicitHeight + 6
    color: bg
    border.width: 1
    border.color: borderColor
    radius: 0

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.fg
        font.family: Theme.fontBody
        font.pixelSize: 11
        font.letterSpacing: 0.3
    }
}
