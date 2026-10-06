import QtQuick

// Past calculations in a drawer from the right, newest first. Clicking one
// puts its result back into the expression.
Item {
    id: panel

    property bool open: false
    property real ui: 1

    signal closeRequested()

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property color accentColor: theme.accent

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    visible: open || drawer.x < width

    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: panel.open ? (theme.dark ? 0.45 : 0.22) : 0

        Behavior on opacity { NumberAnimation { duration: 220 } }

        MouseArea {
            anchors.fill: parent
            enabled: panel.open
            onClicked: panel.closeRequested()
        }
    }

    Rectangle {
        id: drawer

        width: Math.min(panel.width, Math.round(360 * panel.ui))
        height: panel.height
        x: panel.open ? panel.width - width : panel.width
        color: panel.mix(panel.pageColor, panel.inkColor, 0.035)

        Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }

        // Swallow clicks so they don't reach the scrim beneath.
        MouseArea { anchors.fill: parent }

        Rectangle {
            width: 1
            height: parent.height
            color: panel.mix(panel.pageColor, panel.inkColor, 0.12)
        }

        Item {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: Math.round(18 * panel.ui)
            height: Math.round(34 * panel.ui)

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "History"
                color: panel.inkColor
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(8, Math.round(15 * panel.ui))
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                visible: backend.history.length > 0
                text: "Clear"
                color: clearArea.containsMouse ? theme.danger : panel.mix(panel.pageColor, panel.inkColor, 0.55)
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(7, Math.round(12 * panel.ui))

                MouseArea {
                    id: clearArea
                    anchors.fill: parent
                    anchors.margins: -Math.round(8 * panel.ui)
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.clearHistory()
                }
            }
        }

        ListView {
            id: list
            anchors.top: header.bottom
            anchors.topMargin: Math.round(8 * panel.ui)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Math.round(10 * panel.ui)
            anchors.rightMargin: Math.round(10 * panel.ui)
            anchors.bottomMargin: Math.round(10 * panel.ui)
            clip: true
            spacing: Math.round(2 * panel.ui)
            model: backend.history
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: entry

                required property int index
                required property var modelData

                width: list.width
                height: column.implicitHeight + Math.round(18 * panel.ui)
                radius: Math.round(10 * panel.ui)
                color: entryArea.containsMouse ? panel.mix(panel.pageColor, panel.inkColor, 0.075)
                                               : "transparent"

                Behavior on color { ColorAnimation { duration: 120 } }

                Column {
                    id: column
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Math.round(12 * panel.ui)
                    anchors.rightMargin: Math.round(12 * panel.ui)
                    spacing: Math.round(3 * panel.ui)

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideLeft
                        text: entry.modelData.expression + " ="
                        color: panel.mix(panel.pageColor, panel.inkColor, 0.5)
                        font.family: "iA Writer Mono S"
                        font.pixelSize: Math.max(7, Math.round(12.5 * panel.ui))
                    }

                    Row {
                        anchors.right: parent.right
                        spacing: Math.round(8 * panel.ui)

                        Text {
                            anchors.baseline: result.baseline
                            visible: entry.modelData.tag !== ""
                            text: entry.modelData.tag
                            color: panel.accentColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: Math.max(6, Math.round(10 * panel.ui))
                            font.letterSpacing: 0.6 * panel.ui
                        }

                        Text {
                            id: result
                            width: Math.min(implicitWidth, column.width)
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideLeft
                            text: entry.modelData.result
                            color: panel.inkColor
                            font.family: "iA Writer Mono S"
                            font.pixelSize: Math.max(8, Math.round(21 * panel.ui))
                        }
                    }
                }

                MouseArea {
                    id: entryArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        backend.recallHistory(entry.index);
                        panel.closeRequested();
                    }
                }

                Accessible.role: Accessible.ListItem
                Accessible.name: entry.modelData.expression + " equals " + entry.modelData.result
            }
        }

        Column {
            anchors.centerIn: list
            visible: backend.history.length === 0
            spacing: Math.round(6 * panel.ui)

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "No calculations yet"
                color: panel.mix(panel.pageColor, panel.inkColor, 0.55)
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(7, Math.round(13 * panel.ui))
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Results land here after ="
                color: panel.mix(panel.pageColor, panel.inkColor, 0.35)
                font.family: "iA Writer Mono S"
                font.pixelSize: Math.max(6, Math.round(11 * panel.ui))
            }
        }
    }
}
