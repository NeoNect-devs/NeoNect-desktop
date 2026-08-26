import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Avila.Core 1.0

Item {
    id: seekRoot

    property real value: 0.0 // 0.0 to 1.0
    property real duration: 0 // In milliseconds (or seconds)
    property bool interactive: true
    property color gradientStart: "#00E5FF" // Electric cyan
    property color gradientMid: "#0A84FF"   // Vibrant royal blue
    property color gradientEnd: "#0066FF"   // Deep neon blue
    property color trackColor: Qt.rgba(255, 255, 255, 0.12)
    property color hoverTrackColor: Qt.rgba(255, 255, 255, 0.22)
    property real trackHeight: 5
    property real hoverTrackHeight: 7

    signal seekMoved(real progress)
    signal seekFinished(real progress)

    implicitWidth: 200
    implicitHeight: 20

    function formatTime(val) {
        if (!val || isNaN(val) || val < 0) return "0:00";
        var secs = Math.floor(val);
        if (seekRoot.duration > 1000) secs = Math.floor(val / 1000); // ms to sec
        var m = Math.floor(secs / 60);
        var s = Math.floor(secs % 60);
        return m + ":" + (s < 10 ? "0" + s : s);
    }

    // Base Track
    Rectangle {
        id: bgTrack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: seekMouse.containsMouse || seekMouse.pressed ? seekRoot.hoverTrackHeight : seekRoot.trackHeight
        radius: height / 2
        color: seekMouse.containsMouse ? seekRoot.hoverTrackColor : seekRoot.trackColor

        Behavior on height { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
        Behavior on color { ColorAnimation { duration: 120 } }

        // Hover Scrub Preview Highlight
        Rectangle {
            visible: seekMouse.containsMouse && !seekMouse.pressed
            width: Math.max(0, Math.min(bgTrack.width, seekMouse.mouseX))
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(255, 255, 255, 0.15)
        }

        // Active Filled Progress Bar with Multi-stop Gradient
        Rectangle {
            id: fillBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.max(0, Math.min(bgTrack.width, bgTrack.width * Math.max(0.0, Math.min(1.0, seekRoot.value))))
            radius: parent.radius
            clip: true

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: seekRoot.gradientStart }
                GradientStop { position: 0.55; color: seekRoot.gradientMid }
                GradientStop { position: 1.0; color: seekRoot.gradientEnd }
            }

            // Top glassy sheen highlight line
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1.5
                color: Qt.rgba(255, 255, 255, 0.45)
            }
        }

        // Glowing Gradient Thumb Handle
        Rectangle {
            id: thumbHandle
            width: seekMouse.containsMouse || seekMouse.pressed ? 14 : 10
            height: width
            radius: width / 2
            anchors.verticalCenter: parent.verticalCenter
            x: Math.max(0, Math.min(bgTrack.width - width, (bgTrack.width * Math.max(0.0, Math.min(1.0, seekRoot.value))) - (width / 2)))
            color: "#FFFFFF"
            border.color: seekRoot.gradientMid
            border.width: 2

            // Soft glowing drop shadow effect around thumb
            Rectangle {
                anchors.centerIn: parent
                width: parent.width + 6
                height: parent.height + 6
                radius: width / 2
                z: -1
                color: Qt.rgba(0, 180, 255, seekMouse.containsMouse || seekMouse.pressed ? 0.4 : 0.0)
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutBack } }
        }
    }

    // Floating Tooltip on Hover / Scrub
    Rectangle {
        id: tooltipBubble
        visible: seekMouse.containsMouse && seekRoot.duration > 0
        width: timeLabel.implicitWidth + 12
        height: 20
        radius: 4
        color: Qt.rgba(10, 12, 16, 0.92)
        border.color: Qt.rgba(255, 255, 255, 0.15)
        border.width: 1
        anchors.bottom: bgTrack.top
        anchors.bottomMargin: 8
        x: Math.max(0, Math.min(seekRoot.width - width, seekMouse.mouseX - (width / 2)))

        Text {
            id: timeLabel
            anchors.centerIn: parent
            text: {
                var hoverProgress = Math.max(0.0, Math.min(1.0, seekMouse.mouseX / bgTrack.width));
                return seekRoot.formatTime(hoverProgress * seekRoot.duration);
            }
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
        }
    }

    // Scrub Mouse Hitbox
    MouseArea {
        id: seekMouse
        anchors.fill: parent
        hoverEnabled: seekRoot.interactive
        enabled: seekRoot.interactive
        cursorShape: Qt.PointingHandCursor

        function updateSeek(mouseX) {
            var clampedX = Math.max(0, Math.min(bgTrack.width, mouseX));
            var p = clampedX / bgTrack.width;
            seekRoot.value = p;
            seekRoot.seekMoved(p);
        }

        onPressed: (mouse) => updateSeek(mouse.x)
        onPositionChanged: (mouse) => {
            if (pressed) updateSeek(mouse.x);
        }
        onReleased: (mouse) => {
            var clampedX = Math.max(0, Math.min(bgTrack.width, mouse.x));
            var p = clampedX / bgTrack.width;
            seekRoot.seekFinished(p);
        }
    }
}
