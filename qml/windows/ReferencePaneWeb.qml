import QtQuick
import QtWebEngine
import Ironwatch
import IronwatchApp

import "../components"

// Reference pane, embedded-browser variant. Loaded by ThreatIntelWindow only
// when Intel.webEngineAvailable is true, so QtWebEngine is imported here and
// nowhere that must build without the module. Reads Intel.referenceUrl* and
// shows hostile content in a locked-down, off-the-record profile.
Item {
    id: pane

    // No cookies/cache persisted; downloads and popups denied.
    WebEngineProfile {
        id: intelProfile
        offTheRecord: true
        onDownloadRequested: function(d) { d.cancel() }
    }

    WebEngineView {
        id: webView
        anchors.fill: parent
        visible: Intel.referenceUrlSafe && Intel.referenceUrl.length > 0
        profile: intelProfile
        url: Intel.referenceUrlSafe ? Intel.referenceUrl : ""
        settings.javascriptCanOpenWindows: false
        settings.localStorageEnabled: false
        settings.screenCaptureEnabled: false
        onNewWindowRequested: function(req) { /* deny popups */ }
    }

    // Placeholder / IOC danger gate when there is no safe URL to show.
    Item {
        anchors.fill: parent
        visible: !webView.visible
        Column {
            anchors.centerIn: parent; width: parent.width - 48; spacing: 12
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap; color: Theme.textMuted; font.pixelSize: 12
                text: Intel.referenceUrlRaw.length > 0
                    ? "This node is a live indicator of compromise. Its URL is shown defanged and is not loaded automatically."
                    : "Select a node to view its reference (MITRE ATT&CK, CVE record, article, or VirusTotal lookup)."
            }
            Text {
                visible: Intel.referenceUrlRaw.length > 0
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere; font.family: Theme.fontBody; font.pixelSize: 12
                color: Theme.text
                text: Intel.referenceUrlRaw   // e.g. hxxp://evil[.]example/path
            }
            AppButton {
                visible: Intel.referenceUrlRaw.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Open anyway (danger)"
                onClicked: Intel.openRawReference()   // loads the real URL in this locked-down view
            }
        }
    }
}
