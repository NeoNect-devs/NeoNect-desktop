import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: Qt.darker("white", 6)

    anchors {
        top: parent.bottom
        left: parent.left
        right: parent.right
        bottom: parent.bottom
        rightMargin: 7
    }

    ScrollView {
        id: chatScrollView
        clip: true
        //color: Qt.ligher("red",4)
        anchors {
            top: parent.top
            bottom: messageInput.top
            left: parent.left
            right: parent.right
        }
        Layout.fillWidth: true
        Layout.fillHeight: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar {
            id: chatScrollbar
            size: 0.6
            position: 0.2
            active: true
            anchors {
                right: parent.right
                top: parent.top
                bottom: parent.bottom
            }
            orientation: Qt.Vertical
            contentItem: Rectangle {

                implicitWidth: 6
                radius: 5
                implicitHeight: chatScrollView.height
                color: chatScrollbar.pressed ? Qt.darker("grey", 1.5) : "grey"
                opacity: chatScrollbar.policy === ScrollBar.AlwaysOn
                         || (chatScrollbar.active
                             && chatScrollbar.size < 1.0) ? 0.75 : 0
            }
        }

        ColumnLayout {
            width: chatScrollView.width // Ensure the layout uses the full width.
            spacing: 8 // Adds a nice gap between messages.

            MessageBubble { sentByMe: false; messageText: "Hey!" }
            MessageBubble { sentByMe: true; messageText: "Hello there. How's it going?" }
            MessageBubble { sentByMe: false; messageText: "Pretty good. I'm testing the new chat UI." }
            MessageBubble { sentByMe: true; messageText: "Nice! How does it look?" }
            MessageBubble { sentByMe: false; messageText: "Looks great, but I need to test the scrolling with a lot of messages." }
            MessageBubble { sentByMe: true; messageText: "Good idea. Let's fill it up." }
            MessageBubble { sentByMe: false; messageText: "Message 7." }
            MessageBubble { sentByMe: true; messageText: "Message 8." }
            MessageBubble { sentByMe: false; messageText: "Here is a slightly longer message to check how the bubble expands." }
            MessageBubble { sentByMe: true; messageText: "Looks like it's working perfectly. The alignment is correct too." }
            MessageBubble { sentByMe: false; messageText: "11" }
            MessageBubble { sentByMe: true; messageText: "12" }
            MessageBubble { sentByMe: false; messageText: "13" }
            MessageBubble { sentByMe: true; messageText: "14" }
            MessageBubble { sentByMe: false; messageText: "Fifteen." }
            MessageBubble { sentByMe: true; messageText: "This is message number sixteen. I'm just adding some filler content to see how the word wrapping behaves on different screen sizes." }
            MessageBubble { sentByMe: false; messageText: "The wrapping seems correct. The bubble is constrained to 75% of the width." }
            MessageBubble { sentByMe: true; messageText: "Excellent. Performance check: any lag when you scroll fast?" }
            MessageBubble { sentByMe: false; messageText: "A tiny bit, but it's acceptable for a debug build." }
            MessageBubble { sentByMe: true; messageText: "Agreed. Let's keep going." }
            MessageBubble { sentByMe: false; messageText: "This is the twenty-first message." }
            MessageBubble { sentByMe: true; messageText: "Almost halfway there." }
            MessageBubble { sentByMe: false; messageText: "Does it handle special characters? & % $ # @" }
            MessageBubble { sentByMe: true; messageText: "It should, they're just text. & % $ # @ seems fine." }
            MessageBubble { sentByMe: false; messageText: "What about numbers? 1234567890" }
            MessageBubble { sentByMe: true; messageText: "0987654321. Yep." }
            MessageBubble { sentByMe: false; messageText: "Okay, another really long one. This message is designed specifically to push the boundaries of the text wrapping within the message bubble component. We need to be absolutely sure that it doesn't overflow or cause any visual glitches, especially on smaller screens." }
            MessageBubble { sentByMe: true; messageText: "It wrapped perfectly. The layout logic is solid." }
            MessageBubble { sentByMe: false; messageText: "It's almost 1:30 AM, wow." }
            MessageBubble { sentByMe: true; messageText: "Time flies when you're debugging QML. Just ten more." }
            MessageBubble { sentByMe: false; messageText: "31" }
            MessageBubble { sentByMe: true; messageText: "32" }
            MessageBubble { sentByMe: false; messageText: "33" }
            MessageBubble { sentByMe: true; messageText: "Thirty-four." }
            MessageBubble { sentByMe: false; messageText: "Thirty-five!" }
            MessageBubble { sentByMe: true; messageText: "Okay, okay, I get it." }
            MessageBubble { sentByMe: false; messageText: "Are we done yet?" }
            MessageBubble { sentByMe: true; messageText: "Almost..." }
            MessageBubble { sentByMe: false; messageText: "This is the second to last message." }
            MessageBubble { sentByMe: true; messageText: "And this is the final one! Test complete. Scrolling looks good." }
        }
    }

    MessageInputCanvas {
        id: messageInput
        height: 40
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }
}
