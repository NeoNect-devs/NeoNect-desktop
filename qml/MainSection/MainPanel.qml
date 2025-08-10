import QtQuick
import Avila 1.0
Rectangle{
    id:root
    color: "#00000000" //transparent

    TopbarCanvas{
        id:topbar

        height:50

        anchors{
            left: parent.left
            right: parent.right
            top: parent.top
            //bottom: chatbox.top

        }

    }

    MemberListCanvas{
        id:memberlist
        width:175
        anchors{
            top: topbar.bottom
            bottom: parent.bottom
            right: parent.right
            // left: chatbox.right
            topMargin: 3
        }
        visible: topbar.memberlistVisibility
    }

    ChatboxCanvas{
        id:chatbox

        anchors{
            left:parent.left
            right: (memberlist.visible)? memberlist.left : parent.right
            top: topbar.bottom
            bottom: parent.bottom
            topMargin: 3
            rightMargin: (memberlist.visible)? 3 : 0
        }
    }
}
