import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Item {
    id: volControlRoot

    property real volume: 1.0 // 0.0 to 1.0 (0% to 100%)
    property bool isMuted: false
    property color accentColor: ThemeData.accentColor
    property color textColor: "#FFFFFF"
    property real iconSize: 14
    property bool showPercentage: true
    property bool alwaysExpanded: false

    signal volumeChangedManually(real newVolume)
    signal muteToggled()

    readonly property real effectiveVolume: isMuted ? 0.0 : volume
    readonly property int percentage: Math.round(effectiveVolume * 100)

    implicitWidth: expanded ? 140 : 28
    implicitHeight: 28
    clip: false

    property bool expanded: alwaysExpanded || volArea.containsMouse || sliderMouse.containsMouse

    Behavior on implicitWidth { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }

    RowLayout {
        anchors.fill: parent
        spacing: 6

        // Speaker / Mute Toggle Button
        Rectangle {
            width: 24; height: 24
            radius: 12
            color: iconMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : "transparent"

            IconImage {
                anchors.centerIn: parent
                source: (volControlRoot.isMuted || volControlRoot.volume === 0) ? "qrc:/qt/qml/Avila/assets/icons/volume-x.svg" : "qrc:/qt/qml/Avila/assets/icons/volume-2.svg"
                width: volControlRoot.iconSize
                height: volControlRoot.iconSize
                color: (volControlRoot.isMuted || volControlRoot.volume === 0) ? Qt.rgba(255, 255, 255, 0.6) : volControlRoot.textColor
            }

            MouseArea {
                id: iconMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    volControlRoot.muteToggled();
                }
            }
        }

        // Expandable 0% to 100% Volume Slider + Percentage Badge
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: volControlRoot.expanded
            opacity: volControlRoot.expanded ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: 150 } }

            RowLayout {
                anchors.fill: parent
                spacing: 6

                // Interactive Custom Track & Thumb Slider
                Rectangle {
                    id: sliderTrack
                    Layout.fillWidth: true
                    height: 5
                    radius: 2.5
                    color: Qt.rgba(255, 255, 255, 0.2)

                    // Filled Active Portion (0% to 100%) with Gradient
                    Rectangle {
                        width: parent.width * volControlRoot.effectiveVolume
                        height: parent.height
                        radius: 2.5
                        clip: true
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#00E5FF" }
                            GradientStop { position: 1.0; color: "#0A84FF" }
                        }
                    }

                    // Thumb Indicator Handle
                    Rectangle {
                        width: 10; height: 10
                        radius: 5
                        color: "#FFFFFF"
                        border.color: volControlRoot.accentColor
                        border.width: 1.5
                        x: Math.max(0, Math.min(sliderTrack.width - width, (sliderTrack.width * volControlRoot.effectiveVolume) - (width / 2)))
                        anchors.verticalCenter: parent.verticalCenter
                        scale: sliderMouse.containsMouse || sliderMouse.pressed ? 1.25 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                    }

                    MouseArea {
                        id: sliderMouse
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        function updateFromMouse(mouseX) {
                            var clampedX = Math.max(0, Math.min(sliderTrack.width, mouseX));
                            var newVol = clampedX / sliderTrack.width;
                            volControlRoot.volume = newVol;
                            volControlRoot.isMuted = (newVol === 0);
                            volControlRoot.volumeChangedManually(newVol);
                        }

                        onPositionChanged: (mouse) => {
                            if (pressed) updateFromMouse(mouse.x);
                        }
                        onPressed: (mouse) => updateFromMouse(mouse.x)
                    }
                }

                // Live Percentage Indicator (0% to 100%)
                Text {
                    visible: volControlRoot.showPercentage
                    text: volControlRoot.percentage + "%"
                    color: volControlRoot.textColor
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.bold: true
                    Layout.preferredWidth: 28
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }

    MouseArea {
        id: volArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}
