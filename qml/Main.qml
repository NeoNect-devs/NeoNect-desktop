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
        function onTokenChanged() {
            if (NetworkManager && NetworkManager.token && NetworkManager.token !== "") {
                root.appState = "authenticated";
            } else {
                root.appState = "gateway";
            }
        }
    }

    onAppStateChanged: {
        if (appState === "gateway" && typeof viewFlowLoader !== "undefined") {
            viewFlowLoader.sourceComponent = null;
            viewFlowLoader.source = "";
            viewFlowLoader.source = "entrypage/entry.qml";
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
            visible: root.appState === "authenticated" && root.visibility !== Window.Maximized
            MouseArea { width: 4; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.LeftEdge) }
            MouseArea { width: 4; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.RightEdge) }
            MouseArea { height: 4; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.BottomEdge) }
            MouseArea { height: 4; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.TopEdge) }
        }
    }

    // =========================================================================
    // ─── DEVELOPER CHEAT KERNEL ──────────────────────────────────────────────
    // =========================================================================

    // ➔ CHEAT WAY 1: GLOBAL KEYBOARD SHORTCUTS
    Shortcut { sequence: "Ctrl+1"; onActivated: devBypass("gateway", "server") }
    Shortcut { sequence: "Ctrl+2"; onActivated: devBypass("gateway", "login") }
    Shortcut { sequence: "Ctrl+3"; onActivated: devBypass("gateway", "signup") }
    Shortcut { sequence: "Ctrl+4"; onActivated: devBypass("authenticated", "") }

    function devBypass(targetState, targetSubScreen) {
        root.devDeepLink = targetSubScreen
        root.appState = targetState
        if (targetState === "gateway" && viewFlowLoader.item) {
            viewFlowLoader.item.currentScreen = targetSubScreen
        }
    }

    // ➔ CHEAT WAY 2: FLOATING INTERACTIVE HUD PANEL
    Rectangle {
        id: devHud
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 16; z: 99999
        width: expanded ? 460 : 40; height: 40
        radius: 8; color: "#d91e1e24"; border.color: "#ff3366"; border.width: 1
        clip: true

        property bool expanded: true

        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.InOutQuad } }

        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
            spacing: 8

            // Toggle Expand Button
            Text {
                text: devHud.expanded ? "❌" : "🛠️"
                color: "#ff3366"; font.bold: true; font.pointSize: 12
                MouseArea { anchors.fill: parent; onClicked: devHud.expanded = !devHud.expanded }
            }

            Row {
                Layout.fillWidth: true; spacing: 6
                visible: devHud.expanded

                Text { text: "DEV HUD:"; color: "#ff3366"; font.bold: true; verticalAlignment: Text.AlignVCenter; height: 24 }

                Button { text: "Server (Ctrl+1)"; onClicked: root.devBypass("gateway", "server") }
                Button { text: "Login (Ctrl+2)"; onClicked: root.devBypass("gateway", "login") }
                Button { text: "Signup (Ctrl+3)"; onClicked: root.devBypass("gateway", "signup") }
                Button { text: "🔥 Bypass Main App (Ctrl+4)"; onClicked: root.devBypass("authenticated", "") }
            }
        }
    }
}
