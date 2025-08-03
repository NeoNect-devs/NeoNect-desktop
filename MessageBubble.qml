// MessageBubble.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    width: parent.width
    height: messageRow.height

    // --- Public Properties ---
    property string messageText: "This is a default message."
    property bool sentByMe: false
    property bool avatarVisible: false
    property url avatarSource: ""

    RowLayout {
        id: messageRow
        width: parent.width
        spacing: 6

        // --- Avatar for received messages ---
        Item {
            Layout.preferredWidth: 32
            Layout.leftMargin: 12
            visible: !root.sentByMe

            Image {
                id: receivedAvatar
                width: 32
                height: 32
                source: root.avatarSource
                anchors.bottom: parent.bottom
                visible: root.avatarVisible
                fillMode: Image.PreserveAspectCrop

                // Simple circular clipping by overlaying a transparent rectangle with a radius
                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    color: "transparent"
                    border.color: "transparent"
                    antialiasing: true
                }
            }
        }

        // --- Spacer to align sent messages to the right ---
        Item { Layout.fillWidth: root.sentByMe }

        // --- The Bubble ---
        Rectangle {
            id: bubbleBackground
            Layout.preferredWidth: Math.min(messageLabel.implicitWidth + 24, root.width * 0.7)
            Layout.preferredHeight: messageLabel.implicitHeight + 16
            radius: 12
            color: root.sentByMe ? "#5865F2" : "#40444B"

            Text {
                id: messageLabel
                text: root.messageText
                width: bubbleBackground.width - 24
                anchors.centerIn: parent
                wrapMode: Text.Wrap // Handles long unbroken strings
                color: "#DCDDDE"
            }
        }

        // --- Spacer to align received messages to the left ---
        Item { Layout.fillWidth: !root.sentByMe }

        // --- Avatar for sent messages ---
        Item {
            Layout.preferredWidth: 32
            Layout.rightMargin: 12

            visible: root.sentByMe

            Image {
                id: sentAvatar
                width: 32
                height: 32
                source: root.avatarSource
                anchors.bottom: parent.bottom
                visible: root.avatarVisible
                fillMode: Image.PreserveAspectCrop

                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    color: "transparent"
                    border.color: "transparent"
                    antialiasing: true
                }
            }
        }
    }
}
