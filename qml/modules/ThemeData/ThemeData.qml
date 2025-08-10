import QtQuick
pragma Singleton
QtObject {
    // This makes it a singleton that's globally available


    // Colors
    readonly property color mainWindowBackground: "#1F1F1F"
    readonly property color viewsBackground: "#101210"
    readonly property color inputBackground: "#202225"

    readonly property string myBubbleStartGradient: "rgba(197, 207, 211, 1.0)"
    readonly property string myBubbleEndGradient: "rgba(197, 207, 211, 0.75)"
    readonly property string othersBubbleStartGradient: "rgba(31, 32, 31, 0.4)"
    readonly property string othersBubbleEndGradient: "rgba(31, 32, 31, 1.0)"

    readonly property color ownMessageForeground: "#222"
    readonly property color othersMessageForeground: "#eee"

    readonly property color textOnOthersMessage: "#DCDDDE"
    readonly property color textSecondary: "#8e9297"
    readonly property color textOnMyMessage: "black"

    // Fonts
    readonly property int chatFontSize: 14
}
