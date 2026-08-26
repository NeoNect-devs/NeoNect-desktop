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
    property bool isSpecialNav: false
    property string specialType: "" // "friends", "saved-messages"
    property int unreadBadge: 0
    property bool canClose: false

    signal clicked()
    signal closeClicked()

    width: ListView.view ? ListView.view.width : (parent ? parent.width : 200)
    height: itemRoot.isSpecialNav ? 40 : (itemRoot.isDM ? 48 : 36)
    radius: 8
    color: isActive ? Qt.rgba(255, 255, 255, 0.1) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")

    Behavior on color { ColorAnimation { duration: 100 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 10

        // 1. Icon / Avatar Section
        Item {
            width: itemRoot.isSpecialNav ? 28 : (itemRoot.isDM ? 34 : 20)
            height: itemRoot.isSpecialNav ? 28 : (itemRoot.isDM ? 40 : 20)
            Layout.alignment: Qt.AlignVCenter

            // Special Nav Item: Friends
            Rectangle {
                visible: itemRoot.isSpecialNav && itemRoot.specialType === "friends"
                anchors.fill: parent
                radius: 6
                color: itemRoot.isActive ? Qt.rgba(10, 132, 255, 0.25) : "transparent"

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/friends.svg"
                    width: 20; height: 20
                    color: itemRoot.isActive ? "#0A84FF" : (mouseArea.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary)
                }
            }

            // Special Nav Item: Saved Messages
            Rectangle {
                visible: itemRoot.isSpecialNav && itemRoot.specialType === "saved-messages"
                anchors.fill: parent
                radius: 6
                color: itemRoot.isActive ? Qt.rgba(0, 229, 255, 0.25) : Qt.rgba(0, 229, 255, 0.1)

                Text {
                    anchors.centerIn: parent
                    text: "🔖"
                    font.pixelSize: 15
                }
            }

            // Server Channel Hash Icon
            IconImage {
                visible: !itemRoot.isSpecialNav && !itemRoot.isDM
                anchors.centerIn: parent
                source: "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                width: 18; height: 18
                color: itemRoot.isActive ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            // Direct Message Squircle Avatar with Status Pill
            Item {
                visible: !itemRoot.isSpecialNav && itemRoot.isDM
                anchors.fill: parent

                Rectangle {
                    id: dmAvatarBox
                    width: 34; height: 34
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    radius: 8
                    color: itemRoot.isActive ? ThemeData.accentColor : Qt.rgba(10, 132, 255, 0.25)
                    border.color: itemRoot.isActive ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.08)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: itemRoot.channelName ? itemRoot.channelName.charAt(0).toUpperCase() : "@"
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 15
                    }
                }

                // Horizontal Status Pill Under DM Avatar
                Rectangle {
                    id: dmStatusPill
                    width: 24
                    height: 7
                    radius: 3.5
                    color: {
                        var st = (itemRoot.userStatus || "").toLowerCase();
                        if (st === "online") return "#23A55A";
                        if (st === "afk" || st === "idle") return "#FAA81A";
                        if (st === "dnd") return "#F23F43";
                        return "#80848E";
                    }
                    border.color: ThemeData.panelBackground
                    border.width: 1.2
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // 2. Labels & Subtitles
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Layout.alignment: Qt.AlignVCenter

            Text {
                Layout.fillWidth: true
                text: itemRoot.isSpecialNav ? (itemRoot.specialType === "friends" ? "Friends" : "Saved Messages") : (itemRoot.isDM ? itemRoot.channelName.replace(/^\w/, c => c.toUpperCase()) : itemRoot.channelName)
                color: itemRoot.isActive ? ThemeData.textPrimary : (mouseArea.containsMouse ? "#FFFFFF" : ThemeData.textSecondary)
                font.family: "Segoe UI"
                font.pixelSize: 14
                font.weight: itemRoot.isActive ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }

            Text {
                visible: !itemRoot.isSpecialNav && itemRoot.isDM
                Layout.fillWidth: true
                text: {
                    var st = (itemRoot.userStatus || "").toLowerCase();
                    if (st === "online") return "Online";
                    if (st === "afk" || st === "idle") return "Idle / AFK";
                    if (st === "dnd") return "Do Not Disturb";
                    return "Offline";
                }
                color: {
                    var st = (itemRoot.userStatus || "").toLowerCase();
                    if (st === "online") return "#23A55A";
                    if (st === "afk" || st === "idle") return "#FAA81A";
                    if (st === "dnd") return "#F23F43";
                    return ThemeData.textMuted;
                }
                font.family: "Segoe UI"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // 3. Online Badge Count for Friends Tab
        Rectangle {
            visible: itemRoot.isSpecialNav && itemRoot.specialType === "friends" && itemRoot.unreadBadge > 0
            height: 18
            radius: 9
            color: "#23A55A"
            implicitWidth: Math.max(18, badgeText.implicitWidth + 8)

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: itemRoot.unreadBadge.toString()
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.bold: true
            }
        }

        // 4. Hover Close Button for Open DMs
        Rectangle {
            visible: itemRoot.isDM && itemRoot.canClose && (mouseArea.containsMouse || closeMouse.containsMouse)
            width: 20; height: 20
            radius: 10
            color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMouse.containsMouse ? "#FFFFFF" : "#949BA4"
                font.pixelSize: 10
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: itemRoot.closeClicked()
            }
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