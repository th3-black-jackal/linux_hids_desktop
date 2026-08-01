import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Ironwatch
import IronwatchApp

Item {
    id: root

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Column {
            width: root.width
            topPadding: 18
            bottomPadding: 24
            leftPadding: 22
            rightPadding: 22
            spacing: 16

            RowLayout {
                width: parent.width - 44
                spacing: 12

                Column {
                    SectionLabel { text: "Detection engine" }
                    Text { text: "Model output"; color: Theme.text; font.family: Theme.fontHeading; font.weight: Font.DemiBold; font.pixelSize: 20 }
                }
                Item { Layout.fillWidth: true }
            }

            Grid {
                width: parent.width - 44
                columns: 3
                columnSpacing: 16
                rowSpacing: 16

                StatCard {
                    width: (parent.width - 32) / 3
                    valuePixelSize: 26
                    kicker: "Classes observed"
                    value: String(App.classStats.distinctClasses)
                    meta: "distinct classes seen on the wire so far"
                }
                StatCard {
                    width: (parent.width - 32) / 3
                    valuePixelSize: 26
                    kicker: "Windows scored"
                    value: String(App.classStats.totalWindows)
                    meta: "across all connected boards"
                }
                StatCard {
                    width: (parent.width - 32) / 3
                    valuePixelSize: 26
                    kicker: "Feature vector"
                    value: App.selectedNode && App.selectedNode.featureCount > 0
                        ? String(App.selectedNode.featureCount) : "—"
                    meta: "length of the normalized input, per FEATURE_ORDER"
                }
            }

            Row {
                width: parent.width - 44
                spacing: 20

                BlueprintFrame {
                    width: (parent.width - 20) / 2
                    height: policyCol.implicitHeight + 34

                    Column {
                        id: policyCol
                        anchors.fill: parent
                        anchors.margins: 17
                        spacing: 12

                        Text { text: "Severity thresholds"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: "Console-side only. The service reports a class and a confidence; how "
                                + "that maps to a triage severity is decided here and affects nothing on "
                                + "the board."
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Grid {
                            width: parent.width
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 12

                            FormField {
                                width: (parent.width - 14) / 2
                                label: "Critical at ≥"
                                value: App.telemetry.criticalAt.toFixed(2)
                                onEdited: function(text) {
                                    var v = parseFloat(text)
                                    if (!isNaN(v)) App.telemetry.criticalAt = v
                                }
                            }
                            FormField {
                                width: (parent.width - 14) / 2
                                label: "High at ≥"
                                value: App.telemetry.highAt.toFixed(2)
                                onEdited: function(text) {
                                    var v = parseFloat(text)
                                    if (!isNaN(v)) App.telemetry.highAt = v
                                }
                            }
                            FormField {
                                width: (parent.width - 14) / 2
                                label: "Medium at ≥ (board min_confidence)"
                                value: App.telemetry.mediumAt.toFixed(2)
                                onEdited: function(text) {
                                    var v = parseFloat(text)
                                    if (!isNaN(v)) App.telemetry.mediumAt = v
                                }
                            }
                            FormField {
                                width: (parent.width - 14) / 2
                                label: "Normal class names"
                                value: App.telemetry.normalClassesText
                                onEdited: function(text) { App.telemetry.normalClassesText = text }
                            }
                        }

                        FormField {
                            width: parent.width
                            label: "Feature names file (one per line, FEATURE_ORDER order)"
                            value: App.telemetry.featureNamesPath
                            onEdited: function(text) { App.telemetry.featureNamesPath = text }
                        }
                        Text {
                            width: parent.width
                            text: App.telemetry.featureNamesLoaded > 0
                                ? (App.telemetry.featureNamesLoaded + " feature names loaded.")
                                : "No names loaded — features show as f[0], f[1], … Paste FEATURE_ORDER "
                                  + "into a text file to label them."
                            color: Theme.textMutedFaint
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                BlueprintFrame {
                    width: (parent.width - 20) / 2
                    height: classCol.implicitHeight + 34

                    Column {
                        id: classCol
                        anchors.fill: parent
                        anchors.margins: 17
                        spacing: 10

                        Text { text: "Observed classes"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: "CLASS_NAMES, the build version and the training metrics live in the "
                                + "board's model_meta.hpp and are never transmitted. This is what has "
                                + "actually come off the wire."
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Row {
                            width: parent.width
                            Text { text: "IDX"; width: 44; color: Theme.textMuted; font.pixelSize: 11 }
                            Text { text: "CLASS"; width: Math.max(60, parent.width - 44 - 80 - 80 - 80); color: Theme.textMuted; font.pixelSize: 11 }
                            Text { text: "COUNT"; width: 80; rightPadding: 10; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11 }
                            Text { text: "SHARE"; width: 80; rightPadding: 10; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11 }
                            Text { text: "MEAN"; width: 80; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                        Rectangle { width: parent.width; height: 1; color: Theme.divider }

                        Repeater {
                            model: App.classStats
                            delegate: Rectangle {
                                id: clsRow
                                required property int classIdx
                                required property string className
                                required property var count
                                required property string shareText
                                required property string meanConfidence
                                required property bool isNormal

                                width: parent.width
                                height: 28
                                color: "transparent"
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.cellBorder }

                                Text { x: 0; text: String(clsRow.classIdx); width: 44; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text {
                                    x: 44
                                    text: clsRow.className
                                    width: Math.max(60, clsRow.width - 44 - 80 - 80 - 80)
                                    color: clsRow.isNormal ? Theme.textMuted : Theme.text
                                    font.family: "ui-monospace, monospace"
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text { x: clsRow.width - 240; text: String(clsRow.count); width: 80; rightPadding: 10; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { x: clsRow.width - 160; text: clsRow.shareText; width: 80; rightPadding: 10; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                                Text { x: clsRow.width - 80; text: clsRow.meanConfidence; width: 80; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }

                        EmptyState {
                            width: parent.width
                            visible: App.classStats.distinctClasses === 0
                            title: "No classes observed"
                            detail: "Connect a board to populate this table."
                        }
                    }
                }
            }

            BlueprintFrame {
                width: parent.width - 44
                height: boardCfgCol.implicitHeight + 34

                Column {
                    id: boardCfgCol
                    anchors.fill: parent
                    anchors.margins: 17
                    spacing: 8

                    Text { text: "Board-side configuration"; color: Theme.text; font.family: Theme.fontHeading; font.pixelSize: 15; font.weight: Font.DemiBold }
                    Text {
                        width: parent.width
                        text: "These are read by the service from hids.conf at startup (Config::load) and "
                            + "cannot be read or changed from the console — there is no control channel, "
                            + "only the one-way telemetry stream. Model deployment likewise happens on the "
                            + "board; the console has no way to push a build.\n\n"
                            + "    model_path       ONNX/model artifact loaded by InferenceEngine\n"
                            + "    window_sec       seconds per scored window (observed: "
                            + App.telemetry.windowPeriodText + ")\n"
                            + "    telemetry_port   the port this console dials\n"
                            + "    host             the value that arrives as the event's host field\n"
                            + "    alert_streak     consecutive non-normal windows before a Wazuh alert\n"
                            + "    min_confidence   confidence floor for that alert\n"
                            + "    log_only_alerts  whether every window is written to the Wazuh log\n"
                            + "    wazuh_log_path   file the Wazuh agent tails"
                        color: Theme.textMuted
                        font.family: "ui-monospace, monospace"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
