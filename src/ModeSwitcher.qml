import QtQuick

// The segmented control across the top. The highlight slides between the
// modes rather than jumping.
Item {
    id: switcher

    property var modes: ["Basic", "Scientific", "Programmer"]
    property int current: 0
    property real ui: 1

    signal selected(int index)

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground
    readonly property real inset: Math.max(2, Math.round(3 * ui))
    readonly property real segmentWidth: Math.ceil(metrics.advanceWidth + 18 * ui)

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    implicitWidth: segmentWidth * modes.length + inset * 2
    implicitHeight: Math.round(34 * ui)

    TextMetrics {
        id: metrics
        font.family: "iA Writer Mono S"
        font.pixelSize: Math.max(8, Math.round(12 * switcher.ui))
        text: "Programmer"
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: switcher.mix(switcher.pageColor, switcher.inkColor, 0.055)
        border.width: 1
        border.color: switcher.mix(switcher.pageColor, switcher.inkColor, 0.09)
    }

    Rectangle {
        x: switcher.inset + switcher.current * switcher.segmentWidth
        y: switcher.inset
        width: switcher.segmentWidth
        height: switcher.height - switcher.inset * 2
        radius: height / 2
        color: switcher.mix(switcher.pageColor, switcher.inkColor, 0.16)

        Behavior on x { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }

    Repeater {
        model: switcher.modes

        Item {
            required property int index
            required property string modelData

            x: switcher.inset + index * switcher.segmentWidth
            width: switcher.segmentWidth
            height: switcher.height

            Text {
                anchors.centerIn: parent
                text: parent.modelData
                color: parent.index === switcher.current || hover.containsMouse
                    ? switcher.inkColor
                    : switcher.mix(switcher.pageColor, switcher.inkColor, 0.55)
                font.family: "iA Writer Mono S"
                font.pixelSize: metrics.font.pixelSize

                Behavior on color { ColorAnimation { duration: 160 } }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: switcher.selected(parent.index)
            }

            Accessible.role: Accessible.PageTab
            Accessible.name: modelData
            Accessible.onPressAction: switcher.selected(index)
        }
    }
}
