// MemberItem.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0

Rectangle {
    id: root
    width: parent.width
    implicitHeight: 42 // Locked to standard chat panel height metric rules
    radius: 4
    color: mouseArea.containsMouse ? "#2F3136" : "transparent"

    Behavior on color { ColorAnimation { duration: 100 } }

    property string memberName: ""
    property url memberAvatar: ""

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 12

        CircularImage {
            source: root.memberAvatar
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: root.memberName
            color: mouseArea.containsMouse ? "#DCDDDE" : "#8E9297"
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: Font.Medium
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            elide: Text.ElideRight

            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
    }
}
