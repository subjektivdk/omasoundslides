import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "Format.js" as Format

ComboBox {
    id: box

    property string value: "fade"
    property bool overridden: false
    signal chosen(string name)

    implicitHeight: 38
    model: project.transitionNames
    currentIndex: model.indexOf(value)
    displayText: Format.transition(value)
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.4
    Material.foreground: overridden ? theme.accent : "#e6e6ea"

    delegate: ItemDelegate {
        required property string modelData
        required property int index
        width: ListView.view.width
        text: Format.transition(modelData)
        highlighted: box.highlightedIndex === index
        font.pixelSize: 13
    }

    onActivated: (index) => chosen(model[index])
}
