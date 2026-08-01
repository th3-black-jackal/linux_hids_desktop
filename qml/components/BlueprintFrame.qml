import QtQuick
import Ironwatch
import IronwatchApp

// The design system's ".blueprint" treatment: a hairline box with four
// small register-mark crosses straddling its corners. Use as a plain
// container — children go in the default `contentItem`.
Rectangle {
    id: root
    color: "transparent"
    border.width: 1
    border.color: Theme.divider
    radius: 0

    default property alias content: contentItem.data
    property alias contentItem: contentItem

    Item {
        id: contentItem
        anchors.fill: parent
    }

    Repeater {
        model: [
            { corner: "tl" }, { corner: "tr" }, { corner: "bl" }, { corner: "br" }
        ]
        delegate: Item {
            width: 11
            height: 11
            x: modelData.corner === "tl" || modelData.corner === "bl" ? -6 : root.width - 5
            y: modelData.corner === "tl" || modelData.corner === "tr" ? -6 : root.height - 5

            Rectangle {
                x: 5; y: 0; width: 1; height: parent.height
                color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.55)
            }
            Rectangle {
                x: 0; y: 5; width: parent.width; height: 1
                color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.55)
            }
        }
    }
}
