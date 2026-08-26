import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: playerRoot

    property string messageId: ""
    property string audioUrl: ""
    property int duration: 0 // in seconds
    property var waveform: [] // array of float amplitudes 0.0 - 1.0
    property bool fromMe: false

    readonly property bool isCurrentlyActive: AudioManager.currentPlayingId === messageId
    readonly property bool isPlaying: isCurrentlyActive && AudioManager.isPlaying
    readonly property real progress: isCurrentlyActive ? AudioManager.playbackProgress : 0.0

    function formatDuration(secs) {
        if (!secs || secs <= 0) return "0:00";
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
    }

    function getWaveformArray() {
        if (waveform && waveform.length > 0) return waveform;
        var list = [];
        for (var i = 0; i < 28; ++i) {
            list.push(0.2 + 0.3 * Math.abs(Math.sin(i * 0.4)));
        }
        return list;
    }

    implicitWidth: 280
    implicitHeight: 68
    Layout.preferredWidth: 280
    Layout.preferredHeight: 68
    radius: 12
    color: "transparent"

    RowLayout {
        anchors.fill: parent
        spacing: 10

        // Play / Pause Circle Button
        Rectangle {
            width: 40; height: 40
            radius: 20
            color: playerRoot.fromMe ? "#FFFFFF" : ThemeData.accentColor

            IconImage {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: playerRoot.isPlaying ? 0 : 1
                source: playerRoot.isPlaying ? "qrc:/qt/qml/Avila/assets/icons/pause.svg" : "qrc:/qt/qml/Avila/assets/icons/play.svg"
                width: 16; height: 16
                color: playerRoot.fromMe ? ThemeData.accentColor : "#FFFFFF"
            }

            MouseArea {
                id: playBtnMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    AudioManager.togglePlayPause(playerRoot.messageId, playerRoot.audioUrl, playerRoot.duration);
                }
            }
        }

        // Waveform Visualizer & Timer Area
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            // Interactive Waveform Bars
            Item {
                Layout.fillWidth: true
                height: 24

                Row {
                    id: waveformRow
                    anchors.fill: parent
                    spacing: 2.5

                    readonly property var bars: playerRoot.getWaveformArray()
                    readonly property int barCount: bars.length

                    Repeater {
                        model: waveformRow.bars

                        Rectangle {
                            width: Math.max(2, (waveformRow.width - (waveformRow.bars.length - 1) * 2.5) / waveformRow.bars.length)
                            height: Math.max(4, Math.min(22, modelData * 22))
                            radius: 1.5
                            anchors.verticalCenter: parent.verticalCenter

                            readonly property bool isPlayed: (index / waveformRow.bars.length) <= playerRoot.progress
                            color: isPlayed ? (playerRoot.fromMe ? "#FFFFFF" : ThemeData.accentColor) : (playerRoot.fromMe ? Qt.rgba(255, 255, 255, 0.45) : Qt.rgba(255, 255, 255, 0.25))
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: (mouse) => {
                        var p = Math.max(0.0, Math.min(1.0, mouse.x / width));
                        if (!playerRoot.isCurrentlyActive) {
                            AudioManager.playAudio(playerRoot.messageId, playerRoot.audioUrl, playerRoot.duration);
                        }
                        AudioManager.seek(playerRoot.messageId, p);
                    }
                }
            }

            // Duration, Advanced Volume (0-100%) & Speed Controls
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: playerRoot.isCurrentlyActive ?
                          playerRoot.formatDuration((AudioManager.currentPosition / 1000.0)) + " / " + playerRoot.formatDuration(playerRoot.duration) :
                          playerRoot.formatDuration(playerRoot.duration)
                    color: playerRoot.fromMe ? Qt.rgba(255, 255, 255, 0.8) : ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                // Advanced Volume Controller (0% to 100%)
                VolumeController {
                    id: voiceVolCtrl
                    volume: AudioManager.volume
                    isMuted: AudioManager.isMuted
                    textColor: playerRoot.fromMe ? "#FFFFFF" : ThemeData.textSecondary
                    accentColor: playerRoot.fromMe ? "#FFFFFF" : ThemeData.accentColor
                    iconSize: 12
                    onVolumeChangedManually: (v) => AudioManager.setVolume(v)
                    onMuteToggled: AudioManager.toggleMute()
                }

                // Speed Multiplier Toggle (1x, 1.5x, 2x)
                Rectangle {
                    visible: playerRoot.isCurrentlyActive
                    width: 32; height: 18
                    radius: 9
                    color: speedMouse.containsMouse ? (playerRoot.fromMe ? Qt.rgba(255, 255, 255, 0.3) : Qt.rgba(255, 255, 255, 0.15)) : (playerRoot.fromMe ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.08))

                    Text {
                        anchors.centerIn: parent
                        text: AudioManager.playbackSpeed + "x"
                        color: playerRoot.fromMe ? "#FFFFFF" : ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        id: speedMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (AudioManager.playbackSpeed === 1.0) AudioManager.setPlaybackSpeed(1.5);
                            else if (AudioManager.playbackSpeed === 1.5) AudioManager.setPlaybackSpeed(2.0);
                            else AudioManager.setPlaybackSpeed(1.0);
                        }
                    }
                }
            }
        }
    }
}
