// MessagesGroup.qml
import QtQuick
import QtQuick.Layouts

Item {
    id: root
    width: parent.width
    implicitHeight: groupLayout.implicitHeight

    // --- Properties ---
    // These properties define the data this component needs to render.
    property string senderName: ""
    property url senderAvatar: ""
    property bool fromMe: false
    property var messages: []

    RowLayout {
        id: groupLayout
        width: parent.width
        spacing: 6

        // --- PRE-SPACER (for sent messages) ---
        // This spacer expands only when the message is from "Me", pushing the content right.
        Item {
            Layout.preferredWidth:root.width * 0.34
            visible: root.fromMe
        }

        // --- AVATAR (for received messages) ---
        Image {
            id: avatar
            source: root.senderAvatar
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.leftMargin: 6
            Layout.alignment: Qt.AlignTop
            visible: !root.fromMe
            // *** FIX: Use opacity instead of visibility to preserve layout space. ***
            // The avatar becomes transparent but still occupies its 32px width,
            // preventing the message bubbles from shifting.
            opacity: !root.fromMe && messageListView.stickyAvatarIndex !== index ? 1 : 0
            fillMode: Image.PreserveAspectCrop

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: "transparent"
            }
        }

        // --- MESSAGE BUBBLES COLUMN ---
        ColumnLayout {
            id: bubblesColumn
            spacing: 4
            // You can adjust this value. It's a percentage of the total chat width.
            // Note: For received messages, the actual maximum width will be slightly
            // less, as space is also needed for the avatar (32px) and spacing (6px).
            Layout.maximumWidth: root.width * 0.60

            // --- SENDER NAME ---
            // This Text item displays the sender's name above the message bubbles.
            Text {
                text: root.senderName
                // Only show the name for messages from other users.
                visible: !root.fromMe
                color: "#8e9297" // A muted grey, common for metadata in chat apps
                font.pixelSize: 14
                font.bold: true
                // Add some margin to separate the name from the first bubble.
                Layout.bottomMargin: 2
                // For received messages, the layout direction is LeftToRight,
                // so this text will be correctly left-aligned.
            }

            Repeater {
                model: root.messages
                delegate: MessageBubble {
                    // The delegate passes all necessary info to the bubble.
                    messageText: text
                    sentByMe: fromMe
                }
            }
        }

        // --- POST-SPACER (for received messages) ---
        // This spacer expands only for received messages, keeping them left-aligned.
        Item {
            Layout.fillWidth: !root.fromMe
        }
    }
}
