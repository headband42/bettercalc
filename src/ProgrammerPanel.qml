import QtQuick

// Programmer mode's readout: the current value in all four bases (click one
// to work in it) and every bit of the word, each one clickable.
Item {
    id: panel

    property real ui: 1

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color accentColor: theme.accent
    readonly property real rowHeight: Math.round(27 * ui)
    readonly property real rowGap: Math.round(3 * ui)

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    Column {
        id: bases
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: panel.rowGap

        Repeater {
            model: [
                { base: 16, name: "HEX", key: "F5" },
                { base: 10, name: "DEC", key: "F6" },
                { base: 8, name: "OCT", key: "F7" },
                { base: 2, name: "BIN", key: "F8" }
            ]

            Rectangle {
                id: row

                required property var modelData
                readonly property bool current: backend.base === modelData.base

                width: bases.width
                height: panel.rowHeight
                radius: Math.round(8 * panel.ui)
                color: current ? panel.mix(panel.pageColor, panel.inkColor, 0.07)
                     : rowArea.containsMouse ? panel.mix(panel.pageColor, panel.inkColor, 0.035)
                                             : "transparent"

                Behavior on color { ColorAnimation { duration: 140 } }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Math.round(3 * panel.ui)
                    width: Math.max(2, Math.round(3 * panel.ui))
                    height: Math.round(parent.height * 0.5)
                    radius: width / 2
                    color: panel.accentColor
                    opacity: row.current ? 1 : 0

                    Behavior on opacity { NumberAnimation { duration: 140 } }
                }

                Text {
                    id: name
                    anchors.verticalCenter: parent.verticalCenter
                    x: Math.round(14 * panel.ui)
                    text: row.modelData.name
                    color: row.current ? panel.accentColor : panel.mix(panel.pageColor, panel.inkColor, 0.5)
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.max(7, Math.round(11.5 * panel.ui))
                    font.letterSpacing: 0.8 * panel.ui
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: name.right
                    anchors.leftMargin: Math.round(18 * panel.ui)
                    anchors.right: parent.right
                    anchors.rightMargin: Math.round(12 * panel.ui)
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.base === 16 ? backend.hexValue
                        : row.modelData.base === 10 ? backend.decValue
                        : row.modelData.base === 8 ? backend.octValue : backend.binValue
                    color: row.current ? panel.inkColor : panel.mix(panel.pageColor, panel.inkColor, 0.68)
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Math.max(6, Math.round(8 * panel.ui))
                    font.family: "iA Writer Mono S"
                    font.pixelSize: Math.max(7, Math.round(14.5 * panel.ui))
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.base = row.modelData.base
                }

                Accessible.role: Accessible.RadioButton
                Accessible.name: row.modelData.name
                Accessible.checked: row.current
                Accessible.onPressAction: backend.base = row.modelData.base
            }
        }
    }

    // Two rows of thirty-two bits, in nibbles, most significant first. Bits
    // beyond the word size stay visible but faded, so changing the word size
    // reads as a window over the same 64 bits.
    Item {
        id: bitGrid
        anchors.top: bases.bottom
        anchors.topMargin: Math.round(12 * panel.ui)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        readonly property real nibbleGap: Math.round(12 * panel.ui)
        readonly property real cellWidth: (width - 7 * nibbleGap) / 32
        readonly property real labelHeight: Math.round(13 * panel.ui)
        readonly property real cellHeight: Math.max(10, (height - 2 * labelHeight - Math.round(6 * panel.ui)) / 2)

        Repeater {
            model: 2

            Item {
                id: bitRow

                required property int index

                y: index * (bitGrid.cellHeight + bitGrid.labelHeight + Math.round(6 * panel.ui))
                width: bitGrid.width
                height: bitGrid.cellHeight + bitGrid.labelHeight

                Repeater {
                    model: 8

                    Item {
                        id: nibble

                        required property int index
                        readonly property int lowestBit: 63 - (bitRow.index * 32 + index * 4 + 3)

                        x: index * (4 * bitGrid.cellWidth + bitGrid.nibbleGap)
                        width: 4 * bitGrid.cellWidth
                        height: bitRow.height

                        Repeater {
                            model: 4

                            Rectangle {
                                id: cell

                                required property int index
                                readonly property int bit: nibble.lowestBit + 3 - index
                                readonly property bool usable: bit < backend.wordSize
                                readonly property bool set: backend.bits.charAt(63 - bit) === "1"

                                x: index * bitGrid.cellWidth
                                width: bitGrid.cellWidth
                                height: bitGrid.cellHeight
                                radius: Math.round(5 * panel.ui)
                                color: cellArea.containsMouse && usable
                                    ? panel.mix(panel.pageColor, panel.inkColor, 0.08) : "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: cell.set ? "1" : "0"
                                    color: !cell.usable ? panel.mix(panel.pageColor, panel.inkColor, 0.13)
                                         : cell.set ? panel.accentColor
                                         : panel.mix(panel.pageColor, panel.inkColor, 0.4)
                                    font.family: "iA Writer Mono S"
                                    font.pixelSize: Math.max(7, Math.round(Math.min(bitGrid.cellHeight * 0.62,
                                                                                    bitGrid.cellWidth * 0.95)))

                                    Behavior on color { ColorAnimation { duration: 140 } }
                                }

                                MouseArea {
                                    id: cellArea
                                    anchors.fill: parent
                                    enabled: cell.usable
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.toggleBit(cell.bit)
                                }

                                Accessible.role: Accessible.CheckBox
                                Accessible.name: "Bit " + bit
                                Accessible.checked: set
                            }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: Math.round(bitGrid.cellWidth * 0.5 - width / 2)
                            y: bitGrid.cellHeight
                            text: nibble.lowestBit
                            color: panel.mix(panel.pageColor, panel.inkColor,
                                             nibble.lowestBit < backend.wordSize ? 0.36 : 0.14)
                            font.family: "iA Writer Mono S"
                            font.pixelSize: Math.max(6, Math.round(9 * panel.ui))
                        }
                    }
                }
            }
        }
    }
}
