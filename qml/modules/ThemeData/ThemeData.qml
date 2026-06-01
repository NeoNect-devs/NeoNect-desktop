// qml/modules/ThemeData/ThemeData.qml
import QtQuick

pragma Singleton
QtObject {
    // Colors
    readonly property color mainWindowBackground: "#1F1F1F"
    readonly property color viewsBackground: "#101210"
    readonly property color inputBackground: "#202225"

    // FIX: Swapped type declarations from 'string' to native 'color'
    readonly property color myBubbleStartGradient: Qt.rgba(1.0, 1.0, 1.0, 0.15) // Clean white tint overlay
    readonly property color myBubbleEndGradient: Qt.rgba(1.0, 1.0, 1.0, 0.05)
    readonly property color othersBubbleStartGradient: "#1E201E"
    readonly property color othersBubbleEndGradient: "#161816"

    readonly property color ownMessageForeground: "#FFFFFF"
    readonly property color othersMessageForeground: "#DCDDDE"

    readonly property color textOnOthersMessage: "#DCDDDE"
    readonly property color textSecondary: "#8e9297"
    readonly property color textOnMyMessage: "white"

    // Fonts
    readonly property int chatFontSize: 14
}
