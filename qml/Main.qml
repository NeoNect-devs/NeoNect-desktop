// qml/Main.qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Avila 1.0
import Avila.Core 1.0
import "."

Window {
    id: root
    color: ThemeData.windowBackground
    width: 850; height: 640
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint

    property string appState: (NetworkManager && NetworkManager.token && NetworkManager.token !== "") ? "authenticated" : "gateway"
    property string activeTitleText: "Server Connection"
    property string currentActiveChannel: "general"
    property string currentSelectedServer: "server1"

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
        anchors.fill: parent
        color: root.color

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
                                onChannelChanged: (chan) => root.currentActiveChannel = chan
                                onAddFriendRequested: mainPanel.showAddFriendModal = true
                            }
                            MainPanel {
                                id: mainPanel
                                selectedServer: root.currentSelectedServer
                                activeChannel: root.currentActiveChannel
                                onNavigateRequested: (srv, chan) => {
                                    root.currentSelectedServer = srv;
                                    root.currentActiveChannel = chan;
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
            anchors.fill: parent; z: 101
            visible: root.appState === "authenticated" && root.visibility !== Window.Maximized && root.visibility !== Window.FullScreen
            MouseArea { width: 4; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.LeftEdge) }
            MouseArea { width: 4; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.RightEdge) }
            MouseArea { height: 4; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.BottomEdge) }
            MouseArea { height: 4; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.TopEdge) }
        }

        // Global Media Fullscreen Lightbox Modal (Covers entire application)
        MediaLightboxModal {
            id: globalLightboxModal
            anchors.fill: parent
            z: 999999
        }
    }
}

