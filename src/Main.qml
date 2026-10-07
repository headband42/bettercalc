import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window

ApplicationWindow {
    id: win

    visible: true
    title: "Bettercalc"
    minimumWidth: 300
    minimumHeight: 420

    // Every size in the interface is written at its mode's design size;
    // resizing the window scales the whole face along with it.
    readonly property var designSizes: [Qt.size(400, 620), Qt.size(880, 620), Qt.size(760, 820)]
    readonly property size designSize: designSizes[backend.mode]
    readonly property real ui: Math.max(0.45, Math.min(width / designSize.width,
                                                       height / designSize.height))
    property real appliedTextScale: backend.textScale

    function s(pixels) {
        return Math.max(1, Math.round(pixels * ui));
    }

    function mix(base, tint, amount) {
        return Qt.rgba(base.r + (tint.r - base.r) * amount,
                       base.g + (tint.g - base.g) * amount,
                       base.b + (tint.b - base.b) * amount, 1);
    }

    readonly property color pageColor: theme.background
    readonly property color inkColor: theme.foreground

    color: pageColor
    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent

    property bool historyOpen: false
    property bool helpOpen: false

    // A key typed on the keyboard lights up its twin on the keypad.
    property string flashedKey: ""
    Timer {
        id: flashTimer
        interval: 140
        onTriggered: win.flashedKey = ""
    }
    function flash(key) {
        flashedKey = key;
        flashTimer.restart();
    }

    readonly property var basicKeys: [
        { key: "clear", label: "AC", name: "All clear" },
        { key: "sign", label: "±", name: "Toggle sign" },
        { key: "%", label: "%", name: "Percent" },
        { key: "/", label: "÷", kind: "operator", name: "Divide" },
        { key: "7", label: "7" }, { key: "8", label: "8" }, { key: "9", label: "9" },
        { key: "*", label: "×", kind: "operator", name: "Multiply" },
        { key: "4", label: "4" }, { key: "5", label: "5" }, { key: "6", label: "6" },
        { key: "-", label: "−", kind: "operator", name: "Subtract" },
        { key: "1", label: "1" }, { key: "2", label: "2" }, { key: "3", label: "3" },
        { key: "+", label: "+", kind: "operator", name: "Add" },
        { key: "0", label: "0" },
        { key: ".", label: ".", name: "Decimal point" },
        { key: "backspace", label: "", icon: "backspace", name: "Backspace" },
        { key: "=", label: "=", kind: "equals", name: "Equals" }
    ]

    readonly property var scientificKeys: [
        { key: "2nd", label: "2nd", kind: "toggle", small: true, name: "Second functions" },
        { key: "angle", label: "DEG", kind: "function", small: true, name: "Angle unit" },
        { key: "(", label: "(", kind: "function", name: "Open parenthesis" },
        { key: ")", label: ")", kind: "function", name: "Close parenthesis" },
        { key: "mod", label: "mod", kind: "function", small: true, name: "Modulo" },
        { key: "ans", label: "Ans", kind: "function", small: true, name: "Last answer" },

        { key: "square", label: "x<sup>2</sup>", second: "x<sup>3</sup>", kind: "function", small: true, name: "Square" },
        { key: "^", label: "x<sup>y</sup>", kind: "function", small: true, name: "Power" },
        { key: "sqrt", label: "√x", second: "<sup>3</sup>√x", kind: "function", small: true, name: "Square root" },
        { key: "root", label: "<sup>y</sup>√x", kind: "function", small: true, name: "Root" },
        { key: "inverse", label: "1/x", kind: "function", small: true, name: "Reciprocal" },
        { key: "!", label: "x!", kind: "function", small: true, name: "Factorial" },

        { key: "exp", label: "e<sup>x</sup>", kind: "function", small: true, name: "Exponential" },
        { key: "pow10", label: "10<sup>x</sup>", second: "2<sup>x</sup>", kind: "function", small: true, name: "Power of ten" },
        { key: "ln", label: "ln", kind: "function", small: true, name: "Natural logarithm" },
        { key: "log", label: "log", second: "log<sub>2</sub>", kind: "function", small: true, name: "Logarithm" },
        { key: "abs", label: "|x|", kind: "function", small: true, name: "Absolute value" },
        { key: "ee", label: "EE", kind: "function", small: true, name: "Exponent" },

        { key: "sin", trig: true, kind: "function", small: true, name: "Sine" },
        { key: "cos", trig: true, kind: "function", small: true, name: "Cosine" },
        { key: "tan", trig: true, kind: "function", small: true, name: "Tangent" },
        { key: "pi", label: "π", kind: "function", name: "Pi" },
        { key: "e", label: "e", kind: "function", name: "Euler's number" },
        { key: "rand", label: "Rand", kind: "function", small: true, name: "Random number" },

        { key: "hyp", label: "hyp", kind: "toggle", small: true, name: "Hyperbolic functions" },
        { key: "mc", label: "MC", kind: "function", small: true, name: "Memory clear" },
        { key: "mr", label: "MR", kind: "function", small: true, name: "Memory recall" },
        { key: "mplus", label: "M+", kind: "function", small: true, name: "Memory add" },
        { key: "mminus", label: "M−", kind: "function", small: true, name: "Memory subtract" },
        { key: "ms", label: "MS", kind: "function", small: true, name: "Memory store" }
    ]

    readonly property var programmerKeys: [
        { key: "A", label: "A" }, { key: "B", label: "B" },
        { key: "and", label: "AND", kind: "function", small: true },
        { key: "or", label: "OR", kind: "function", small: true },
        { key: "C", label: "C" }, { key: "D", label: "D" },
        { key: "xor", label: "XOR", kind: "function", small: true },
        { key: "not", label: "NOT", kind: "function", small: true },
        { key: "E", label: "E" }, { key: "F", label: "F" },
        { key: "nand", label: "NAND", kind: "function", small: true },
        { key: "nor", label: "NOR", kind: "function", small: true },
        { key: "shl", label: "<<", kind: "function", small: true, name: "Shift left" },
        { key: "shr", label: ">>", kind: "function", small: true, name: "Shift right" },
        { key: "rol", label: "ROL", kind: "function", small: true, name: "Rotate left" },
        { key: "ror", label: "ROR", kind: "function", small: true, name: "Rotate right" },
        { key: "(", label: "(", kind: "function", name: "Open parenthesis" },
        { key: ")", label: ")", kind: "function", name: "Close parenthesis" },
        { key: "ans", label: "Ans", kind: "function", small: true, name: "Last answer" },
        { key: "word", label: "QWORD", kind: "function", small: true, name: "Word size" }
    ]

    readonly property var programmerPad: [
        { key: "clear", label: "AC", name: "All clear" },
        { key: "sign", label: "±", name: "Toggle sign" },
        { key: "mod", label: "mod", small: true, name: "Remainder" },
        { key: "/", label: "÷", kind: "operator", name: "Divide" },
        { key: "7", label: "7" }, { key: "8", label: "8" }, { key: "9", label: "9" },
        { key: "*", label: "×", kind: "operator", name: "Multiply" },
        { key: "4", label: "4" }, { key: "5", label: "5" }, { key: "6", label: "6" },
        { key: "-", label: "−", kind: "operator", name: "Subtract" },
        { key: "1", label: "1" }, { key: "2", label: "2" }, { key: "3", label: "3" },
        { key: "+", label: "+", kind: "operator", name: "Add" },
        { key: "0", label: "0", span: 2 },
        { key: "backspace", label: "", icon: "backspace", name: "Backspace" },
        { key: "=", label: "=", kind: "equals", name: "Equals" }
    ]

    function press(key) {
        flash(key);
        backend.pressKey(key);
    }

    // The keypad key a typed character corresponds to, for the flash.
    function keyForCharacter(text) {
        var programmer = backend.mode === 2;
        if (/^[0-9]$/.test(text))
            return text;
        if (programmer && /^[a-fA-F]$/.test(text))
            return text.toUpperCase();
        var map = programmer
            ? { "%": "mod", "^": "xor", "&": "and", "|": "or", "~": "not", "<": "shl", ">": "shr" }
            : { "%": "%", "^": "^", "!": "!", ",": ".", "E": "ee" };
        if (map[text] !== undefined)
            return map[text];
        return text;
    }

    function switchMode(mode) {
        if (mode === backend.mode)
            return;
        if (visibility === Window.Windowed)
            backend.saveWindowSize(backend.mode, width, height, false);
        backend.mode = mode;
        if (visibility === Window.Windowed)
            applySize(mode);
    }

    function applySize(mode) {
        var saved = backend.windowSize(mode);
        if (saved.valid) {
            width = saved.width;
            height = saved.height;
        } else {
            width = Math.round(designSizes[mode].width * backend.textScale);
            height = Math.round(designSizes[mode].height * backend.textScale);
        }
    }

    Shortcut {
        sequences: ["Ctrl+C", "Meta+C"]
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.copyResult();
            display.showCopied();
        }
    }
    Shortcut {
        sequences: ["Ctrl+V", "Meta+V"]
        context: Qt.ApplicationShortcut
        onActivated: backend.paste()
    }
    Shortcut {
        sequence: "Ctrl+Z"
        context: Qt.ApplicationShortcut
        onActivated: backend.undo()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        context: Qt.ApplicationShortcut
        onActivated: backend.redo()
    }
    Shortcut {
        sequence: "Ctrl+1"
        context: Qt.ApplicationShortcut
        onActivated: win.switchMode(0)
    }
    Shortcut {
        sequence: "Ctrl+2"
        context: Qt.ApplicationShortcut
        onActivated: win.switchMode(1)
    }
    Shortcut {
        sequence: "Ctrl+3"
        context: Qt.ApplicationShortcut
        onActivated: win.switchMode(2)
    }
    Shortcut {
        sequence: "Ctrl+H"
        context: Qt.ApplicationShortcut
        onActivated: {
            win.helpOpen = false;
            win.historyOpen = !win.historyOpen;
        }
    }
    Shortcut {
        sequence: "Ctrl+D"
        context: Qt.ApplicationShortcut
        onActivated: backend.cycleAngleUnit()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: win.close()
    }

    Item {
        id: face
        anchors.fill: parent
        anchors.margins: win.s(18)
        focus: true

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape && (win.helpOpen || win.historyOpen)) {
                win.helpOpen = false;
                win.historyOpen = false;
                event.accepted = true;
                return;
            }
            if (event.text === "?") {
                win.historyOpen = false;
                win.helpOpen = !win.helpOpen;
                event.accepted = true;
                return;
            }
            if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
                return;

            if (backend.mode === 2) {
                var functionKeys = {};
                functionKeys[Qt.Key_F5] = function() { backend.base = 16; };
                functionKeys[Qt.Key_F6] = function() { backend.base = 10; };
                functionKeys[Qt.Key_F7] = function() { backend.base = 8; };
                functionKeys[Qt.Key_F8] = function() { backend.base = 2; };
                functionKeys[Qt.Key_F12] = function() { backend.wordSize = 64; };
                functionKeys[Qt.Key_F2] = function() { backend.wordSize = 32; };
                functionKeys[Qt.Key_F3] = function() { backend.wordSize = 16; };
                functionKeys[Qt.Key_F4] = function() { backend.wordSize = 8; };
                if (functionKeys[event.key] !== undefined) {
                    functionKeys[event.key]();
                    event.accepted = true;
                    return;
                }
            }

            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                    || event.key === Qt.Key_Space) {
                win.press("=");
            } else if (event.key === Qt.Key_Backspace) {
                win.press("backspace");
            } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Delete) {
                win.press("clear");
            } else if (event.text.length === 1 && event.text > " ") {
                win.flash(win.keyForCharacter(event.text));
                backend.typeText(event.text);
            } else {
                return;
            }
            event.accepted = true;
        }

        Item {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: switcher.height

            ModeSwitcher {
                id: switcher
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                ui: win.ui
                current: backend.mode
                onSelected: function(index) { win.switchMode(index); }
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: win.s(4)

                IconButton {
                    width: win.s(32)
                    height: width
                    icon: "history"
                    accessibleName: "History"
                    active: win.historyOpen
                    onActivated: {
                        win.helpOpen = false;
                        win.historyOpen = !win.historyOpen;
                    }
                }

                IconButton {
                    width: win.s(32)
                    height: width
                    icon: "help"
                    accessibleName: "Keyboard shortcuts"
                    active: win.helpOpen
                    onActivated: {
                        win.historyOpen = false;
                        win.helpOpen = !win.helpOpen;
                    }
                }
            }
        }

        Display {
            id: display
            anchors.top: header.bottom
            anchors.topMargin: win.s(16)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: win.s(6)
            anchors.rightMargin: win.s(6)
            height: win.s(backend.mode === 2 ? 132 : 172)
            ui: win.ui
            mode: backend.mode
        }

        ProgrammerPanel {
            id: programmerPanel
            anchors.top: display.bottom
            anchors.topMargin: win.s(14)
            anchors.left: parent.left
            anchors.right: parent.right
            height: visible ? win.s(214) : 0
            visible: backend.mode === 2
            ui: win.ui
        }

        Rectangle {
            id: divider
            anchors.top: programmerPanel.visible ? programmerPanel.bottom : display.bottom
            anchors.topMargin: win.s(16)
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: win.mix(win.pageColor, win.inkColor, 0.12)
        }

        Item {
            id: keypad
            anchors.top: divider.bottom
            anchors.topMargin: win.s(18)
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            readonly property real gap: win.s(10)
            readonly property real blockGap: win.s(18)

            KeyGrid {
                visible: backend.mode === 0
                anchors.fill: parent
                keys: win.basicKeys
                columns: 4
                gap: keypad.gap
                flashedKey: win.flashedKey
                onKeyPressed: function(key) { win.press(key); }
            }

            Item {
                visible: backend.mode === 1
                anchors.fill: parent

                KeyGrid {
                    id: scientificBlock
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.round((parent.width - keypad.blockGap - 8 * keypad.gap) * 0.6 + 5 * keypad.gap)
                    keys: win.scientificKeys
                    columns: 6
                    gap: keypad.gap
                    flashedKey: win.flashedKey
                    onKeyPressed: function(key) { win.press(key); }
                }

                KeyGrid {
                    anchors.left: scientificBlock.right
                    anchors.leftMargin: keypad.blockGap
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    keys: win.basicKeys
                    columns: 4
                    gap: keypad.gap
                    flashedKey: win.flashedKey
                    onKeyPressed: function(key) { win.press(key); }
                }
            }

            Item {
                visible: backend.mode === 2
                anchors.fill: parent

                KeyGrid {
                    id: programmerBlock
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.round((parent.width - keypad.blockGap) / 2)
                    keys: win.programmerKeys
                    columns: 4
                    gap: keypad.gap
                    flashedKey: win.flashedKey
                    onKeyPressed: function(key) { win.press(key); }
                }

                KeyGrid {
                    anchors.left: programmerBlock.right
                    anchors.leftMargin: keypad.blockGap
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    keys: win.programmerPad
                    columns: 4
                    gap: keypad.gap
                    flashedKey: win.flashedKey
                    onKeyPressed: function(key) { win.press(key); }
                }
            }
        }
    }

    HistoryPanel {
        anchors.fill: parent
        open: win.historyOpen
        ui: win.ui
        onCloseRequested: win.historyOpen = false
    }

    HelpOverlay {
        anchors.fill: parent
        open: win.helpOpen
        ui: win.ui
        onCloseRequested: win.helpOpen = false
    }

    Connections {
        target: backend

        // Follow the desktop's text size live by growing or shrinking the
        // window, so the face re-flows along with the rest of the desktop.
        function onTextScaleChanged() {
            var factor = backend.textScale / win.appliedTextScale;
            win.appliedTextScale = backend.textScale;
            if (win.visibility === Window.Windowed) {
                win.width = Math.round(win.width * factor);
                win.height = Math.round(win.height * factor);
            }
        }
    }

    // Remember the last windowed size rather than whatever the window happens
    // to measure at teardown: a maximized window reports screen-sized
    // dimensions, and the close sequence hides the window before destruction.
    property size normalSize: Qt.size(width, height)
    property bool wasMaximized: false

    function trackNormalSize() {
        if (win.visibility === Window.Windowed)
            normalSize = Qt.size(width, height);
    }

    onWidthChanged: trackNormalSize()
    onHeightChanged: trackNormalSize()
    onVisibilityChanged: {
        if (win.visibility === Window.Maximized || win.visibility === Window.FullScreen)
            wasMaximized = true;
        else if (win.visibility === Window.Windowed)
            wasMaximized = false;
    }

    Component.onCompleted: {
        applySize(backend.mode);
        if (backend.windowSize(backend.mode).maximized)
            showMaximized();
    }

    Component.onDestruction: backend.saveWindowSize(backend.mode, normalSize.width,
                                                    normalSize.height, wasMaximized)
}
