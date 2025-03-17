import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
Rectangle{
    id:root
    color: Qt.darker("white",6)

     anchors{
        top: parent.bottom
        left: parent.left
        right: parent.right
        bottom: parent.bottom
        rightMargin: 7
    }
    ScrollView{
        id:chatScrollView
        //color: Qt.ligher("red",4)
        anchors{
            top: parent.top
            bottom: messageInput.top
            left:parent.left
            right: parent.right
        }
        //ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical: ScrollBar{
            id:chatScrollbar
            size: 0.6
            position: 0.2
            active: true
            anchors{
                right:parent.right
                top:parent.top
                bottom: parent.bottom
            }
            orientation: Qt.Vertical
            contentItem: Rectangle{

                implicitWidth: 6
                radius: 5
                implicitHeight: chatScrollView.height
                color: chatScrollbar.pressed ? Qt.darker("grey",1.5): "grey"
                opacity: chatScrollbar.policy === ScrollBar.AlwaysOn || (chatScrollbar.active && chatScrollbar.size < 1.0) ? 0.75 : 0
            }
        }

        Label{
            text: "ABC\nDEF\nGHI\nJKL"
            font.pixelSize: 160
        }


    }
    MessageInputCanvas{
        id:messageInput
        height: 40
        anchors{
            bottom: parent.bottom
            left: parent.left
            right: parent.right

        }
    }

}
