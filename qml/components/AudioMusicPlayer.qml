import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtMultimedia
import Avila.Core 1.0

Rectangle {
    id: musicRoot

    property string messageId: ""
    property string audioUrl: ""
    property string fileName: "Track.mp3"
    property int fileSize: 0
    property int duration: 180 // seconds
    property bool fromMe: false

    property real volumeLevel: 1.0
    property bool isMuted: false
    property bool hasPlaybackError: false

    readonly property bool isPlaying: musicPlayer.playbackState === MediaPlayer.PlayingState || (AudioManager.currentPlayingId === messageId && AudioManager.isPlaying)
    readonly property real progress: musicPlayer.duration > 0 ? (musicPlayer.position / musicPlayer.duration) : (AudioManager.currentPlayingId === messageId ? AudioManager.playbackProgress : 0.0)

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return "Audio File";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }

    function formatTime(secs) {
        if (!secs || isNaN(secs) || secs < 0) return "0:00";
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
    }

    implicitWidth: 320
    implicitHeight: 84
    Layout.preferredWidth: 320
    Layout.preferredHeight: 84
    Layout.minimumWidth: 280
    Layout.maximumWidth: 360
    radius: 12
    color: fromMe ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(255, 255, 255, 0.05)
    border.color: fromMe ? Qt.rgba(255, 255, 255, 0.18) : Qt.rgba(255, 255, 255, 0.1)
    border.width: 1

    MediaPlayer {
        id: musicPlayer
        source: musicRoot.audioUrl
        audioOutput: AudioOutput {
            id: musicAudio
            volume: musicRoot.isMuted ? 0.0 : musicRoot.volumeLevel
            muted: musicPlayer.playbackState !== MediaPlayer.PlayingState
        }

        onErrorOccurred: (error, errorString) => {
            console.log("[AudioMusicPlayer] Playback notice:", errorString);
            musicRoot.hasPlaybackError = true;
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        // Vinyl / Music Art Play Button
        Rectangle {
            width: 44; height: 44
            radius: 22
            color: fromMe ? "#FFFFFF" : ThemeData.accentColor

            IconImage {
                anchors.centerIn: parent
                source: musicRoot.isPlaying ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
                width: 18; height: 18
                color: musicRoot.fromMe ? ThemeData.accentColor : "#FFFFFF"
            }

            MouseArea {
                id: playMusicMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (musicRoot.isPlaying) {
                        if (musicPlayer.playbackState === MediaPlayer.PlayingState) {
                            musicPlayer.pause();
                        } else {
                            AudioManager.pauseAudio();
                        }
                    } else {
                        if (musicRoot.hasPlaybackError || !musicRoot.audioUrl) {
                            AudioManager.playAudio(musicRoot.messageId, musicRoot.audioUrl, musicRoot.duration);
                        } else {
                            musicPlayer.play();
                        }
                    }
                }
            }
        }

        // Track Info & Scrubber
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                IconImage {
                    source: "qrc:/qt/qml/Avila/assets/icons/music.svg"
                    width: 14; height: 14
                    color: musicRoot.fromMe ? "#FFFFFF" : ThemeData.accentColor
                }

                Text {
                    Layout.fillWidth: true
                    text: musicRoot.fileName
                    color: musicRoot.fromMe ? "#FFFFFF" : ThemeData.textPrimary
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    text: musicRoot.formatBytes(musicRoot.fileSize)
                    color: musicRoot.fromMe ? Qt.rgba(255, 255, 255, 0.7) : ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }
            }

            // Interactive Gradient Progress Bar
            GradientSeekBar {
                id: musicScrubber
                Layout.fillWidth: true
                value: musicRoot.progress
                duration: musicPlayer.duration > 0 ? musicPlayer.duration : (musicRoot.duration * 1000)
                gradientStart: musicRoot.fromMe ? "#FFFFFF" : "#00E5FF"
                gradientMid: musicRoot.fromMe ? "#E0F2FE" : "#0A84FF"
                gradientEnd: musicRoot.fromMe ? "#BAE6FD" : "#0066FF"
                onSeekMoved: (p) => {
                    if (musicPlayer.duration > 0) {
                        musicPlayer.position = p * musicPlayer.duration;
                    } else if (AudioManager.currentPlayingId === musicRoot.messageId) {
                        AudioManager.seek(musicRoot.messageId, p);
                    }
                }
                onSeekFinished: (p) => {
                    if (musicPlayer.duration > 0) {
                        musicPlayer.position = p * musicPlayer.duration;
                    } else if (AudioManager.currentPlayingId === musicRoot.messageId) {
                        AudioManager.seek(musicRoot.messageId, p);
                    }
                }
            }

            // Time, Duration & Advanced Volume Controller (0% to 100%)
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: musicRoot.formatTime(musicPlayer.position > 0 ? (musicPlayer.position / 1000) : (AudioManager.currentPosition / 1000))
                    color: musicRoot.fromMe ? Qt.rgba(255, 255, 255, 0.8) : ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: musicRoot.formatTime(musicPlayer.duration > 0 ? (musicPlayer.duration / 1000) : musicRoot.duration)
                    color: musicRoot.fromMe ? Qt.rgba(255, 255, 255, 0.8) : ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                }

                // Advanced Volume Controller (0% to 100%)
                VolumeController {
                    id: musicVolCtrl
                    volume: musicRoot.volumeLevel
                    isMuted: musicRoot.isMuted
                    textColor: musicRoot.fromMe ? "#FFFFFF" : ThemeData.textSecondary
                    accentColor: musicRoot.fromMe ? "#FFFFFF" : ThemeData.accentColor
                    iconSize: 12
                    onVolumeChangedManually: (v) => {
                        musicRoot.volumeLevel = v;
                        musicRoot.isMuted = (v === 0);
                    }
                    onMuteToggled: {
                        musicRoot.isMuted = !musicRoot.isMuted;
                    }
                }
            }
        }
    }
}
