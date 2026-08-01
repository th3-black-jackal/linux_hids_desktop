import QtQuick
import Ironwatch
import IronwatchApp

// .btn / .btn-primary / .btn-secondary from styles.css, flattened to the
// blueprint frame's square-cornered, hairline-bordered variant.
Rectangle {
    id: root
    property string text: ""
    property string variant: "secondary" // "primary" | "secondary"
    default property alias content: row.data

    signal clicked()

    implicitHeight: 30
    implicitWidth: row.implicitWidth + 24
    radius: 0
    border.width: 1
    border.color: variant === "primary" ? Theme.accent : Theme.divider
    color: {
        if (!enabled) return variant === "primary" ? Theme.accent : "transparent"
        if (variant === "primary")
            return mouse.pressed ? Qt.darker(Theme.accent, 1.25) : (mouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent)
        return mouse.pressed
            ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.14)
            : (mouse.containsMouse ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.07) : "transparent")
    }
    opacity: enabled ? 1.0 : 0.45

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6

        Text {
            text: root.text
            visible: text.length > 0
            anchors.verticalCenter: parent.verticalCenter
            color: root.variant === "primary" ? Theme.bg : Theme.text
            font.family: Theme.fontHeading
            font.weight: Font.DemiBold
            font.pixelSize: 13
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
