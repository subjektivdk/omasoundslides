import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "Format.js" as Format

// A seconds field: shows "5.00", accepts "5.5" or "5,5", commits on Enter or
// when focus leaves, Escape reverts. Up/Down and the mouse wheel nudge it.
TextField {
    id: field

    property real value: 0
    property real minimum: 0
    property real step: 0.5
    // Shown in the accent color when the value is set on this item rather
    // than inherited from the project defaults.
    property bool overridden: false
    // Tells the window this field never needs the space bar, so Space can
    // keep playing and pausing while it has focus.
    readonly property bool numeric: true
    property real wheelRemainder: 0
    signal committed(real value)

    implicitWidth: 84
    implicitHeight: 38
    topPadding: 6
    bottomPadding: 6
    horizontalAlignment: Text.AlignRight
    font.pixelSize: 14
    font.family: "monospace"
    color: overridden ? theme.accent : "#e6e6ea"
    selectByMouse: true
    opacity: enabled ? 1 : 0.4
    validator: RegularExpressionValidator { regularExpression: /^\d{0,3}([.,]\d{0,2})?$/ }

    function show() { text = Format.seconds(value); }
    function commit() {
        var v = Format.parseSeconds(text);
        if (!isNaN(v) && v >= minimum && Math.abs(v - value) > 0.0001)
            committed(v);
        show();
    }
    function nudge(delta) {
        var v = Math.max(minimum, Math.round((value + delta) * 100) / 100);
        if (Math.abs(v - value) > 0.0001)
            committed(v);
    }

    Component.onCompleted: show()
    onValueChanged: if (!activeFocus) show()
    onEditingFinished: commit()
    // Enter commits and hands the keyboard back to the shortcuts.
    onAccepted: field.focus = false
    onActiveFocusChanged: if (activeFocus) selectAll()

    // Scroll to adjust, like the images on the timeline. Shift: five steps.
    WheelHandler {
        acceptedModifiers: Qt.NoModifier
        onWheel: (event) => field.nudge(Format.wheelNotches(field, event) * keys.scrollStep)
    }
    WheelHandler {
        acceptedModifiers: Qt.ShiftModifier
        onWheel: (event) => field.nudge(Format.wheelNotches(field, event) * keys.scrollStep * 5)
    }

    // Text fields claim the space bar before shortcuts see it. A number has
    // no use for it, so hand it to play/pause when that is bound to Space.
    Keys.onSpacePressed: (event) => {
        const win = field.ApplicationWindow.window;
        if (win && keys.keys.play_pause.indexOf("Space") >= 0) {
            field.commit();
            win.togglePlay();
        }
    }
    Keys.onEscapePressed: {
        show();
        field.focus = false;
    }
    Keys.onUpPressed: nudge(step)
    Keys.onDownPressed: nudge(-step)
}
