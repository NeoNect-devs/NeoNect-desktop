// MessageBubble.qml
import QtQuick
import QtQuick.Layouts

// The root Item now fills the width of its column in the parent layout.
// This provides a stable width for the bubble inside to be positioned against.
Item {
    id: root
    Layout.fillWidth: true
    // *** FIX: Use implicitHeight to notify the parent layout of size changes. ***
        // ColumnLayout is designed to work with implicitHeight, not height.
    implicitHeight: bubble.height

    // --- Public Properties ---
    property string messageText: "..."
    property bool sentByMe: false

    // --- Visuals ---
    Rectangle {
        id: bubble

        // --- FIX: Anchor the bubble to the right or left within the root Item. ---
        anchors.right: root.sentByMe ? parent.right : undefined
        anchors.left: !root.sentByMe ? parent.left : undefined

        // The bubble's width is calculated based on its text content, but is
        // constrained by the root Item's width (which is stable).
        width: Math.min(messageLabel.implicitWidth + 24, root.width)
        // The bubble's height is determined by the actual height of the wrapped text.
        height: messageLabel.height + 16

        color: root.sentByMe ? "#BCC5C9" : "#1D1F1D"
        radius: 12

        Text {
            id: messageLabel
            text: root.messageText
            color: root.sentByMe ? "black" : "#DCDDDE"
            font.pixelSize: 14

            // The Text's width is explicitly constrained by the bubble's final width.
            // This binding is what makes the text wrap correctly.
            width: bubble.width - 24
            wrapMode: Text.Wrap
            anchors.centerIn: parent
        }
    }
}
