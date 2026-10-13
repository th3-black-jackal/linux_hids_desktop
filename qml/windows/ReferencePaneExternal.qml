import QtQuick
import Ironwatch
import IronwatchApp

import "../components"

// Reference pane, fallback variant — used when Qt WebEngine is not available
// in the build (Intel.webEngineAvailable == false). No embedded browser: it
// shows the reference URL and opens it in the system browser on request.
// Imports only standard modules, so it builds on any Qt kit.
//
// Note: opening a URL here hands it to the operator's real browser, outside
// the app's sandbox. Reference/reputation URLs (MITRE, NVD, VirusTotal, news)
// are safe to open; a raw IOC URL still requires the explicit danger opt-in.
Item {
    id: pane

    Column {
        anchors.centerIn: parent
        width: parent.width - 48
        spacing: 14

        // A safe reference URL is selected.
        Text {
            visible: Intel.referenceUrlSafe && Intel.referenceUrl.length > 0
            width: parent.width; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere; color: Theme.text
            font.family: Theme.fontBody; font.pixelSize: 12
            text: Intel.referenceUrl
        }
        AppButton {
            visible: Intel.referenceUrlSafe && Intel.referenceUrl.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Open in system browser"; variant: "primary"
            onClicked: Qt.openUrlExternally(Intel.referenceUrl)
        }

        // A live IOC URL: defanged, behind an explicit opt-in.
        Text {
            visible: Intel.referenceUrlRaw.length > 0
            width: parent.width; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 12
            text: "This node is a live indicator of compromise. Opening it sends your browser to a hostile URL."
        }
        Text {
            visible: Intel.referenceUrlRaw.length > 0
            width: parent.width; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere; color: Theme.text
            font.family: Theme.fontBody; font.pixelSize: 12
            text: Intel.referenceUrlRaw   // defanged
        }
        AppButton {
            visible: Intel.referenceUrlRaw.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Open anyway (danger)"
            // Reveal the real URL; the operator then clicks "Open in system browser".
            onClicked: Intel.openRawReference()
        }

        // Nothing selected.
        Text {
            visible: Intel.referenceUrl.length === 0 && Intel.referenceUrlRaw.length === 0
            width: parent.width; horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 12
            text: "Select a node to view its reference (MITRE ATT&CK, CVE record, article, or VirusTotal lookup)."
        }
    }
}
