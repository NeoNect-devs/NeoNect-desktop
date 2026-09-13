// SideSection/SidebarCanvas.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: sidebarRoot
    width: 60

    property string selectedServer: "dms"
    property string activeChannel: "friends"
    signal serverSelected(string serverName)
    signal channelSelected(string channelName)

    Rectangle {
        anchors.fill: parent
        color: ThemeData.sidebarBackground
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        Item {
            width: sidebarRoot.width
            height: sidebarColumnTrack.implicitHeight + 76

            Column {
                id: sidebarColumnTrack
                width: sidebarRoot.width
                topPadding: 16
                bottomPadding: 60
                spacing: 10

                // ===========================================================
                // DYNAMIC DIRECT MESSAGES PILL BUTTON
                // ===========================================================
                Item {
                    id: dmPillContainer
                    width: 48
                    height: 48
                    anchors.horizontalCenter: parent.horizontalCenter

                    scale: dmMouseArea.containsMouse || sidebarRoot.selectedServer === "dms" ? 1.15 : 1.0
                    Behavior on scale {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                    }

                    Rectangle {
                        id: dmBorderLayer
                        anchors.fill: parent
                        radius: dmMouseArea.containsMouse || sidebarRoot.selectedServer === "dms" ? 14 : 12
                        color: sidebarRoot.selectedServer === "dms" ? "#FFFFFF" : "transparent"
                        Behavior on radius {
                            NumberAnimation {
                                duration: 150
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: sidebarRoot.selectedServer === "dms" ? 2 : 0
                        radius: dmMouseArea.containsMouse || sidebarRoot.selectedServer === "dms" ? 12 : 10
                        color: sidebarRoot.selectedServer === "dms" ? "#00A36C" : (dmMouseArea.containsMouse ? "#2A2C2A" : "#1E201E")

                        Behavior on radius {
                            NumberAnimation {
                                duration: 150
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on color {
                            ColorAnimation {
                                duration: 150
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "💬"
                            font.pixelSize: 18
                        }
                    }

                    MouseArea {
                        id: dmMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            sidebarRoot.serverSelected("dms");
                            sidebarRoot.channelSelected("friends");
                        }
                    }
                }

                // MINIMALIST SEPARATOR LINE
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 24
                    height: 1
                    color: "#232523"
                }

                // No servers yet (Backend only supports DMs)
                Column {
                    spacing: 6
                    width: parent.width
                }
            }
        }
    }
}

