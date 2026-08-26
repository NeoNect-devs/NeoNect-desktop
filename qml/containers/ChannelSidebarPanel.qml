import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0
import "../components"

Rectangle {
    id: sidebarRoot

    property string selectedServer: "server1"
    property string activeChannel: "general"

    // Bounds properties
    readonly property real minWidth: 200
    readonly property real maxWidth: 320

    signal channelSelected(string channelName)
    signal channelChanged(string channelName)
    signal addFriendRequested

    // Layout constraints (for RowLayout/ColumnLayout)
    Layout.fillHeight: true
    Layout.minimumWidth: minWidth
    Layout.maximumWidth: maxWidth
    Layout.preferredWidth: 240

    // SplitView constraints (for SplitView parent containers)
    SplitView.minimumWidth: minWidth
    SplitView.maximumWidth: maxWidth
    SplitView.preferredWidth: 240

    // Hard-clamped width calculation
    implicitWidth: 240
    width: Math.min(Math.max(minWidth, implicitWidth), maxWidth)

    // Enforce bounds if width is updated externally or via drag
    onWidthChanged: {
        if (width < minWidth) {
            width = minWidth;
        } else if (width > maxWidth) {
            width = maxWidth;
        }
    }

    color: ThemeData.panelBackground
    border.color: Qt.darker(ThemeData.panelBackground, 1.25)
    border.width: 1

    ListModel {
        id: serverChannelsModel
        ListElement {
            name: "general"
            isDM: false
            userStatus: "online"
        }
        ListElement {
            name: "welcome-rules"
            isDM: false
            userStatus: "online"
        }
        ListElement {
            name: "announcements"
            isDM: false
            userStatus: "online"
        }
    }

    ListModel {
        id: openDmListModel
    }

    property var friendStatusMap: ({
        "alex": "online",
        "beatrice": "online",
        "charlie": "afk",
        "david": "offline",
        "eva": "dnd",
        "frank": "offline",
        "grace": "online",
        "henry": "afk"
    })

    // Active Open DMs tracked in session
    property var openDms: ["alex", "beatrice"]

    function openDirectMessage(username) {
        if (!username) return;
        var lower = username.toLowerCase();
        if (openDms.indexOf(lower) === -1) {
            openDms.push(lower);
        }
        syncDmModel();
        sidebarRoot.activeChannel = lower;
        sidebarRoot.channelSelected(lower);
        sidebarRoot.channelChanged(lower);
    }

    function closeDirectMessage(username) {
        var lower = username.toLowerCase();
        var idx = openDms.indexOf(lower);
        if (idx !== -1) {
            openDms.splice(idx, 1);
            syncDmModel();
            if (sidebarRoot.activeChannel === lower) {
                sidebarRoot.activeChannel = "friends";
                sidebarRoot.channelSelected("friends");
                sidebarRoot.channelChanged("friends");
            }
        }
    }

    function syncDmModel() {
        openDmListModel.clear();
        for (var i = 0; i < openDms.length; ++i) {
            var friendName = openDms[i].toLowerCase();
            var st = sidebarRoot.friendStatusMap[friendName] || "offline";
            openDmListModel.append({
                name: friendName,
                isDM: true,
                userStatus: st
            });
        }
    }

    Connections {
        target: NetworkManager
        function onFriendsChanged() {
            sidebarRoot.syncDmModel();
        }
        function onIncomingRelayMessageReceived(fromUsername, target, text, timestamp) {
            var lower = fromUsername.toLowerCase();
            sidebarRoot.friendStatusMap[lower] = "online";
            if (sidebarRoot.openDms.indexOf(lower) === -1) {
                sidebarRoot.openDms.push(lower);
            }
            sidebarRoot.syncDmModel();
        }
        function onFriendStatusUpdated(username, status) {
            sidebarRoot.friendStatusMap[username.toLowerCase()] = status;
            sidebarRoot.syncDmModel();
        }
    }

    Component.onCompleted: syncDmModel()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ─── PINNED TOP DIRECT MESSAGES NAVIGATION (Friends & Saved Messages) ───
        ColumnLayout {
            visible: sidebarRoot.selectedServer === "dms"
            Layout.fillWidth: true
            spacing: 2

            // 1. Friends Dashboard Navigation
            ChannelListItem {
                channelName: "Friends"
                isSpecialNav: true
                specialType: "friends"
                unreadBadge: 4
                isSelected: sidebarRoot.activeChannel === "friends"
                onClicked: {
                    sidebarRoot.activeChannel = "friends";
                    sidebarRoot.channelSelected("friends");
                    sidebarRoot.channelChanged("friends");
                }
            }

            // 2. Saved Messages (Cloud Bookmark / Notes to Self)
            ChannelListItem {
                channelName: "Saved Messages"
                isSpecialNav: true
                specialType: "saved-messages"
                isSelected: sidebarRoot.activeChannel === "saved-messages"
                onClicked: {
                    sidebarRoot.activeChannel = "saved-messages";
                    sidebarRoot.channelSelected("saved-messages");
                    sidebarRoot.channelChanged("saved-messages");
                }
            }
        }

        // Section Title & Action
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 6
            Layout.topMargin: sidebarRoot.selectedServer === "dms" ? 8 : 4
            spacing: 8

            Text {
                text: sidebarRoot.selectedServer === "dms" ? "DIRECT MESSAGES" : sidebarRoot.selectedServer.toUpperCase()
                color: ThemeData.textSecondary
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 0.5
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Rectangle {
                id: addFriendBtn
                visible: sidebarRoot.selectedServer === "dms"
                width: 24; height: 24
                radius: 4
                color: addFriendMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.12) : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: addFriendMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                    font.pixelSize: 16
                    font.bold: true
                }

                MouseArea {
                    id: addFriendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sidebarRoot.addFriendRequested()
                }
            }
        }

        // List of Active Channels OR Active Open DMs
        ListView {
            id: channelListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: sidebarRoot.selectedServer === "dms" ? openDmListModel : serverChannelsModel
            spacing: 3
            clip: true

            ScrollBar.vertical: ScrollBar {
                id: sidebarScrollBar
                parent: channelListView
                anchors.top: channelListView.top
                anchors.right: channelListView.right
                anchors.bottom: channelListView.bottom
                width: 4
                policy: ScrollBar.AsNeeded
                palette.window: "transparent"
                palette.base: "transparent"

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: ThemeData.scrollBarThumb
                }
                background: Item {}
            }

            delegate: ChannelListItem {
                channelName: model.name
                isDM: model.isDM
                canClose: model.isDM
                userStatus: model.userStatus
                isSelected: sidebarRoot.activeChannel === model.name
                onClicked: {
                    sidebarRoot.activeChannel = model.name;
                    sidebarRoot.channelSelected(model.name);
                    sidebarRoot.channelChanged(model.name);
                }
                onCloseClicked: {
                    sidebarRoot.closeDirectMessage(model.name);
                }
            }
        }
    }
}
