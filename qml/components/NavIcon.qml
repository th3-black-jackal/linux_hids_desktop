import QtQuick
import QtQuick.Shapes
import Ironwatch
import IronwatchApp

// The sidebar/workspace glyphs, ported from the prototype's inline SVGs
// (24x24 viewBox, currentColor stroke) via QtQuick.Shapes' PathSvg.
// ShapePath has no `visible` property, so each icon gets its own Shape
// (which does) rather than toggling individual ShapePath children.
//
// "assistant" has no prototype original — it was drawn to match: same
// 1.5px hairline, same square-ish geometry as the "nodes" die.
Item {
    id: root
    property string kind: "overview" // overview | detections | events | nodes | model | integrations | assistant
    property color color: Theme.text
    implicitWidth: 16
    implicitHeight: 16

    component IconShape: Shape {
        width: 24
        height: 24
        scale: root.width / 24
        transformOrigin: Item.TopLeft
    }

    IconShape {
        visible: root.kind === "overview"
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M3,3 h7 v9 h-7 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M14,3 h7 v5 h-7 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M14,12 h7 v9 h-7 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M3,16 h7 v5 h-7 Z" }
        }
    }

    IconShape {
        visible: root.kind === "detections"
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M10.3,3.6 1.8,18 a2,2 0 0 0 1.7,3 h17 a2,2 0 0 0 1.7,-3 L13.7,3.6 a2,2 0 0 0 -3.4,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M12,9 v4" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: root.color
            PathSvg { path: "M11.6,17 a0.4,0.4 0 1 0 0.8,0 a0.4,0.4 0 1 0 -0.8,0 Z" }
        }
    }

    IconShape {
        visible: root.kind === "nodes"
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M4,4 h16 v16 h-16 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M9,9 h6 v6 h-6 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M9,2 v2 M15,2 v2 M9,20 v2 M15,20 v2 M2,9 h2 M2,15 h2 M20,9 h2 M20,15 h2" }
        }
    }

    IconShape {
        visible: root.kind === "model"
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M10,5 a2,2 0 1 0 4,0 a2,2 0 1 0 -4,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M3,17 a2,2 0 1 0 4,0 a2,2 0 1 0 -4,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            PathSvg { path: "M17,17 a2,2 0 1 0 4,0 a2,2 0 1 0 -4,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M12,7 v4 M12,11 6.5,15.5 M12,11 17.5,15.5" }
        }
    }

    IconShape {
        visible: root.kind === "events"
        // Stacked rows narrowing downward: a table with a filter applied.
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M3,5 h18 M6,10 h12 M9,15 h6 M10.5,20 h3" }
        }
    }

    IconShape {
        visible: root.kind === "assistant"
        // Speech frame with a tail, plus three dots for the streaming answer.
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            joinStyle: ShapePath.MiterJoin
            PathSvg { path: "M3,4 h18 v12 h-11 l-4,4 v-4 h-3 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: root.color
            PathSvg { path: "M7.4,10 a0.6,0.6 0 1 0 1.2,0 a0.6,0.6 0 1 0 -1.2,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: root.color
            PathSvg { path: "M11.4,10 a0.6,0.6 0 1 0 1.2,0 a0.6,0.6 0 1 0 -1.2,0 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: root.color
            PathSvg { path: "M15.4,10 a0.6,0.6 0 1 0 1.2,0 a0.6,0.6 0 1 0 -1.2,0 Z" }
        }
    }

    IconShape {
        visible: root.kind === "integrations"
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: "M12,15 a3,3 0 1 0 0,-6 a3,3 0 1 0 0,6 Z" }
        }
        ShapePath {
            strokeColor: root.color; strokeWidth: 1.5; fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathSvg { path: "M19.4,15 a1.7,1.7 0 0 0 0.3,1.9 l0.1,0.1 a2,2 0 1 1 -2.8,2.8 l-0.1,-0.1 a1.7,1.7 0 0 0 -2.9,1.2 a2,2 0 1 1 -4,0 a1.7,1.7 0 0 0 -2.9,-1.2 l-0.1,0.1 a2,2 0 1 1 -2.8,-2.8 l0.1,-0.1 A1.7,1.7 0 0 0 3,15 a2,2 0 1 1 0,-4 a1.7,1.7 0 0 0 1.4,-2.9 l-0.1,-0.1 a2,2 0 1 1 2.8,-2.8 l0.1,0.1 A1.7,1.7 0 0 0 10,4.6 a2,2 0 1 1 4,0 a1.7,1.7 0 0 0 2.9,1.2 l0.1,-0.1 a2,2 0 1 1 2.8,2.8 l-0.1,0.1 A1.7,1.7 0 0 0 21,11 a2,2 0 1 1 0,4 Z" }
        }
    }
}
