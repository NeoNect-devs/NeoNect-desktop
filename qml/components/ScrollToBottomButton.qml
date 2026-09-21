import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Item {
    id: scrollBtnRoot

    property var targetListView: null
    property int unreadCount: 0
    signal clicked()

    readonly property bool shouldBeVisible: targetListView ? (!targetListView.atYEnd && targetListView.count > 0) : false

    width: 42
    height: 42
    visible: opacity > 0
    opacity: shouldBeVisible ? 1.0 : 0.0
    scale: shouldBeVisible ? 1.0 : 0.7

    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
    Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

    Rectangle {
        id: buttonCircle
        anchors.fill: parent
        radius: width / 2
        color: btnMouse.containsMouse ? "#383A40" : "#2B2D31"
        border.color: btnMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.25) : Qt.rgba(255, 255, 255, 0.12)
        border.width: 1

        // Soft drop shadow effect
        Rectangle {
            anchors.centerIn: parent
            width: parent.width + 8
            height: parent.height + 8
            radius: width / 2
            z: -1
            color: Qt.rgba(0, 0, 0, btnMouse.containsMouse ? 0.45 : 0.28)
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // Downward Arrow Icon
        IconImage {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 1
            source: "qrc:/qt/qml/NeoNect/assets/icons/arrow-back.svg"
            rotation: 270 // Rotates left-pointing arrow straight down
            width: 16
            height: 16
            color: btnMouse.containsMouse ? "#FFFFFF" : "#DBDEE1"
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (scrollBtnRoot.targetListView) {
                    scrollBtnRoot.targetListView.positionViewAtEnd();
                }
                scrollBtnRoot.unreadCount = 0;
                scrollBtnRoot.clicked();
            }
        }
    }

    // New Incoming Messages Counter Badge
    Rectangle {
        id: unreadBadge
        visible: scrollBtnRoot.unreadCount > 0
        anchors.top: parent.top
        anchors.topMargin: -4
        anchors.right: parent.right
        anchors.rightMargin: -4
        height: 18
        width: Math.max(18, badgeText.implicitWidth + 8)
        radius: 9
        color: ThemeData.accentColor
        border.color: "#1E1F22"
        border.width: 2
        scale: scrollBtnRoot.unreadCount > 0 ? 1.0 : 0.0

        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutBack } }

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: scrollBtnRoot.unreadCount > 99 ? "99+" : scrollBtnRoot.unreadCount.toString()
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
        }
    }
}
