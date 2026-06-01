// qml/MainSection/ChatView/MessageBubble.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0

Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: bubbleContainer.height

    property string messageText: "..."
    property bool sentByMe: false

    Rectangle {
        id: bubbleContainer
        anchors.right: root.sentByMe ? parent.right : undefined
        anchors.left: !root.sentByMe ? parent.left : undefined

        width: Math.min(messageLabel.implicitWidth + 24, root.width)
        height: messageLabel.implicitHeight + 16
        radius: 12

        // Solid background boundary fallback protection layer
        color: root.sentByMe ? "#2A2C2A" : "#1E201E"

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: root.sentByMe ? ThemeData.myBubbleStartGradient : ThemeData.othersBubbleStartGradient
            }
            GradientStop {
                position: 1.0
                color: root.sentByMe ? ThemeData.myBubbleEndGradient : ThemeData.othersBubbleEndGradient
            }
        }

        Text {
            id: messageLabel
            text: root.messageText
            color: root.sentByMe ? ThemeData.ownMessageForeground : ThemeData.othersMessageForeground
            font.family: "Segoe UI"
            font.pixelSize: ThemeData.chatFontSize

            width: bubbleContainer.width - 24
            wrapMode: Text.Wrap
            anchors.centerIn: parent
        }
    }
}
