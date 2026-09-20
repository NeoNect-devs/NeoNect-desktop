import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.impl
import "../components"

Rectangle {
    id: root
    property Window windowTarget
    property string appState: "gateway"
    property string titleText: ""
    property bool showBackButton: false
    signal backClicked
    signal brandClicked
    signal navigateToChat(string channel)

    height: 45
    color: root.appState === "gateway" ? Qt.darker(ThemeData.windowBackground, 1.15) : "transparent"
    z: 100

    // ─── TOP-LEFT OVERLAPPING BRAND BUTTON ────────────────────────────
    NeoNectBrandButton {
        id: topBrandButton
        anchors.left: parent.left
        anchors.top: parent.top
        visible: root.appState === "authenticated"
        z: 101 // Floats above the window drag MouseArea
        onClicked: root.brandClicked()
    }

    // Window Drag Handler
    MouseArea {
        id: dragArea
        anchors.fill: parent

        property bool isPressed: false
        property bool nativeMoveStarted: false
        property point pressLocalPos: Qt.point(0, 0)
        property point pressGlobalPos: Qt.point(0, 0)
        property real pressRatio: 0.5
        property bool wasMaximizedOnPress: false

        onPressed: mouse => {
            if (mouse.button !== Qt.LeftButton) return;
            isPressed = true;
            nativeMoveStarted = false;
            pressLocalPos = Qt.point(mouse.x, mouse.y);
            var g = dragArea.mapToGlobal(mouse.x, mouse.y);
            pressGlobalPos = Qt.point(g.x, g.y);
            wasMaximizedOnPress = (root.windowTarget.visibility === Window.Maximized);
            pressRatio = root.width > 0 ? (mouse.x / root.width) : 0.5;

            if (!wasMaximizedOnPress) {
                if (root.windowTarget.startSystemMove()) {
                    nativeMoveStarted = true;
                }
            }
        }

        onPositionChanged: mouse => {
            if (!isPressed || nativeMoveStarted) return;

            var g = dragArea.mapToGlobal(mouse.x, mouse.y);
            var dx = g.x - pressGlobalPos.x;
            var dy = g.y - pressGlobalPos.y;
            var distSq = dx * dx + dy * dy;

            if (wasMaximizedOnPress) {
                if (distSq > 16) {
                    root.windowTarget.visibility = Window.Windowed;

                    var targetW = root.windowTarget.width > 0 ? root.windowTarget.width : 850;
                    root.windowTarget.x = Math.max(0, g.x - (targetW * pressRatio));
                    root.windowTarget.y = Math.max(0, g.y - pressLocalPos.y);

                    wasMaximizedOnPress = false;
                    if (root.windowTarget.startSystemMove()) {
                        nativeMoveStarted = true;
                    }
                }
            } else {
                if (distSq > 4) {
                    if (root.windowTarget.startSystemMove()) {
                        nativeMoveStarted = true;
                    } else {
                        root.windowTarget.x += dx;
                        root.windowTarget.y += dy;
                        pressGlobalPos = Qt.point(g.x, g.y);
                    }
                }
            }
        }

        onReleased: mouse => {
            if (!nativeMoveStarted && !wasMaximizedOnPress && mouse) {
                var g = dragArea.mapToGlobal(mouse.x, mouse.y);
                if (g.y <= 5 && root.appState !== "gateway") {
                    root.windowTarget.visibility = Window.Maximized;
                }
            }
            isPressed = false;
            nativeMoveStarted = false;
        }

        onCanceled: {
            isPressed = false;
            nativeMoveStarted = false;
        }

        onDoubleClicked: {
            if (root.appState === "gateway") return;
            isPressed = false;
            nativeMoveStarted = false;
            root.windowTarget.visibility = (root.windowTarget.visibility === Window.Maximized)
                ? Window.Windowed
                : Window.Maximized;
        }
    }

    // Gateway Navigation Bar
    RowLayout {
        anchors.fill: parent
        visible: root.appState === "gateway"
        spacing: 0

        Rectangle {
            Layout.leftMargin: 10
            Layout.alignment: Qt.AlignVCenter
            width: 35
            height: 35
            radius: 4
            color: backMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
            visible: root.showBackButton
            Image {
                anchors.centerIn: parent
                width: 18
                height: 18
                source: "../../assets/icons/arrow-back.svg"
                fillMode: Image.PreserveAspectFit
            }
            MouseArea {
                id: backMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.backClicked()
            }
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: root.titleText + ((typeof appProfile !== "undefined" && appProfile !== "") ? "  [" + appProfile + "]" : "")
            color: "gray"
            font.pointSize: 11
        }
        RowLayout {
            Layout.rightMargin: 8
            Layout.alignment: Qt.AlignVCenter
            spacing: 4

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 32
                radius: 4
                color: closeMouseArea.containsMouse ? "#e81123" : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: closeMouseArea.containsMouse ? "white" : "gray"
                    font.pointSize: 12
                }
                MouseArea {
                    id: closeMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.windowTarget.close()
                }
            }
        }
    }

    // Authenticated Window Controls (Minimize / Maximize / Close)
    Item {
        anchors.fill: parent
        visible: root.appState === "authenticated"

        RowLayout {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.rightMargin: 8
            spacing: 4

            // ─── NOTIFICATION CENTER BUTTON (BEHIND / BEFORE MINIMIZE) ───
            Rectangle {
                id: notifCenterBtn
                Layout.preferredWidth: 36
                Layout.preferredHeight: 32
                radius: 4
                color: notifMouse.containsMouse || notifFlyout.visible ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

                IconImage {
                    anchors.centerIn: parent
                    width: 17
                    height: 17
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                    color: notifMouse.containsMouse || notifFlyout.visible ? ThemeData.textPrimary : ThemeData.textSecondary
                }

                // RED DOT INDICATOR FOR UNREAD NOTIFICATIONS
                Rectangle {
                    id: redDotIndicator
                    visible: (typeof NotificationManager !== "undefined" && NotificationManager) ? (NotificationManager.unreadCount > 0) : false
                    width: 8
                    height: 8
                    radius: 4
                    color: "#FF3B30"
                    border.color: ThemeData.windowBackground
                    border.width: 1.5
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.topMargin: 5
                    anchors.rightMargin: 6

                    // Subtle pulsating animation when unread
                    SequentialAnimation on scale {
                        running: redDotIndicator.visible
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 1.25; duration: 800; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 1.25; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                    }
                }

                MouseArea {
                    id: notifMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (notifFlyout.visible) {
                            notifFlyout.close();
                        } else {
                            notifFlyout.open();
                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                NotificationManager.resetUnreadCount();
                            }
                        }
                    }
                }

                NotificationCenterFlyout {
                    id: notifFlyout
                    x: -(width - notifCenterBtn.width)
                    y: notifCenterBtn.height + 6
                    onItemClicked: (channel) => {
                        root.navigateToChat(channel);
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 32
                radius: 4
                color: minM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "—"
                    color: minM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                }
                MouseArea {
                    id: minM
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.windowTarget.visibility = Window.Minimized
                }
            }
            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 32
                radius: 4
                color: maxM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: root.windowTarget.visibility === Window.Maximized ? "🗗" : "🗖"
                    color: maxM.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                }
                MouseArea {
                    id: maxM
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.windowTarget.visibility = (root.windowTarget.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized
                }
            }
            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 32
                radius: 4
                color: authCloseM.containsMouse ? "#e81123" : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: authCloseM.containsMouse ? "#FFFFFF" : ThemeData.textSecondary
                }
                MouseArea {
                    id: authCloseM
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        root.windowTarget.close();
                        Qt.quit();
                    }
                }

            }
        }
    }
}
