// qml/MainSection/MainPanel.qml
import QtQuick
import Avila 1.0

// Relative parent directory lookups step up one level to look back into root folders
import "../entrypage"
import "../SideSection"

Rectangle {
    id: root
    color: "#00000000" // transparent

    // ===========================================================
    // NETWORK STATE CONTEXT ACCESSORS
    // ===========================================================
    // These safely pull the validated credentials down from Main.qml
    readonly property string activeSessionToken: parent ? parent.sessionToken : ""
    readonly property string nodeServerEndpoint: parent ? parent.verifiedServerUrl : ""

    TopbarCanvas {
        id: topbar
        height: 50

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
    }

    MemberListCanvas {
        id: memberlist
        width: 175

        anchors {
            top: topbar.bottom
            bottom: parent.bottom
            right: parent.right
            topMargin: 3
        }
        visible: topbar.memberlistVisibility
    }

    ChatboxCanvas {
        id: chatbox

        anchors {
            left: parent.left
            right: (memberlist.visible) ? memberlist.left : parent.right
            top: topbar.bottom
            bottom: parent.bottom
            topMargin: 3
            rightMargin: (memberlist.visible) ? 3 : 0
        }
    }
}
