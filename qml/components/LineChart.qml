import QtQuick
import Ironwatch
import IronwatchApp

// Minimal multi-series line chart, standing in for the prototype's raw
// SVG polylines (anomaly-score trend, per-node resource envelope). Series
// points are given in a fixed "viewBox" coordinate space (matching the
// prototype's 600x150 SVG viewBox) and scaled to the item's actual size
// at paint time.
Canvas {
    id: root
    property real viewBoxW: 600
    property real viewBoxH: 150
    property int gridLines: 3
    // [{ points: [{x,y},...], color, lineWidth, dashed }]
    property var series: []
    // [{ x, y, color, radius }] in viewBox space
    property var markers: []
    // < 0 hides the threshold line
    property real thresholdPixelY: -1
    property color thresholdColor: Theme.accent700

    onSeriesChanged: requestPaint()
    onMarkersChanged: requestPaint()
    onThresholdPixelYChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    Connections {
        target: Theme
        function onDarkChanged() { root.requestPaint() }
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        if (width <= 0 || height <= 0)
            return

        var sx = width / viewBoxW
        var sy = height / viewBoxH

        ctx.strokeStyle = Theme.divider
        ctx.lineWidth = 1
        ctx.setLineDash([])
        for (var g = 1; g <= gridLines; g++) {
            var gy = (viewBoxH / (gridLines + 1)) * g * sy
            ctx.beginPath()
            ctx.moveTo(0, gy)
            ctx.lineTo(width, gy)
            ctx.stroke()
        }

        if (thresholdPixelY >= 0) {
            ctx.strokeStyle = thresholdColor
            ctx.lineWidth = 1
            ctx.setLineDash([5, 4])
            ctx.beginPath()
            ctx.moveTo(0, thresholdPixelY * sy)
            ctx.lineTo(width, thresholdPixelY * sy)
            ctx.stroke()
            ctx.setLineDash([])
        }

        for (var s = 0; s < series.length; s++) {
            var line = series[s]
            if (!line.points || line.points.length === 0)
                continue
            ctx.strokeStyle = line.color
            ctx.lineWidth = line.lineWidth || 1.6
            ctx.setLineDash(line.dashed ? [4, 3] : [])
            ctx.beginPath()
            for (var i = 0; i < line.points.length; i++) {
                var p = line.points[i]
                var px = p.x * sx, py = p.y * sy
                if (i === 0) ctx.moveTo(px, py)
                else ctx.lineTo(px, py)
            }
            ctx.stroke()
        }
        ctx.setLineDash([])

        for (var m = 0; m < markers.length; m++) {
            var mk = markers[m]
            ctx.fillStyle = mk.color
            ctx.beginPath()
            ctx.arc(mk.x * sx, mk.y * sy, mk.radius || 3.5, 0, Math.PI * 2)
            ctx.fill()
        }
    }
}
