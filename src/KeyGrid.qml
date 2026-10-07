import QtQuick

// Lays a block of keys out on an even grid, filling whatever space it is
// given; a key may span columns. Labels that depend on 2nd, hyp, the angle
// unit or the word size are resolved here so the key lists stay plain data.
Item {
    id: grid

    property var keys: []
    property int columns: 4
    property real gap: 10
    property real labelScale: 1
    property string flashedKey

    signal keyPressed(string key)

    readonly property var slots: {
        var placed = [];
        var column = 0;
        var row = 0;
        for (var i = 0; i < keys.length; ++i) {
            var span = keys[i].span || 1;
            if (column + span > columns) {
                column = 0;
                ++row;
            }
            placed.push({ row: row, column: column, span: span });
            column += span;
            if (column >= columns) {
                column = 0;
                ++row;
            }
        }
        return placed;
    }
    readonly property int rows: slots.length > 0 ? slots[slots.length - 1].row + 1 : 0
    readonly property real cellWidth: (width - (columns - 1) * gap) / columns
    readonly property real cellHeight: rows > 0 ? (height - (rows - 1) * gap) / rows : 0

    function labelFor(entry) {
        if (entry.key === "angle")
            return backend.angleUnit;
        if (entry.key === "word")
            return backend.wordName;
        if (entry.trig) {
            var name = entry.key + (backend.hyperbolic ? "h" : "");
            return backend.second ? name + "<sup>−1</sup>" : name;
        }
        if (backend.second && entry.second !== undefined)
            return entry.second;
        return entry.label;
    }

    function isRich(label) {
        return label.indexOf("<") >= 0 && label !== "<<" && label !== ">>";
    }

    Repeater {
        model: grid.keys.length

        CalcButton {
            required property int index
            readonly property var entry: grid.keys[index]
            readonly property var slot: grid.slots[index]

            x: slot.column * (grid.cellWidth + grid.gap)
            y: slot.row * (grid.cellHeight + grid.gap)
            width: grid.cellWidth * slot.span + grid.gap * (slot.span - 1)
            height: grid.cellHeight
            cellWidth: grid.cellWidth

            label: grid.labelFor(entry)
            richLabel: grid.isRich(label)
            keyValue: entry.key
            kind: entry.kind || "number"
            iconName: entry.icon || ""
            accessibleName: entry.name || label
            labelScale: (entry.small ? 0.72 : 1) * grid.labelScale
            active: entry.key === "2nd" ? backend.second
                  : entry.key === "hyp" ? backend.hyperbolic : false
            enabled: {
                // Reading base and mode here re-runs the check when they change.
                backend.base;
                backend.mode;
                return backend.digitEnabled(entry.key);
            }
            flashed: grid.flashedKey !== "" && grid.flashedKey === entry.key
            onActivated: grid.keyPressed(entry.key)
        }
    }
}
