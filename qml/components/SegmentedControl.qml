import QtQuick
import Ironwatch
import IronwatchApp

// .seg / .seg-opt — exclusive-choice segmented control (view window,
// severity filter, etc).
Rectangle {
    id: root
    property var options: [] // list<string>
    property int currentIndex: 0
    signal activated(int index)

    color: "transparent"
    border.width: 1
    border.color: Theme.divider
    radius: 0
    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth

    Row {
        id: row
        spacing: 0

        Repeater {
            model: root.options
            delegate: Rectangle {
                id: opt
                required property string modelData
                required property int index
                readonly property bool selected: index === root.currentIndex

                height: label.implicitHeight + 14
                width: label.implicitWidth + 24
                color: selected ? Theme.accent : (mouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.07) : "transparent")

                Rectangle {
                    visible: index > 0
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: Theme.divider
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: opt.modelData
                    color: opt.selected ? Theme.bg : Theme.text
                    font.family: Theme.fontBody
                    font.pixelSize: 13
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activated(opt.index)
                }
            }
        }
    }
}
