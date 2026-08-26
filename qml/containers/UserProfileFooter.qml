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

    property string userStatus: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "online" : "offline"

    function getStatusColor(st) {
        switch(st) {
            case "online": return "#23A55A"; // Emerald Green
            case "afk": return "#FAA81A";    // Amber Yellow (AFK / Idle)
            case "dnd": return "#F23F43";    // Crimson Red (Do Not Disturb)
            case "offline":
            default: return "#80848E";       // Muted Gray (Offline)
        }
    }

    function getStatusLabel(st) {
        switch(st) {
            case "online": return "Online";
            case "afk": return "Idle / AFK";
            case "dnd": return "Do Not Disturb";
            case "offline":
            default: return "Offline";
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            width: root.sidebarOffsetWidth
            Layout.preferredWidth: root.sidebarOffsetWidth
            Layout.fillHeight: true

            Item {
                id: avatarWrapper
                anchors.centerIn: parent
                width: 34; height: 38

                Rectangle {
                    id: avatarBox
                    width: 34; height: 34
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: 8
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

                // ─── SMALL HORIZONTAL STATUS PILL UNDER PROFILE PIC ───
                Rectangle {
                    id: statusPill
                    width: 22
                    height: 5
                    radius: 2.5
                    color: root.getStatusColor(root.userStatus)
                    border.color: "#0F1110"
                    border.width: 1
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter

                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                MouseArea {
                    id: avatarStatusMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: statusPopup.open()
                }

                // Status selection popup
                Popup {
                    id: statusPopup
                    y: -160
                    x: 6
                    width: 175
                    padding: 6
                    modal: true
                    focus: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                    background: Rectangle {
                        radius: 10
                        color: "#111214"
                        border.color: Qt.rgba(255, 255, 255, 0.12)
                        border.width: 1
                    }

                    contentItem: ColumnLayout {
                        spacing: 3

                        Repeater {
                            model: [
                                { name: "Online", key: "online", color: "#23A55A" },
                                { name: "Idle / AFK", key: "afk", color: "#FAA81A" },
                                { name: "Do Not Disturb", key: "dnd", color: "#F23F43" },
                                { name: "Invisible / Offline", key: "offline", color: "#80848E" }
                            ]

                            Rectangle {
                                Layout.fillWidth: true
                                height: 30
                                radius: 6
                                color: optMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : (root.userStatus === modelData.key ? Qt.rgba(255, 255, 255, 0.04) : "transparent")

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8; anchors.rightMargin: 8
                                    spacing: 8

                                    Rectangle {
                                        width: 8; height: 8
                                        radius: 2.5
                                        color: modelData.color
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: root.userStatus === modelData.key
                                    }
                                }

                                MouseArea {
                                    id: optMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.userStatus = modelData.key;
                                        statusPopup.close();
                                    }
                                }
                            }
                        }
                    }
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
                    text: root.getStatusLabel(root.userStatus)
                    color: root.getStatusColor(root.userStatus)
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