import QtQuick
Rectangle{
    id:root
    color: "#00000000" //transparent

    anchors
    {
        left: parent.right
        right: parent.right
        bottom: parent.bottom
        top: parent.top
        rightMargin:7
        bottomMargin:10

    }

    TopbarCanvas{
        id:topbar

        height:50
        anchors{
            left: parent.left
            right: parent.right
            top: parent.top
        }

    }

    MemberListCanvas{
        id:memberlist
        width:175
        anchors{
            top: topbar.bottom
            bottom: parent.bottom
            right: parent.right
            //left: chatbox.right
        }
        visible: topbar.memberlistVisibility
    }

    ChatboxCanvas{
        id:chatbox

        anchors{
            left:parent.left
            right: (memberlist.visible)? memberlist.left : parent.right
            rightMargin: (memberlist.visible)? 7 : 0
            top: topbar.bottom
            bottom: parent.bottom
        }
    }
}
