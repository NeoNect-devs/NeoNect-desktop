import QtQuick
import QtQuick.Controls

Window {
    width: 640
    height: 480
    visible: true
    title: "Avila"
    color: Qt.darker("grey",2)

    SidebarCanvas
    {
        id: sidebar
        width: 67
        anchors{
            top: parent.top
            left: parent.left
            bottom: parent.bottom
        }
    }

    MainPanel{
        id: mainPanel
        anchors
        {
            left: sidebar.right
            right: parent.right
            bottom: parent.bottom
            top: parent.top
            rightMargin:7
            bottomMargin:10

        }
    }
}
