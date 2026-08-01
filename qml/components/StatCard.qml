import QtQuick
import Ironwatch
import IronwatchApp

// .card.blueprint KPI tile: kicker / big value / meta line.
BlueprintFrame {
    id: root
    property string kicker: ""
    property string value: ""
    property string valueSuffix: ""
    property int valuePixelSize: 38
    property string meta: ""

    implicitHeight: col.implicitHeight + 24

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 13
        spacing: 4

        Text {
            text: root.kicker
            color: Theme.accent
            font.family: Theme.fontBody
            font.pixelSize: 10
            font.letterSpacing: 1.0
            font.capitalization: Font.AllUppercase
        }

        Row {
            spacing: 4
            Text {
                id: valueText
                text: root.value
                color: Theme.text
                font.family: Theme.fontHeading
                font.weight: Font.DemiBold
                font.pixelSize: root.valuePixelSize
            }
            Text {
                visible: root.valueSuffix.length > 0
                text: root.valueSuffix
                color: Theme.text
                font.family: Theme.fontHeading
                font.pixelSize: 18
                anchors.baseline: valueText.baseline
            }
        }

        Text {
            width: parent.width
            text: root.meta
            color: Theme.textMutedFaint
            font.family: Theme.fontBody
            font.pixelSize: 11
            // Meta lines now carry protocol caveats and can run long.
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
