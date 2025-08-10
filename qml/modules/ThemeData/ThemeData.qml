import QtQuick
pragma Singleton
QtObject {
    // This makes it a singleton that's globally available


    // Colors
    readonly property color viewsBackground: "#101210"
    readonly property color inputBackground: "#202225"
    readonly property color myBubble: "#BCC5C9"
    readonly property color othersBubble: "#1D1F1D"
    readonly property color textOnOthersMessage: "#DCDDDE"
    readonly property color textSecondary: "#8e9297"
    readonly property color textOnMyMessage: "black"

    // Fonts
    readonly property int chatFontSize: 14
}
