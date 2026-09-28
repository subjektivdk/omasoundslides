// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "Format.js" as Format

// Soundslides' transition choices: straight cut, crossfade and fade out/in,
// the last two at fast, medium or slow speed. One choice sets both the
// transition and its length.
ComboBox {
    id: box

    property string transition: "fade"
    property real duration: 1
    property bool overridden: false
    signal chosen(string transition, real duration)

    function presetIndex() {
        for (var i = 0; i < model.length; ++i) {
            var p = model[i];
            if (p.transition === transition && (transition === "none" || Math.abs(p.duration - duration) < 0.001))
                return i;
        }
        return -1;
    }

    implicitHeight: 38
    model: project.transitionPresets
    textRole: "label"
    currentIndex: presetIndex()
    // A length that isn't one of the three speeds (typed into the project
    // file by hand) still shows what it is.
    displayText: currentIndex >= 0 ? currentText
                 : Format.transition(transition) + " – " + Format.seconds(duration) + " s"
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.4
    Material.foreground: overridden ? theme.accent : theme.text

    delegate: ItemDelegate {
        required property var modelData
        required property int index
        width: ListView.view.width
        text: modelData.label
        highlighted: box.highlightedIndex === index
        font.pixelSize: 13
    }

    onActivated: (index) => chosen(model[index].transition, model[index].duration)
}
