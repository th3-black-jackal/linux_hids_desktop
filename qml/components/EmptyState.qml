import QtQuick
import Ironwatch
import IronwatchApp

// Shown wherever a panel has no data yet. With mock data removed, "nothing
// here" is the normal startup state until a board connects, so it needs to
// say *why* rather than just render blank.
Item {
    id: root
    property string title: ""
    property string detail: ""
    property string actionText: ""
    signal actionClicked()

    implicitHeight: col.implicitHeight + 32

    Column {
        id: col
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 460)
        spacing: 8

        Text {
            width: parent.width
            text: root.title
            color: Theme.text
            font.family: Theme.fontHeading
            font.weight: Font.DemiBold
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            visible: root.detail.length > 0
            text: root.detail
            color: Theme.textMuted
            font.family: Theme.fontBody
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        Item {
            width: parent.width
            height: btn.visible ? btn.implicitHeight + 6 : 0
            AppButton {
                id: btn
                visible: root.actionText.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                text: root.actionText
                onClicked: root.actionClicked()
            }
        }
    }
}
