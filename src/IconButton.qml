import QtQuick

// A round, borderless button for the header: the history clock and the
// help mark. Both are drawn, so they take the theme's ink at any size.
Rectangle {
    id: control

    property string icon: "history" // "history" or "help"
    property bool active: false
    property string accessibleName: icon

    signal activated()

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color glyphColor: active || hitArea.containsMouse
        ? inkColor : mix(pageColor, inkColor, 0.6)

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    radius: width / 2
    color: active ? mix(pageColor, inkColor, 0.14)
                  : hitArea.containsMouse ? mix(pageColor, inkColor, 0.07) : "transparent"

    Behavior on color { ColorAnimation { duration: 140 } }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: activated()

    Canvas {
        id: glyph
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.52)
        height: width

        onPaint: {
            var context = getContext("2d");
            var w = width;
            var h = height;
            context.clearRect(0, 0, w, h);
            context.strokeStyle = String(control.glyphColor);
            context.fillStyle = String(control.glyphColor);
            context.lineWidth = Math.max(1.3, w * 0.085);
            context.lineCap = "round";
            context.lineJoin = "round";

            var cx = w / 2;
            var cy = h / 2;
            var r = w * 0.4;

            if (control.icon === "history") {
                // A clock face whose rim turns back on itself.
                var start = Math.PI * 0.95;
                var end = Math.PI * 2.75;
                context.beginPath();
                context.arc(cx, cy, r, start, end, false);
                context.stroke();

                var sx = cx + r * Math.cos(start);
                var sy = cy + r * Math.sin(start);
                context.beginPath();
                context.moveTo(sx - w * 0.15, sy - w * 0.06);
                context.lineTo(sx, sy);
                context.lineTo(sx + w * 0.06, sy - w * 0.15);
                context.stroke();

                context.beginPath();
                context.moveTo(cx, cy - r * 0.55);
                context.lineTo(cx, cy);
                context.lineTo(cx + r * 0.42, cy + r * 0.28);
                context.stroke();
            } else {
                context.beginPath();
                context.arc(cx, cy, r, 0, Math.PI * 2, false);
                context.stroke();

                context.beginPath();
                context.arc(cx, cy - r * 0.2, r * 0.32, Math.PI * 1.1, Math.PI * 2.3, false);
                context.lineTo(cx, cy + r * 0.2);
                context.stroke();

                context.beginPath();
                context.arc(cx, cy + r * 0.52, Math.max(0.8, w * 0.05), 0, Math.PI * 2, false);
                context.fill();
            }
        }

        Connections {
            target: control
            function onGlyphColorChanged() { glyph.requestPaint(); }
        }
        onWidthChanged: requestPaint()
    }

    MouseArea {
        id: hitArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.activated()
    }
}
