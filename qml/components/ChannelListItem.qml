import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: itemRoot

    property string channelName: ""
    property bool isActive: false
    property alias isSelected: itemRoot.isActive
    property bool isDM: false
    property string userStatus: "offline"
    property string avatarUrl: ""

    signal clicked()

    width: ListView.view ? ListView.view.width : (parent ? parent.width : 200)
    height: 36
    radius: 6
    color: isActive ? Qt.rgba(255, 255, 255, 0.1) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")

    Behavior on color { ColorAnimation { duration: 100 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 10

        Item {
            width: 20; height: 20
            Layout.alignment: Qt.AlignVCenter

            IconImage {
                visible: !itemRoot.isDM
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                width: 18; height: 18
                color: itemRoot.isActive ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            Rectangle {
                visible: itemRoot.isDM
                anchors.centerIn: parent
                width: 10; height: 10
                radius: 5
                color: itemRoot.userStatus === "online" ? "#23A55A" : "#80848E"
            }
        }

        Text {
            Layout.fillWidth: true
            text: itemRoot.channelName
            color: itemRoot.isActive ? ThemeData.textPrimary : ThemeData.textSecondary
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: itemRoot.isActive ? Font.Bold : Font.Normal
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: itemRoot.clicked()
    }
}