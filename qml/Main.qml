import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Avila 1.0

Window {
    width: 640
    height: 480
    visible: true
    title: "Avila"
    color: ThemeData.mainWindowBackground

    RowLayout {
        anchors.fill: parent

        SidebarCanvas {
            id: sidebar
            Layout.preferredWidth: 67
            Layout.fillHeight: true
            visible: false
        }

        MainPanel {
            id: mainPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 6

        }
    }
}
