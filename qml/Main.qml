// qml/Main.qml
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Avila 1.0
import "entrypage"
import "MainSection"
import "SideSection"

Window {
    id: root
    color: ThemeData.mainWindowBackground
    width: 850
    height: 640
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint

    property string appState: "gateway"
    property string activeTitleText: "Server Connection"
    property string sessionToken: ""
    property string verifiedServerUrl: ""
    property string currentActiveChannel: "general"
    property string currentSelectedServer: "server1"

    Rectangle {
        id: mainBackground
        anchors.fill: parent
        color: root.color

        Loader {
            id: viewFlowLoader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: titleBar.bottom

            source: root.appState === "gateway" ? "entrypage/entry.qml" : ""
            sourceComponent: root.appState === "authenticated" ? chatDashboardComponent : null

            Connections {
                target: viewFlowLoader.item
                ignoreUnknownSignals: true
                function onRequestTitleChange(newTitle) { root.activeTitleText = newTitle }
                function onAuthenticationSuccess(token, serverUrl) {
                    root.sessionToken = token
                    root.verifiedServerUrl = serverUrl
                    root.appState = "authenticated"
                }
            }
        }

        // ===========================================================
        // SYSTEM TITLE BAR
        // ===========================================================
        Rectangle {
            id: titleBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 45
            z: 100
            color: root.appState === "gateway" ? Qt.darker(ThemeData.mainWindowBackground, 1.15) : "transparent"

            MouseArea {
                anchors.fill: parent
                property point clickPos: Qt.point(0, 0)
                onPressed: (mouse) => { clickPos = Qt.point(mouse.x, mouse.y) }
                onPositionChanged: (mouse) => {
                    if (root.visibility === Window.Maximized) {
                        let ratioX = mouse.x / root.width
                        root.visibility = Window.Windowed
                        clickPos.x = root.width * ratioX
                        clickPos.y = mouse.y
                        return;
                    }
                    let delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y)
                    root.x += delta.x; root.y += delta.y
                }
                onDoubleClicked: { if (root.appState === "authenticated") root.visibility = (root.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized }
            }

            // STATE A: ENTRY SECURITY GATEWAY
            RowLayout {
                anchors.fill: parent
                visible: root.appState === "gateway"
                spacing: 0

                Rectangle {
                    Layout.leftMargin: 10; Layout.alignment: Qt.AlignVCenter
                    width: 35; height: 35; radius: 4
                    color: backMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                    visible: viewFlowLoader.item && viewFlowLoader.item.currentSubScreen !== "address_entry"
                    Image { anchors.centerIn: parent; width: 18; height: 18; source: "../assets/icons/arrow-back.svg"; fillMode: Image.PreserveAspectFit }
                    MouseArea { id: backMouseArea; anchors.fill: parent; hoverEnabled: true; onClicked: { viewFlowLoader.item.currentSubScreen = "address_entry" } }
                }
                Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: root.activeTitleText; color: "gray"; font.pointSize: 11 }
                Rectangle {
                    Layout.rightMargin: 10; Layout.alignment: Qt.AlignVCenter
                    width: 35; height: 35; radius: 4
                    color: closeMouseArea.containsMouse ? "#e81123" : "transparent"
                    Text { anchors.centerIn: parent; text: "✕"; color: closeMouseArea.containsMouse ? "white" : "gray"; font.pointSize: 12 }
                    MouseArea { id: closeMouseArea; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                }
            }

            // STATE B: AUTHENTICATED
            Item {
                anchors.fill: parent
                visible: root.appState === "authenticated"

                Item {
                    id: brandingContainer
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    implicitHeight: 60
                    width: logoRow.implicitWidth + 16
                    z: 105

                    Rectangle {
                        id: gradientBorderLayer; anchors.fill: parent
                        topLeftRadius: 0; bottomLeftRadius: 0; topRightRadius: 16; bottomRightRadius: 16
                        opacity: brandingMouseArea.containsMouse ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: 200 } }
                        gradient: Gradient {
                            orientation: Gradient.Diagonal
                            GradientStop { position: 0.0; color: "#FFFFFF" }
                            GradientStop { position: 1.0; color: "#3A3A3A" }
                        }
                    }

                    Rectangle {
                        id: innerSolidBackground; anchors.fill: parent
                        anchors.topMargin: 1; anchors.rightMargin: 1; anchors.bottomMargin: 1
                        topLeftRadius: 0; bottomLeftRadius: 0; topRightRadius: 15; bottomRightRadius: 15
                        color: "#101210"
                    }

                    Row {
                        id: logoRow; anchors.bottom: parent.bottom; anchors.bottomMargin: 2; anchors.left: parent.left; anchors.leftMargin: 8; spacing: 2
                        Item {
                            width: 48; height: 48; anchors.verticalCenter: parent.verticalCenter
                            Image {
                                id: chatLogo; anchors.fill: parent; source: "../assets/logo.png"; fillMode: Image.PreserveAspectFit; antialiasing: true
                                scale: brandingMouseArea.containsMouse ? 1.06 : 1.0; opacity: brandingMouseArea.containsMouse ? 1.0 : 0.88
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                            }
                        }
                        Text { text: "Avila"; color: "#FFFFFF"; font.family: "Segoe UI"; font.pixelSize: 18; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
                    }
                    MouseArea { id: brandingMouseArea; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                }

                RowLayout {
                    anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.rightMargin: 8; spacing: 4
                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: minM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                        Text { anchors.centerIn: parent; text: "—"; color: "white"; font.pointSize: 10 }
                        MouseArea { id: minM; anchors.fill: parent; hoverEnabled: true; onClicked: root.visibility = Window.Minimized }
                    }
                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: maxM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                        Text { anchors.centerIn: parent; text: root.visibility === Window.Maximized ? "🗗" : "🗖"; color: "white"; font.pointSize: 11 }
                        MouseArea { id: maxM; anchors.fill: parent; hoverEnabled: true; onClicked: root.visibility = (root.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized }
                    }
                    Rectangle {
                        Layout.preferredWidth: 38; Layout.preferredHeight: 32; radius: 4
                        color: chatCloseM.containsMouse ? "#e81123" : "transparent"
                        Text { anchors.centerIn: parent; text: "✕"; color: "white"; font.pointSize: 11 }
                        MouseArea { id: chatCloseM; anchors.fill: parent; hoverEnabled: true; onClicked: root.close() }
                    }
                }
            }
        }

        // ===========================================================
        // AUTHENTICATED CHAT WORKSPACE ARCHITECTURE
        // ===========================================================
        Component {
            id: chatDashboardComponent

            Item {
                anchors.top: titleBar.bottom
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right

                RowLayout {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    spacing: 0

                    SidebarCanvas {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 60
                    }

                    Rectangle {
                        id: chatViewWrapperContainer
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "transparent"
                        border.color: "#232523"
                        border.width: 1

                        SplitView {
                            anchors.fill: parent
                            anchors.margins: 1
                            orientation: Qt.Horizontal
                            handle: Rectangle { implicitWidth: 3; color: ThemeData.mainWindowBackground }

                            // Middle Channels View Column
                            Rectangle {
                                id: channelSidebarSpace
                                SplitView.minimumWidth: 180
                                SplitView.preferredWidth: 240
                                SplitView.maximumWidth: 350
                                Layout.fillHeight: true
                                color: "#101210"

                                ScrollView {
                                    id: channelScrollView
                                    anchors.fill: parent
                                    clip: true

                                    ColumnLayout {
                                        width: channelScrollView.availableWidth
                                        spacing: 2
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        anchors.topMargin: 8
                                        anchors.bottomMargin: 60

                                        Text {
                                            text: "TEXT CHANNELS"
                                            color: "#949BA4"
                                            font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold
                                            Layout.fillWidth: true; Layout.leftMargin: 4; Layout.topMargin: 8
                                            visible: root.currentSelectedServer !== "dms"
                                        }
                                        ChannelListItem {
                                            name: "welcome-rules"; type: "channel"; Layout.fillWidth: true; visible: root.currentSelectedServer !== "dms"
                                            isSelected: root.currentActiveChannel === "welcome-rules"; onClicked: root.currentActiveChannel = "welcome-rules"
                                        }
                                        ChannelListItem {
                                            name: "announcements"; type: "channel"; Layout.fillWidth: true; visible: root.currentSelectedServer !== "dms"; hasUnread: true; notificationCount: 3
                                            isSelected: root.currentActiveChannel === "announcements"; onClicked: root.currentActiveChannel = "announcements"
                                        }
                                        ChannelListItem {
                                            name: "general"; type: "channel"; Layout.fillWidth: true; visible: root.currentSelectedServer !== "dms"
                                            isSelected: root.currentActiveChannel === "general"; onClicked: root.currentActiveChannel = "general"
                                        }

                                        Text {
                                            text: "DIRECT MESSAGES"
                                            color: "#949BA4"
                                            font.family: "Segoe UI"; font.pixelSize: 12; font.weight: Font.Bold
                                            Layout.fillWidth: true; Layout.leftMargin: 4; Layout.topMargin: 8
                                            visible: root.currentSelectedServer === "dms"
                                        }
                                        ChannelListItem {
                                            name: "Alex (Core)"; type: "dm"; Layout.fillWidth: true; visible: root.currentSelectedServer === "dms"
                                            isSelected: root.currentActiveChannel === "dm-alex"; onClicked: root.currentActiveChannel = "dm-alex"
                                        }
                                        ChannelListItem {
                                            name: "Sarah_Dev"; type: "dm"; Layout.fillWidth: true; visible: root.currentSelectedServer === "dms"; hasUnread: true
                                            isSelected: root.currentActiveChannel === "dm-sarah"; onClicked: root.currentActiveChannel = "dm-sarah"
                                        }
                                    }
                                }
                            }

                            // Conversation Area Viewport Panel
                            MainPanel {
                                SplitView.fillWidth: true
                                Layout.fillHeight: true
                            }
                        }
                    }
                }

                // ===========================================================
                // FLOATING CONTIGUOUS USER PROFILE CARD
                // ===========================================================
                Rectangle {
                    id: unifiedProfileFooterCard
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left

                    width: 60 + channelSidebarSpace.width
                    height: 52
                    z: 10

                    color: "#0F1110"
                    topLeftRadius: 12
                    topRightRadius: 12
                    bottomLeftRadius: 0
                    bottomRightRadius: 0

                    border.color: "#232523"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Sidebar Region Anchor (Avatar)
                        Item {
                            width: 60
                            Layout.preferredWidth: 60
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.centerIn: parent
                                width: 36; height: 36; radius: 18; color: "#1E201E"
                                Text { anchors.centerIn: parent; text: "👤"; font.pixelSize: 16 }

                                Rectangle {
                                    anchors.bottom: parent.bottom; anchors.right: parent.right
                                    width: 10; height: 10; radius: 5; color: "#00A36C"
                                    border.color: "#0F1110"; border.width: 1.5
                                }
                            }
                        }

                        // Channels Alignment Panel (Meta & Controls)
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 4
                            Layout.rightMargin: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Layout.alignment: Qt.AlignVCenter

                                Text {
                                    text: "UserAccount"
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"; font.pixelSize: 13; font.weight: Font.DemiBold
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                                Text {
                                    text: "Online"
                                    color: "#A2A4A2"
                                    font.family: "Segoe UI"; font.pixelSize: 11
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                            }

                            Row {
                                spacing: 4
                                Layout.alignment: Qt.AlignVCenter

                                Rectangle {
                                    width: 28; height: 28; radius: 6
                                    color: micMouse.containsMouse ? "#1E201E" : "transparent"
                                    border.color: micMouse.containsMouse ? "#2A2C2A" : "transparent"
                                    Text { anchors.centerIn: parent; text: "🎙️"; font.pixelSize: 11 }
                                    MouseArea { id: micMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                                }
                                Rectangle {
                                    width: 28; height: 28; radius: 6
                                    color: audioMouse.containsMouse ? "#1E201E" : "transparent"
                                    border.color: audioMouse.containsMouse ? "#2A2C2A" : "transparent"
                                    Text { anchors.centerIn: parent; text: "🎧"; font.pixelSize: 11 }
                                    MouseArea { id: audioMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                                }
                                Rectangle {
                                    width: 28; height: 28; radius: 6
                                    color: settingsMouse.containsMouse ? "#1E201E" : "transparent"
                                    border.color: settingsMouse.containsMouse ? "#2A2C2A" : "transparent"
                                    Text { anchors.centerIn: parent; text: "⚙️"; font.pixelSize: 11 }
                                    MouseArea { id: settingsMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ===========================================================
        // RESIZE HITBOXES
        // ===========================================================
        Item {
            anchors.fill: parent
            z: 101
            visible: root.appState === "authenticated" && root.visibility !== Window.Maximized

            MouseArea { width: 6; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.LeftEdge) }
            MouseArea { width: 6; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; cursorShape: Qt.SizeHorCursor; onPressed: root.startSystemResize(Qt.RightEdge) }
            MouseArea { height: 6; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.BottomEdge) }
            MouseArea { height: 6; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; cursorShape: Qt.SizeVerCursor; onPressed: root.startSystemResize(Qt.TopEdge) }
            MouseArea { width: 10; height: 10; anchors.right: parent.right; anchors.bottom: parent.bottom; cursorShape: Qt.SizeFDiagCursor; onPressed: root.startSystemResize(Qt.RightEdge | Qt.BottomEdge) }
            MouseArea { width: 10; height: 10; anchors.left: parent.left; anchors.bottom: parent.bottom; cursorShape: Qt.SizeBDiagCursor; onPressed: root.startSystemResize(Qt.LeftEdge | Qt.BottomEdge) }
            MouseArea { width: 10; height: 10; anchors.right: parent.right; anchors.top: parent.top; cursorShape: Qt.SizeBDiagCursor; onPressed: root.startSystemResize(Qt.RightEdge | Qt.TopEdge) }
            MouseArea { width: 10; height: 10; anchors.left: parent.left; anchors.top: parent.top; cursorShape: Qt.SizeFDiagCursor; onPressed: root.startSystemResize(Qt.LeftEdge | Qt.TopEdge) }
        }
    }
}
