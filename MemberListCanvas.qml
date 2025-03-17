import QtQuick

Rectangle{
    id:root
    width:175

    color: Qt.darker("white",6)
    anchors{
        top: topbar.bottom
        bottom: parent.bottom
        right: parent.right
    }

}
