import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtMultimedia
import NeoNect.Core 1.0
import "UIHelpers.js" as UIHelpers

Rectangle {
    id: lightboxRoot

    property bool active: false
    property string mediaUrl: ""
    property string mediaType: "image" // "image" | "video"
    property string fileName: "Media"
    property real zoomScale: 1.0

    property real volumeLevel: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.volume : 1.0
    property bool isMuted: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.isMuted : false
    property bool hasPlaybackError: false
    property string playbackErrorMsg: ""
    property int pendingSeekPos: 0
    property bool resumePlayback: true

    readonly property bool isPlaying: videoContentLoader.item ? videoContentLoader.item.isPlaying : false
    readonly property bool isWindowFullScreen: (lightboxRoot.Window.window && lightboxRoot.Window.window.visibility === Window.FullScreen)

    signal closeRequested()
    signal downloadRequested(string url, string name)

    function formatTime(secs) {
        if (!secs || isNaN(secs) || secs < 0) return "0:00";
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
    }

    function toggleWindowFullScreen() {
        var win = lightboxRoot.Window.window;
        if (win) {
            if (win.visibility === Window.FullScreen) {
                win.visibility = Window.Windowed;
            } else {
                win.visibility = Window.FullScreen;
            }
        }
    }

    function open(url, type, name, startPosMs, isPlaying) {
        lightboxRoot.mediaUrl = UIHelpers.formatMediaSource(url);
        lightboxRoot.mediaType = type || "image";
        lightboxRoot.fileName = name || "Media";
        lightboxRoot.zoomScale = 1.0;
        lightboxRoot.hasPlaybackError = false;

        var seekPos = (startPosMs !== undefined && startPosMs > 0) ? startPosMs : 0;
        var shouldPlay = (isPlaying !== undefined) ? isPlaying : true;
        lightboxRoot.pendingSeekPos = seekPos;
        lightboxRoot.resumePlayback = shouldPlay;
        lightboxRoot.active = true;

        if (lightboxRoot.mediaType === "video" && videoContentLoader.item) {
            videoContentLoader.item.startPlayback(seekPos, shouldPlay);
        }
    }

    function close() {
        lightboxRoot.resumePlayback = false;
        lightboxRoot.pendingSeekPos = 0;
        lightboxRoot.active = false;
        var win = lightboxRoot.Window.window;
        if (win && win.visibility === Window.FullScreen) {
            win.visibility = Window.Windowed;
        }
        lightboxRoot.closeRequested();
    }

    onActiveChanged: {
        if (!active) {
            lightboxRoot.resumePlayback = false;
            lightboxRoot.pendingSeekPos = 0;
            lightboxRoot.mediaUrl = "";
        }
    }

    anchors.fill: parent
    visible: active || opacity > 0
    opacity: active ? 1.0 : 0.0
    z: 100000
    color: "#000000"

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

    // Full backdrop mouse absorber to prevent click-through to window titlebar or background
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onPressed: (mouse) => mouse.accepted = true
        onReleased: (mouse) => mouse.accepted = true
        onWheel: (wheel) => wheel.accepted = true
    }

    Shortcut {
        sequence: "F11"
        enabled: lightboxRoot.active
        onActivated: lightboxRoot.toggleWindowFullScreen()
    }

    Shortcut {
        sequence: "Escape"
        enabled: lightboxRoot.active
        onActivated: lightboxRoot.close()
    }

    // Top Header Controls Bar
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: Qt.rgba(0, 0, 0, 0.7)
        z: 10

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: (mouse) => mouse.accepted = true
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: lightboxRoot.fileName
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            // Image Zoom Controls (Lazy-Loaded only for images)
            Loader {
                active: lightboxRoot.active && lightboxRoot.mediaType === "image"
                visible: active
                sourceComponent: Component {
                    RowLayout {
                        spacing: 14

                        Rectangle {
                            width: 36; height: 36
                            radius: 18
                            color: zoomOutMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                            Text {
                                anchors.centerIn: parent
                                text: "−"
                                color: "#FFFFFF"
                                font.pixelSize: 20
                                font.bold: true
                            }

                            MouseArea {
                                id: zoomOutMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: lightboxRoot.zoomScale = Math.max(0.5, lightboxRoot.zoomScale - 0.25)
                            }
                        }

                        Text {
                            text: Math.round(lightboxRoot.zoomScale * 100) + "%"
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            width: 36; height: 36
                            radius: 18
                            color: zoomInMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: "#FFFFFF"
                                font.pixelSize: 20
                                font.bold: true
                            }

                            MouseArea {
                                id: zoomInMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: lightboxRoot.zoomScale = Math.min(3.0, lightboxRoot.zoomScale + 0.25)
                            }
                        }
                    }
                }
            }

            // Download Button
            Rectangle {
                width: 36; height: 36
                radius: 18
                color: downloadMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/NeoNect/assets/icons/download.svg"
                    width: 18; height: 18
                    color: "#FFFFFF"
                }

                MouseArea {
                    id: downloadMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: lightboxRoot.downloadRequested(lightboxRoot.mediaUrl, lightboxRoot.fileName)
                }
            }

            // Close Button
            Rectangle {
                width: 36; height: 36
                radius: 18
                color: closeBtnMouse.containsMouse ? Qt.rgba(255, 50, 50, 0.6) : Qt.rgba(255, 255, 255, 0.1)

                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "#FFFFFF"
                    font.pixelSize: 16
                }

                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: lightboxRoot.close()
                }
            }
        }
    }

    // Media Center Viewport
    Item {
        id: mediaViewport
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true

        // 1. Image Viewport (Lazy-Loaded strictly for images)
        Loader {
            id: imageContentLoader
            anchors.fill: parent
            active: lightboxRoot.active && lightboxRoot.mediaType === "image"
            visible: active
            sourceComponent: Component {
                Item {
                    anchors.fill: parent
                    Image {
                        id: lightboxImage
                        anchors.centerIn: parent
                        width: parent.width * lightboxRoot.zoomScale
                        height: parent.height * lightboxRoot.zoomScale
                        source: lightboxRoot.mediaUrl
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                }
            }
        }

        // 2. Video Fullscreen Viewport & Transport Controls (Lazy-Loaded strictly for videos)
        Loader {
            id: videoContentLoader
            anchors.fill: parent
            active: lightboxRoot.active && lightboxRoot.mediaType === "video"
            visible: active
            onLoaded: {
                if (item && lightboxRoot.mediaType === "video") {
                    item.startPlayback(lightboxRoot.pendingSeekPos, lightboxRoot.resumePlayback);
                }
            }
            sourceComponent: Component {
                Item {
                    id: videoPlayerItemRoot
                    anchors.fill: parent

                    readonly property bool isPlaying: lightboxPlayer.playbackState === MediaPlayer.PlayingState

                    function startPlayback(seekPos, shouldPlay) {
                        if (seekPos > 0) {
                            lightboxPlayer.position = seekPos;
                        }
                        if (shouldPlay) {
                            lightboxPlayer.play();
                        } else {
                            lightboxPlayer.pause();
                        }
                    }

                    MediaPlayer {
                        id: lightboxPlayer
                        source: lightboxRoot.mediaUrl
                        audioOutput: AudioOutput {
                            id: lightboxAudio
                            volume: (AudioManager.isMuted || !lightboxRoot.active) ? 0.0 : AudioManager.volume
                            muted: AudioManager.isMuted || !lightboxRoot.active
                        }
                        videoOutput: lightboxVideoOutput

                        onMediaStatusChanged: {
                            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                                if (lightboxRoot.pendingSeekPos > 0) {
                                    lightboxPlayer.position = lightboxRoot.pendingSeekPos;
                                    lightboxRoot.pendingSeekPos = 0;
                                }
                                if (lightboxRoot.resumePlayback) {
                                    lightboxPlayer.play();
                                } else {
                                    lightboxPlayer.pause();
                                }
                            }
                        }

                        onErrorOccurred: (error, errorString) => {
                            console.log("[MediaLightboxModal] Playback error:", errorString);
                            lightboxRoot.hasPlaybackError = true;
                            lightboxRoot.playbackErrorMsg = errorString || "Codec not supported by GPU";
                        }
                    }

                    // In-App Video Display Surface (Fills modal viewport above bottom transport bar)
                    VideoOutput {
                        id: lightboxVideoOutput
                        visible: !lightboxRoot.hasPlaybackError
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: bottomTransportBar.top
                        fillMode: VideoOutput.PreserveAspectFit
                    }

                    // Interactive Surface Click Area (Toggles Play/Pause on Video Click)
                    MouseArea {
                        id: surfaceClickArea
                        anchors.fill: lightboxVideoOutput
                        cursorShape: Qt.PointingHandCursor
                        enabled: !(typeof lbVolCtrl !== "undefined" && lbVolCtrl && lbVolCtrl.expanded)
                        onClicked: {
                            lightboxRoot.hasPlaybackError = false;
                            if (lightboxPlayer.playbackState === MediaPlayer.PlayingState) {
                                lightboxPlayer.pause();
                            } else {
                                lightboxPlayer.play();
                            }
                        }
                    }

                    // Central Play Button (Visible only when video is paused)
                    Rectangle {
                        id: centralLbPlayBtn
                        width: 72; height: 72
                        radius: 36
                        anchors.centerIn: lightboxVideoOutput
                        color: centralPlayMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(0, 0, 0, 0.75)
                        border.color: "#FFFFFF"
                        border.width: 2
                        visible: opacity > 0 && !lightboxRoot.hasPlaybackError
                        opacity: (lightboxPlayer.playbackState !== MediaPlayer.PlayingState) ? 1.0 : 0.0
                        scale: centralPlayMouse.containsMouse ? 1.1 : 1.0

                        Behavior on opacity { NumberAnimation { duration: 150 } }
                        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

                        IconImage {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 3
                            source: "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
                            width: 28; height: 28
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: centralPlayMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                lightboxRoot.hasPlaybackError = false;
                                lightboxPlayer.play();
                            }
                        }
                    }

                    // Codec / Playback Error Recovery Card
                    Rectangle {
                        visible: lightboxRoot.hasPlaybackError
                        anchors.centerIn: lightboxVideoOutput
                        width: 400; height: 210
                        radius: 12
                        color: "#18191D"
                        border.color: Qt.rgba(255, 255, 255, 0.15)
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            IconImage {
                                Layout.alignment: Qt.AlignHCenter
                                source: "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg"
                                width: 40; height: 40
                                color: "#F1C40F"
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Hardware Codec Not Supported"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 15
                                font.bold: true
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "This video stream (AV1/MKV) cannot be rendered by the GPU."
                                color: ThemeData.textSecondary
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 190; height: 36
                                radius: 6
                                color: lbExtBtnMouse.containsMouse ? "#0066CC" : ThemeData.accentColor

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 8

                                    IconImage {
                                        source: "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
                                        width: 14; height: 14
                                        color: "#FFFFFF"
                                    }

                                    Text {
                                        text: "Play in System Player ↗"
                                        color: "#FFFFFF"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }

                                MouseArea {
                                    id: lbExtBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: AudioManager.openMediaFile(lightboxRoot.mediaUrl)
                                }
                            }
                        }
                    }

                // Fullscreen Video Transport Control Bar with Advanced 0-100% Volume & Fullscreen Toggle
                Rectangle {
                    id: bottomTransportBar
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 64
                    color: Qt.rgba(0, 0, 0, 0.85)
                    z: 10

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 24; anchors.rightMargin: 24
                        spacing: 16

                        // Play / Pause Button
                        Rectangle {
                            width: 40; height: 40
                            radius: 20
                            color: lbPlayMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(255, 255, 255, 0.15)

                            IconImage {
                                anchors.centerIn: parent
                                source: (lightboxPlayer.playbackState === MediaPlayer.PlayingState) ? "qrc:/qt/qml/NeoNect/assets/icons/pause.svg" : "qrc:/qt/qml/NeoNect/assets/icons/play.svg"
                                width: 18; height: 18
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: lbPlayMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    lightboxRoot.hasPlaybackError = false;
                                    if (lightboxPlayer.playbackState === MediaPlayer.PlayingState) {
                                        lightboxPlayer.pause();
                                    } else {
                                        lightboxPlayer.play();
                                    }
                                }
                            }
                        }

                        // Position & Duration Text
                        Text {
                            text: lightboxRoot.formatTime(lightboxPlayer.position / 1000) + " / " + lightboxRoot.formatTime(lightboxPlayer.duration / 1000)
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 13
                            font.bold: true
                        }

                        // Interactive Gradient Progress Bar
                        GradientSeekBar {
                            id: lbScrubber
                            Layout.fillWidth: true
                            value: lightboxPlayer.duration > 0 ? (lightboxPlayer.position / lightboxPlayer.duration) : 0.0
                            duration: lightboxPlayer.duration > 0 ? lightboxPlayer.duration : 0
                            trackHeight: 6
                            hoverTrackHeight: 8
                            onSeekMoved: (p) => {
                                if (lightboxPlayer.duration > 0) {
                                    lightboxPlayer.position = p * lightboxPlayer.duration;
                                }
                            }
                            onSeekFinished: (p) => {
                                if (lightboxPlayer.duration > 0) {
                                    lightboxPlayer.position = p * lightboxPlayer.duration;
                                }
                            }
                        }

                        // Advanced 0% - 100% Volume Controller
                        VolumeController {
                            id: lbVolCtrl
                            volume: AudioManager.volume
                            isMuted: AudioManager.isMuted
                            textColor: "#FFFFFF"
                            accentColor: ThemeData.accentColor
                            iconSize: 18
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

                        // True Fullscreen Window Toggle Button
                        Rectangle {
                            width: 40; height: 40
                            radius: 20
                            color: bottomFsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.25) : Qt.rgba(255, 255, 255, 0.12)

                            IconImage {
                                anchors.centerIn: parent
                                source: lightboxRoot.isWindowFullScreen ? "qrc:/qt/qml/NeoNect/assets/icons/minimize.svg" : "qrc:/qt/qml/NeoNect/assets/icons/maximize.svg"
                                width: 18; height: 18
                                color: "#FFFFFF"
                            }

                            MouseArea {
                                id: bottomFsMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: lightboxRoot.toggleWindowFullScreen()
                            }
                        }
                    }
                }
            }
        }
    }
}
}


