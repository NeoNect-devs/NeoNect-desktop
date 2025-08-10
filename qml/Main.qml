import QtQuick
import QtQuick.Controls
import Avila 1.0
Window {
    width: 640
    height: 480
    visible: true
    title: "Avila"
    color: "#1F1F1F"

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
