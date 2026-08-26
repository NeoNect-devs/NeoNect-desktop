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
    property string mediaType: "image" // "image", "video"
    property string fileName: "Media"
    property real zoomScale: 1.0

    property real volumeLevel: 1.0
    property bool isMuted: false
    property bool hasPlaybackError: false
    property string playbackErrorMsg: ""

    signal closeRequested()
    signal downloadRequested(string url, string name)

    function formatTime(secs) {
        if (!secs || isNaN(secs) || secs < 0) return "0:00";
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
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
        lightboxRoot.active = false;
        lightboxRoot.closeRequested();
    }

    anchors.fill: parent
    visible: active || opacity > 0
    opacity: active ? 1.0 : 0.0
    z: 100000
    color: Qt.rgba(0, 0, 0, 0.94)

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

    MediaPlayer {
        id: lightboxPlayer
        source: lightboxRoot.mediaType === "video" ? lightboxRoot.mediaUrl : ""
        audioOutput: AudioOutput {
            id: lightboxAudio
            volume: lightboxRoot.isMuted ? 0.0 : lightboxRoot.volumeLevel
        }
        videoOutput: lightboxVideoOutput

        onErrorOccurred: (error, errorString) => {
            console.log("[MediaLightboxModal] Playback error:", errorString);
            lightboxRoot.hasPlaybackError = true;
            lightboxRoot.playbackErrorMsg = errorString || "Codec not supported by GPU";
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: lightboxRoot.close()
    }

    // Lightbox Controls Top Bar
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: Qt.rgba(0, 0, 0, 0.6)
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20; anchors.rightMargin: 20
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: lightboxRoot.fileName
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 15
                font.bold: true
                elide: Text.ElideRight
            }

            // Zoom Out
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

            // Zoom In
            Rectangle {
                visible: lightboxRoot.mediaType === "image"
                width: 36; height: 36
                radius: 18
                color: zoomInMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: "#FFFFFF"
                    font.pixelSize: 18
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

            // Open in External Player
            Rectangle {
                visible: lightboxRoot.mediaType === "video"
                width: 140; height: 32
                radius: 6
                color: openExtMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

                Text {
                    anchors.centerIn: parent
                    text: "System Player ↗"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    id: openExtMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: AudioManager.openMediaFile(lightboxRoot.mediaUrl)
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
                color: closeBtnMouse.containsMouse ? Qt.rgba(255, 50, 50, 0.4) : Qt.rgba(255, 255, 255, 0.1)

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
    Flickable {
        anchors.fill: parent
        anchors.topMargin: 56
        anchors.bottomMargin: lightboxRoot.mediaType === "video" ? 64 : 0
        contentWidth: Math.max(width, contentItemContainer.width)
        contentHeight: Math.max(height, contentItemContainer.height)
        clip: true

        Item {
            id: contentItemContainer
            width: lightboxRoot.width * lightboxRoot.zoomScale
            height: (lightboxRoot.height - 56 - (lightboxRoot.mediaType === "video" ? 64 : 0)) * lightboxRoot.zoomScale
            anchors.centerIn: parent

            // 1. Image Display
            Image {
                visible: lightboxRoot.mediaType === "image"
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.9, 800)
                height: Math.min(parent.height * 0.9, 600)
                source: lightboxRoot.mediaUrl
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            // 2. In-App Video Display Surface
            VideoOutput {
                id: lightboxVideoOutput
                visible: !lightboxRoot.hasPlaybackError && lightboxRoot.mediaType === "video"
                anchors.centerIn: parent
                width: Math.min(parent.width * 0.9, 900)
                height: Math.min(parent.height * 0.9, 540)
                fillMode: VideoOutput.PreserveAspectFit

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (lightboxPlayer.playbackState === MediaPlayer.PlayingState) {
                            lightboxPlayer.pause();
                        } else {
                            lightboxPlayer.play();
                        }
                    }
                }
            }

            // Codec / Playback Error Recovery Card in Lightbox
            Rectangle {
                visible: lightboxRoot.hasPlaybackError && lightboxRoot.mediaType === "video"
                anchors.centerIn: parent
                width: 380; height: 200
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
                        width: 180; height: 36
                        radius: 6
                        color: lbExtBtnMouse.containsMouse ? "#4752C4" : ThemeData.accentColor

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
    }

    // Fullscreen Video Transport Control Bar with Advanced 0-100% Volume
    Rectangle {
        visible: lightboxRoot.mediaType === "video"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64
        color: Qt.rgba(0, 0, 0, 0.8)
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
                    source: lightboxPlayer.playbackState === MediaPlayer.PlayingState ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
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

            // Progress Slider
            Slider {
                id: lbScrubber
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                value: lightboxPlayer.duration > 0 ? (lightboxPlayer.position / lightboxPlayer.duration) : 0.0
                onMoved: {
                    if (lightboxPlayer.duration > 0) {
                        lightboxPlayer.position = lbScrubber.value * lightboxPlayer.duration;
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
        }
    }
}
