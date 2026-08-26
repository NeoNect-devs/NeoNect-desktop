import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtMultimedia
import Avila.Core 1.0

Rectangle {
    id: videoRoot

    property string messageId: ""
    property string videoUrl: ""
    property string fileName: "Video.mp4"
    property int fileSize: 0
    property int duration: 45 // seconds
    property bool fromMe: false

    property real volumeLevel: 1.0
    property bool isMuted: false
    property bool hasPlaybackError: false
    property string playbackErrorMsg: ""

    signal openFullscreenRequested(string url, string name)

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
    border.color: Qt.rgba(255, 255, 255, 0.1)
    border.width: 1

    MediaPlayer {
        id: player
        source: videoRoot.videoUrl
        audioOutput: AudioOutput {
            id: audioOut
            volume: (videoRoot.isMuted || player.playbackState !== MediaPlayer.PlayingState) ? 0.0 : videoRoot.volumeLevel
            muted: videoRoot.isMuted || player.playbackState !== MediaPlayer.PlayingState
        }
        videoOutput: videoOutputItem

        onErrorOccurred: (error, errorString) => {
            console.log("[VideoPlayerItem] Playback error:", errorString);
            videoRoot.hasPlaybackError = true;
            videoRoot.playbackErrorMsg = errorString || "Codec not supported by GPU";
        }
    }

    Component.onCompleted: {
        if (videoRoot.videoUrl && videoRoot.videoUrl !== "") {
            // Pre-load and pause at frame 0 to extract thumbnail
            player.pause();
        }
    }

    onVideoUrlChanged: {
        if (videoRoot.videoUrl && videoRoot.videoUrl !== "") {
            player.pause();
        }
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
            source: "qrc:/qt/qml/Avila/assets/icons/video.svg"
            width: 64; height: 64
            color: Qt.rgba(255, 255, 255, 0.08)
        }
    }

    // 2. Real Decoded Video Frame Surface (Thumbnail & Playback)
    VideoOutput {
        id: videoOutputItem
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: !videoRoot.hasPlaybackError
    }

    // 3. Interactive Full Click Area
    MouseArea {
        id: videoClickArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            videoRoot.hasPlaybackError = false;
            if (player.playbackState === MediaPlayer.PlayingState) {
                player.pause();
            } else {
                player.play();
            }
        }
    }

    // 4. Central Glowing Play / Pause Button
    Rectangle {
        id: playBtn
        width: 56; height: 56
        radius: 28
        color: playBtnMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(0, 0, 0, 0.75)
        border.color: "#FFFFFF"
        border.width: 2
        anchors.centerIn: parent
        visible: !videoRoot.hasPlaybackError && (player.playbackState !== MediaPlayer.PlayingState || videoClickArea.containsMouse)
        scale: playBtnMouse.containsMouse ? 1.12 : 1.0

        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

        IconImage {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: player.playbackState === MediaPlayer.PlayingState ? 0 : 2
            source: player.playbackState === MediaPlayer.PlayingState ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
            width: 24; height: 24
            color: "#FFFFFF"
        }

        MouseArea {
            id: playBtnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                videoRoot.hasPlaybackError = false;
                if (player.playbackState === MediaPlayer.PlayingState) {
                    player.pause();
                } else {
                    player.play();
                }
            }
        }
    }

    // 5. Top Filename & External Play Bar
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        height: 28
        radius: 6
        color: Qt.rgba(0, 0, 0, 0.65)
        visible: !videoRoot.hasPlaybackError && (player.playbackState !== MediaPlayer.PlayingState || videoClickArea.containsMouse)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8; anchors.rightMargin: 8
            spacing: 6

            IconImage {
                source: "qrc:/qt/qml/Avila/assets/icons/video.svg"
                width: 14; height: 14
                color: "#FFFFFF"
            }

            Text {
                Layout.fillWidth: true
                text: videoRoot.fileName
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.bold: true
                elide: Text.ElideRight
            }

            // Quick External Open Icon
            Rectangle {
                width: 20; height: 20
                radius: 4
                color: quickExtMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "↗"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.bold: true
                }

                MouseArea {
                    id: quickExtMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: AudioManager.openMediaFile(videoRoot.videoUrl)
                }
            }
        }
    }

    // 6. Bottom Controls Bar with Advanced Volume Controller (0% to 100%)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 38
        color: Qt.rgba(0, 0, 0, 0.85)
        visible: !videoRoot.hasPlaybackError

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
                    source: player.playbackState === MediaPlayer.PlayingState ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
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

            // Advanced 0% - 100% Volume Controller
            VolumeController {
                id: videoVolCtrl
                volume: videoRoot.volumeLevel
                isMuted: videoRoot.isMuted
                textColor: "#FFFFFF"
                accentColor: ThemeData.accentColor
                onVolumeChangedManually: (v) => {
                    videoRoot.volumeLevel = v;
                    videoRoot.isMuted = (v === 0);
                }
                onMuteToggled: {
                    videoRoot.isMuted = !videoRoot.isMuted;
                }
            }

            // Fullscreen Expansion Button
            Rectangle {
                width: 48; height: 22
                radius: 4
                color: fsMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "Full ↗"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    id: fsMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        player.pause();
                        videoRoot.openFullscreenRequested(videoRoot.videoUrl, videoRoot.fileName);
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
                source: "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg"
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

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 160; height: 32
                radius: 6
                color: extBtnMouse.containsMouse ? "#0066CC" : ThemeData.accentColor

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    IconImage {
                        source: "qrc:/qt/qml/Avila/assets/icons/play.svg"
                        width: 14; height: 14
                        color: "#FFFFFF"
                    }

                    Text {
                        text: "Open System Player ↗"
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
