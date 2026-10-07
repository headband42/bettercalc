import QtQuick

// Every shortcut on one card, opened with ?.
Item {
    id: overlay

    property bool open: false
    property real ui: 1

    signal closeRequested()

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color accentColor: theme.accent
    readonly property bool wide: width > 640 * ui

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    readonly property var sections: [
        {
            title: "Everywhere",
            rows: [
                ["0–9  .  ( )", "Numbers and grouping"],
                ["+ − * /", "Arithmetic"],
                ["Enter  =  Space", "Calculate"],
                ["Backspace", "Delete the last entry"],
                ["Esc  Delete", "Clear"],
                ["Ctrl Z  Ctrl ⇧ Z", "Undo, redo"],
                ["Ctrl C  Ctrl V", "Copy result, paste"],
                ["Ctrl 1  2  3", "Basic, scientific, programmer"],
                ["Ctrl H", "History"],
                ["?", "This help"],
                ["Ctrl Q", "Quit"]
            ]
        },
        {
            title: "Basic & scientific",
            rows: [
                ["%", "Percent: 200 + 10% is 220"],
                ["^  !", "Power, factorial"],
                ["E", "Exponent: 1.5E3 is 1500"],
                ["a–z", "Type sin( sqrt( ln( pi ans"],
                ["Ctrl D", "Degrees, radians, gradians"]
            ]
        },
        {
            title: "Programmer",
            rows: [
                ["A–F", "Hex digits"],
                ["&  |  ^  ~", "AND, OR, XOR, NOT"],
                ["<  >", "Shift left, shift right"],
                ["%", "Remainder (mod)"],
                ["F5 F6 F7 F8", "HEX, DEC, OCT, BIN"],
                ["F12 F2 F3 F4", "QWORD, DWORD, WORD, BYTE"]
            ]
        }
    ]

    visible: opacity > 0
    opacity: open ? 1 : 0

    Behavior on opacity { NumberAnimation { duration: 180 } }

    Rectangle {
        anchors.fill: parent
        color: overlay.pageColor
        opacity: 0.97
    }

    MouseArea {
        anchors.fill: parent
        enabled: overlay.open
        onClicked: overlay.closeRequested()
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: Math.round(24 * overlay.ui)
        contentWidth: width
        contentHeight: content.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: flick.width
            spacing: Math.round(18 * overlay.ui)

            Text {
                text: "Keyboard"
                color: overlay.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(9, Math.round(20 * overlay.ui))
            }

            Flow {
                width: parent.width
                spacing: Math.round(28 * overlay.ui)

                Repeater {
                    model: overlay.sections

                    Column {
                        required property var modelData

                        width: overlay.wide ? (content.width - Math.round(28 * overlay.ui)) / 2 : content.width
                        spacing: Math.round(7 * overlay.ui)

                        Text {
                            text: parent.modelData.title.toUpperCase()
                            color: overlay.accentColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: Math.max(7, Math.round(11 * overlay.ui))
                            font.letterSpacing: 1 * overlay.ui
                            bottomPadding: Math.round(3 * overlay.ui)
                        }

                        Repeater {
                            model: parent.modelData.rows

                            Row {
                                id: shortcutRow

                                required property var modelData
                                readonly property real keysWidth: Math.round(140 * overlay.ui)

                                width: parent.width
                                spacing: Math.round(12 * overlay.ui)

                                Text {
                                    width: shortcutRow.keysWidth
                                    text: shortcutRow.modelData[0]
                                    color: overlay.inkColor
                                    font.family: "iA Writer Mono S"
                                    font.pixelSize: Math.max(7, Math.round(12.5 * overlay.ui))
                                }

                                Text {
                                    width: shortcutRow.width - shortcutRow.keysWidth - shortcutRow.spacing
                                    text: shortcutRow.modelData[1]
                                    wrapMode: Text.WordWrap
                                    color: overlay.mix(overlay.pageColor, overlay.inkColor, 0.6)
                                    font.family: "iA Writer Mono S"
                                    font.pixelSize: Math.max(7, Math.round(12.5 * overlay.ui))
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "Press ? or Esc to close"
                color: overlay.mix(overlay.pageColor, overlay.inkColor, 0.4)
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(7, Math.round(11 * overlay.ui))
            }
        }
    }
}
