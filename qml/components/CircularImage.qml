import QtQuick
import QtQuick.Effects
import "UIHelpers.js" as UIHelpers

Item {
    id: root

    property string source: ""
    property real cornerRadius: 8
    property int fillMode: Image.PreserveAspectCrop
    readonly property bool ready: imageLoader.status === Image.Ready && root.source !== ""

    Rectangle {
        id: maskRect
        anchors.fill: parent
        radius: root.cornerRadius
        color: "black"
        visible: false
        layer.enabled: true
    }

    Image {
        id: imageLoader
        anchors.fill: parent
        source: UIHelpers.formatMediaSource(root.source)
        fillMode: root.fillMode
        sourceSize.width: 96
        sourceSize.height: 96
        asynchronous: true
        mipmap: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: imageLoader
        maskEnabled: true
        maskSource: maskRect
        visible: root.ready
    }
}
