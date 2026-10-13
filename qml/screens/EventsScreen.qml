import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

// The event filter window: the same table as Detections, over the SIEM
// database rather than the live MQTT feed (SIEM_API_DOCUMENTATION.md).
//
// Filtering happens on the *backend*. Nothing here re-filters rows the
// console already holds, which is what lets the header count mean "matching
// rows in the database" rather than "rows that happen to be loaded".
Item {
    id: root

    // The row the operator clicked, as an index into the loaded page set.
    property int selectedRow: -1

    function applyFilters() {
        root.selectedRow = -1
        App.siem.clearDetail()
        App.siem.refresh()
    }

    Component.onCompleted: if (App.siem.configured && App.siem.events.count === 0) App.siem.refresh()

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
                    SectionLabel { text: "SIEM database" }
                    Text {
                        text: "Events"
                        color: Theme.text
                        font.family: Theme.fontHeading
                        font.weight: Font.DemiBold
                        font.pixelSize: 18
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: App.siem.events.count > 0
                        ? (App.siem.events.count + " of " + App.siem.events.totalCount + " loaded")
                        : ""
                    color: Theme.textMuted
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                }

                AppButton {
                    text: App.siem.loading ? "Loading…" : "Refresh"
                    enabled: !App.siem.loading && App.siem.configured
                    onClicked: root.applyFilters()
                }
                AppButton {
                    text: "Clear filters"
                    enabled: !App.siem.filters.isDefault
                    onClicked: { App.siem.filters.clear(); root.applyFilters() }
                }
                AppButton {
                    text: "Apply"
                    variant: "primary"
                    enabled: App.siem.configured
                    onClicked: root.applyFilters()
                }
            }
        }

        // ── Filter bar ───────────────────────────────────────────────────
        Rectangle {
            id: filterBar
            width: parent.width
            height: filterCol.implicitHeight + 24
            color: Theme.bg

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }

            Column {
                id: filterCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                anchors.topMargin: 12
                spacing: 10

                Row {
                    width: parent.width
                    spacing: 18

                    Column {
                        spacing: 5
                        Text { text: "RANGE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                        SegmentedControl {
                            options: ["1 h", "24 h", "7 d", "All time"]
                            currentIndex: {
                                switch (App.siem.filters.quickRange) {
                                case "1h": return 0
                                case "24h": return 1
                                case "7d": return 2
                                default: return 3
                                }
                            }
                            onActivated: function(index) {
                                App.siem.filters.quickRange = ["1h", "24h", "7d", ""][index]
                                root.applyFilters()
                            }
                        }
                    }

                    Column {
                        spacing: 5
                        Text { text: "TYPE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                        SegmentedControl {
                            // §5.2: `type` takes a class name, or the two
                            // pseudo-values `alert` (debouncer-confirmed) and
                            // `event` (everything).
                            options: ["All", "Alerts only"]
                            currentIndex: App.siem.filters.type === "alert" ? 1 : 0
                            onActivated: function(index) {
                                App.siem.filters.type = index === 1 ? "alert" : ""
                                root.applyFilters()
                            }
                        }
                    }

                    FormField {
                        width: 150
                        label: "Board (comma-separated)"
                        value: App.siem.filters.host
                        onEdited: function(t) { App.siem.filters.host = t; root.applyFilters() }
                    }
                    FormField {
                        width: 130
                        label: "Class name"
                        value: App.siem.filters.className
                        onEdited: function(t) { App.siem.filters.className = t; root.applyFilters() }
                    }
                    FormField {
                        width: 108
                        label: "Min confidence"
                        value: App.siem.filters.minConfidence
                        onEdited: function(t) { App.siem.filters.minConfidence = t; root.applyFilters() }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 18

                    FormField {
                        width: 168
                        label: "Since (UTC, ISO-8601)"
                        value: App.siem.filters.since
                        onEdited: function(t) { App.siem.filters.since = t; root.applyFilters() }
                    }
                    FormField {
                        width: 168
                        label: "Until (UTC, ISO-8601)"
                        value: App.siem.filters.until
                        onEdited: function(t) { App.siem.filters.until = t; root.applyFilters() }
                    }
                    FormField {
                        width: 120
                        label: "Day (YYYY-MM-DD)"
                        value: App.siem.filters.date
                        onEdited: function(t) { App.siem.filters.date = t; root.applyFilters() }
                    }
                    FormField {
                        width: 110
                        label: "Source topic"
                        value: App.siem.filters.sourceTopic
                        onEdited: function(t) { App.siem.filters.sourceTopic = t; root.applyFilters() }
                    }

                    // events/stats/ for exactly this filter (§4.3).
                    Column {
                        spacing: 5
                        visible: App.siem.statsTotal > 0
                        Text { text: "MATCHING"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                        Row {
                            spacing: 14
                            height: 34
                            Text {
                                text: App.siem.statsTotal + " events"
                                color: Theme.text; font.pixelSize: 13
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: App.siem.statsAlerts + " alerts"
                                color: Theme.text; font.pixelSize: 13
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: App.siem.statsByClass.length + " classes · "
                                      + App.siem.statsByHost.length + " boards"
                                color: Theme.textMuted; font.pixelSize: 12
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                // §6: the 400 body names the offending param, so the message
                // points at the control that has to change.
                Text {
                    width: parent.width
                    visible: App.siem.lastError.length > 0
                    text: App.siem.lastError
                    color: Theme.text
                    font.family: "ui-monospace, monospace"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── Table + inspector ────────────────────────────────────────────
        Row {
            width: parent.width
            height: parent.height - 58 - filterBar.height

            Item {
                id: tableArea
                width: parent.width - 392
                height: parent.height

                // y of the row the explain popup was opened from.
                property real explainY: 0

                Column {
                    anchors.fill: parent
                    spacing: 0

                    // Header row. Clicking a sortable column re-queries with
                    // `ordering` (§7.1) rather than sorting locally — the
                    // table only ever holds part of the result set.
                    Row {
                        id: headerRow
                        width: parent.width
                        height: 30

                        component SortHeader: Item {
                            id: sh
                            property string title: ""
                            property string field: ""
                            property int cellWidth: 100
                            property int leftPad: 0
                            property bool alignRight: false
                            width: cellWidth
                            height: 30

                            Text {
                                anchors.fill: parent
                                leftPadding: sh.leftPad
                                rightPadding: sh.alignRight ? 12 : 0
                                horizontalAlignment: sh.alignRight ? Text.AlignRight : Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                text: sh.title + (sh.field.length === 0 ? ""
                                      : (App.siem.filters.orderingFor(sh.field) === "desc" ? "  ▼"
                                      : (App.siem.filters.orderingFor(sh.field) === "asc" ? "  ▲" : "")))
                                color: App.siem.filters.orderingFor(sh.field).length > 0
                                    ? Theme.text : Theme.textMuted
                                font.pixelSize: 11
                            }

                            MouseArea {
                                anchors.fill: parent
                                enabled: sh.field.length > 0
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    App.siem.filters.toggleOrdering(sh.field)
                                    root.applyFilters()
                                }
                            }
                        }

                        SortHeader { title: "#"; cellWidth: 44; leftPad: 22 }
                        SortHeader { title: "TIME"; field: "event_time"; cellWidth: 124 }
                        SortHeader { title: "SEVERITY"; cellWidth: 104 }
                        SortHeader { title: "BOARD"; cellWidth: 140 }
                        SortHeader {
                            title: "CLASS"
                            cellWidth: Math.max(60, headerRow.width - 44 - 124 - 104 - 140 - 90 - 100)
                        }
                        SortHeader { title: "CONFIDENCE"; field: "confidence"; cellWidth: 90; alignRight: true }
                        SortHeader { title: "SOURCE"; cellWidth: 100 }
                    }
                    Rectangle { width: parent.width; height: 1; color: Theme.divider }

                    Item {
                        width: parent.width
                        height: parent.height - 31

                        ListView {
                            id: eventsView
                            anchors.fill: parent
                            clip: true
                            model: App.siem.events

                            // §7.2: lazy loading by following `next`. Fired
                            // from the view rather than a button so a long
                            // result set behaves like one continuous table.
                            onContentYChanged: {
                                if (App.siem.events.hasMore && !App.siem.loading
                                        && contentY > 0
                                        && contentY + height > contentHeight - 400)
                                    App.siem.loadMore()
                            }

                            delegate: Rectangle {
                                id: erow
                                required property int index
                                required property string n
                                required property int eventId
                                required property string time
                                required property string timeRaw
                                required property int classIdx
                                required property string severity
                                required property string node
                                required property string className
                                required property string confidenceText
                                required property bool alert
                                required property string sourceTopic

                                width: ListView.view.width
                                height: 32
                                color: erow.index === root.selectedRow
                                    ? Theme.rowSelected
                                    : (erowMouse.containsMouse ? Theme.rowHover : "transparent")

                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.cellBorder }

                                // §7.2: highlight rows where alert === true.
                                // A left rule rather than a fill, so it reads
                                // alongside the severity chip instead of
                                // fighting it.
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 1
                                    width: 3
                                    visible: erow.alert
                                    color: Theme.accent
                                }

                                Text { x: 0; text: erow.n; width: 44; leftPadding: 22; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { x: 44; text: erow.time; width: 124; color: Theme.textMuted; font.family: "ui-monospace, monospace"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                TagBadge {
                                    x: 44 + 124
                                    width: 96
                                    text: erow.severity
                                    bg: Theme.severityColors(erow.severity).bg
                                    fg: Theme.severityColors(erow.severity).fg
                                    borderColor: Theme.severityColors(erow.severity).border
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text { x: 44 + 124 + 104; text: erow.node; width: 140; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    x: 44 + 124 + 104 + 140
                                    text: erow.className
                                    width: Math.max(60, erow.width - 44 - 124 - 104 - 140 - 90 - 100)
                                    color: Theme.text
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    x: erow.width - 100 - 90
                                    text: erow.confidenceText
                                    width: 90
                                    rightPadding: 12
                                    horizontalAlignment: Text.AlignRight
                                    color: Theme.text
                                    font.pixelSize: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    x: erow.width - 100
                                    text: erow.sourceTopic
                                    width: 100
                                    color: Theme.textMuted
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                MouseArea {
                                    id: erowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    cursorShape: Qt.PointingHandCursor
                                    onDoubleClicked: function(mouse) {
                                        if (mouse.button === Qt.LeftButton) {
                                            root.selectedRow = erow.index
                                            App.openEventRowInWeb(erow.eventId, erow.node,
                                                                  erow.className, erow.classIdx,
                                                                  erow.confidenceText,
                                                                  erow.timeRaw, erow.severity)
                                        }
                                    }
                                    onClicked: function(mouse) {
                                        root.selectedRow = erow.index
                                        if (mouse.button === Qt.RightButton) {
                                            tableArea.explainY = erow.mapToItem(tableArea, 0, 0).y
                                            App.explainEventRow(erow.node, erow.className,
                                                                erow.classIdx, erow.confidenceText,
                                                                erow.timeRaw, erow.severity,
                                                                erow.eventId)
                                        } else {
                                            // features/raw are only on the detail
                                            // endpoint (§7.5), so this is a second
                                            // request, made only when a row is
                                            // actually opened.
                                            App.siem.fetchDetail(erow.eventId)
                                        }
                                    }
                                }
                            }

                            footer: Item {
                                width: eventsView.width
                                height: App.siem.events.hasMore || App.siem.loading ? 44 : 0
                                Text {
                                    anchors.centerIn: parent
                                    visible: App.siem.loading
                                    text: "loading…"
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                }
                                AppButton {
                                    anchors.centerIn: parent
                                    visible: App.siem.events.hasMore && !App.siem.loading
                                    text: "Load more"
                                    onClicked: App.siem.loadMore()
                                }
                            }
                        }

                        EmptyState {
                            anchors.fill: parent
                            visible: App.siem.events.count === 0 && !App.siem.loading
                                     && !App.explainer.active
                            title: !App.siem.configured
                                ? "No backend configured"
                                : (App.siem.lastError.length > 0
                                    ? "The query failed"
                                    : "No events match this filter")
                            detail: !App.siem.configured
                                ? "The SIEM database lives on the backend. Set its base URL and "
                                  + "API token on the Integrations screen."
                                : (App.siem.lastError.length > 0
                                    ? App.siem.lastError
                                    : "Widen the time range, or clear the filters.")
                            actionText: !App.siem.configured ? "Open Integrations" : ""
                            onActionClicked: App.goIntegrations()
                        }
                    }
                }

                AlertExplainPopup {
                    anchors.fill: parent
                    anchorY: tableArea.explainY
                }
            }

            // ── Inspector ────────────────────────────────────────────────
            Rectangle {
                width: 392
                height: parent.height
                color: "transparent"

                Rectangle { anchors.left: parent.left; width: 1; anchors.top: parent.top; anchors.bottom: parent.bottom; color: Theme.divider }

                EmptyState {
                    anchors.fill: parent
                    anchors.margins: 20
                    visible: App.siem.detailId < 0
                    title: "Nothing selected"
                    detail: "Pick a row to fetch its stored feature vector and the original MQTT "
                          + "payload the backend recorded."
                }

                Loader {
                    anchors.fill: parent
                    anchors.leftMargin: 21
                    anchors.rightMargin: 20
                    anchors.topMargin: 16
                    anchors.bottomMargin: 22
                    active: App.siem.detailId >= 0
                    visible: active
                    sourceComponent: detailComponent
                }

                Component {
                    id: detailComponent

                    ScrollView {
                        contentWidth: availableWidth
                        clip: true

                        Column {
                            width: 392 - 41
                            spacing: 16

                            Column {
                                width: parent.width
                                spacing: 6
                                Row {
                                    spacing: 8
                                    TagBadge {
                                        readonly property string sev: App.siem.detail.class_name !== undefined
                                            ? Theme.severityColors("info").toString() : ""
                                        text: "event " + App.siem.detailId
                                        bg: "transparent"
                                        fg: Theme.textMuted
                                        borderColor: Theme.divider
                                    }
                                    TagBadge {
                                        visible: App.siem.detail.alert === true
                                        text: "confirmed alert"
                                        bg: Theme.accent200
                                        fg: Theme.accent900
                                        borderColor: Theme.accent300
                                    }
                                }
                                Text {
                                    width: parent.width
                                    text: App.siem.detail.class_name !== undefined
                                        ? String(App.siem.detail.class_name) : "…"
                                    color: Theme.text
                                    font.family: Theme.fontHeading
                                    font.weight: Font.DemiBold
                                    font.pixelSize: 18
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    visible: App.siem.detailLoading
                                    text: "fetching /api/siem/events/" + App.siem.detailId + "/ …"
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                }
                                Text {
                                    width: parent.width
                                    visible: App.siem.detailError.length > 0
                                    text: App.siem.detailError
                                    color: Theme.text
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: Theme.divider }

                            Grid {
                                width: parent.width
                                columns: 2
                                columnSpacing: 14
                                rowSpacing: 10

                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "BOARD"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: App.siem.detail.host !== undefined ? String(App.siem.detail.host) : "—"; color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "SOURCE TOPIC"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: App.siem.detail.source_topic !== undefined ? String(App.siem.detail.source_topic) : "—"; color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "EVENT TIME (UTC)"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: App.siem.detail.event_time !== undefined ? String(App.siem.detail.event_time) : "—"; color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "STORED AT (UTC)"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { width: parent.width; text: App.siem.detail.received_at !== undefined ? String(App.siem.detail.received_at) : "—"; color: Theme.text; font.family: "ui-monospace, monospace"; font.pixelSize: 12; elide: Text.ElideRight }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "CLASS INDEX"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { text: App.siem.detail.class_idx !== undefined ? String(App.siem.detail.class_idx) : "—"; color: Theme.text; font.pixelSize: 13 }
                                }
                                Column {
                                    width: (parent.width - 14) / 2
                                    Text { text: "CONFIDENCE"; color: Theme.textMuted; font.pixelSize: 10; font.letterSpacing: 1.0 }
                                    Text { text: App.siem.detail.confidence !== undefined ? Number(App.siem.detail.confidence).toFixed(4) : "—"; color: Theme.text; font.pixelSize: 13 }
                                }
                            }

                            AppButton {
                                width: parent.width
                                visible: App.webLink.enabled && App.siem.detailId >= 0
                                text: "Open in Orchicyb AI ATTACK"
                                onClicked: App.openEventRowInWeb(
                                    App.siem.detailId,
                                    App.siem.detail.host !== undefined ? String(App.siem.detail.host) : "",
                                    App.siem.detail.class_name !== undefined ? String(App.siem.detail.class_name) : "",
                                    App.siem.detail.class_idx !== undefined ? Number(App.siem.detail.class_idx) : -1,
                                    App.siem.detail.confidence !== undefined ? Number(App.siem.detail.confidence).toFixed(3) : "",
                                    App.siem.detail.event_time !== undefined ? String(App.siem.detail.event_time) : "",
                                    "")
                            }

                            Rectangle { width: parent.width; height: 1; color: Theme.divider }

                            Column {
                                width: parent.width
                                spacing: 8

                                Text { text: "FEATURE VECTOR"; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }

                                Text {
                                    width: parent.width
                                    // §3.2: features may legitimately be null
                                    // if the producer disabled them. That is a
                                    // fact about the row, not a fetch failure.
                                    visible: !App.siem.detailLoading
                                             && (App.siem.detail.features === undefined
                                                 || App.siem.detail.features === null)
                                    text: App.siem.detailError.length > 0
                                        ? "Not loaded."
                                        : "The producer stored no feature vector for this event."
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    visible: App.siem.detail.features !== undefined
                                             && App.siem.detail.features !== null
                                    text: (App.siem.detail.features ? App.siem.detail.features.length : 0)
                                          + " normalized floats, in FEATURE_ORDER. Index only — the "
                                          + "backend does not send the name mapping."
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }

                                Grid {
                                    width: parent.width
                                    columns: 4
                                    columnSpacing: 8
                                    rowSpacing: 3
                                    visible: App.siem.detail.features !== undefined
                                             && App.siem.detail.features !== null

                                    Repeater {
                                        model: App.siem.detail.features ? App.siem.detail.features : []
                                        delegate: Text {
                                            required property int index
                                            required property var modelData
                                            width: (parent.width - 24) / 4
                                            text: "f[" + index + "] " + Number(modelData).toFixed(3)
                                            color: Theme.textMuted
                                            font.family: "ui-monospace, monospace"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }

                            Rectangle { width: parent.width; height: 1; color: Theme.divider }

                            Column {
                                width: parent.width
                                spacing: 8
                                Text { text: "ORIGINAL PAYLOAD"; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
                                Text {
                                    width: parent.width
                                    text: "Exactly the MQTT JSON the board published, as stored for "
                                        + "audit and replay."
                                    color: Theme.textMuted
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                Rectangle {
                                    width: parent.width
                                    height: rawText.implicitHeight + 16
                                    color: Theme.surface
                                    border.width: 1
                                    border.color: Theme.divider
                                    visible: App.siem.detail.raw !== undefined
                                    Text {
                                        id: rawText
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 8
                                        text: App.siem.detail.raw !== undefined
                                            ? JSON.stringify(App.siem.detail.raw, null, 2) : ""
                                        color: Theme.text
                                        font.family: "ui-monospace, monospace"
                                        font.pixelSize: 11
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: "This screen is read-only: /api/siem/ has no write endpoints. "
                                    + "Acknowledgement and triage state live in the console's own "
                                    + "Detections queue and are not persisted to the backend."
                                color: Theme.textMutedFaint
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
