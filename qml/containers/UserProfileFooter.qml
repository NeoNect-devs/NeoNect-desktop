import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila 1.0
import Avila.Core 1.0
import "../components"

Rectangle {
    id: root
    property int sidebarOffsetWidth: 60
    property int channelOffsetWidth: 240

    width: sidebarOffsetWidth + channelOffsetWidth
    height: 52
    z: 10
    color: "#0F1110"
    topLeftRadius: 12; topRightRadius: 12
    border.color: "#232523"; border.width: 1

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            width: root.sidebarOffsetWidth
            Layout.preferredWidth: root.sidebarOffsetWidth
            Layout.fillHeight: true

            Item {
                anchors.centerIn: parent
                width: 34; height: 34

                Rectangle {
                    anchors.fill: parent
                    radius: 17
                    color: ThemeData.accentColor

                    Text {
                        anchors.centerIn: parent
                        text: (NetworkManager && NetworkManager.currentUsername && NetworkManager.currentUsername !== "") ?
                              NetworkManager.currentUsername.charAt(0).toUpperCase() : "A"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 15
                    }
                }

                // Online / Offline Status Badge Dot
                Rectangle {
                    width: 10; height: 10
                    radius: 5
                    color: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "#23A55A" : "#80848E"
                    border.color: "#0F1110"
                    border.width: 1.5
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 4; Layout.rightMargin: 10

            ColumnLayout {
                Layout.fillWidth: true; spacing: 0
                Text {
                    text: (NetworkManager && NetworkManager.currentUsername && NetworkManager.currentUsername !== "") ?
                          NetworkManager.currentUsername : "Guest User"
                    color: ThemeData.textPrimary
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "Online" : "Offline"
                    color: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "#23A55A" : "#80848E"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }


            Row {
                spacing: 2

                Rectangle {
                    width: 28; height: 28; radius: 6
                    color: friendsM.containsMouse ? "#1E201E" : "transparent"
                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Avila/assets/icons/friends.svg"
                        width: 16; height: 16
                        color: friendsM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                    }
                    MouseArea { id: friendsM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                }

                Rectangle {
                    width: 28; height: 28; radius: 6
                    color: micM.containsMouse ? "#1E201E" : "transparent"
                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Avila/assets/icons/mic.svg"
                        width: 16; height: 16
                        color: micM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                    }
                    MouseArea { id: micM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                }

                Rectangle {
                    width: 28; height: 28; radius: 6
                    color: deafenM.containsMouse ? "#1E201E" : "transparent"
                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Avila/assets/icons/headphones.svg"
                        width: 16; height: 16
                        color: deafenM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                    }
                    MouseArea { id: deafenM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                }

                Rectangle {
                    width: 28; height: 28; radius: 6
                    color: setM.containsMouse ? "#1E201E" : "transparent"
                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Avila/assets/icons/settings.svg"
                        width: 16; height: 16
                        color: setM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                    }
                    MouseArea { id: setM; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                }
            }
        }
    }
}