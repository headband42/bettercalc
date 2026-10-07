import QtQuick

// One keypad key. Digits sit almost flush with the page, functions lift a
// step, operators a step more, and = takes the theme's accent. A key typed on
// the keyboard flashes here too, so the keypad echoes what was pressed.
Rectangle {
    id: control

    property string label
    property bool richLabel: false
    property string keyValue: label
    property string kind: "number" // "number", "function", "operator", "toggle" or "equals"
    property string iconName
    property string accessibleName: label
    property bool active: false
    property bool flashed: false
    property real labelScale: 1
    // Labels size to one grid cell, so a key spanning two columns keeps the
    // same type size as its neighbours.
    property real cellWidth: width

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color accentColor: theme.accent
    readonly property color accentInk: theme.accentForeground

    signal activated()

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    readonly property bool down: (hitArea.pressed && hitArea.containsMouse) || flashed
    readonly property bool hovered: hitArea.containsMouse
    readonly property real restingLift: kind === "operator" ? 0.15
        : (kind === "function" || kind === "toggle") ? 0.085 : 0.045
    readonly property color labelColor: kind === "equals" ? accentInk : inkColor

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: if (enabled) activated()

    radius: Math.round(Math.min(width, height) * 0.24)
    antialiasing: true
    color: {
        if (kind === "equals")
            return mix(accentColor, pageColor, down ? 0.22 : (hovered ? 0.1 : 0));
        if (kind === "toggle" && active)
            return mix(pageColor, accentColor, down ? 0.44 : (hovered ? 0.36 : 0.3));
        return mix(pageColor, inkColor, restingLift + (down ? 0.09 : (hovered ? 0.045 : 0)));
    }
    border.width: kind === "number" || (kind === "toggle" && active) ? 1 : 0
    border.color: kind === "toggle" && active ? mix(pageColor, accentColor, 0.7)
                                              : mix(pageColor, inkColor, 0.11)
    opacity: enabled ? 1 : 0.28
    scale: down ? 0.955 : 1

    Behavior on color { ColorAnimation { duration: 110 } }
    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 160 } }

    Text {
        anchors.centerIn: parent
        visible: control.iconName === ""
        text: control.label
        textFormat: control.richLabel ? Text.RichText : Text.PlainText
        color: control.labelColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.max(8, Math.round(Math.min(parent.height * 0.4, control.cellWidth * 0.3)
                                               * control.labelScale))
    }

    Canvas {
        id: backspaceIcon
        anchors.centerIn: parent
        width: Math.round(Math.min(parent.height * 0.4, control.cellWidth * 0.3) * 1.3)
        height: Math.round(width * 0.72)
        visible: control.iconName === "backspace"

        // Drawn rather than taken from a font, so it matches the stroke of
        // the digits at every size. From Omacalc.
        onPaint: {
            var context = getContext("2d");
            var w = width;
            var h = height;
            var notch = w * 0.28;
            context.clearRect(0, 0, w, h);
            context.strokeStyle = String(control.labelColor);
            context.lineWidth = Math.max(1.4, w * 0.07);
            context.lineCap = "round";
            context.lineJoin = "round";

            context.beginPath();
            context.moveTo(notch, 1);
            context.lineTo(w - 1, 1);
            context.lineTo(w - 1, h - 1);
            context.lineTo(notch, h - 1);
            context.lineTo(1, h / 2);
            context.closePath();
            context.stroke();

            var cx = notch + (w - notch) / 2;
            var cy = h / 2;
            var arm = h * 0.18;
            context.beginPath();
            context.moveTo(cx - arm, cy - arm);
            context.lineTo(cx + arm, cy + arm);
            context.moveTo(cx + arm, cy - arm);
            context.lineTo(cx - arm, cy + arm);
            context.stroke();
        }

        Connections {
            target: control
            function onLabelColorChanged() { backspaceIcon.requestPaint(); }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    MouseArea {
        id: hitArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: control.activated()
    }
}
