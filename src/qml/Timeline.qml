import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Omasoundslides
import "Format.js" as Format

// Two tracks on one time axis: the images (each as wide as it lasts, the
// next one overlapping it by its transition) and the audio waveform. Click
// or drag on the ruler or the audio to move the playhead; click an image to
// select it; scroll over an image to change its duration; drag its right
// edge to set the duration by hand; Ctrl + scroll zooms.
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

    signal seekRequested(real seconds)
    signal imageClicked(int index)

    color: "#161618"
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
    // Leftover wheel movement between events (see Format.wheelNotches).
    property real wheelRemainder: 0
    function wheelSteps(event) {
        var d = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
        return d / 120;
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
                    x: (t - tl.viewStart) * tl.pps
                    height: ruler.height
                    Rectangle { width: 1; height: 6; anchors.bottom: parent.bottom; color: "#4a4a50" }
                    Label {
                        x: 4
                        text: Format.clock(parent.t)
                        color: "#8a8a90"
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

                    x: (start - tl.viewStart) * tl.pps
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
                        // The overlap with the previous image: fades in from its side.
                        Rectangle {
                            visible: block.hasTransition
                            width: block.transitionDuration * tl.pps
                            height: parent.height
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: "#e0000000" }
                                GradientStop { position: 1; color: "#00000000" }
                            }
                        }
                        Rectangle {
                            x: 4
                            y: 4
                            visible: block.width > 26
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
                        border.color: block.selected ? theme.accent : (hover.hovered ? "#8a8a90" : "#000000")
                    }

                    HoverHandler { id: hover }
                    TapHandler {
                        onTapped: { tl.forceActiveFocus(); tl.imageClicked(block.index); }
                    }
                    WheelHandler {
                        acceptedModifiers: Qt.NoModifier
                        onWheel: (event) => block.nudge(Format.wheelNotches(tl, event) * keys.scrollStep)
                    }
                    WheelHandler {
                        acceptedModifiers: Qt.ShiftModifier
                        onWheel: (event) => block.nudge(Format.wheelNotches(tl, event) * keys.scrollStep * 5)
                    }
                    function nudge(delta) {
                        if (delta !== 0)
                            project.setDuration(index, Math.max(0.1, duration + delta));
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
                    x: (start + duration - tl.viewStart) * tl.pps - width / 2
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
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return;
                        var dx = mapToItem(tl, mouse.x, 0).x - pressX;
                        project.setDuration(index, Math.max(0.1, pressDuration + dx / tl.pps));
                    }
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
            color: "#1c1c1f"

            Waveform {
                anchors.fill: parent
                preview: audioPreview
                viewStart: tl.viewStart
                pixelsPerSecond: tl.pps
                color: Qt.lighter(theme.accent, 1.5)
            }
            Label {
                anchors.centerIn: parent
                visible: !audioPreview.ready
                text: audioPreview.building ? "Reading the audio…"
                      : (project.audio.length === 0 ? "No audio. Add some with + Audio or drop a file here." : "")
                color: "#6a6a70"
                font.pixelSize: 12
            }
            // The fades, drawn as the waveform dimming towards silence.
            readonly property real audioEnd: project.videoDuration > 0
                ? Math.min(audioPreview.duration, project.videoDuration) : audioPreview.duration
            Rectangle {
                visible: audioPreview.ready && project.audioFadeIn > 0
                x: (0 - tl.viewStart) * tl.pps
                width: Math.min(project.audioFadeIn, audioTrack.audioEnd) * tl.pps
                height: parent.height
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#f01c1c1f" }
                    GradientStop { position: 1; color: "#001c1c1f" }
                }
            }
            Rectangle {
                visible: audioPreview.ready && project.audioFadeOut > 0
                width: Math.min(project.audioFadeOut, audioTrack.audioEnd) * tl.pps
                x: (audioTrack.audioEnd - tl.viewStart) * tl.pps - width
                height: parent.height
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#001c1c1f" }
                    GradientStop { position: 1; color: "#f01c1c1f" }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                onPressed: (mouse) => { tl.forceActiveFocus(); tl.seekRequested(tl.secondsAt(mouse.x)); }
                onPositionChanged: (mouse) => { if (pressed) tl.seekRequested(tl.secondsAt(mouse.x)); }
            }
        }

        // Where the video ends, so a mismatch with the audio is visible.
        Rectangle {
            visible: project.videoDuration > 0
            x: (project.videoDuration - tl.viewStart) * tl.pps
            y: ruler.height
            width: 1
            height: parent.height - y
            color: "#ffffff50"
        }

        // --- playhead ---
        Item {
            x: (tl.position - tl.viewStart) * tl.pps
            height: parent.height
            z: 2000
            Rectangle { x: -1; width: 2; height: parent.height; color: "white" }
            Rectangle { x: -5; y: 0; width: 10; height: 10; radius: 2; rotation: 45; color: "white" }
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

    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: (event) => tl.zoom(Math.pow(1.25, tl.wheelSteps(event)), tl.secondsAt(event.x - area.x))
    }
    WheelHandler {
        acceptedModifiers: Qt.NoModifier
        onWheel: (event) => {
            tl.viewStart -= tl.wheelSteps(event) * tl.visibleSeconds * 0.1;
            tl.clampView();
        }
    }
}
