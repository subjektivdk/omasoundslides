// SPDX-FileCopyrightText: 2026 Martin Jensen
//
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Shapes
import Omasoundslides
import "Format.js" as Format

// Two tracks on one time axis: the images (each as wide as it lasts, the
// next one overlapping it by its transition) and the audio waveform, with
// markers across both.
//
// - Click or drag on the ruler or the audio: move the playhead.
// - Click an image: select it. Drag its right edge to change its duration;
//   the edge snaps to the playhead, markers, the audio end and the images
//   before it (hold Shift to place it freely).
// - Click a transition: choose another one.
// - Drag the small squares on the audio: fade in / fade out.
// - Drag a marker's flag to move it, double-click it to remove it.
// - Scroll to zoom around the mouse; Shift + scroll or a sideways swipe pans.
Rectangle {
    id: tl

    property real position: 0
    property int selected: -1
    property real pps: 10            // pixels per second
    property real viewStart: 0       // seconds at the left edge
    property bool fitted: true       // follows the length while zoomed out

    readonly property real length: Math.max(project.videoDuration, audioPreview.duration, 1)
    readonly property real visibleSeconds: area.width / pps
    readonly property real minPps: area.width / length
    readonly property real maxPps: 400
    // How close (in pixels) an edge must come to a snap point to jump to it.
    readonly property real snapPixels: 8
    readonly property color markerColor: theme.marker

    // Shown while an edge is being snapped, in seconds; -1 when not snapping.
    property real snapLine: -1
    // The duration feedback shown while dragging an image's edge.
    property int feedbackIndex: -1
    property real feedbackFrom: 0

    signal seekRequested(real seconds)
    signal imageClicked(int index)

    color: theme.panel
    radius: 12

    function fit() {
        if (area.width <= 0)
            return;
        pps = area.width / length;
        viewStart = 0;
        fitted = true;
    }
    function clampView() {
        viewStart = Math.max(0, Math.min(viewStart, length - visibleSeconds));
    }
    function zoom(factor, anchorSeconds) {
        var anchorX = (anchorSeconds - viewStart) * pps;
        pps = Math.max(minPps, Math.min(maxPps, pps * factor));
        fitted = pps <= minPps * 1.001;
        viewStart = anchorSeconds - anchorX / pps;
        clampView();
    }
    function ensureVisible(seconds) {
        if (seconds < viewStart || seconds > viewStart + visibleSeconds * 0.95) {
            viewStart = seconds - visibleSeconds * 0.1;
            clampView();
        }
    }
    function blockAt(index) {
        return blocks.itemAt(index);
    }
    function secondsAt(x) {
        return Math.max(0, viewStart + x / pps);
    }
    function xOf(seconds) {
        return (seconds - viewStart) * pps;
    }
    function wheelSteps(event) {
        var d = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
        return d / 120;
    }

    // The nearest snap point to `seconds` within snapPixels, or `seconds`
    // itself. Only things that stay put while image `index` changes count:
    // the playhead, markers, the audio end and the images before it.
    function snapTime(seconds, index) {
        var best = seconds;
        var bestDistance = snapPixels / pps;
        function consider(candidate) {
            var distance = Math.abs(candidate - seconds);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = candidate;
            }
        }
        consider(position);
        for (var m = 0; m < project.markers.length; ++m)
            consider(project.markers[m]);
        if (audioPreview.ready)
            consider(audioPreview.duration);
        for (var i = 0; i < index; ++i) {
            consider(project.startOf(i));
            consider(project.startOf(i) + project.durationOf(i));
        }
        consider(project.startOf(index));
        return best;
    }

    function showFeedback(index, from) {
        if (feedbackIndex !== index || !feedbackTimer.running)
            feedbackFrom = from;
        feedbackIndex = index;
        feedbackTimer.restart();
    }

    Timer {
        id: feedbackTimer
        interval: 1200
        onTriggered: tl.feedbackIndex = -1
    }

    onLengthChanged: fitted ? fit() : clampView()
    Component.onCompleted: fit()

    Item {
        id: area
        anchors.fill: parent
        anchors.margins: 10
        anchors.bottomMargin: 16
        clip: true

        onWidthChanged: tl.fitted ? tl.fit() : tl.clampView()

        // --- ruler ---
        Item {
            id: ruler
            width: parent.width
            height: 20

            readonly property real interval: {
                var steps = [0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600];
                for (var i = 0; i < steps.length; ++i)
                    if (steps[i] * tl.pps >= 70)
                        return steps[i];
                return 1200;
            }
            readonly property real first: Math.floor(tl.viewStart / interval) * interval

            Repeater {
                model: Math.ceil(tl.visibleSeconds / ruler.interval) + 2
                delegate: Item {
                    required property int index
                    readonly property real t: ruler.first + index * ruler.interval
                    x: tl.xOf(t)
                    height: ruler.height
                    Rectangle { width: 1; height: 6; anchors.bottom: parent.bottom; color: theme.border }
                    Label {
                        x: 4
                        text: Format.clock(parent.t)
                        color: theme.textMuted
                        font.pixelSize: 10
                        font.family: "monospace"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                onPressed: (mouse) => { tl.forceActiveFocus(); tl.seekRequested(tl.secondsAt(mouse.x)); }
                onPositionChanged: (mouse) => { if (pressed) tl.seekRequested(tl.secondsAt(mouse.x)); }
            }
        }

        // --- images ---
        Item {
            id: imageTrack
            y: ruler.height + 4
            width: parent.width
            height: 84

            Repeater {
                id: blocks
                model: project
                delegate: Item {
                    id: block

                    required property int index
                    required property url url
                    required property string fileName
                    required property real duration
                    required property bool durationSet
                    required property string transition
                    required property bool transitionSet
                    required property real transitionDuration
                    required property bool transitionDurationSet
                    required property real start
                    required property bool isFirst

                    readonly property bool selected: index === tl.selected
                    readonly property bool hasTransition: !isFirst && transition !== "none" && transitionDuration > 0

                    x: tl.xOf(start)
                    width: Math.max(3, duration * tl.pps)
                    height: imageTrack.height
                    z: index
                    visible: x + width >= 0 && x <= imageTrack.width

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "black"
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: block.url
                            sourceSize.height: 168
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            autoTransform: true
                        }

                        // The overlap with the previous image, drawn as what
                        // happens in it: a crossfade rises from one image to
                        // the next; a fade out/in dips through black.
                        Shape {
                            id: transitionShape
                            visible: block.hasTransition
                            width: block.transitionDuration * tl.pps
                            height: parent.height
                            preferredRendererType: Shape.CurveRenderer
                            readonly property bool dip: block.transition === "fadeblack"

                            ShapePath {
                                strokeWidth: 0
                                strokeColor: "transparent"
                                fillColor: transitionShape.dip ? "#d0000000" : "#90000000"
                                startX: 0; startY: 0
                                PathLine { x: transitionShape.dip ? transitionShape.width / 2 : 0; y: transitionShape.height }
                                PathLine { x: transitionShape.dip ? transitionShape.width : 0; y: transitionShape.dip ? 0 : transitionShape.height }
                                PathLine { x: transitionShape.dip ? 0 : transitionShape.width; y: 0 }
                            }
                            ShapePath {
                                strokeWidth: 1.5
                                strokeColor: hoverTransition.hovered ? theme.accent : "#c0ffffff"
                                fillColor: "transparent"
                                startX: 0; startY: transitionShape.dip ? 0 : transitionShape.height
                                PathLine { x: transitionShape.dip ? transitionShape.width / 2 : transitionShape.width; y: transitionShape.dip ? transitionShape.height : 0 }
                                PathLine { x: transitionShape.width; y: 0 }
                            }

                            HoverHandler { id: hoverTransition; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                onTapped: {
                                    tl.forceActiveFocus();
                                    transitionMenu.index = block.index;
                                    transitionMenu.popup();
                                }
                            }
                            ToolTip.visible: hoverTransition.hovered
                            ToolTip.delay: 500
                            ToolTip.text: Format.transition(block.transition) + " – " + Format.seconds(block.transitionDuration) + " s · click to change"
                        }

                        Rectangle {
                            x: Math.max(4, transitionShape.visible ? transitionShape.width + 4 : 4)
                            y: 4
                            visible: block.width > x + 22
                            width: number.implicitWidth + 8
                            height: 16
                            radius: 3
                            color: block.selected ? theme.accent : "#000000b0"
                            Label {
                                id: number
                                anchors.centerIn: parent
                                text: block.index + 1
                                color: block.selected ? theme.accentForeground : "white"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 4
                            visible: block.width > 64
                            width: durationLabel.implicitWidth + 8
                            height: 16
                            radius: 3
                            color: "#000000b0"
                            Label {
                                id: durationLabel
                                anchors.centerIn: parent
                                text: Format.seconds(block.duration)
                                color: block.durationSet ? Qt.lighter(theme.accent, 1.6) : "white"
                                font.pixelSize: 10
                                font.family: "monospace"
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: "transparent"
                        border.width: block.selected ? 2 : 1
                        border.color: block.selected ? theme.accent : (hover.hovered ? theme.textMuted : "#000000")
                    }

                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: { tl.forceActiveFocus(); tl.imageClicked(block.index); }
                    }
                }
            }

            // Drag handles on every image's end, above all images: the end of
            // one image sits inside the next one's transition.
            Repeater {
                model: project
                delegate: MouseArea {
                    id: handle
                    required property int index
                    required property real start
                    required property real duration

                    property real pressX: 0
                    property real pressDuration: 0

                    z: 1000
                    x: tl.xOf(start + duration) - width / 2
                    width: 10
                    height: imageTrack.height
                    visible: x + width >= 0 && x <= imageTrack.width
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true
                    preventStealing: true

                    Rectangle {
                        anchors.centerIn: parent
                        width: 3
                        height: parent.height * 0.5
                        radius: 1.5
                        color: theme.accent
                        visible: handle.containsMouse || handle.pressed
                    }

                    onPressed: (mouse) => {
                        pressX = mapToItem(tl, mouse.x, 0).x;
                        pressDuration = duration;
                        tl.imageClicked(index);
                        tl.showFeedback(index, duration);
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return;
                        var end = start + pressDuration + (mapToItem(tl, mouse.x, 0).x - pressX) / tl.pps;
                        var snapped = (mouse.modifiers & Qt.ShiftModifier) ? end : tl.snapTime(end, index);
                        tl.snapLine = snapped !== end ? snapped : -1;
                        tl.showFeedback(index, pressDuration);
                        project.setDuration(index, Math.max(0.1, snapped - start));
                    }
                    onReleased: tl.snapLine = -1
                    onCanceled: tl.snapLine = -1
                }
            }

            // "9.32 s → 10.10 s (+0.78)" while an image's edge is being dragged.
            Rectangle {
                id: feedback
                readonly property int index: tl.feedbackIndex
                readonly property real now: {
                    project.videoDuration; // re-read after every edit
                    return index >= 0 ? project.durationOf(index) : 0;
                }
                readonly property real endX: index >= 0 ? tl.xOf(project.startOf(index) + now) : 0
                visible: index >= 0 && index < project.count
                z: 1500
                x: Math.max(2, Math.min(endX - width - 6, imageTrack.width - width - 2))
                y: (imageTrack.height - height) / 2
                width: feedbackLabel.implicitWidth + 14
                height: 24
                radius: 6
                color: Qt.alpha(theme.panel, 0.94)
                border.color: theme.accent
                Label {
                    id: feedbackLabel
                    anchors.centerIn: parent
                    readonly property real delta: feedback.now - tl.feedbackFrom
                    text: Format.seconds(tl.feedbackFrom) + " s → " + Format.seconds(feedback.now) + " s ("
                          + (delta >= 0 ? "+" : "−") + Format.seconds(Math.abs(delta)) + ")"
                    color: theme.textStrong
                    font.pixelSize: 11
                    font.family: "monospace"
                }
            }
        }

        // --- audio ---
        Rectangle {
            id: audioTrack
            y: imageTrack.y + imageTrack.height + 6
            width: parent.width
            height: parent.height - y
            radius: 4
            color: theme.raised

            readonly property real end: project.audioEnd
            readonly property real endX: tl.xOf(end)
            readonly property real fadeInX: tl.xOf(Math.min(project.audioFadeIn, end))
            readonly property real fadeOutX: tl.xOf(Math.max(0, end - project.audioFadeOut))

            Waveform {
                anchors.fill: parent
                preview: audioPreview
                viewStart: tl.viewStart
                pixelsPerSecond: tl.pps
                color: theme.waveform
            }
            Label {
                anchors.centerIn: parent
                visible: !audioPreview.ready
                text: audioPreview.building ? "Reading the audio…"
                      : (project.audio.length === 0 ? "No audio. Add some with + Audio or drop a file here." : "")
                color: theme.textFaint
                font.pixelSize: 12
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                onPressed: (mouse) => { tl.forceActiveFocus(); tl.seekRequested(tl.secondsAt(mouse.x)); }
                onPositionChanged: (mouse) => { if (pressed) tl.seekRequested(tl.secondsAt(mouse.x)); }
            }

            // The fades: the waveform is cut away above a line that rises from
            // silence at the start and falls to silence at the end.
            Shape {
                anchors.fill: parent
                visible: audioPreview.ready
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeWidth: 0
                    strokeColor: "transparent"
                    fillColor: Qt.alpha(theme.raised, 0.88)
                    startX: tl.xOf(0); startY: 0
                    PathLine { x: tl.xOf(0); y: audioTrack.height }
                    PathLine { x: audioTrack.fadeInX; y: 0 }
                }
                ShapePath {
                    strokeWidth: 0
                    strokeColor: "transparent"
                    fillColor: Qt.alpha(theme.raised, 0.88)
                    startX: audioTrack.fadeOutX; startY: 0
                    PathLine { x: audioTrack.endX; y: audioTrack.height }
                    PathLine { x: audioTrack.endX; y: 0 }
                }
                ShapePath {
                    strokeWidth: project.audioFadeIn > 0 ? 1.5 : 0
                    strokeColor: project.audioFadeIn > 0 ? Qt.alpha(theme.text, 0.75) : "transparent"
                    fillColor: "transparent"
                    startX: tl.xOf(0); startY: audioTrack.height
                    PathLine { x: audioTrack.fadeInX; y: 0 }
                }
                ShapePath {
                    strokeWidth: project.audioFadeOut > 0 ? 1.5 : 0
                    strokeColor: project.audioFadeOut > 0 ? Qt.alpha(theme.text, 0.75) : "transparent"
                    fillColor: "transparent"
                    startX: audioTrack.fadeOutX; startY: 0
                    PathLine { x: audioTrack.endX; y: audioTrack.height }
                }
            }
            // Audio after the end of the pictures is cut off in the export.
            Rectangle {
                visible: audioPreview.ready && audioTrack.end < audioPreview.duration - 0.01
                x: Math.max(0, audioTrack.endX)
                width: Math.max(0, parent.width - x)
                height: parent.height
                color: Qt.alpha(theme.panel, 0.75)
            }

            // Fade handles: drag them in from the corners.
            component FadeHandle: Rectangle {
                id: fadeHandle
                property bool fadeIn: true
                signal dragged(real seconds)

                width: 10
                height: 10
                y: 1
                radius: 2
                color: fadeArea.containsMouse || fadeArea.pressed ? theme.accent : theme.text
                visible: audioPreview.ready

                MouseArea {
                    id: fadeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    onPositionChanged: (mouse) => {
                        if (pressed)
                            fadeHandle.dragged(tl.secondsAt(mapToItem(audioTrack, mouse.x, 0).x));
                    }
                }
                ToolTip.visible: fadeArea.containsMouse || fadeArea.pressed
                ToolTip.text: (fadeIn ? "Fade in " + Format.seconds(project.audioFadeIn)
                                      : "Fade out " + Format.seconds(project.audioFadeOut)) + " s"
            }
            FadeHandle {
                objectName: "fadeInHandle"
                fadeIn: true
                x: audioTrack.fadeInX - (project.audioFadeIn > 0 ? width / 2 : 0)
                onDragged: (seconds) => project.audioFadeIn = Math.round(Math.max(0, Math.min(seconds, audioTrack.end)) * 10) / 10
            }
            FadeHandle {
                objectName: "fadeOutHandle"
                fadeIn: false
                x: audioTrack.fadeOutX - (project.audioFadeOut > 0 ? width / 2 : width)
                onDragged: (seconds) => project.audioFadeOut = Math.round(Math.max(0, Math.min(audioTrack.end - seconds, audioTrack.end)) * 10) / 10
            }
        }

        // Where the video ends, so a mismatch with the audio is visible.
        Rectangle {
            visible: project.videoDuration > 0
            x: tl.xOf(project.videoDuration)
            y: ruler.height
            width: 1
            height: parent.height - y
            color: Qt.alpha(theme.text, 0.35)
        }

        // --- markers ---
        Repeater {
            model: project.markers
            delegate: Item {
                id: marker
                required property real modelData
                required property int index
                x: tl.xOf(modelData)
                height: area.height
                z: 1800
                visible: x >= -6 && x <= area.width + 6

                Rectangle { x: -0.5; y: ruler.height; width: 1; height: parent.height - y; color: tl.markerColor; opacity: 0.8 }
                Rectangle {
                    id: flag
                    x: -5
                    y: 1
                    width: 10
                    height: 14
                    radius: 2
                    color: flagArea.containsMouse || flagArea.pressed ? Qt.lighter(tl.markerColor, 1.25) : tl.markerColor
                    MouseArea {
                        id: flagArea
                        anchors.fill: parent
                        anchors.margins: -3
                        hoverEnabled: true
                        preventStealing: true
                        cursorShape: Qt.SizeHorCursor
                        property bool moved: false
                        onPressed: moved = false
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return;
                            moved = true;
                            project.moveMarker(marker.index, tl.secondsAt(mapToItem(area, mouse.x, 0).x));
                        }
                        onClicked: if (!moved) tl.seekRequested(marker.modelData)
                        onDoubleClicked: project.removeMarker(marker.index)
                    }
                    ToolTip.visible: flagArea.containsMouse
                    ToolTip.delay: 500
                    ToolTip.text: "Marker at " + Format.time(marker.modelData) + " · drag to move, double-click to remove"
                }
            }
        }

        // The snap point an edge is being pulled to.
        Rectangle {
            visible: tl.snapLine >= 0
            x: tl.xOf(tl.snapLine) - 1
            y: ruler.height
            width: 2
            height: parent.height - y
            color: theme.accent
            z: 1900
        }

        // --- playhead ---
        Item {
            x: tl.xOf(tl.position)
            height: parent.height
            z: 2000
            Rectangle { x: -1; width: 2; height: parent.height; color: theme.playhead }
            Rectangle { x: -5; y: 0; width: 10; height: 10; radius: 2; rotation: 45; color: theme.playhead }
        }
    }

    // Soundslides' transition choices for the image whose overlap was clicked.
    Menu {
        id: transitionMenu
        property int index: -1

        Repeater {
            model: project.transitionPresets
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                onTriggered: project.setTransitionPreset(transitionMenu.index, modelData.transition, modelData.duration)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Use the project default"
            onTriggered: project.resetTransitionPreset(transitionMenu.index)
        }
    }

    ScrollBar {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        orientation: Qt.Horizontal
        policy: size < 1 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        size: Math.min(1, tl.visibleSeconds / tl.length)
        position: tl.viewStart / tl.length
        onPositionChanged: if (pressed) tl.viewStart = position * tl.length
    }

    function pan(steps) {
        viewStart -= steps * visibleSeconds * 0.1;
        clampView();
    }

    // The wheel zooms around the mouse, on the images and on the audio alike:
    // scrolling down (towards you) zooms in, up zooms out.
    // A sideways swipe (touchpad, tilt wheel) or Shift + wheel pans instead.
    WheelHandler {
        // Touchpads report as their own device type; without this their
        // scrolling is silently ignored.
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        objectName: "zoomWheel"
        acceptedModifiers: Qt.NoModifier
        onWheel: (event) => tl.zoom(Math.pow(1.25, -event.angleDelta.y / 120), tl.secondsAt(event.x - area.x))
    }
    // A WheelHandler only sees one direction; sideways gets its own.
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        orientation: Qt.Horizontal
        onWheel: (event) => tl.pan(event.angleDelta.x / 120)
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.ControlModifier
        onWheel: (event) => tl.zoom(Math.pow(1.25, -tl.wheelSteps(event)), tl.secondsAt(event.x - area.x))
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.ShiftModifier
        onWheel: (event) => tl.pan(tl.wheelSteps(event))
    }
}
