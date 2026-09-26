import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia
import "Format.js" as Format

ApplicationWindow {
    id: win
    width: 1240
    height: 820
    minimumWidth: 960
    minimumHeight: 720
    visible: true
    title: app.projectName + (project.modified ? " •" : "") + " — omasoundslides"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    color: "#0e0e10"

    readonly property bool hasImages: project.count > 0
    // Keyboard shortcuts step aside while a text field has focus or a dialog
    // is up. Number fields never need the space bar, so Space still plays and
    // pauses in them; only free text (the project name) keeps it.
    readonly property bool typing: activeFocusItem !== null && ("cursorPosition" in activeFocusItem)
    readonly property bool typingText: typing && activeFocusItem.numeric !== true
    readonly property bool overlayUp: helpVisible || confirm.visible || app.exporting
    readonly property bool keysFree: !typing && !overlayUp
    property bool helpVisible: false
    property string noticeText: ""
    property bool quitting: false
    property bool quitAfterSave: false

    // ------------------------------------------------------------ selection
    property int selected: -1
    // Bumped on every model change, so bindings that look up delegates or
    // frames re-evaluate.
    property int revision: 0
    readonly property var current: {
        revision;
        return selected >= 0 && selected < project.count ? timeline.blockAt(selected) : null;
    }

    // ------------------------------------------------------------ playback
    property real position: 0
    property bool playing: false
    readonly property real endTime: project.videoDuration > 0 ? project.videoDuration : audioPreview.duration
    readonly property var frame: {
        revision;
        return project.frameAt(position);
    }
    // The playhead clock: a position and the wall time it was taken at.
    property real clockStartPos: 0
    property double clockStartWall: 0

    function showNotice(text) {
        noticeText = text;
        noticeTimer.restart();
    }

    function syncPlayer() {
        if (playing && audioPreview.ready && position < audioPreview.duration - 0.05) {
            player.position = Math.round(position * 1000);
            player.play();
        } else {
            player.pause();
        }
    }
    function play() {
        if (endTime <= 0)
            return;
        if (position >= endTime - 0.02)
            position = 0;
        clockStartPos = position;
        clockStartWall = Date.now();
        playing = true;
        syncPlayer();
    }
    function pause() {
        playing = false;
        player.pause();
    }
    function togglePlay() {
        if (playing)
            pause();
        else
            play();
    }
    function seek(seconds) {
        position = Math.max(0, Math.min(seconds, Math.max(0, endTime)));
        if (playing) {
            clockStartPos = position;
            clockStartWall = Date.now();
            syncPlayer();
        }
        followPlayhead();
    }
    function followPlayhead() {
        var f = project.frameAt(position);
        if (f.current >= 0)
            selected = f.current;
        timeline.ensureVisible(position);
    }

    // ------------------------------------------------------------ editing
    function select(index) {
        if (project.count === 0) {
            selected = -1;
            return;
        }
        selected = Math.max(0, Math.min(index, project.count - 1));
        seek(project.visibleStartOf(selected));
    }
    function moveSelected(delta) {
        var to = selected + delta;
        if (selected < 0 || to < 0 || to >= project.count)
            return;
        project.moveImage(selected, to);
        select(to);
    }
    function removeSelected() {
        if (selected < 0)
            return;
        var at = selected;
        project.removeImage(at);
        select(Math.min(at, project.count - 1));
    }
    function nudgeDuration(delta) {
        if (current === null)
            return;
        project.setDuration(selected, Math.max(0.1, current.duration + delta));
    }
    function addImages() {
        app.addImagesDialog(selected >= 0 ? selected + 1 : -1);
    }
    function fitToAudio() {
        var overrides = project.durationOverrideCount();
        if (overrides === 0) {
            doFitToAudio();
            return;
        }
        confirm.ask("Fit images to the audio",
                    overrides + (overrides === 1 ? " image has" : " images have")
                    + " its own duration. It will be reset so every image gets the same duration.",
                    [{ label: "Cancel" }, { label: "Fit", primary: true, run: doFitToAudio }]);
    }
    function doFitToAudio() {
        if (project.fitToAudio())
            showNotice("Each image is now shown for " + Format.seconds(project.defaultDuration) + " s");
    }
    function newProject() {
        if (!project.modified) {
            app.newProject();
            return;
        }
        confirm.ask("New project",
                    "The project has unsaved changes. Start a new one anyway?",
                    [{ label: "Cancel" }, { label: "Discard changes", run: app.newProject }]);
    }
    function requestQuit() {
        if (!project.modified) {
            forceQuit();
            return;
        }
        confirm.ask("Unsaved changes",
                    "The project has unsaved changes. Quit anyway?",
                    [{ label: "Cancel" },
                     { label: "Quit", run: forceQuit },
                     { label: "Save", primary: true, run: function() { win.quitAfterSave = true; app.save(); } }]);
    }
    function forceQuit() {
        quitting = true;
        pause();
        app.cancelExport();
        win.close();
        Qt.quit();
    }

    onClosing: (close) => {
        if (quitting)
            return;
        close.accepted = false;
        requestQuit();
    }

    MediaPlayer {
        id: player
        source: audioPreview.source
        audioOutput: AudioOutput {
            // The project's fade in/out, as the export will apply it.
            volume: project.audioGainAt(win.position)
        }
        // Each position report re-anchors the clock to the audio, so picture
        // and sound stay together without the playhead jumping around.
        onPositionChanged: {
            if (!win.playing || player.playbackState !== MediaPlayer.PlayingState)
                return;
            var p = player.position / 1000;
            if (p >= win.position - 0.25) {
                win.clockStartPos = p;
                win.clockStartWall = Date.now();
            }
        }
    }

    // The playhead runs on the wall clock, anchored to the audio while there
    // is some, so it keeps going through silence after the audio ends.
    Timer {
        interval: 16
        repeat: true
        running: win.playing
        onTriggered: {
            var t = win.clockStartPos + (Date.now() - win.clockStartWall) / 1000;
            // Never step backwards while playing; a late position report is
            // corrected by slowing down, not by jumping.
            t = Math.max(t, win.position);
            if (t >= win.endTime) {
                win.position = win.endTime;
                win.pause();
            } else {
                win.position = t;
            }
            win.followPlayhead();
        }
    }

    Timer {
        id: noticeTimer
        interval: 6000
        onTriggered: win.noticeText = ""
    }

    Connections {
        target: app
        function onNotice(text) { win.showNotice(text); }
        function onSaved() {
            if (win.quitAfterSave)
                win.forceQuit();
        }
    }
    Connections {
        target: audioPreview
        function onChanged() { win.syncPlayer(); }
    }
    Connections {
        target: project
        function onModelReset() {
            win.revision++;
            win.pause();
            win.position = 0;
            timeline.fit();
            win.select(0);
        }
        function onRowsInserted(parent, first, last) {
            win.revision++;
            if (win.selected >= first)
                win.selected += last - first + 1;
            if (win.selected < 0)
                win.select(first);
        }
        function onRowsRemoved(parent, first, last) {
            win.revision++;
            if (win.selected > last)
                win.selected -= last - first + 1;
            else if (win.selected >= first)
                win.selected = Math.min(first, project.count - 1);
        }
        function onRowsMoved() { win.revision++; }
        function onDataChanged() { win.revision++; }
        // When an edit moves the selected image away from the playhead, bring
        // the playhead to it, so the preview keeps showing what you edit.
        function onTimingChanged() {
            if (win.playing || win.selected < 0)
                return;
            if (project.frameAt(win.position).current !== win.selected)
                win.position = project.visibleStartOf(win.selected);
            if (win.position > win.endTime)
                win.position = Math.max(0, win.endTime);
        }
    }

    // ------------------------------------------------------------ shortcuts
    // Every key comes from keybindings.conf (see the KeyBindings class).
    component Action: Shortcut {
        property string name
        sequences: keys.keys[name] !== undefined ? keys.keys[name] : []
        enabled: win.keysFree
    }

    Action { name: "play_pause"; enabled: !win.typingText && !win.overlayUp; onActivated: win.togglePlay() }
    Action { name: "seek_back"; onActivated: win.seek(win.position - 1) }
    Action { name: "seek_forward"; onActivated: win.seek(win.position + 1) }
    Action { name: "seek_back_long"; onActivated: win.seek(win.position - 5) }
    Action { name: "seek_forward_long"; onActivated: win.seek(win.position + 5) }
    Action { name: "seek_back_short"; onActivated: win.seek(win.position - 0.2) }
    Action { name: "seek_forward_short"; onActivated: win.seek(win.position + 0.2) }
    Action { name: "previous_image"; onActivated: win.select(win.selected - 1) }
    Action { name: "next_image"; onActivated: win.select(win.selected + 1) }
    Action { name: "go_to_start"; onActivated: win.seek(0) }
    Action { name: "go_to_end"; onActivated: win.seek(win.endTime) }
    Action { name: "duration_longer"; onActivated: win.nudgeDuration(keys.durationStep) }
    Action { name: "duration_shorter"; onActivated: win.nudgeDuration(-keys.durationStep) }
    Action { name: "move_image_left"; onActivated: win.moveSelected(-1) }
    Action { name: "move_image_right"; onActivated: win.moveSelected(1) }
    Action { name: "remove_image"; onActivated: win.removeSelected() }
    Action { name: "zoom_in"; onActivated: timeline.zoom(1.5, win.position) }
    Action { name: "zoom_out"; onActivated: timeline.zoom(1 / 1.5, win.position) }
    Action { name: "zoom_fit"; onActivated: timeline.fit() }
    Action { name: "add_images"; enabled: !win.overlayUp; onActivated: win.addImages() }
    Action { name: "add_audio"; enabled: !win.overlayUp; onActivated: app.addAudioDialog() }
    Action { name: "new_project"; enabled: !win.overlayUp; onActivated: win.newProject() }
    Action { name: "open_project"; enabled: !win.overlayUp; onActivated: app.openProjectDialog() }
    Action { name: "save"; enabled: !win.overlayUp; onActivated: app.save() }
    Action { name: "save_as"; enabled: !win.overlayUp; onActivated: app.saveAs() }
    Action { name: "export"; enabled: !win.overlayUp; onActivated: { win.pause(); app.exportDialog(); } }
    Action { name: "edit_keybindings"; enabled: !win.overlayUp; onActivated: Qt.openUrlExternally("file://" + keys.path) }
    Action { name: "help"; enabled: !win.typing && !confirm.visible; onActivated: win.helpVisible = !win.helpVisible }
    Action { name: "quit"; onActivated: win.requestQuit() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (confirm.visible)
                confirm.visible = false;
            else if (win.helpVisible)
                win.helpVisible = false;
            else if (win.typing)
                timeline.forceActiveFocus();
        }
    }

    // ------------------------------------------------------------ layout
    // A click on anything that isn't a control (the preview, empty space)
    // takes the keyboard back from a text field, so the shortcuts work again.
    Item {
        anchors.fill: parent
        TapHandler { onTapped: timeline.forceActiveFocus() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // --- toolbar ---
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: app.projectName + (project.modified ? " •" : "")
                color: "white"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                Layout.rightMargin: 12
                Layout.maximumWidth: 360
                elide: Text.ElideRight
            }
            Button { text: "+ Images"; flat: true; focusPolicy: Qt.NoFocus; onClicked: win.addImages() }
            Button { text: "+ Audio"; flat: true; focusPolicy: Qt.NoFocus; onClicked: app.addAudioDialog() }
            Item { Layout.fillWidth: true }
            Button { text: "New"; flat: true; focusPolicy: Qt.NoFocus; onClicked: win.newProject() }
            Button { text: "Open"; flat: true; focusPolicy: Qt.NoFocus; onClicked: app.openProjectDialog() }
            Button { text: "Save"; flat: true; focusPolicy: Qt.NoFocus; onClicked: app.save() }
            AccentButton {
                text: "Export"
                enabled: win.hasImages && !app.exporting
                onClicked: { win.pause(); app.exportDialog(); }
            }
        }

        // --- preview + transport | inspector ---
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // The output frame, so letterboxing looks like the export.
                    Rectangle {
                        id: frameRect
                        objectName: "previewFrame"
                        readonly property real aspect: project.outputWidth / project.outputHeight
                        readonly property var f: win.frame
                        // Fade out/in: the first image fades to black, then the next fades in.
                        readonly property bool dip: f.to >= 0 && f.transition === "fadeblack"

                        width: Math.min(parent.width, parent.height * aspect)
                        height: width / aspect
                        anchors.centerIn: parent
                        color: "black"
                        radius: win.hasImages ? 0 : 12

                        component PreviewImage: Image {
                            anchors.fill: parent
                            sourceSize.width: Math.max(1, frameRect.width)
                            sourceSize.height: Math.max(1, frameRect.height)
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            autoTransform: true
                            retainWhileLoading: true
                        }

                        // The two images of the frame, mixed like the export mixes them.
                        PreviewImage {
                            source: project.urlOf(frameRect.f.from)
                            opacity: frameRect.dip ? Math.max(0, 1 - 2 * frameRect.f.mix) : 1
                        }
                        PreviewImage {
                            source: frameRect.f.to >= 0 ? project.urlOf(frameRect.f.to) : ""
                            opacity: frameRect.f.to < 0 ? 0
                                     : (frameRect.dip ? Math.max(0, 2 * frameRect.f.mix - 1) : frameRect.f.mix)
                        }
                        // Warm the cache with the next image so playback doesn't blink.
                        PreviewImage {
                            visible: false
                            source: project.urlOf(Math.max(frameRect.f.from, frameRect.f.to) + 1)
                        }

                        Column {
                            anchors.centerIn: parent
                            visible: !win.hasImages
                            spacing: 14
                            AccentButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Add images"
                                font.pixelSize: 18
                                onClicked: win.addImages()
                            }
                            Label {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "or drop images and audio into the window"
                                color: "#8a8a90"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                // --- transport ---
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RoundButton {
                        implicitWidth: 44
                        implicitHeight: 44
                        text: win.playing ? "❚❚" : "▶"
                        font.pixelSize: 15
                        enabled: win.endTime > 0
                        focusPolicy: Qt.NoFocus
                        Material.background: "#2c2c2f"
                        ToolTip.visible: hovered
                        ToolTip.text: win.playing ? "Pause" : "Play"
                        onClicked: win.togglePlay()
                    }
                    Label {
                        text: Format.time(win.position) + " / " + Format.time(win.endTime)
                        color: "#d6d6da"
                        font.pixelSize: 13
                        font.family: "monospace"
                    }
                    Item { Layout.fillWidth: true }

                    // Audio files, played back to back.
                    Repeater {
                        model: project.audio
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            implicitWidth: audioRow.implicitWidth + 20
                            implicitHeight: 28
                            radius: 14
                            color: "#1f1f22"
                            Row {
                                id: audioRow
                                anchors.centerIn: parent
                                spacing: 8
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "♪ " + modelData.fileName
                                    color: "#d6d6da"
                                    font.pixelSize: 12
                                }
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: Format.time(modelData.duration)
                                    color: "#8a8a90"
                                    font.pixelSize: 12
                                    font.family: "monospace"
                                }
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: "✕"
                                    color: removeHover.hovered ? "white" : "#6a6a70"
                                    font.pixelSize: 12
                                    HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: project.removeAudio(index) }
                                }
                            }
                        }
                    }

                    ToolButton {
                        text: "−"
                        font.pixelSize: 16
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: "Zoom out (Ctrl + scroll)"
                        onClicked: timeline.zoom(1 / 1.5, win.position)
                    }
                    ToolButton {
                        text: "Fit"
                        font.pixelSize: 12
                        focusPolicy: Qt.NoFocus
                        enabled: !timeline.fitted
                        ToolTip.visible: hovered
                        ToolTip.text: "Show the whole timeline"
                        onClicked: timeline.fit()
                    }
                    ToolButton {
                        text: "+"
                        font.pixelSize: 16
                        focusPolicy: Qt.NoFocus
                        ToolTip.visible: hovered
                        ToolTip.text: "Zoom in (Ctrl + scroll)"
                        onClicked: timeline.zoom(1.5, win.position)
                    }
                }
            }

            Inspector {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                item: win.current
            }
        }

        // --- timeline ---
        Timeline {
            id: timeline
            objectName: "timeline"
            Layout.fillWidth: true
            Layout.preferredHeight: 196
            position: win.position
            selected: win.selected
            focus: true
            onSeekRequested: (seconds) => win.seek(seconds)
            onImageClicked: (index) => {
                if (index !== win.selected)
                    win.select(index);
            }
        }

        // --- status line ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 22

            readonly property real diff: project.videoDuration - project.audioDuration
            readonly property bool mismatch: project.audioDuration > 0 && Math.abs(diff) > 0.05

            Label {
                Layout.fillWidth: true
                verticalAlignment: Text.AlignVCenter
                textFormat: Text.StyledText
                elide: Text.ElideRight
                font.pixelSize: 13
                font.family: "monospace"
                color: win.noticeText !== "" ? theme.accent : "#b8b8bc"
                text: {
                    if (win.noticeText !== "")
                        return win.noticeText;
                    if (project.problems.length > 0)
                        return "<font color=\"#ff8a80\">" + project.problems[0] + "</font>";
                    if (!win.hasImages)
                        return "";
                    var s = project.count + (project.count === 1 ? " image" : " images")
                          + " · video " + Format.time(project.videoDuration)
                          + " · audio " + Format.time(project.audioDuration);
                    if (parent.mismatch)
                        s += " · <font color=\"" + Qt.lighter(theme.accent, 1.6) + "\">"
                           + (parent.diff > 0 ? "the images run " + Format.seconds(parent.diff) + " s longer"
                                              : "the audio runs " + Format.seconds(-parent.diff) + " s longer")
                           + "</font>";
                    return s;
                }
            }
            Label {
                text: "? Shortcuts"
                color: helpHover.hovered ? "white" : "#8a8a90"
                font.pixelSize: 13
                HoverHandler { id: helpHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: win.helpVisible = !win.helpVisible }
            }
        }
    }

    // ------------------------------------------------------------ drag and drop
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (!drop.hasUrls)
                return;
            app.addDroppedUrls(drop.urls, win.selected >= 0 ? win.selected + 1 : -1);
            drop.acceptProposedAction();
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            visible: parent.containsDrag
            color: "transparent"
            radius: 12
            border.width: 3
            border.color: theme.accent
        }
    }

    // ------------------------------------------------------------ overlays
    component Card: Rectangle {
        radius: 12
        color: "#1c1c1e"
    }

    component DialogButton: Rectangle {
        id: dialogButton
        property string text: ""
        property bool primary: false
        signal clicked()

        width: dialogButtonLabel.implicitWidth + 28
        height: 34
        radius: 8
        color: primary ? theme.accent : "#2c2c2f"
        border.color: activeFocus ? (primary ? theme.accentForeground : theme.accent) : "transparent"
        border.width: activeFocus ? 2 : 0
        activeFocusOnTab: true

        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        Keys.onSpacePressed: clicked()

        Label {
            id: dialogButtonLabel
            anchors.centerIn: parent
            text: dialogButton.text
            color: dialogButton.primary ? theme.accentForeground : "white"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: dialogButton.clicked()
        }
    }

    // Export progress.
    Rectangle {
        visible: app.exporting
        anchors.fill: parent
        color: "#000000cc"
        MouseArea { anchors.fill: parent }

        Card {
            anchors.centerIn: parent
            width: 380
            height: exportColumn.implicitHeight + 48

            Column {
                id: exportColumn
                anchors.centerIn: parent
                width: parent.width - 56
                spacing: 12

                Label {
                    text: app.exportPhase
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                ProgressBar {
                    width: parent.width
                    from: 0
                    to: 1
                    value: app.exportProgress
                    indeterminate: app.exportPhase === "Checking files…"
                }
                Row {
                    width: parent.width
                    Label {
                        width: parent.width - cancelButton.width
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.round(app.exportProgress * 100) + " %"
                        color: "#d6d6da"
                        font.pixelSize: 13
                        font.family: "monospace"
                    }
                    DialogButton {
                        id: cancelButton
                        text: "Cancel"
                        onClicked: app.cancelExport()
                    }
                }
            }
        }
    }

    // A small confirm dialog: ask(title, text, [{label, primary, run}]).
    Rectangle {
        id: confirm
        visible: false
        anchors.fill: parent
        color: "#000000cc"

        property string heading: ""
        property string body: ""
        property var actions: []

        function ask(heading, body, actions) {
            confirm.heading = heading;
            confirm.body = body;
            confirm.actions = actions;
            visible = true;
        }

        onVisibleChanged: {
            if (visible)
                Qt.callLater(function() {
                    var last = confirmButtons.itemAt(confirm.actions.length - 1);
                    if (last)
                        last.forceActiveFocus();
                });
        }

        MouseArea { anchors.fill: parent; onClicked: confirm.visible = false }

        Card {
            anchors.centerIn: parent
            width: 440
            height: confirmColumn.implicitHeight + 48
            MouseArea { anchors.fill: parent }

            Column {
                id: confirmColumn
                anchors.centerIn: parent
                width: parent.width - 64
                spacing: 8

                Label {
                    text: confirm.heading
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Label {
                    width: parent.width
                    text: confirm.body
                    color: "#d6d6da"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    bottomPadding: 12
                }
                Row {
                    anchors.right: parent.right
                    spacing: 10
                    Repeater {
                        id: confirmButtons
                        model: confirm.actions
                        delegate: DialogButton {
                            required property var modelData
                            text: modelData.label
                            primary: modelData.primary === true
                            onClicked: {
                                confirm.visible = false;
                                if (modelData.run)
                                    modelData.run();
                            }
                        }
                    }
                }
            }
        }
    }

    // Keyboard shortcuts, straight from keybindings.conf.
    Rectangle {
        visible: win.helpVisible
        anchors.fill: parent
        color: "#000000cc"
        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

        Card {
            anchors.centerIn: parent
            width: helpColumn.width + 56
            height: helpColumn.height + 48
            MouseArea { anchors.fill: parent }

            Column {
                id: helpColumn
                anchors.centerIn: parent
                spacing: 12

                Label {
                    text: "Keyboard shortcuts"
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Grid {
                    columns: 2
                    columnSpacing: 36
                    rowSpacing: 7
                    flow: Grid.TopToBottom
                    rows: Math.ceil(keys.actions.length / 2)

                    Repeater {
                        model: keys.actions
                        delegate: Row {
                            required property var modelData
                            spacing: 14
                            Label {
                                width: 150
                                horizontalAlignment: Text.AlignRight
                                text: modelData.keys === "" ? "—" : modelData.keys
                                color: Qt.lighter(theme.accent, 1.5)
                                font.pixelSize: 12
                                font.family: "monospace"
                                elide: Text.ElideLeft
                            }
                            Label {
                                text: modelData.description
                                color: "#d6d6da"
                                font.pixelSize: 12
                            }
                        }
                    }
                }

                Column {
                    spacing: 5
                    topPadding: 6
                    Repeater {
                        model: [
                            "Scroll over an image or a number: ±" + Format.seconds(keys.scrollStep) + " s (Shift: ×5)",
                            "Drag the right edge of an image: set its duration",
                            "Scroll on the timeline: pan  ·  Ctrl + scroll: zoom",
                            "Click or drag on the ruler or the audio: move the playhead"
                        ]
                        delegate: Label {
                            required property string modelData
                            text: modelData
                            color: "#b8b8bc"
                            font.pixelSize: 12
                        }
                    }
                }

                Row {
                    spacing: 12
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: keys.path
                        color: "#8a8a90"
                        font.pixelSize: 12
                        font.family: "monospace"
                    }
                    DialogButton {
                        text: "Edit shortcuts"
                        onClicked: {
                            win.helpVisible = false;
                            Qt.openUrlExternally("file://" + keys.path);
                        }
                    }
                }
            }
        }
    }
}
