import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

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

    Layout.fillWidth: true
    width: ListView.view ? ListView.view.width : (parent ? parent.width : 200)
    implicitWidth: 200
    height: itemRoot.isSpecialNav ? 40 : (itemRoot.isDM ? 48 : 36)
    radius: 8
    color: isActive ? Qt.rgba(255, 255, 255, 0.1) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent")

    Behavior on color { ColorAnimation { duration: 100 } }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: itemRoot.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 10

        // ─── 1. ICON / AVATAR CONTAINER ───
        Item {
            width: itemRoot.isSpecialNav ? 32 : (itemRoot.isDM ? 34 : 20)
            height: itemRoot.isSpecialNav ? 32 : (itemRoot.isDM ? 40 : 20)
            Layout.alignment: Qt.AlignVCenter

            // Special Nav: Friends Icon Box
            Rectangle {
                visible: itemRoot.isSpecialNav && itemRoot.specialType === "friends"
                anchors.fill: parent
                radius: 8
                color: itemRoot.isActive ? Qt.rgba(10, 132, 255, 0.25) : (mouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(255, 255, 255, 0.04))

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/NeoNect/assets/icons/friends.svg"
                    width: 18; height: 18
                    color: itemRoot.isActive ? "#0A84FF" : (mouseArea.containsMouse ? "#FFFFFF" : "#949BA4")
                }
            }

            // Special Nav: Saved Messages Vector Bookmark Box
            Rectangle {
                visible: itemRoot.isSpecialNav && itemRoot.specialType === "saved-messages"
                anchors.fill: parent
                radius: 8
                color: itemRoot.isActive ? Qt.rgba(0, 229, 255, 0.25) : (mouseArea.containsMouse ? Qt.rgba(0, 229, 255, 0.12) : Qt.rgba(0, 229, 255, 0.06))

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bookmark.svg"
                    width: 18; height: 18
                    color: itemRoot.isActive ? "#00E5FF" : (mouseArea.containsMouse ? "#00E5FF" : "#80848E")
                }
            }

            // Server Channel: Hash Icon
            IconImage {
                visible: !itemRoot.isSpecialNav && !itemRoot.isDM
                anchors.centerIn: parent
                source: "qrc:/qt/qml/NeoNect/assets/icons/hash.svg"
                width: 18; height: 18
                color: itemRoot.isActive ? ThemeData.textPrimary : ThemeData.textSecondary
            }

            // DM Contact: Squircle Avatar with Status Pill
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

                    CircularImage {
                        id: dmAvatarImg
                        anchors.fill: parent
                        source: itemRoot.isDM && (typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.getAvatarUrl(itemRoot.channelName) : ""
                        cornerRadius: 8
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !dmAvatarImg.ready
                        text: itemRoot.isDM && (typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.getDisplayName(itemRoot.channelName).charAt(0).toUpperCase() : (itemRoot.channelName ? itemRoot.channelName.charAt(0).toUpperCase() : "@")
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

        // ─── 2. CHANNEL / DM NAME LABEL ───
        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: itemRoot.isSpecialNav ? (itemRoot.specialType === "friends" ? "Friends" : "Saved Messages") : (itemRoot.isDM && (typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.getDisplayName(itemRoot.channelName) : (itemRoot.isDM ? itemRoot.channelName.replace(/^\w/, c => c.toUpperCase()) : itemRoot.channelName))
            color: itemRoot.isActive ? "#FFFFFF" : (mouseArea.containsMouse ? "#FFFFFF" : ThemeData.textSecondary)
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: itemRoot.isActive ? Font.DemiBold : Font.Normal
            elide: Text.ElideRight
        }

        // ─── 3. UNREAD MESSAGE COUNT BADGE ───
        Rectangle {
            visible: itemRoot.unreadBadge > 0
            height: 18
            radius: 9
            color: (itemRoot.isSpecialNav && itemRoot.specialType === "friends") ? "#23A55A" : "#0A84FF"
            implicitWidth: Math.max(18, badgeText.implicitWidth + 8)
            Layout.alignment: Qt.AlignVCenter

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: itemRoot.unreadBadge > 99 ? "99+" : itemRoot.unreadBadge.toString()
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 10
                font.bold: true
            }
        }

        // ─── 4. HOVER CLOSE BUTTON (FOR OPEN DMS) ───
        Rectangle {
            visible: itemRoot.isDM && itemRoot.canClose && (mouseArea.containsMouse || closeMouse.containsMouse)
            width: 20; height: 20
            radius: 10
            color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"
            Layout.alignment: Qt.AlignVCenter

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
                z: 10
                onClicked: function(mouse) {
                    mouse.accepted = true;
                    itemRoot.closeClicked();
                }
            }
        }
    }
}