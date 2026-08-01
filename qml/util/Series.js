.pragma library

// Ported verbatim from the Claude Design prototype so the demo charts
// trace the same shapes: a seeded linear-congruential walk clamped to
// [lo, hi], with three tent-pole spikes injected at fixed indices when
// `spiky` is set (used for the anomaly-score chart's threshold breaches).
function poly(seed, n, lo, hi, spiky) {
    var v = (lo + hi) / 2;
    var out = [];
    for (var i = 0; i < n; i++) {
        seed = (seed * 1103515245 + 12345) % 2147483648;
        var r = (seed / 2147483648) - 0.5;
        v += r * (hi - lo) * 0.28;
        if (spiky && (i === 22 || i === 41 || i === 55))
            v = hi;
        v = Math.max(lo, Math.min(hi, v));
        out.push({ x: i * (600 / (n - 1)), y: v });
    }
    return out;
}

function spikesFrom(points) {
    var idx = [22, 41, 55];
    var out = [];
    for (var i = 0; i < idx.length; i++) {
        if (idx[i] < points.length)
            out.push(points[idx[i]]);
    }
    return out;
}
