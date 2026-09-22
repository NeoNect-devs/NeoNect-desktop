import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtMultimedia
import NeoNect.Core 1.0

Rectangle {
    id: videoRoot

    property string messageId: ""
    property string videoUrl: ""
    property string fileName: "Video.mp4"
    property int fileSize: 0
    property int duration: 45 // seconds
    property bool fromMe: false

    property real volumeLevel: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.volume : 1.0
    property bool isMuted: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.isMuted : false
    property bool hasPlaybackError: false
    property string playbackErrorMsg: ""
    property bool controlsLoaded: false

    readonly property bool isPlaying: player.playbackState === MediaPlayer.PlayingState

    signal openFullscreenRequested(string url, string name, int startPosMs, bool isPlaying)

    function formatTime(secs) {
        if (!secs || isNaN(secs) || secs < 0) return "0:00";
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
    }

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return "";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }

    // Dynamic Resolution and Aspect Ratio Scaling
    readonly property real metaW: (player.metaData && player.metaData.value(MediaMetaData.Resolution)) ? player.metaData.value(MediaMetaData.Resolution).width : 0
    readonly property real metaH: (player.metaData && player.metaData.value(MediaMetaData.Resolution)) ? player.metaData.value(MediaMetaData.Resolution).height : 0

    readonly property real naturalWidth: metaW > 0 ? metaW : 340
    readonly property real naturalHeight: metaH > 0 ? metaH : 204
    readonly property real videoRatio: (naturalWidth > 0 && naturalHeight > 0) ? (naturalWidth / naturalHeight) : (16 / 9)

    readonly property real calcWidth: {
        var maxW = 380;
        var maxH = 300;
        var minW = 200;
        var w = naturalWidth;
        var h = naturalHeight;
        if (w > maxW) {
            h = maxW / videoRatio;
            w = maxW;
        }
        if (h > maxH) {
            w = maxH * videoRatio;
            h = maxH;
        }
        return Math.max(minW, Math.min(maxW, w));
    }
    readonly property real calcHeight: Math.max(140, Math.min(300, calcWidth / videoRatio))

    implicitWidth: calcWidth
    implicitHeight: calcHeight
    Layout.preferredWidth: calcWidth
    Layout.preferredHeight: calcHeight
    width: calcWidth
    height: calcHeight

    radius: 12
    clip: true
    color: "#111214"
    border.color: videoHoverHandler.hovered ? Qt.rgba(10, 132, 255, 0.4) : Qt.rgba(255, 255, 255, 0.1)
    border.width: 1

    Behavior on border.color { ColorAnimation { duration: 150 } }

    HoverHandler {
        id: videoHoverHandler
        onHoveredChanged: {
            if (hovered) {
                unloadTimer.stop();
            } else {
                if (!videoRoot.isPlaying && videoRoot.controlsLoaded) {
                    unloadTimer.restart();
                }
            }
        }
    }

    Timer {
        id: unloadTimer
        interval: 3500
        repeat: false
        onTriggered: {
            if (!videoRoot.isPlaying && !videoHoverHandler.hovered) {
                videoRoot.controlsLoaded = false;
            }
        }
    }

    MediaPlayer {
        id: player
        source: videoRoot.videoUrl
        audioOutput: AudioOutput {
            id: audioOut
            volume: (videoRoot.isMuted || !videoRoot.isPlaying) ? 0.0 : videoRoot.volumeLevel
            muted: videoRoot.isMuted || !videoRoot.isPlaying
        }
        videoOutput: videoOutputItem

        onPlaybackStateChanged: {
            if (playbackState === MediaPlayer.PlayingState) {
                videoRoot.controlsLoaded = true;
                unloadTimer.stop();
            } else if (playbackState === MediaPlayer.PausedState) {
                if (!videoHoverHandler.hovered) {
                    unloadTimer.restart();
                }
            }
        }

        onErrorOccurred: (error, errorString) => {
            console.log("[VideoPlayerItem] Playback error:", errorString);
            videoRoot.hasPlaybackError = true;
            videoRoot.playbackErrorMsg = errorString || "Codec not supported by GPU";
        }
    }

    Component.onCompleted: {
        if (videoRoot.videoUrl && videoRoot.videoUrl !== "") {
            player.pause();
        }
    }

    Component.onDestruction: {
        unloadTimer.stop();
        videoRoot.controlsLoaded = false;
        if (player.playbackState !== MediaPlayer.StoppedState) {
            player.stop();
        }
        player.source = "";
    }

    onVisibleChanged: {
        if (!visible) {
            if (player.playbackState === MediaPlayer.PlayingState) {
                player.pause();
            }
            unloadTimer.stop();
            videoRoot.controlsLoaded = false;
        }
    }

    onVideoUrlChanged: {
        if (videoRoot.videoUrl && videoRoot.videoUrl !== "") {
            player.pause();
        }
        videoRoot.controlsLoaded = false;
    }

    // 1. Video Canvas / Poster Background Layer
    Rectangle {
        id: posterBackground
        anchors.fill: parent
        color: "#16171A"

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#22242B" }
                GradientStop { position: 0.6; color: "#16181E" }
                GradientStop { position: 1.0; color: "#0B0C0E" }
            }
        }

        // Film icon watermark when video frame is loading
        IconImage {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -8
            source: "qrc:/qt/qml/NeoNect/assets/icons/video.svg"
            width: 64; height: 64
            color: Qt.rgba(255, 255, 255, 0.08)
        }
    }

    // 2. In-App Video Rendering Surface
    VideoOutput {
        id: videoOutputItem
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: !videoRoot.hasPlaybackError
    }

    // 3. Surface Click Area (Toggles inline Play/Pause)
    MouseArea {
        id: surfaceClickArea
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (player.playbackState === MediaPlayer.PlayingState) {
                player.pause();
            } else {
                videoRoot.controlsLoaded = true;
                player.play();
            }
        }
    }

    // 4. Lightweight Idle Duration Badge (Only visible when controls are unloaded)
    Rectangle {
        visible: !videoRoot.controlsLoaded && !videoRoot.isPlaying && videoRoot.duration > 0
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 8
        height: 20
        radius: 10
        color: Qt.rgba(0, 0, 0, 0.65)
        implicitWidth: idleDurText.implicitWidth + 12

        Text {
            id: idleDurText
            anchors.centerIn: parent
            text: videoRoot.formatTime(videoRoot.duration)
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
        }
    }

    // 5. Central Glowing Play Button (Visible only when paused)
    Rectangle {
        id: playBtn
        z: 5
        width: 48; height: 48
        radius: 24
        color: playBtnMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(0, 0, 0, 0.72)
        border.color: "#FFFFFF"
        border.width: 1.5
        anchors.centerIn: parent
        visible: opacity > 0
        opacity: (!videoRoot.isPlaying && !videoRoot.hasPlaybackError) ? 1.0 : 0.0
        scale: playBtnMouse.containsMouse ? 1.08 : 1.0

        Behavior on opacity { NumberAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

        IconImage {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 2
            source: "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
            width: 20; height: 20
            color: "#FFFFFF"
        }

        MouseArea {
            id: playBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                videoRoot.controlsLoaded = true;
                player.play();
            }
        }
    }

    // 6. Dynamic Overlay Loader for Top Info Bar & Bottom Transport Bar
    Loader {
        id: controlsOverlayLoader
        anchors.fill: parent
        active: videoRoot.controlsLoaded
        z: 10
        sourceComponent: controlsOverlayComponent
    }

    Component {
        id: controlsOverlayComponent

        Item {
            anchors.fill: parent

            readonly property bool isControlsVisible: !videoRoot.hasPlaybackError && (!videoRoot.isPlaying || videoHoverHandler.hovered || (typeof videoVolCtrl !== "undefined" && videoVolCtrl && videoVolCtrl.expanded))

            // Top Info Bar (Filename & Size Badge)
            Rectangle {
                id: topInfoBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 28
                color: Qt.rgba(0, 0, 0, 0.55)
                visible: opacity > 0
                opacity: isControlsVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: videoRoot.fileName
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: videoRoot.fileSize > 0
                        text: videoRoot.formatBytes(videoRoot.fileSize)
                        color: "#949BA4"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                    }
                }
            }

            // Bottom Transport Bar (Inline Controls)
            Rectangle {
                id: bottomBar
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 38
                color: Qt.rgba(0, 0, 0, 0.75)
                visible: opacity > 0
                opacity: isControlsVisible ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: 150 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    spacing: 6

                    // Play / Pause Toggle
                    Rectangle {
                        width: 24; height: 24
                        radius: 12
                        color: smPlayMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : "transparent"

                        IconImage {
                            anchors.centerIn: parent
                            source: videoRoot.isPlaying ? "qrc:/qt/qml/NeoNect/assets/icons/pause.svg" : "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
                            width: 14; height: 14
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: smPlayMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (player.playbackState === MediaPlayer.PlayingState) {
                                    player.pause();
                                } else {
                                    player.play();
                                }
                            }
                        }
                    }

                    Text {
                        text: videoRoot.formatTime(player.position / 1000)
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Interactive Gradient Progress Scrubber
                    GradientSeekBar {
                        id: videoScrubber
                        Layout.fillWidth: true
                        value: player.duration > 0 ? (player.position / player.duration) : 0.0
                        duration: player.duration > 0 ? player.duration : (videoRoot.duration * 1000)
                        onSeekMoved: (p) => {
                            if (player.duration > 0) {
                                player.position = p * player.duration;
                            }
                        }
                        onSeekFinished: (p) => {
                            if (player.duration > 0) {
                                player.position = p * player.duration;
                            }
                        }
                    }

                    Text {
                        text: videoRoot.formatTime(player.duration > 0 ? (player.duration / 1000) : videoRoot.duration)
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Volume Controller
                    VolumeController {
                        id: videoVolCtrl
                        volume: videoRoot.volumeLevel
                        isMuted: videoRoot.isMuted
                        textColor: "#FFFFFF"
                        accentColor: ThemeData.accentColor
                        onVolumeChangedManually: (v) => {
                            if (typeof AudioManager !== "undefined" && AudioManager) {
                                AudioManager.setVolume(v);
                            }
                        }
                        onMuteToggled: {
                            if (typeof AudioManager !== "undefined" && AudioManager) {
                                AudioManager.toggleMute();
                            }
                        }
                    }

                    // Fullscreen Expansion Button
                    Rectangle {
                        width: 26; height: 26
                        radius: 13
                        color: fsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : "transparent"

                        IconImage {
                            anchors.centerIn: parent
                            source: "qrc:/qt/qml/NeoNect/assets/icons/maximize.svg"
                            width: 14; height: 14
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: fsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var curPos = player.position;
                                var wasPlaying = videoRoot.isPlaying;
                                player.pause();
                                videoRoot.openFullscreenRequested(videoRoot.videoUrl, videoRoot.fileName, curPos, wasPlaying);
                            }
                        }
                    }
                }
            }
        }
    }

    // 7. Codec / Playback Error Recovery Card
    Rectangle {
        anchors.fill: parent
        color: "#16171A"
        visible: videoRoot.hasPlaybackError
        z: 15

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 10

            IconImage {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg"
                width: 32; height: 32
                color: "#F1C40F"
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "GPU Codec Not Supported"
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.bold: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Play externally in default media player"
                color: ThemeData.textSecondary
                font.family: "Segoe UI"
                font.pixelSize: 10
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Rectangle {
                    width: 80; height: 32
                    radius: 6
                    color: retryBtnMouse.containsMouse ? "#E53935" : Qt.rgba(229, 57, 53, 0.25)
                    border.color: "#E53935"
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4

                        IconImage {
                            source: "qrc:/qt/qml/NeoNect/assets/icons/refresh.svg"
                            width: 12; height: 12
                            color: "#FFFFFF"
                        }

                        Text {
                            text: "Retry"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: retryBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            videoRoot.hasPlaybackError = false;
                            player.source = "";
                            player.source = videoRoot.videoUrl;
                            videoRoot.controlsLoaded = true;
                            player.play();
                            if (videoRoot.messageId && typeof MessageService !== "undefined" && MessageService) {
                                MessageService.retryMessage(videoRoot.messageId);
                            }
                        }
                    }
                }

                Rectangle {
                    width: 140; height: 32
                    radius: 6
                    color: extBtnMouse.containsMouse ? "#0066CC" : ThemeData.accentColor

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6

                        IconImage {
                            source: "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
                            width: 14; height: 14
                            color: "#FFFFFF"
                        }

                        Text {
                            text: "Open Player ↗"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: extBtnMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: AudioManager.openMediaFile(videoRoot.videoUrl)
                    }
                }
            }
        }
    }
}
