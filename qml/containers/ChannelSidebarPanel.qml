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
        id: dmListModel
    }

    property var friendStatusMap: ({})

    function syncFriendsModel() {
        dmListModel.clear();
        var list = (NetworkManager && NetworkManager.friends && NetworkManager.friends.length > 0) ?
                    NetworkManager.friends : ["alex", "beatrice", "charlie"];
        for (var i = 0; i < list.length; ++i) {
            var friendName = list[i].toLowerCase();
            var st = sidebarRoot.friendStatusMap[friendName] || "offline";
            dmListModel.append({
                name: list[i],
                isDM: true,
                userStatus: st
            });

        }
    }

    Connections {
        target: NetworkManager
        function onFriendsChanged() {
            sidebarRoot.syncFriendsModel();
        }
        function onIncomingRelayMessageReceived(fromUsername, text, timestamp) {
            sidebarRoot.friendStatusMap[fromUsername.toLowerCase()] = "online";
            sidebarRoot.syncFriendsModel();
        }
        function onFriendStatusUpdated(username, status) {
            sidebarRoot.friendStatusMap[username.toLowerCase()] = status;
            sidebarRoot.syncFriendsModel();
        }
    }


    Component.onCompleted: syncFriendsModel()



    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.topMargin: 4
            spacing: 8

            Text {
                text: sidebarRoot.selectedServer === "dms" ? "DIRECT MESSAGES" : sidebarRoot.selectedServer.toUpperCase()
                color: ThemeData.textPrimary
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Rectangle {
                id: addFriendBtn
                visible: sidebarRoot.selectedServer === "dms"
                width: 28
                height: 28
                radius: 6
                color: addFriendMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.12) : "transparent"

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/friends.svg"
                    width: 16
                    height: 16
                    color: addFriendMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
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

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: ThemeData.textSecondary
            opacity: 0.2
        }

        ListView {
            id: channelListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: sidebarRoot.selectedServer === "dms" ? dmListModel : serverChannelsModel
            spacing: 4
            clip: true

            ScrollBar.vertical: ScrollBar {
                id: sidebarScrollBar
                parent: channelListView
                anchors.top: channelListView.top
                anchors.right: channelListView.right
                anchors.bottom: channelListView.bottom
                width: 6
                policy: ScrollBar.AsNeeded
                palette.window: "transparent"
                palette.base: "transparent"

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: sidebarScrollBar.pressed ? ThemeData.accentColor : (sidebarScrollBar.hovered ? "#7289DA" : "#4E5058")
                }
                background: Item {}
            }

            delegate: ChannelListItem {
                channelName: model.name
                isDM: model.isDM
                userStatus: model.userStatus
                isSelected: sidebarRoot.activeChannel === model.name
                onClicked: {
                    sidebarRoot.activeChannel = model.name;
                    sidebarRoot.channelSelected(model.name);
                    sidebarRoot.channelChanged(model.name);
                }
            }
        }
    }
}
