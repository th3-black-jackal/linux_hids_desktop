import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

import "../util/Citations.js" as Citations

// Operator-facing chat with the ATT&CK RAG backend (see API_DOCUMENTATION.md).
//
// Everything on this screen comes from the backend's own response: the answer
// text, and the `sources` it says it retrieved. The console adds no
// interpretation of its own — in particular it does not link a citation the
// backend did not ground (see util/Citations.js).
Item {
    id: root

    function send(text) {
        var q = text.trim()
        // Same guards the Send button enforces, because Enter bypasses it.
        if (q.length === 0 || App.chat.busy || !App.chat.configured)
            return
        App.chat.ask(q)
        composer.text = ""
        transcriptView.positionViewAtEnd()
    }

    Connections {
        target: App.chat

        // A question drafted elsewhere (the Detections inspector) lands in
        // the composer rather than being sent — the operator edits it first.
        function onDraftChanged() {
            composer.text = App.chat.draft
            composer.forceActiveFocus()
            composer.cursorPosition = composer.length
        }
        function onTokenReceived(text) {
            if (transcriptView.atYEnd || root.stickToBottom)
                transcriptView.positionViewAtEnd()
        }
        function onSourcesReceived(sources) { transcriptView.positionViewAtEnd() }
    }

    // Follow the stream unless the operator has scrolled up to re-read
    // something; then leave the viewport where they put it.
    property bool stickToBottom: true

    Column {
        anchors.fill: parent
        spacing: 0

        // ── Header ───────────────────────────────────────────────────────
        Rectangle {
            width: parent.width
            height: 58
            color: Theme.bg

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 10

                Column {
                    SectionLabel { text: "Knowledge base" }
                    Text {
                        text: "ATT&CK Assistant"
                        color: Theme.text
                        font.family: Theme.fontHeading
                        font.weight: Font.DemiBold
                        font.pixelSize: 18
                    }
                }

                Item { Layout.fillWidth: true }

                TagBadge {
                    readonly property string s: App.chat.busy
                        ? "generating"
                        : (App.chat.configured ? "ready" : "not configured")
                    readonly property var c: Theme.nodeStateColors(
                        App.chat.busy ? "connecting"
                                       : (App.chat.configured ? "connected" : "offline"))
                    text: s
                    bg: c.bg; fg: c.fg; borderColor: c.border
                    Layout.alignment: Qt.AlignVCenter
                }

                AppButton {
                    text: "Stop"
                    visible: App.chat.busy
                    onClicked: App.chat.cancel()
                }
                AppButton {
                    text: "New conversation"
                    enabled: App.chat.transcript.count > 0
                    onClicked: App.chat.newConversation()
                }
            }
        }

        // ── Transcript ───────────────────────────────────────────────────
        Item {
            width: parent.width
            height: parent.height - 58 - composerBox.height

            ListView {
                id: transcriptView
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                topMargin: 16
                bottomMargin: 16
                clip: true
                spacing: 16
                visible: App.chat.transcript.count > 0
                model: App.chat.transcript

                onMovementEnded: root.stickToBottom = atYEnd
                onCountChanged: positionViewAtEnd()

                delegate: Item {
                    id: turn
                    required property int index
                    required property string role
                    required property string text
                    required property var sources
                    required property string state
                    required property string error
                    required property string time

                    readonly property bool isUser: role === "user"

                    width: ListView.view.width
                    height: bubble.height

                    Column {
                        id: bubble
                        width: turn.isUser ? Math.min(parent.width, 640) : parent.width
                        anchors.right: turn.isUser ? parent.right : undefined
                        anchors.left: turn.isUser ? undefined : parent.left
                        spacing: 6

                        Row {
                            spacing: 8
                            layoutDirection: turn.isUser ? Qt.RightToLeft : Qt.LeftToRight
                            width: bubble.width

                            SectionLabel {
                                text: turn.isUser ? "Operator" : "Assistant"
                                bottomPadding: 0
                            }
                            Text {
                                text: turn.time
                                color: Theme.textMutedFaint
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Question / answer body.
                        Rectangle {
                            width: bubble.width
                            height: body.implicitHeight + 24
                            color: turn.isUser ? Theme.surface : "transparent"
                            border.width: 1
                            border.color: turn.state === "error" ? Theme.accent700 : Theme.divider

                            Column {
                                id: body
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 12
                                spacing: 8

                                Text {
                                    width: parent.width
                                    visible: turn.text.length > 0
                                    // Plain text for the operator's own words;
                                    // rich text for the answer, so citations
                                    // can be links.
                                    textFormat: turn.isUser ? Text.PlainText : Text.RichText
                                    text: turn.isUser
                                        ? turn.text
                                        : Citations.render(turn.text, turn.sources,
                                                           Theme.accent700, Theme.textMuted)
                                    color: Theme.text
                                    font.family: Theme.fontBody
                                    font.pixelSize: 13
                                    wrapMode: Text.WordWrap
                                    onLinkActivated: function(link) { Qt.openUrlExternally(link) }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.NoButton
                                        cursorShape: parent.hoveredLink.length > 0
                                            ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    }
                                }

                                // Waiting for the first token. The backend
                                // retrieves and loads the model before it
                                // emits anything, so this gap is normal and
                                // can run into seconds.
                                Row {
                                    spacing: 8
                                    visible: turn.state === "waiting"
                                    Rectangle {
                                        width: 7; height: 7
                                        color: Theme.accent
                                        anchors.verticalCenter: parent.verticalCenter
                                        SequentialAnimation on opacity {
                                            running: turn.state === "waiting"
                                            loops: Animation.Infinite
                                            NumberAnimation { from: 1.0; to: 0.2; duration: 700 }
                                            NumberAnimation { from: 0.2; to: 1.0; duration: 700 }
                                        }
                                    }
                                    Text {
                                        text: "retrieving context and generating…"
                                        color: Theme.textMuted
                                        font.family: Theme.fontBody
                                        font.pixelSize: 12
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Text {
                                    width: parent.width
                                    visible: turn.state === "error"
                                    text: turn.error
                                    color: Theme.text
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    visible: turn.state === "cancelled"
                                    text: "Stopped before the answer finished."
                                    color: Theme.textMuted
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                }

                                // Only on the most recent turn: retryLast()
                                // re-sends the last question, so offering it
                                // higher up the transcript would be a lie
                                // about which question is being retried.
                                AppButton {
                                    text: "Retry"
                                    visible: !turn.isUser && !App.chat.busy
                                             && (turn.state === "error" || turn.state === "cancelled")
                                             && turn.index === App.chat.transcript.count - 1
                                    onClicked: App.chat.retryLast()
                                }
                            }
                        }

                        // Retrieved sources, shown as soon as the `sources`
                        // event lands — before a single token of the answer.
                        // The label sits outside the Flow: Flow refuses to lay
                        // out children that use anchors, and silently stacks
                        // every chip at the origin if one does.
                        Column {
                            width: bubble.width
                            visible: !turn.isUser && turn.sources && turn.sources.length > 0
                            spacing: 5

                            Text {
                                text: "GROUNDED IN"
                                color: Theme.textMuted
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                font.letterSpacing: 1.0
                            }

                            Flow {
                                width: parent.width
                                spacing: 6

                                Repeater {
                                    model: turn.sources
                                    delegate: TagBadge {
                                        id: chip
                                        required property var modelData
                                        text: modelData.title && modelData.title.length > 0
                                            ? modelData.title : modelData.id
                                        bg: chipMouse.containsMouse ? Theme.rowSelected : "transparent"
                                        fg: modelData.url && modelData.url.length > 0
                                            ? Theme.accent700 : Theme.textMuted
                                        borderColor: Theme.divider

                                        MouseArea {
                                            id: chipMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: chip.modelData.url && chip.modelData.url.length > 0
                                                ? Qt.PointingHandCursor : Qt.ArrowCursor
                                            onClicked: if (chip.modelData.url && chip.modelData.url.length > 0)
                                                           Qt.openUrlExternally(chip.modelData.url)
                                            ToolTip.visible: containsMouse
                                            ToolTip.delay: 400
                                            ToolTip.text: chip.modelData.type + " · similarity "
                                                + Number(chip.modelData.score).toFixed(3)
                                                + (chip.modelData.url ? "\n" + chip.modelData.url : "")
                                        }
                                }
                            }
                            }
                        }

                        // Honest footnote: the answer may cite ids the
                        // retrieval step never returned.
                        Text {
                            width: bubble.width
                            readonly property var g: (!turn.isUser && turn.state === "done")
                                ? Citations.groundedCount(turn.text, turn.sources)
                                : { cited: 0, grounded: 0 }
                            visible: g.cited > g.grounded
                            text: (g.cited - g.grounded) + " of " + g.cited + " citations in this "
                                + "answer are not in the retrieved sources and are not linked."
                            color: Theme.textMutedFaint
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // Nothing asked yet — either because the backend is unconfigured,
            // or just because the conversation is new.
            EmptyState {
                anchors.fill: parent
                anchors.margins: 24
                visible: App.chat.transcript.count === 0 && !App.chat.configured
                title: "No assistant backend configured"
                detail: "The ATT&CK assistant runs on a separate machine. Set its base URL "
                      + "and API token on the Integrations screen, then come back."
                actionText: "Open Integrations"
                onActionClicked: App.goIntegrations()
            }

            Column {
                anchors.centerIn: parent
                width: Math.min(parent.width - 60, 520)
                spacing: 14
                visible: App.chat.transcript.count === 0 && App.chat.configured

                Text {
                    width: parent.width
                    text: "Ask about a detection"
                    color: Theme.text
                    font.family: Theme.fontHeading
                    font.weight: Font.DemiBold
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    width: parent.width
                    text: "Answers are generated by an LLM on the backend and grounded in "
                        + "retrieved MITRE ATT&CK entries. Treat them as a research aid, not "
                        + "as a verdict on an alert — check the cited sources."
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Column {
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: [
                            "How do adversaries block PLC command messages, and how do I mitigate it?",
                            "Which ATT&CK techniques involve unusual system-call patterns on Linux hosts?",
                            "What mitigations exist for unauthorized command execution on an edge device?"
                        ]
                        delegate: Rectangle {
                            id: suggestion
                            required property string modelData
                            width: parent.width
                            height: sText.implicitHeight + 18
                            color: sMouse.containsMouse ? Theme.rowHover : "transparent"
                            border.width: 1
                            border.color: Theme.divider

                            Text {
                                id: sText
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: suggestion.modelData
                                color: Theme.text
                                font.family: Theme.fontBody
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }

                            MouseArea {
                                id: sMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.send(suggestion.modelData)
                            }
                        }
                    }
                }
            }
        }

        // ── Composer ─────────────────────────────────────────────────────
        Rectangle {
            id: composerBox
            width: parent.width
            // Grows with the question — a draft seeded from a detection runs
            // to three or four lines, and a fixed 64px box hid the start of
            // it — but stops well short of eating the transcript.
            height: inputFrame.height + 44
            color: Theme.bg

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.divider }

            // Failures are reported on the turn they belong to, in the
            // transcript — there is deliberately no banner here repeating
            // the same text a few pixels lower down.
            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 22
                anchors.topMargin: 7
                visible: App.chat.busy
                text: "One generation at a time — the backend serializes on a single GPU."
                color: Theme.textMutedFaint
                font.family: Theme.fontBody
                font.pixelSize: 11
            }

            Rectangle {
                id: inputFrame
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                anchors.bottomMargin: 12
                height: Math.max(64, Math.min(180, composer.implicitHeight + 14))
                color: Theme.surface
                border.width: 1
                border.color: composer.activeFocus ? Theme.accent : Theme.divider

                ScrollView {
                    anchors.left: parent.left
                    anchors.right: sendCol.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 6
                    anchors.rightMargin: 10
                    clip: true

                    TextArea {
                        id: composer
                        background: null
                        color: Theme.text
                        font.family: Theme.fontBody
                        font.pixelSize: 13
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        placeholderText: App.chat.configured
                            ? "Ask about an alert, a class label, or an ATT&CK technique — Enter to send, Shift+Enter for a new line"
                            : "Configure the assistant backend on the Integrations screen first"
                        placeholderTextColor: Theme.textMuted

                        Keys.onReturnPressed: function(event) {
                            if (event.modifiers & Qt.ShiftModifier) {
                                event.accepted = false
                            } else {
                                event.accepted = true
                                root.send(text)
                            }
                        }
                        Keys.onEnterPressed: function(event) {
                            if (event.modifiers & Qt.ShiftModifier) {
                                event.accepted = false
                            } else {
                                event.accepted = true
                                root.send(text)
                            }
                        }
                    }
                }

                Column {
                    id: sendCol
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 8
                    spacing: 4

                    AppButton {
                        width: 84
                        text: App.chat.busy ? "Generating" : "Send"
                        variant: "primary"
                        // One generation at a time: the backend serializes on
                        // a single GPU (§6.7).
                        enabled: !App.chat.busy && App.chat.configured
                                 && composer.text.trim().length > 0
                        onClicked: root.send(composer.text)
                    }
                }
            }
        }
    }
}
