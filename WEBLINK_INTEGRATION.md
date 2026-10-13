# Open an event in the web app

Double-click an event and it opens in `orchicyb-ai-attack.lovable.app` in the
operator's browser, carrying the event's identifying fields.

## 1. How it is triggered

| Gesture | Where |
|---|---|
| **Double-click a row** | Detections, Events |
| **Open in web app** button | the inspector on both screens |

**Plain selection deliberately does not open anything.** Single-click already
means "show me this row" — it fills the inspector, and on the Events screen it
fetches `events/<id>/`. Selection also moves without a mouse: the Detections
inspector follows the newest alert until an operator picks one, so a fresh
MQTT alert changes the selection on its own. Binding a browser launch to that
would open a tab every time a board reported something.

Double-click is the conventional "open this" gesture and cannot fire from a
model update, which is why it carries the feature instead. If single-click
really is wanted, it is one line in each table's `onClicked`.

## 2. The URL is a template, not a hard-coded path

The web app's deep-link format is not documented anywhere this console can
see, and it was not reachable from the build environment to inspect. Rather
than invent a path and assert it works, the URL is a template on
**Integrations → Open event in web app**:

```
https://orchicyb-ai-attack.lovable.app/?event={id}&host={host}&class={class}&time={time}
```

**Query parameters, not a path segment, for the default.** A single-page app
ignores query parameters it does not recognise and still renders; an
unrecognised *path* is a 404 or a blank catch-all route. So if the real format
turns out to be something else, the worst case here is "the app opens on its
front page with the event in the query string" rather than "the app breaks".

Change the template once the real format is known — that is the only place
the URL appears. `{}`-placeholders work in the path too, e.g.
`https://…/event/{id}`.

| Placeholder | Value |
|---|---|
| `{id}` | SIEM event id — **empty for a live MQTT-only detection** |
| `{uid}` | console reference: `IW-DB-42`, or `IW-<date>-<n>` when live |
| `{host}` | board hostname |
| `{class}` / `{classIdx}` | model class label and index |
| `{confidence}` | `0.000`–`1.000` |
| `{severity}` | console-side grading |
| `{time}` | event time, ISO-8601 UTC, exactly as the board sent it |

Clearing the template disables the feature: the buttons hide and double-click
does nothing.

## 3. Two behaviours worth knowing

**A query parameter whose placeholder resolves to nothing is dropped.** That
is what lets one template serve both row kinds. A database row gives
`?event=1173&host=…`; a live MQTT detection has no server-side id and gives
`?host=…&class=…&time=…` — not `?event=&host=…`. Placeholders in the *path*
cannot be dropped this way; a path template plus a live row yields a trailing
empty segment, which is a reason to prefer the query form.

**Values are percent-encoded.** Class names come off the wire and are not
guaranteed to be tame — the test publisher deliberately sends a non-ASCII one.
Verified: `xmrig / crypto miner` → `xmrig%20%2F%20crypto%20miner`, a class
name containing `&` → `%26` (so it cannot inject a query parameter), and
non-ASCII → UTF-8 percent-encoding.

## 4. Only http and https are opened

The built URL's scheme is checked before it is handed to the desktop. Values
are percent-encoded so they cannot inject a scheme, but a mistyped template
could — and handing an arbitrary scheme to the OS opener on a keystroke is not
something a security tool should do. `javascript:` and `file:` templates are
refused with the reason shown on the Integrations screen.

A browser that fails to launch is also reported, rather than looking like a
click that did nothing.

## 5. What is sent, and where

Everything in the URL is already on the operator's screen. **But it does leave
the machine**: this URL goes to a public host, so board hostnames, class
labels and timestamps end up in that site's logs, in browser history, and in
any referrer it forwards. On a plain-HTTP template they cross the network in
the clear too.

If that matters for a given deployment, point the template at an internal
instance, or clear it to disable the feature entirely. Nothing else in the
console talks to anything outside the broker and the configured backend.
