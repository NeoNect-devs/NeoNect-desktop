// SideSection/SidebarCanvas.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: sidebarRoot
    width: 60

    Rectangle {
        anchors.fill: parent
        color: "#101210"
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        // This structural container handles the scroll bounds safely
        Item {
            // We lock this explicitly to the parent canvas dimension of 60
            width: sidebarRoot.width
            height: sidebarColumnTrack.implicitHeight + 76 // matches paddings

            Column {
                id: sidebarColumnTrack
                // We lock the column track width to 60 to guarantee a perfect center axis line
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

                    scale: dmMouseArea.containsMouse || root.currentSelectedServer === "dms" ? 1.15 : 1.0
                    Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Rectangle {
                        id: dmBorderLayer
                        anchors.fill: parent
                        radius: dmMouseArea.containsMouse || root.currentSelectedServer === "dms" ? 14 : 24
                        color: root.currentSelectedServer === "dms" ? "#FFFFFF" : "transparent"
                        Behavior on radius { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: root.currentSelectedServer === "dms" ? 2 : 0
                        radius: dmMouseArea.containsMouse || root.currentSelectedServer === "dms" ? 12 : 24
                        color: root.currentSelectedServer === "dms" ? "#00A36C" : (dmMouseArea.containsMouse ? "#2A2C2A" : "#1E201E")

                        Behavior on radius { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation { duration: 150 } }

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
                            root.currentSelectedServer = "dms"
                            root.currentActiveChannel = "dm-alex"
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

                // ===========================================================
                // CUSTOM SERVER ICON LIST
                // ===========================================================
                Column {
                    spacing: 6
                    width: parent.width

                    // SERVER BUTTON 1
                    Item {
                        width: 48
                        height: 48
                        anchors.horizontalCenter: parent.horizontalCenter
                        property bool hasUnreads: true
                        property int mentionCount: 0

                        Rectangle {
                            id: server1Pillar
                            anchors.right: parent.left
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: root.currentSelectedServer === "server1" ? 32 : (server1Mouse.containsMouse ? 18 : (parent.hasUnreads ? 6 : 0))
                            radius: 2
                            color: "#FFFFFF"
                            visible: root.currentSelectedServer === "server1" || server1Mouse.containsMouse || parent.hasUnreads
                        }

                        Item {
                            anchors.fill: parent
                            scale: server1Mouse.containsMouse || root.currentSelectedServer === "server1" ? 1.15 : 1.0

                            Rectangle {
                                anchors.fill: parent
                                radius: server1Mouse.containsMouse || root.currentSelectedServer === "server1" ? 14 : 24
                                color: root.currentSelectedServer === "server1" ? "#FFFFFF" : "transparent"
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: root.currentSelectedServer === "server1" ? 2 : 0
                                radius: server1Mouse.containsMouse || root.currentSelectedServer === "server1" ? 12 : 24
                                color: root.currentSelectedServer === "server1" ? "#00A36C" : (server1Mouse.containsMouse ? "#2A2C2A" : "#1E201E")

                                Text { anchors.centerIn: parent; text: "A"; color: "white"; font.weight: Font.DemiBold }
                            }
                        }

                        MouseArea {
                            id: server1Mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { root.currentSelectedServer = "server1"; root.currentActiveChannel = "general" }
                        }
                    }

                    // SERVER BUTTON 2
                    Item {
                        width: 48
                        height: 48
                        anchors.horizontalCenter: parent.horizontalCenter
                        property bool hasUnreads: false
                        property int mentionCount: 3

                        Rectangle {
                            id: server2Pillar
                            anchors.right: parent.left
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: root.currentSelectedServer === "server2" ? 32 : (server2Mouse.containsMouse ? 18 : (parent.hasUnreads ? 6 : 0))
                            radius: 2
                            color: "#FFFFFF"
                            visible: root.currentSelectedServer === "server2" || server2Mouse.containsMouse || parent.hasUnreads
                        }

                        Item {
                            anchors.fill: parent
                            scale: server2Mouse.containsMouse || root.currentSelectedServer === "server2" ? 1.15 : 1.0

                            Rectangle {
                                anchors.fill: parent
                                radius: server2Mouse.containsMouse || root.currentSelectedServer === "server2" ? 14 : 24
                                color: root.currentSelectedServer === "server2" ? "#FFFFFF" : "transparent"
                            }

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: root.currentSelectedServer === "server2" ? 2 : 0
                                radius: server2Mouse.containsMouse || root.currentSelectedServer === "server2" ? 12 : 24
                                color: root.currentSelectedServer === "server2" ? "#00A36C" : (server2Mouse.containsMouse ? "#2A2C2A" : "#1E201E")

                                Text { anchors.centerIn: parent; text: "B"; color: "white"; font.weight: Font.DemiBold }
                            }
                        }

                        Rectangle {
                            anchors.top: parent.top; anchors.right: parent.right; anchors.topMargin: -2; anchors.rightMargin: -2
                            width: 16; height: 16; radius: 8; color: "#FF3333"; z: 10
                            visible: parent.mentionCount > 0
                            Text { anchors.centerIn: parent; text: parent.parent.mentionCount.toString(); color: "white"; font.pixelSize: 9; font.weight: Font.Bold }
                        }

                        MouseArea {
                            id: server2Mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { root.currentSelectedServer = "server2"; root.currentActiveChannel = "general" }
                        }
                    }
                }
            }
        }
    }
}
