// MemberItem.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0

// The root is now a Rectangle to provide a background for the hover effect.
Rectangle {
    id: root
    width: parent.width
    // The height will be determined by the content, with some vertical padding.
    implicitHeight: layout.implicitHeight + 10

    // --- Properties ---
    property string memberName: ""
    property url memberAvatar: ""

    // --- Visuals ---
    // The background color changes when the mouse is hovering over the item.
    color: mouseArea.containsMouse ? "#2E2F34" : "transparent"
    radius: 4 // Add rounded corners to the background.

    // The layout containing the avatar and name.
    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: 5  // Add padding from the left edge.
        anchors.rightMargin: 5 // Add padding from the right edge.
        spacing: 10

        CircularImage {
            source: root.memberAvatar
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: root.memberName
            color: "#8e9297"
            font.pixelSize: 14
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
        }
    }

    // MouseArea to detect the hover state.
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true // This is required to detect mouse enter/exit events.
        cursorShape: Qt.PointingHandCursor
    }
}
