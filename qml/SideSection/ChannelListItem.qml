// ChannelListItem.qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: control

    // Public API / Properties
    property string name: "channel-name"
    property string type: "channel" // Options: "channel" (hash icon) or "dm" (avatar/user icon)
    property string avatarSource: "" // Used if type === "dm"
    property bool isSelected: false
    property bool hasUnread: false
    property int notificationCount: 0

    signal clicked()

    // Styling configurations
    Layout.fillWidth: true
    height: 34
    radius: 4

    // Dynamic background color based on interaction states
    color: isSelected
           ? Qt.rgba(255, 255, 255, 0.1)  // Selected state
           : (itemMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent") // Hover vs Idle

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 8

        // Icon Column (Hash tag or DM Avatar)
        Item {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignVCenter

            // Render text-based symbol if no image asset is used for speed/simplicity
            Text {
                anchors.centerIn: parent
                visible: control.type === "channel"
                text: "#"
                font.pixelSize: 18
                font.weight: Font.Medium
                color: control.isSelected || itemMouseArea.containsMouse ? "#FFFFFF" : "#80848E"
            }

            // Simple Circle Avatar fallback for Direct Messages
            Rectangle {
                anchors.fill: parent
                visible: control.type === "dm"
                radius: width / 2
                color: "#35373C"

                Image {
                    anchors.fill: parent
                    source: control.avatarSource
                    fillMode: Image.PreserveAspectFit
                    visible: control.avatarSource !== ""
                }

                Text {
                    anchors.centerIn: parent
                    visible: control.avatarSource === ""
                    text: control.name.charAt(0).toUpperCase()
                    color: "white"
                    font.pixelSize: 10
                }
            }
        }

        // Channel/User Name Label
        Text {
            Layout.fillWidth: true
            text: control.name
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: control.hasUnread ? Font.Bold : Font.Normal
            elide: Text.ElideRight

            // Discord typography color updates on active highlight
            color: control.hasUnread || control.isSelected || itemMouseArea.containsMouse
                   ? "#F2F3F5"
                   : "#949BA4"
        }

        // Notification Badge Indicator
        Rectangle {
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            radius: 8
            color: "#F23F43" // Discord Red
            visible: control.notificationCount > 0
            Layout.alignment: Qt.AlignVCenter

            Text {
                anchors.centerIn: parent
                text: control.notificationCount > 99 ? "99+" : control.notificationCount
                color: "white"
                font.pixelSize: 9
                font.weight: Font.Bold
            }
        }
    }

    // Unread small left pillar indicator
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: -4 // Inset slightly past bounds or flush
        anchors.verticalCenter: parent.verticalCenter
        width: 4
        height: 8
        radius: 2
        color: "#FFFFFF"
        visible: control.hasUnread && !control.isSelected
    }

    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
