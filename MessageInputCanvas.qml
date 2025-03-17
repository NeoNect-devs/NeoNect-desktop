import QtQuick

Rectangle{
    id:root
    color:Qt.darker("grey")
    //width: parent.width
    height: 40
    clip: true
    topRightRadius:22
    bottomRightRadius:22
    anchors{
        bottom: parent.bottom
        left: parent.left
        right: parent.right
        margins: 4
    }
    Rectangle{
        id:txtmessageInput
        width: 50
        //height: 20
        color:Qt.darker("grey")
        anchors{
            top:parent.top
            bottom: parent.bottom
            left: parent.left
            right: btnSend.left

        }

    }
    Rectangle{
        id:btnSend
        width: 40
        //height: 20
        color: "lightblue"
        radius:40
        anchors{
            top:parent.top
            bottom: parent.bottom

            right: parent.right
        }

    }
}
