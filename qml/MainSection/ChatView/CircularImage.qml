// CircularImage.qml
import QtQuick

// This component now displays any given image source as a rounded square.
Item {
    id: root
    width: 32
    height: 32

    // --- Properties ---
    property alias source: imageLoader.source
    // You can control the corner radius from outside if you want.
    property int cornerRadius: 8

    // We use a separate, non-visible Image item just to load the picture.
    Image {
        id: imageLoader
        visible: false
        onStatusChanged: {
            if (imageLoader.status === Image.Ready) {
                canvas.requestPaint();
            }
        }
    }

    // The Canvas item is where the actual drawing happens.
    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (imageLoader.status === Image.Ready) {
                ctx.save();

                // --- FIX: Create a rounded rectangle clipping path ---
                ctx.beginPath();
                ctx.moveTo(cornerRadius, 0);
                ctx.lineTo(width - cornerRadius, 0);
                ctx.arcTo(width, 0, width, cornerRadius, cornerRadius);
                ctx.lineTo(width, height - cornerRadius);
                ctx.arcTo(width, height, width - cornerRadius, height, cornerRadius);
                ctx.lineTo(cornerRadius, height);
                ctx.arcTo(0, height, 0, height - cornerRadius, cornerRadius);
                ctx.lineTo(0, cornerRadius);
                ctx.arcTo(0, 0, cornerRadius, 0, cornerRadius);
                ctx.closePath();

                // Apply the clipping path.
                ctx.clip();

                // Draw the loaded image onto the canvas.
                ctx.drawImage(imageLoader, 0, 0, width, height);

                ctx.restore();
            }
        }
    }
}
