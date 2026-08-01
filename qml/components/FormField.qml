import QtQuick
import Ironwatch
import IronwatchApp

// .field / .input pairing used across the config forms.
Column {
    id: root
    property string label: ""
    // Seed/display value. Assigning to it replaces what the field shows.
    property string value: ""
    // What the field currently contains, updated on every keystroke — read
    // this when the field is an input rather than a bound display of state
    // (e.g. the "add board" row, which is read on button click, not on
    // editingFinished).
    readonly property alias currentText: input.text
    property bool isPassword: false
    property bool readOnly: false
    signal edited(string text)

    spacing: 5
    width: 200

    onValueChanged: if (input.text !== value) input.text = value

    Text {
        text: root.label
        color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.70)
        font.family: Theme.fontBody
        font.pixelSize: 12
    }

    Rectangle {
        width: parent.width
        height: 34
        color: Theme.surface
        border.width: 1
        border.color: input.activeFocus ? Theme.accent : Theme.divider
        radius: 0

        TextInput {
            id: input
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            verticalAlignment: TextInput.AlignVCenter
            // Seeded from `value` (see onValueChanged above) rather than bound
            // to it, so typing isn't fought by the binding.
            Component.onCompleted: text = root.value
            echoMode: root.isPassword ? TextInput.Password : TextInput.Normal
            readOnly: root.readOnly
            color: Theme.text
            font.family: Theme.fontBody
            font.pixelSize: 13
            selectByMouse: true
            onEditingFinished: root.edited(text)
        }
    }
}
