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
    height: 38
    radius: 6
    color: isActive ? Qt.rgba(255, 255, 255, 0.1) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")

    Behavior on color { ColorAnimation { duration: 100 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 10

        Item {
            width: itemRoot.isDM ? 22 : 20
            height: itemRoot.isDM ? 29 : 20
            Layout.alignment: Qt.AlignVCenter

            IconImage {
                visible: !itemRoot.isDM
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                width: 18; height: 18
                color: itemRoot.isActive ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            Item {
                visible: itemRoot.isDM
                anchors.fill: parent

                Rectangle {
                    id: dmAvatarBox
                    width: 22; height: 22
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: 6
                    color: itemRoot.isActive ? ThemeData.accentColor : Qt.rgba(255, 255, 255, 0.12)

                    Text {
                        anchors.centerIn: parent
                        text: itemRoot.channelName ? itemRoot.channelName.charAt(0).toUpperCase() : "@"
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                // Horizontal Status Pill Under DM Avatar
                Rectangle {
                    id: dmStatusPill
                    width: 16
                    height: 5.5
                    radius: 2.75
                    color: {
                        var st = (itemRoot.userStatus || "").toLowerCase();
                        if (st === "online") return "#23A55A";
                        if (st === "afk" || st === "idle") return "#FAA81A";
                        if (st === "dnd") return "#F23F43";
                        return "#80848E";
                    }
                    border.color: ThemeData.panelBackground
                    border.width: 0.8
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                }
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