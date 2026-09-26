import QtQuick
import QtQuick.Controls

// The one primary action on screen, filled with the theme accent.
Button {
    id: button
    focusPolicy: Qt.NoFocus
    font.pixelSize: 14
    font.weight: Font.DemiBold
    leftPadding: 18
    rightPadding: 18

    background: Rectangle {
        implicitHeight: 36
        radius: 8
        color: theme.accent
        opacity: button.enabled ? (button.down ? 0.8 : 1) : 0.4
    }
    contentItem: Label {
        text: button.text
        font: button.font
        color: theme.accentForeground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }
}
