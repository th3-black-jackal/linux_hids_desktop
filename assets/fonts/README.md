# Fonts

The design uses **Barlow** (body) and **Barlow Condensed** (headings), pulled from
Google Fonts in the HTML prototype. This desktop build references those family
names directly (`Theme.fontBody` / `Theme.fontHeading` in `qml/Theme.qml`) and
relies on Qt's font matching to fall back to a system sans-serif if the family
isn't installed.

For pixel-accurate typography, drop the actual `.ttf`/`.otf` files here
(e.g. `Barlow-Regular.ttf`, `Barlow-Medium.ttf`, `Barlow-SemiBold.ttf`,
`BarlowCondensed-Regular.ttf`, `BarlowCondensed-SemiBold.ttf`), add them to the
`RESOURCES` list in `CMakeLists.txt`, and load them with `FontLoader` at
startup (e.g. in `Main.qml`) instead of relying on family-name matching.
