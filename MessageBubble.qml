import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    width: parent.width // Take the full width of its container (e.g., the ScrollView's layout).

    // --- Public Properties ---
    // These can be set from outside when you create a MessageBubble.
    property string messageText: "This is a default message that is long enough to demonstrate word wrapping."
    property bool sentByMe: false // Set to true for your messages, false for others.

    // This Spacer pushes the bubble to the right if sentByMe is true.
    Item { Layout.fillWidth: !root.sentByMe }

    // --- The Bubble ---
    Rectangle {
        id: bubbleBackground
        // Adjust width based on the text content, but don't let it get too wide.
        //width: Math.min(messageLabel.implicitWidth + 24, messagebubble.width * 0.75)
        Layout.preferredWidth: Math.min(messageLabel.implicitWidth + 24, root.width * 0.75)
        //height: messageLabel.implicitHeight + 16
        Layout.preferredHeight: messageLabel.implicitHeight + 16
        radius: 12

        // Change color based on the sender.
        color: root.sentByMe ? "#5865F2" : "#313338"

        Text {
            id: messageLabel
            text: root.messageText
            width: bubbleBackground.width - 24 // 12 pixels of padding on each sid
            //anchors.fill: parent
            //anchors.margins: root.sentByMe ? 10 : 12 // Adjust margins slightly for better text alignment.
            anchors.centerIn: parent
            wrapMode: Text.WordWrap // Crucial for long messages.
            color: "#DCDDDE"
        }
    }

    // This Spacer pushes the bubble to the left if sentByMe is false.
    Item { Layout.fillWidth: root.sentByMe }
}
