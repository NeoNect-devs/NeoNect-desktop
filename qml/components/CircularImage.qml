// CircularImage.qml
import QtQuick

Item {
    id: root
    width: 32
    height: 32

    property string source: ""
    property int cornerRadius: 8

    // Native architectural layout mask boundary box running directly on your GPU pipeline
    Rectangle {
        id: maskContainer
        anchors.fill: parent
        radius: root.cornerRadius
        color: "#2F3136" // Fallback placeholder color layer
        clip: true

        Image {
            id: imageLoader
            anchors.fill: parent
            source: root.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true

            // Smoothes visual sampling scaling artifact lines
            mipmap: true
        }
    }
}
