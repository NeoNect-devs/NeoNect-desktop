import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: recorderBarRoot

    signal voiceSent(var voiceData)
    signal voiceCancelled()

    height: 48
    radius: 10
    color: "#181216"
    border.color: "#E53935"
    border.width: 1

    function formatTime(seconds) {
        var m = Math.floor(seconds / 60);
        var s = seconds % 60;
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s);
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // Pulsing Red Recording Indicator
        Rectangle {
            width: 12; height: 12
            radius: 6
            color: "#E53935"

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 600; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
            }
        }

        // Duration Counter
        Text {
            text: recorderBarRoot.formatTime(AudioManager.recordingDuration)
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.bold: true
        }

        // Live Audio Equalizer Waveform Bars
        Row {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 3
            clip: true

            Repeater {
                model: AudioManager.liveWaveform

                Rectangle {
                    width: 3
                    height: Math.max(4, Math.min(28, modelData * 30))
                    radius: 1.5
                    color: "#E53935"
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on height {
                        NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
                    }
                }
            }
        }

        // Slide to cancel / Trash Button
        Rectangle {
            width: 36; height: 36
            radius: 18
            color: cancelMouse.containsMouse ? Qt.rgba(229, 57, 53, 0.25) : "transparent"

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/trash.svg"
                width: 18; height: 18
                color: "#E53935"
            }

            MouseArea {
                id: cancelMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    AudioManager.cancelRecording();
                    recorderBarRoot.voiceCancelled();
                }
            }
        }

        // Send Voice Button
        Rectangle {
            width: 36; height: 36
            radius: 18
            color: sendVoiceMouse.containsMouse ? Qt.darker(ThemeData.accentColor, 1.15) : ThemeData.accentColor

            IconImage {
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/send.svg"
                width: 16; height: 16
                color: "#FFFFFF"
            }

            MouseArea {
                id: sendVoiceMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    var data = AudioManager.stopRecording();
                    if (data && data.duration > 0) {
                        recorderBarRoot.voiceSent(data);
                    } else {
                        recorderBarRoot.voiceCancelled();
                    }
                }
            }
        }
    }
}
