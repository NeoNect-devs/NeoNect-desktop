import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtMultimedia
import Avila.Core 1.0

Rectangle {
    id: lightboxRoot

    property bool active: false
    property string mediaUrl: ""
    property string mediaType: "image" // "image" | "video"
    property string fileName: "Media"
    property real zoomScale: 1.0

    property real volumeLevel: 1.0
    property bool isMuted: false
    property bool hasPlaybackError: false
    property string playbackErrorMsg: ""

    readonly property bool isPlaying: lightboxPlayer.playbackState === MediaPlayer.PlayingState
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

    function open(url, type, name) {
        lightboxRoot.mediaUrl = url;
        lightboxRoot.mediaType = type || "image";
        lightboxRoot.fileName = name || "Media";
        lightboxRoot.zoomScale = 1.0;
        lightboxRoot.hasPlaybackError = false;
        lightboxRoot.active = true;

        if (lightboxRoot.mediaType === "video") {
            lightboxPlayer.play();
        }
    }

    function close() {
        if (lightboxRoot.mediaType === "video") {
            lightboxPlayer.stop();
        }
        var win = lightboxRoot.Window.window;
        if (win && win.visibility === Window.FullScreen) {
            win.visibility = Window.Windowed;
        }
        lightboxRoot.active = false;
        lightboxRoot.closeRequested();
    }

    anchors.fill: parent
    visible: active || opacity > 0
    opacity: active ? 1.0 : 0.0
    z: 100000
    color: Qt.rgba(0, 0, 0, 0.96)

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

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

    MediaPlayer {
        id: lightboxPlayer
        source: lightboxRoot.mediaType === "video" ? lightboxRoot.mediaUrl : ""
        audioOutput: AudioOutput {
            id: lightboxAudio
            volume: (lightboxRoot.isMuted || !lightboxRoot.isPlaying) ? 0.0 : lightboxRoot.volumeLevel
            muted: lightboxRoot.isMuted || !lightboxRoot.isPlaying
        }
        videoOutput: lightboxVideoOutput

        onErrorOccurred: (error, errorString) => {
            console.log("[MediaLightboxModal] Playback error:", errorString);
            lightboxRoot.hasPlaybackError = true;
            lightboxRoot.playbackErrorMsg = errorString || "Codec not supported by GPU";
        }
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

            // Zoom Out (Images only)
            Rectangle {
                visible: lightboxRoot.mediaType === "image"
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

            // Zoom Reset (Images only)
            Text {
                visible: lightboxRoot.mediaType === "image"
                text: Math.round(lightboxRoot.zoomScale * 100) + "%"
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 12
            }

            // Zoom In (Images only)
            Rectangle {
                visible: lightboxRoot.mediaType === "image"
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

            // Fullscreen Window Toggle Button
            Rectangle {
                width: 36; height: 36
                radius: 18
                color: topFsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                IconImage {
                    anchors.centerIn: parent
                    source: lightboxRoot.isWindowFullScreen ? "qrc:/qt/qml/Avila/assets/icons/minimize.svg" : "qrc:/qt/qml/Avila/assets/icons/maximize.svg"
                    width: 16; height: 16
                    color: "#FFFFFF"
                }

                MouseArea {
                    id: topFsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: lightboxRoot.toggleWindowFullScreen()
                }
            }

            // Download Button
            Rectangle {
                width: 36; height: 36
                radius: 18
                color: downloadMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/download.svg"
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

    // Media Center Viewport (Fills whole app window, preserving aspect ratio)
    Item {
        id: mediaViewport
        anchors.top: topBar.bottom
        anchors.bottom: (lightboxRoot.mediaType === "video" ? bottomTransportBar.top : parent.bottom)
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true

        // 1. Image Display (Centered & Scaled to fill viewport cleanly)
        Image {
            id: lightboxImage
            visible: lightboxRoot.mediaType === "image"
            anchors.centerIn: parent
            width: parent.width * lightboxRoot.zoomScale
            height: parent.height * lightboxRoot.zoomScale
            source: lightboxRoot.mediaUrl
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        // 2. In-App Video Display Surface (Fills 100% of the app's viewport)
        VideoOutput {
            id: lightboxVideoOutput
            visible: !lightboxRoot.hasPlaybackError && lightboxRoot.mediaType === "video"
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectFit
        }

        // 3. Interactive Surface Click Area (Toggles Play/Pause on Video Click)
        MouseArea {
            id: surfaceClickArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (lightboxRoot.mediaType === "video") {
                    lightboxRoot.hasPlaybackError = false;
                    if (lightboxPlayer.playbackState === MediaPlayer.PlayingState) {
                        lightboxPlayer.pause();
                    } else {
                        lightboxPlayer.play();
                    }
                }
            }
        }

        // 4. Central Play Button (Visible only when video is paused)
        Rectangle {
            id: centralLbPlayBtn
            width: 72; height: 72
            radius: 36
            anchors.centerIn: parent
            color: centralPlayMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(0, 0, 0, 0.75)
            border.color: "#FFFFFF"
            border.width: 2
            visible: opacity > 0 && lightboxRoot.mediaType === "video" && !lightboxRoot.hasPlaybackError
            opacity: (!lightboxRoot.isPlaying) ? 1.0 : 0.0
            scale: centralPlayMouse.containsMouse ? 1.1 : 1.0

            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

            IconImage {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: 3
                source: "qrc:/qt/qml/Avila/assets/icons/play.svg"
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

        // 5. Codec / Playback Error Recovery Card in Lightbox
        Rectangle {
            visible: lightboxRoot.hasPlaybackError && lightboxRoot.mediaType === "video"
            anchors.centerIn: parent
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
                    source: "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg"
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
                            source: "qrc:/qt/qml/Avila/assets/icons/play.svg"
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
    }

    // Fullscreen Video Transport Control Bar with Advanced 0-100% Volume & Fullscreen Toggle
    Rectangle {
        id: bottomTransportBar
        visible: lightboxRoot.mediaType === "video"
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
                    source: lightboxRoot.isPlaying ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
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
                volume: lightboxRoot.volumeLevel
                isMuted: lightboxRoot.isMuted
                textColor: "#FFFFFF"
                accentColor: ThemeData.accentColor
                iconSize: 18
                alwaysExpanded: true
                onVolumeChangedManually: (v) => {
                    lightboxRoot.volumeLevel = v;
                    lightboxRoot.isMuted = (v === 0);
                }
                onMuteToggled: {
                    lightboxRoot.isMuted = !lightboxRoot.isMuted;
                }
            }

            // True Fullscreen Window Toggle Button
            Rectangle {
                width: 40; height: 40
                radius: 20
                color: bottomFsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.25) : Qt.rgba(255, 255, 255, 0.12)

                IconImage {
                    anchors.centerIn: parent
                    source: lightboxRoot.isWindowFullScreen ? "qrc:/qt/qml/Avila/assets/icons/minimize.svg" : "qrc:/qt/qml/Avila/assets/icons/maximize.svg"
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
