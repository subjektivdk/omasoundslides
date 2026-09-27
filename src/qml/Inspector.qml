import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// Right-hand panel with two tabs, like Soundslides' Item Inspector and
// Project Inspector. Values in the accent color are set on the image itself;
// ↺ puts the project default back.
Rectangle {
    id: inspector

    // The filmstrip's current delegate, or null.
    property var item: null
    readonly property bool hasItem: item !== null && project.count > 0

    color: "#161618"
    radius: 12

    readonly property int controlWidth: 186

    component FieldLabel: Label {
        color: "#b8b8bc"
        font.pixelSize: 13
        Layout.fillWidth: true
        elide: Text.ElideRight
    }

    component ResetButton: ToolButton {
        property bool shown: false
        text: "↺"
        font.pixelSize: 16
        implicitWidth: 30
        implicitHeight: 30
        opacity: shown ? 1 : 0
        enabled: shown
        focusPolicy: Qt.NoFocus
        ToolTip.visible: hovered
        ToolTip.text: "Use the project default"
    }

    // A slim scroll bar: a quarter of Material's default width.
    component SlimScrollBar: ScrollBar {
        id: bar
        policy: ScrollBar.AsNeeded
        padding: 0
        implicitWidth: 4
        background: Item {}
        contentItem: Rectangle {
            implicitWidth: 4
            radius: 2
            color: bar.pressed ? theme.accent : "#6a6a70"
            opacity: bar.size < 1 && (bar.active || bar.hovered) ? 1 : 0.35
            visible: bar.size < 1
        }
    }

    component Hint: Label {
        Layout.fillWidth: true
        color: "#8a8a90"
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        TabBar {
            id: tabs
            Layout.fillWidth: true
            Material.background: "transparent"
            TabButton { text: "Image"; focusPolicy: Qt.NoFocus }
            TabButton { text: "Project"; focusPolicy: Qt.NoFocus }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            currentIndex: tabs.currentIndex

            // --- the selected image ---
            ScrollView {
                id: imagePage
                // Room for the scroll bar, so it never covers a field.
                rightPadding: 8
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical: SlimScrollBar {
                    parent: imagePage
                    x: imagePage.width - width
                    height: imagePage.height
                }

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    Label {
                        text: inspector.hasItem ? "Image " + (inspector.item.index + 1) + " of " + project.count : "No image selected"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Label {
                        visible: inspector.hasItem
                        Layout.fillWidth: true
                        text: inspector.hasItem ? inspector.item.fileName : ""
                        color: "#8a8a90"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                    Label {
                        visible: inspector.hasItem
                        text: inspector.hasItem
                              ? "Shown " + Format.time(inspector.item.start) + " – " + Format.time(inspector.item.start + inspector.item.duration)
                              : ""
                        color: "#8a8a90"
                        font.pixelSize: 12
                        font.family: "monospace"
                        Layout.bottomMargin: 6
                    }

                    GridLayout {
                        visible: inspector.hasItem
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 6
                        rowSpacing: 8

                        FieldLabel { text: "Duration (s)" }
                        NumberField {
                            objectName: "imageDurationField"
                            Layout.preferredWidth: inspector.controlWidth
                            minimum: 0.1
                            value: inspector.hasItem ? inspector.item.duration : 0
                            overridden: inspector.hasItem && inspector.item.durationSet
                            onCommitted: (v) => project.setDuration(inspector.item.index, v)
                        }
                        ResetButton {
                            shown: inspector.hasItem && inspector.item.durationSet
                            onClicked: project.resetDuration(inspector.item.index)
                        }

                        FieldLabel { text: "Transition in" }
                        TransitionBox {
                            objectName: "imageTransitionBox"
                            Layout.preferredWidth: inspector.controlWidth
                            enabled: inspector.hasItem && !inspector.item.isFirst
                            transition: inspector.hasItem ? inspector.item.transition : "none"
                            duration: inspector.hasItem ? inspector.item.transitionDuration : 0
                            overridden: inspector.hasItem && (inspector.item.transitionSet || inspector.item.transitionDurationSet)
                            onChosen: (transition, duration) => project.setTransitionPreset(inspector.item.index, transition, duration)
                        }
                        ResetButton {
                            shown: inspector.hasItem && (inspector.item.transitionSet || inspector.item.transitionDurationSet)
                            onClicked: project.resetTransitionPreset(inspector.item.index)
                        }
                    }

                    Hint {
                        visible: inspector.hasItem && inspector.item.isFirst
                        text: "The first image has no transition in."
                    }
                    Hint {
                        visible: inspector.hasItem
                        text: "Colored values are set on this image. Grey values come from the project."
                    }

                    RowLayout {
                        visible: inspector.hasItem
                        Layout.topMargin: 4
                        spacing: 6
                        Button {
                            text: "◀"
                            flat: true
                            enabled: inspector.hasItem && inspector.item.index > 0
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.text: "Move left (Ctrl ←)"
                            onClicked: win.moveSelected(-1)
                        }
                        Button {
                            text: "▶"
                            flat: true
                            enabled: inspector.hasItem && inspector.item.index < project.count - 1
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.text: "Move right (Ctrl →)"
                            onClicked: win.moveSelected(1)
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "Remove"
                            flat: true
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.text: "Remove the image from the show (Delete)"
                            onClicked: win.removeSelected()
                        }
                    }
                }
            }

            // --- project defaults and output ---
            ScrollView {
                id: projectPage
                // Room for the scroll bar, so it never covers a field.
                rightPadding: 8
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical: SlimScrollBar {
                    parent: projectPage
                    x: projectPage.width - width
                    height: projectPage.height
                }

                ColumnLayout {
                    width: parent.width
                    spacing: 8

                    Label {
                        text: "Name"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    TextField {
                        id: nameField
                        objectName: "projectNameField"
                        Layout.fillWidth: true
                        implicitHeight: 38
                        topPadding: 6
                        bottomPadding: 6
                        font.pixelSize: 14
                        placeholderText: "Untitled"
                        selectByMouse: true
                        Component.onCompleted: text = project.name
                        onEditingFinished: project.name = text
                        onAccepted: focus = false
                        Keys.onEscapePressed: {
                            text = project.name;
                            focus = false;
                        }
                        Connections {
                            target: project
                            function onNameChanged() {
                                if (!nameField.activeFocus)
                                    nameField.text = project.name;
                            }
                        }
                    }
                    Hint {
                        text: "Used as the file name when you save and export."
                        Layout.bottomMargin: 8
                    }

                    Label {
                        text: "Defaults for all images"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 8

                        FieldLabel { text: "Duration (s)" }
                        NumberField {
                            Layout.preferredWidth: inspector.controlWidth
                            minimum: 0.1
                            value: project.defaultDuration
                            onCommitted: (v) => project.defaultDuration = v
                        }

                        FieldLabel { text: "Transition" }
                        TransitionBox {
                            objectName: "defaultTransitionBox"
                            Layout.preferredWidth: inspector.controlWidth
                            transition: project.defaultTransition
                            duration: project.defaultTransitionDuration
                            onChosen: (transition, duration) => project.setDefaultTransitionPreset(transition, duration)
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: "Fit images to the audio"
                        enabled: project.count > 0 && project.audioDuration > 0
                        focusPolicy: Qt.NoFocus
                        onClicked: win.fitToAudio()
                    }
                    Hint {
                        text: "Gives every image the same duration so the show ends with the audio."
                    }

                    // Markers, which Audacity calls labels.
                    Label {
                        objectName: "labelsHeading"
                        text: "Labels"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.topMargin: 12
                    }
                    Hint {
                        text: "The markers on the timeline: where the images should change."
                    }

                    Button {
                        objectName: "fitToMarkersButton"
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: "Fit images to markers"
                        enabled: project.count > 1 && project.markers.length > 0
                        focusPolicy: Qt.NoFocus
                        onClicked: win.fitToMarkers()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Hint {
                            text: project.markers.length === 0
                                  ? "Press M while the audio plays to mark where each image should change."
                                  : project.markers.length + (project.markers.length === 1 ? " marker" : " markers")
                                    + " for " + Math.max(0, project.count - 1) + " image changes."
                        }
                        ToolButton {
                            visible: project.markers.length > 0
                            text: "Clear"
                            font.pixelSize: 12
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.text: "Remove all markers"
                            onClicked: project.clearMarkers()
                        }
                    }

                    // Round trip with Audacity's label track (File → Export
                    // Other → Export Labels / Import Labels).
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Button {
                            Layout.fillWidth: true
                            text: "Import labels…"
                            flat: true
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: "Replace the markers with labels exported from Audacity (.txt)"
                            onClicked: app.importMarkersDialog()
                        }
                        Button {
                            Layout.fillWidth: true
                            text: "Export labels…"
                            flat: true
                            enabled: project.markers.length > 0
                            focusPolicy: Qt.NoFocus
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: "Save the markers as an Audacity label file (.txt)"
                            onClicked: app.exportMarkersDialog()
                        }
                    }
                    Hint {
                        text: "Labels from Audacity become markers; a region label marks its start."
                    }

                    Label {
                        text: "Audio"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.topMargin: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 8

                        FieldLabel { text: "Fade in (s)" }
                        NumberField {
                            objectName: "audioFadeInField"
                            Layout.preferredWidth: inspector.controlWidth
                            step: 0.5
                            value: project.audioFadeIn
                            onCommitted: (v) => project.audioFadeIn = v
                        }

                        FieldLabel { text: "Fade out (s)" }
                        NumberField {
                            objectName: "audioFadeOutField"
                            Layout.preferredWidth: inspector.controlWidth
                            step: 0.5
                            value: project.audioFadeOut
                            onCommitted: (v) => project.audioFadeOut = v
                        }
                    }
                    Hint {
                        text: "0 means no fade. The fade out ends where the audio ends in the video."
                    }

                    Label {
                        text: "Video"
                        color: "white"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.topMargin: 12
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 8

                        FieldLabel { text: "Resolution" }
                        ComboBox {
                            Layout.preferredWidth: inspector.controlWidth
                            implicitHeight: 38
                            font.pixelSize: 13
                            readonly property var sizes: [[1920, 1080], [1280, 720], [3840, 2160], [1080, 1080], [1080, 1920]]
                            model: ["1080p", "720p", "4K", "Square", "Portrait"]
                            currentIndex: {
                                for (var i = 0; i < sizes.length; ++i)
                                    if (sizes[i][0] === project.outputWidth && sizes[i][1] === project.outputHeight)
                                        return i;
                                return -1;
                            }
                            displayText: currentIndex >= 0 ? currentText : project.outputWidth + "×" + project.outputHeight
                            onActivated: (i) => project.setResolution(sizes[i][0], sizes[i][1])
                        }

                        FieldLabel { text: "Frames/sec" }
                        ComboBox {
                            Layout.preferredWidth: inspector.controlWidth
                            implicitHeight: 38
                            font.pixelSize: 13
                            model: [24, 25, 30, 50, 60]
                            currentIndex: model.indexOf(project.fps)
                            displayText: project.fps
                            onActivated: (i) => project.fps = model[i]
                        }

                        FieldLabel { text: "Quality" }
                        ComboBox {
                            objectName: "exportQualityBox"
                            Layout.preferredWidth: inspector.controlWidth
                            implicitHeight: 38
                            font.pixelSize: 13
                            readonly property var values: ["standard", "high"]
                            model: ["Standard", "High"]
                            currentIndex: values.indexOf(project.exportQuality)
                            onActivated: (i) => project.exportQuality = values[i]
                        }
                    }
                    Hint {
                        text: "MP4 (H.264), plays everywhere. High is a little sharper and about 20 % larger."
                    }
                }
            }
        }
    }
}
