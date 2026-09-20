// qml/Main.qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import NeoNect.Core 1.0

Window {
    id: root
    color: "transparent"
    width: 850; height: 640
    minimumWidth: 800; minimumHeight: 560
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint

    property string appState: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "authenticated" : "gateway"
    property string activeTitleText: "Server Connection"
    property string currentActiveChannel: "friends"
    property string currentSelectedServer: "dms"

    Connections {
        target: NetworkManager
        ignoreUnknownSignals: true
        function onTokenChanged() {
            if (NetworkManager && NetworkManager.token && NetworkManager.token !== "") {
                root.appState = "authenticated";
            } else {
                root.appState = "gateway";
            }
        }
    }

    onAppStateChanged: {
        if (typeof viewFlowLoader !== "undefined") {
            if (appState === "gateway") {
                viewFlowLoader.sourceComponent = null;
                viewFlowLoader.source = "";
                viewFlowLoader.source = "entrypage/entry.qml";
            } else if (appState === "authenticated") {
                viewFlowLoader.source = "";
                viewFlowLoader.sourceComponent = chatDashboardComponent;
            }
        }
    }

    Rectangle {
        id: windowBackground
        anchors.fill: parent
        color: ThemeData.windowBackground
        radius: (root.visibility === Window.Maximized || root.visibility === Window.FullScreen) ? 0 : 10
        clip: true

        WindowTitleBar {
            id: titleBar
            windowTarget: root
            appState: root.appState
            titleText: root.activeTitleText
            showBackButton: (root.appState === "gateway" && viewFlowLoader.item) ? viewFlowLoader.item.showTitleBackButton : false
            anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right

            onBackClicked: {
                if (root.appState === "gateway" && viewFlowLoader.item) {
                    viewFlowLoader.item.goBack();
                }
            }

            onBrandClicked: {
                settingsModal.open();
            }

            onNavigateToChat: (channel) => {
                if (channel && channel !== "") {
                    root.currentSelectedServer = "dms";
                    root.currentActiveChannel = channel.toLowerCase();
                    if (typeof channelsPanel !== "undefined" && channelsPanel && channelsPanel.openDirectMessage) {
                        channelsPanel.openDirectMessage(channel.toLowerCase());
                    }
                    if (typeof mainPanel !== "undefined" && mainPanel && mainPanel.focusMessageInput) {
                        mainPanel.focusMessageInput();
                    }
                }
            }
        }

        Loader {
            id: viewFlowLoader
            anchors.top: titleBar.bottom; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            source: root.appState === "gateway" ? "entrypage/entry.qml" : ""
            sourceComponent: root.appState === "authenticated" ? chatDashboardComponent : null



            Connections {
                target: viewFlowLoader.item
                ignoreUnknownSignals: true
                function onRequestTitleChange(newTitle) { root.activeTitleText = newTitle }
                function onAuthenticationSuccess(token, serverUrl) {
                    root.appState = "authenticated"
                }
            }
        }

        Component {
            id: chatDashboardComponent
            Item {
                RowLayout {
                    anchors.fill: parent; spacing: 0
                    SidebarCanvas {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 60
                        selectedServer: root.currentSelectedServer
                        activeChannel: root.currentActiveChannel
                        onServerSelected: (srv) => root.currentSelectedServer = srv
                        onChannelSelected: (chan) => root.currentActiveChannel = chan
                    }


                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        color: "transparent"; border.color: "#232523"; border.width: 1

                        SplitView {
                            anchors.fill: parent; anchors.margins: 1; orientation: Qt.Horizontal
                            handle: Rectangle { implicitWidth: 3; color: ThemeData.windowBackground }

                            ChannelSidebarPanel {
                                id: channelsPanel
                                selectedServer: root.currentSelectedServer
                                activeChannel: root.currentActiveChannel
                                onChannelSelected: function(chan) {
                                    if (chan && chan !== "") {
                                        root.currentActiveChannel = chan;
                                    }
                                }
                                onChannelChanged: function(chan) {
                                    if (chan && chan !== "") {
                                        root.currentActiveChannel = chan;
                                    }
                                }
                                onAddFriendRequested: mainPanel.showAddFriendModal = true
                            }
                            MainPanel {
                                id: mainPanel
                                selectedServer: root.currentSelectedServer
                                activeChannel: root.currentActiveChannel
                                onNavigateRequested: (srv, chan) => {
                                    root.currentSelectedServer = srv;
                                    if (chan && chan !== "") {
                                        root.currentActiveChannel = chan;
                                        if (srv === "dms" && chan !== "friends" && chan !== "saved-messages" && typeof channelsPanel !== "undefined" && channelsPanel && channelsPanel.openDirectMessage) {
                                            channelsPanel.openDirectMessage(chan);
                                        }
                                    }
                                }
                                onOpenDirectMessageRequested: (username) => {
                                    if (username && username !== "") {
                                        root.currentSelectedServer = "dms";
                                        channelsPanel.openDirectMessage(username);
                                        root.currentActiveChannel = username.toLowerCase();
                                    }
                                }
                                onOpenMediaModalRequested: (url, type, name) => {
                                    globalLightboxModal.open(url, type, name);
                                }
                                SplitView.fillWidth: true
                                Layout.fillHeight: true
                            }

                        }
                    }
                }
                UserProfileFooter {
                    anchors.bottom: parent.bottom; anchors.left: parent.left
                    channelOffsetWidth: channelsPanel.width
                }
            }
        }

        // Framework Frameless Window Geometry Sizing hitboxes
        Item {
            id: resizeHitboxes
            anchors.fill: parent
            z: 1000
            visible: root.visibility !== Window.Maximized && root.visibility !== Window.FullScreen

            // Corner Hitboxes (14x14 pixels, z: 2 over edges)
            MouseArea {
                width: 14; height: 14
                anchors.left: parent.left; anchors.top: parent.top
                cursorShape: Qt.SizeFDiagCursor
                z: 2
                onPressed: root.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
            }
            MouseArea {
                width: 14; height: 14
                anchors.right: parent.right; anchors.top: parent.top
                cursorShape: Qt.SizeBDiagCursor
                z: 2
                onPressed: root.startSystemResize(Qt.RightEdge | Qt.TopEdge)
            }
            MouseArea {
                width: 14; height: 14
                anchors.left: parent.left; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeBDiagCursor
                z: 2
                onPressed: root.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
            }
            MouseArea {
                width: 14; height: 14
                anchors.right: parent.right; anchors.bottom: parent.bottom
                cursorShape: Qt.SizeFDiagCursor
                z: 2
                onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }

            // Edge Hitboxes (6 pixels with 14px margins so corners take precedence)
            MouseArea {
                width: 6
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.topMargin: 14; anchors.bottomMargin: 14
                cursorShape: Qt.SizeHorCursor
                z: 1
                onPressed: root.startSystemResize(Qt.LeftEdge)
            }
            MouseArea {
                width: 6
                anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                anchors.topMargin: 14; anchors.bottomMargin: 14
                cursorShape: Qt.SizeHorCursor
                z: 1
                onPressed: root.startSystemResize(Qt.RightEdge)
            }
            MouseArea {
                height: 6
                anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 14; anchors.rightMargin: 14
                cursorShape: Qt.SizeVerCursor
                z: 1
                onPressed: root.startSystemResize(Qt.TopEdge)
            }
            MouseArea {
                height: 6
                anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 14; anchors.rightMargin: 14
                cursorShape: Qt.SizeVerCursor
                z: 1
                onPressed: root.startSystemResize(Qt.BottomEdge)
            }
        }

        // Global Media Fullscreen Lightbox Modal (Covers entire application)
        MediaLightboxModal {
            id: globalLightboxModal
            anchors.fill: parent
            z: 999999
        }

        // NeoNect Settings & Profile Center Modal (Launched by NeoNectBrandButton)
        SettingsProfileModal {
            id: settingsModal
            onLogoutRequested: {
                NetworkManager.logout();
                root.appState = "gateway";
            }
            onSendTestNotificationRequested: {
                if (typeof NotificationManager === "undefined" || !NotificationManager) {
                    globalNotifStack.showNotification({
                        title: "NeoNect System",
                        body: "Hey! This is a test notification from NeoNect Notification System ⚡",
                        type: "message",
                        avatar: "N",
                        actionText: "Reply",
                        channel: "system",
                        duration: 5000
                    });
                }
            }
        }
    }

    // Global Custom Notification System Stack (Corner of Desktop Screen)
    NotificationStackView {
        id: globalNotifStack
        onActionTriggered: (notifId, action, channel) => {
            root.showNormal();
            root.raise();
            root.requestActivate();
            if (channel && channel !== "") {
                root.currentSelectedServer = "dms";
                root.currentActiveChannel = channel.toLowerCase();
                if (typeof channelsPanel !== "undefined" && channelsPanel && channelsPanel.openDirectMessage) {
                    channelsPanel.openDirectMessage(channel.toLowerCase());
                }
                if (action === "reply") {
                    if (typeof mainPanel !== "undefined" && mainPanel && mainPanel.focusMessageInput) {
                        mainPanel.focusMessageInput();
                    }
                }
            }
        }
    }
}

