import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Item {
    id: volControlRoot

    property real volume: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.volume : 1.0
    property bool isMuted: (typeof AudioManager !== "undefined" && AudioManager) ? AudioManager.isMuted : false
    property color accentColor: ThemeData.accentColor
    property color textColor: "#FFFFFF"
    property real iconSize: 14
    property bool showPercentage: true
    property bool alwaysExpanded: false

    signal volumeChangedManually(real newVolume)
    signal muteToggled()

    readonly property real effectiveVolume: isMuted ? 0.0 : volume
    readonly property int percentage: Math.round(effectiveVolume * 100)

    implicitWidth: 28
    implicitHeight: 28
    clip: false

    property bool expanded: alwaysExpanded

    function setVolumeManually(newVol) {
        var clamped = Math.max(0.0, Math.min(1.0, newVol));
        if (typeof AudioManager !== "undefined" && AudioManager) {
            AudioManager.setVolume(clamped);
            if (clamped === 0) {
                AudioManager.setMuted(true);
            }
        } else {
            volControlRoot.volume = clamped;
            volControlRoot.isMuted = (clamped === 0);
        }
        volControlRoot.volumeChangedManually(clamped);
    }

    function toggleMute() {
        if (typeof AudioManager !== "undefined" && AudioManager) {
            AudioManager.toggleMute();
        } else {
            volControlRoot.isMuted = !volControlRoot.isMuted;
        }
        volControlRoot.muteToggled();
    }

    Timer {
        id: hideTimer
        interval: 650
        repeat: false
        onTriggered: {
            if (!volControlRoot.alwaysExpanded && !dragArea.pressed && (!popupContainer.visible || (!bgMouse.containsMouse && !dragArea.containsMouse)) && !btnMouse.containsMouse) {
                volControlRoot.expanded = false;
            }
        }
    }

    // ─── VERTICAL POPUP SLIDER CARD (Floats seamlessly above speaker button via Overlay) ───
    Popup {
        id: popupContainer
        x: Math.round((volControlRoot.width - width) / 2)
        y: Math.round(speakerBtn.y - height)
        width: 44
        height: verticalCard.height + 6
        padding: 0
        margins: 0
        modal: false
        focus: false
        dim: false
        closePolicy: volControlRoot.alwaysExpanded ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)
        visible: volControlRoot.expanded

        background: Item {}

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 150; easing.type: Easing.OutQuad }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 150; easing.type: Easing.OutQuad }
        }

        onClosed: {
            if (!volControlRoot.alwaysExpanded) {
                volControlRoot.expanded = false;
            }
        }

        contentItem: Item {
            anchors.fill: parent

            // Full bridge MouseArea spanning across card down to speaker button (zero gap)
            MouseArea {
                id: bgMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.AllButtons

                onEntered: {
                    hideTimer.stop();
                    volControlRoot.expanded = true;
                }
                onExited: {
                    if (!dragArea.pressed && !btnMouse.containsMouse && !dragArea.containsMouse) {
                        hideTimer.restart();
                    }
                }
                onPressed: (mouse) => mouse.accepted = true
                onWheel: (wheel) => {
                    var step = wheel.angleDelta.y > 0 ? 0.05 : -0.05;
                    volControlRoot.setVolumeManually(volControlRoot.volume + step);
                    wheel.accepted = true;
                    hideTimer.stop();
                }
            }

            // The Floating Card
            Rectangle {
                id: verticalCard
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                width: 38
                height: 116
                radius: 10
                color: "#F016181C"
                border.color: Qt.rgba(255, 255, 255, 0.2)
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: 4

                    // Percentage Display at Top
                    Text {
                        visible: volControlRoot.showPercentage
                        Layout.alignment: Qt.AlignHCenter
                        text: volControlRoot.percentage + "%"
                        color: volControlRoot.effectiveVolume === 0 ? Qt.rgba(255, 255, 255, 0.5) : volControlRoot.textColor
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    // Vertical Track Area (Full-card-width interactive drag hit area)
                    Item {
                        id: trackContainer
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        // Track Background Groove
                        Rectangle {
                            id: sliderTrack
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 6
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            width: 5
                            radius: 2.5
                            color: Qt.rgba(255, 255, 255, 0.22)

                            // Active Filled Gradient from Bottom Upwards
                            Rectangle {
                                anchors.bottom: parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                height: volControlRoot.effectiveVolume > 0
                                        ? Math.max(0, Math.min(parent.height, parent.height - (thumbHandle.y + thumbHandle.height / 2)))
                                        : 0
                                radius: 2.5
                                clip: true

                                gradient: Gradient {
                                    orientation: Gradient.Vertical
                                    GradientStop { position: 0.0; color: "#00E5FF" }
                                    GradientStop { position: 1.0; color: "#0A84FF" }
                                }
                            }

                            // Thumb Handle
                            Rectangle {
                                id: thumbHandle
                                width: 14; height: 14
                                radius: 7
                                color: "#FFFFFF"
                                border.color: volControlRoot.accentColor
                                border.width: 2
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: Math.max(0, Math.min(sliderTrack.height - height, (1.0 - volControlRoot.effectiveVolume) * (sliderTrack.height - height)))
                                scale: dragArea.pressed || dragArea.containsMouse ? 1.25 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                            }
                        }

                        // Full-width interactive drag & click area (Effortless to grab anywhere)
                        MouseArea {
                            id: dragArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            preventStealing: true

                            function updateFromMouse(mouseY) {
                                var usableH = sliderTrack.height - thumbHandle.height;
                                if (usableH <= 0) return;
                                var trackY = mouseY - sliderTrack.y - (thumbHandle.height / 2);
                                var clampedY = Math.max(0, Math.min(usableH, trackY));
                                var newVol = 1.0 - (clampedY / usableH);
                                volControlRoot.setVolumeManually(newVol);
                            }

                            onEntered: {
                                hideTimer.stop();
                                volControlRoot.expanded = true;
                            }
                            onExited: {
                                if (!pressed && !btnMouse.containsMouse && !bgMouse.containsMouse) {
                                    hideTimer.restart();
                                }
                            }
                            onPressed: (mouse) => {
                                hideTimer.stop();
                                volControlRoot.expanded = true;
                                updateFromMouse(mouse.y);
                            }
                            onPositionChanged: (mouse) => {
                                if (pressed) {
                                    hideTimer.stop();
                                    updateFromMouse(mouse.y);
                                }
                            }
                            onReleased: {
                                if (!containsMouse && !btnMouse.containsMouse && !bgMouse.containsMouse) {
                                    hideTimer.restart();
                                }
                            }
                            onWheel: (wheel) => {
                                var step = wheel.angleDelta.y > 0 ? 0.05 : -0.05;
                                volControlRoot.setVolumeManually(volControlRoot.volume + step);
                                wheel.accepted = true;
                                hideTimer.stop();
                            }
                        }
                    }
                }
            }
        }
    }

    // ─── SPEAKER / MUTE TOGGLE BUTTON (Base Item) ─────────────────────────
    Rectangle {
        id: speakerBtn
        anchors.centerIn: parent
        width: 26; height: 26
        radius: 13
        color: btnMouse.containsMouse || volControlRoot.expanded ? Qt.rgba(255, 255, 255, 0.18) : "transparent"

        IconImage {
            anchors.centerIn: parent
            source: (volControlRoot.isMuted || volControlRoot.effectiveVolume === 0)
                    ? "qrc:/qt/qml/NeoNect/assets/icons/volume-x.svg"
                    : "qrc:/qt/qml/NeoNect/assets/icons/volume-2.svg"
            width: volControlRoot.iconSize
            height: volControlRoot.iconSize
            color: (volControlRoot.isMuted || volControlRoot.effectiveVolume === 0)
                   ? Qt.rgba(255, 255, 255, 0.55)
                   : volControlRoot.textColor
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onEntered: {
                hideTimer.stop();
                volControlRoot.expanded = true;
            }
            onExited: {
                if (!popupContainer.visible || (!dragArea.pressed && !bgMouse.containsMouse && !dragArea.containsMouse)) {
                    hideTimer.restart();
                }
            }
            onClicked: {
                volControlRoot.toggleMute();
                volControlRoot.expanded = true;
                hideTimer.stop();
            }
            onWheel: (wheel) => {
                var step = wheel.angleDelta.y > 0 ? 0.05 : -0.05;
                volControlRoot.setVolumeManually(volControlRoot.volume + step);
                wheel.accepted = true;
                hideTimer.stop();
            }
        }
    }
}
