import QtQuick

// The omasoundslides wordmark from logo.txt, drawn block by block instead of
// as text: each █ is a full cell, ▄ and ▀ half cells. That keeps it crisp at
// any size, even a few pixels per letter. The letters take the Omarchy
// theme's foreground and the equalizer bars its accent, live.
Canvas {
    id: mark

    property string source: logoText
    property color ink: theme.foreground
    property color accent: theme.accent

    readonly property var lines: source.replace(/\n+$/, "").split("\n")
    readonly property int columns: lines.reduce((widest, line) => Math.max(widest, line.length), 0)
    // A character is one cell wide and two cells tall, like a terminal's.
    readonly property real cell: height / (lines.length * 2)
    // The bars sit right of "slides", on the lower half of the logo.
    readonly property int barsRow: Math.floor(lines.length / 2)
    readonly property int barsColumn: 50

    implicitHeight: 36
    implicitWidth: columns * cell

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        for (var r = 0; r < lines.length; ++r) {
            var line = lines[r];
            for (var c = 0; c < line.length; ++c) {
                var ch = line[c];
                if (ch === " ")
                    continue;
                ctx.fillStyle = (r > barsRow && c >= barsColumn) ? accent : ink;
                var x = c * cell;
                var y = r * 2 * cell;
                if (ch === "█")
                    ctx.fillRect(x, y, cell, 2 * cell);
                else if (ch === "▀")
                    ctx.fillRect(x, y, cell, cell);
                else if (ch === "▄")
                    ctx.fillRect(x, y + cell, cell, cell);
            }
        }
    }

    onSourceChanged: requestPaint()
    onInkChanged: requestPaint()
    onAccentChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
}
