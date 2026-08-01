import QtQuick
import Ironwatch
import IronwatchApp

// Small muted uppercase kicker label ("Workspace", "Live monitor", …).
Text {
    color: Theme.textMuted
    font.family: Theme.fontBody
    font.pixelSize: 11
    font.letterSpacing: 1.2
    font.capitalization: Font.AllUppercase
}
