.pragma library

// The backend's answers carry inline ATT&CK ids like [T0803] or [T1059.004]
// (API_DOCUMENTATION.md §6.6). This turns an answer into rich text where
// every citation that matches a returned source becomes a clickable link to
// its MITRE URL.
//
// Citations with no matching source are deliberately *not* linked: the model
// can and does emit ids that were never retrieved, and a link that quietly
// guesses a MITRE URL for one would present a hallucinated reference as a
// grounded one. Those render as plain marked-up text instead.

var ID_RE = /\[[A-Z]\d{4}(?:\.\d{3})?\]/g;

function escapeHtml(s) {
    return String(s)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

// Builds { "T0803": "https://attack.mitre.org/techniques/T0803", ... } from
// the SSE `sources` payload, skipping entries with no URL.
function sourceUrls(sources) {
    var map = {};
    if (!sources)
        return map;
    for (var i = 0; i < sources.length; i++) {
        var s = sources[i];
        if (s && s.id && s.url)
            map[String(s.id)] = String(s.url);
    }
    return map;
}

// text -> rich text. `linkColor` and `citedColor` come from the theme so the
// inline HTML stays in step with light/dark mode.
function render(text, sources, linkColor, citedColor) {
    if (!text)
        return "";

    var urls = sourceUrls(sources);
    var out = "";
    var last = 0;
    var m;

    ID_RE.lastIndex = 0;
    while ((m = ID_RE.exec(text)) !== null) {
        out += escapeHtml(text.substring(last, m.index));

        var token = m[0];                       // "[T0803]"
        var id = token.substring(1, token.length - 1);
        // A sub-technique citation [T1059.004] is grounded by its parent
        // source T1059 when the backend returned that instead.
        var url = urls[id] || urls[id.split(".")[0]];

        if (url) {
            out += '<a href="' + escapeHtml(url) + '" style="color:' + linkColor
                 + '; text-decoration:none">' + escapeHtml(token) + '</a>';
        } else {
            out += '<span style="color:' + citedColor + '">' + escapeHtml(token) + '</span>';
        }
        last = m.index + token.length;
    }
    out += escapeHtml(text.substring(last));

    // Qt's rich text collapses newlines; the answers are prose with real
    // paragraph breaks, so keep them.
    return out.replace(/\n/g, "<br/>");
}

// How many of the citations in `text` were actually backed by a source.
function groundedCount(text, sources) {
    if (!text)
        return { cited: 0, grounded: 0 };

    var urls = sourceUrls(sources);
    var cited = 0, grounded = 0, m;
    ID_RE.lastIndex = 0;
    while ((m = ID_RE.exec(text)) !== null) {
        cited++;
        var id = m[0].substring(1, m[0].length - 1);
        if (urls[id] || urls[id.split(".")[0]])
            grounded++;
    }
    return { cited: cited, grounded: grounded };
}
