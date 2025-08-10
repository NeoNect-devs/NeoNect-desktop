// MessageBubble.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Avila 1.0

Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: bubble.height

    property string messageText: "..."
    property bool sentByMe: false

    // The root is now an Item containing a Canvas for drawing.
    Item {
        id: bubble
        anchors.right: root.sentByMe ? parent.right : undefined
        anchors.left: !root.sentByMe ? parent.left : undefined

        width: Math.min(messageLabel.implicitWidth + 24, root.width)
        height: messageLabel.height + 16

        // --- FIX: Use a Canvas for robust custom drawing ---
        Canvas {
            id: canvas
            anchors.fill: parent

            // Redraw the canvas whenever its size changes.
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.save();

                // 1. Create the rounded rectangle path.
                ctx.beginPath();
                ctx.moveTo(12, 0);
                ctx.lineTo(width - 12, 0);
                ctx.arcTo(width, 0, width, 12, 12);
                ctx.lineTo(width, height - 12);
                ctx.arcTo(width, height, width - 12, height, 12);
                ctx.lineTo(12, height);
                ctx.arcTo(0, height, 0, height - 12, 12);
                ctx.lineTo(0, 12);
                ctx.arcTo(0, 0, 12, 0, 12);
                ctx.closePath();

                // 2. Define the gradient fill style.
                var gradient = ctx.createLinearGradient(0, 0, width, 0); // 90deg angle
                if (root.sentByMe) {
                    gradient.addColorStop(0, ThemeData.myBubbleStartGradient);
                    gradient.addColorStop(1, ThemeData.myBubbleEndGradient);
                } else {
                    gradient.addColorStop(0, ThemeData.othersBubbleStartGradient);
                    gradient.addColorStop(1, ThemeData.othersBubbleEndGradient);
                }
                ctx.fillStyle = gradient;

                // 3. Fill the path with the gradient.
                ctx.fill();
                ctx.restore();
            }
        }

        // The Text is a sibling to the Canvas, placed on top.
        Text {
            id: messageLabel
            text: root.messageText
            color: root.sentByMe ? ThemeData.ownMessageForeground : ThemeData.othersMessageForeground
            font.pixelSize: ThemeData.chatFontSize

            width: bubble.width - 24
            wrapMode: Text.Wrap
            anchors.centerIn: parent
        }
    }
}
