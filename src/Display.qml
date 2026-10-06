import QtQuick

// The top of the face: a row of state badges, the expression being typed
// with any parentheses it still owes shown as ghosts, and the big line
// holding the number typed, the live preview, the result or what went wrong.
Item {
    id: display

    property real ui: 1
    property int mode: 0

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color accentColor: theme.accent
    readonly property color mutedColor: mix(pageColor, inkColor, 0.5)
    readonly property string displayState: backend.displayState

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function showCopied() {
        if (backend.displayState !== "error")
            copiedAnimation.restart();
    }

    component Badge: Rectangle {
        id: badge

        property string text
        property bool lit: false
        property bool clickable: false

        signal clicked()

        implicitWidth: Math.round(badgeText.implicitWidth + 14 * display.ui)
        implicitHeight: Math.round(20 * display.ui)
        radius: height / 2
        color: lit ? display.mix(display.pageColor, display.accentColor, 0.18)
                   : badgeArea.containsMouse ? display.mix(display.pageColor, display.inkColor, 0.07)
                                             : "transparent"
        border.width: 1
        border.color: lit ? display.mix(display.pageColor, display.accentColor, 0.6)
                          : display.mix(display.pageColor, display.inkColor, 0.16)

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: badge.text
            color: badge.lit ? display.accentColor : display.mix(display.pageColor, display.inkColor, 0.62)
            font.family: "iA Writer Mono S"
            font.pixelSize: Math.max(7, Math.round(10.5 * display.ui))
            font.letterSpacing: 0.6 * display.ui
        }

        MouseArea {
            id: badgeArea
            anchors.fill: parent
            enabled: badge.clickable
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: badge.clicked()
        }
    }

    Row {
        id: badges
        anchors.top: parent.top
        anchors.left: parent.left
        spacing: Math.round(6 * display.ui)

        Badge {
            visible: display.mode === 1
            text: backend.angleUnit
            clickable: true
            onClicked: backend.cycleAngleUnit()
        }
        Badge {
            visible: display.mode === 1 && backend.second
            text: "2nd"
            lit: true
        }
        Badge {
            visible: display.mode === 1 && backend.hyperbolic
            text: "HYP"
            lit: true
        }
        Badge {
            visible: display.mode === 2
            text: ({ 2: "BIN", 8: "OCT", 10: "DEC", 16: "HEX" })[backend.base]
            lit: true
        }
        Badge {
            visible: display.mode === 2
            text: backend.wordName
            clickable: true
            onClicked: backend.cycleWordSize()
        }
        Badge {
            visible: display.mode !== 2 && backend.hasMemory
            text: "M"
        }
    }

    Text {
        id: copiedNote
        anchors.top: parent.top
        anchors.right: parent.right
        text: "Copied"
        opacity: 0
        color: display.accentColor
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.max(7, Math.round(11 * display.ui))

        SequentialAnimation {
            id: copiedAnimation
            NumberAnimation { target: copiedNote; property: "opacity"; to: 1; duration: 90 }
            PauseAnimation { duration: 900 }
            NumberAnimation { target: copiedNote; property: "opacity"; to: 0; duration: 350 }
        }
    }

    Text {
        id: expressionText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: resultText.top
        anchors.bottomMargin: Math.round(4 * display.ui)
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideLeft
        textFormat: Text.StyledText
        text: {
            var owed = "";
            for (var i = 0; i < backend.pendingParentheses; ++i)
                owed += ")";
            var ghost = display.mix(display.pageColor, display.inkColor, 0.28);
            return display.escaped(backend.expression)
                + (owed ? "<font color=\"" + ghost + "\">" + owed + "</font>" : "");
        }
        color: display.displayState === "result" ? display.mutedColor
                                           : display.mix(display.pageColor, display.inkColor, 0.8)
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.max(8, Math.round((display.mode === 2 ? 19 : 22) * display.ui))

        Behavior on color { ColorAnimation { duration: 180 } }
    }

    Text {
        id: resultText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.round((display.mode === 2 ? 62 : 82) * display.ui)
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignBottom
        text: backend.display
        color: display.displayState === "error" ? theme.danger
             : display.displayState === "preview" ? display.mix(display.pageColor, display.inkColor, 0.46)
             : display.inkColor
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: Math.max(8, Math.round(16 * display.ui))
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.max(10, Math.round((display.displayState === "error" ? 30
                                                 : display.mode === 2 ? 50 : 70) * display.ui))
        transform: Translate { id: settle }

        Behavior on color { ColorAnimation { duration: 180 } }

        // Clicking the number copies it.
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                backend.copyResult();
                display.showCopied();
            }
        }
    }

    // When = lands, the result rises into place.
    Connections {
        target: backend
        function onEvaluated() { settleAnimation.restart(); }
    }

    ParallelAnimation {
        id: settleAnimation
        NumberAnimation {
            target: settle; property: "y"
            from: 12 * display.ui; to: 0
            duration: 280; easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: resultText; property: "opacity"
            from: 0.25; to: 1
            duration: 220; easing.type: Easing.OutQuad
        }
    }
}
