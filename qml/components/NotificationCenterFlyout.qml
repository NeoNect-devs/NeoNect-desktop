// qml/components/NotificationCenterFlyout.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Popup {
    id: flyoutRoot

    width: 360
    height: Math.min(460, headerSection.height + 1 + (notifListView.count > 0 ? Math.min(390, notifListView.contentHeight + 10) : emptySection.height) + 12)
    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    signal itemClicked(string channel)

    // Smooth drop-down & fade entrance animation
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "y"; from: flyoutRoot.y - 8; to: flyoutRoot.y; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 180; easing.type: Easing.OutBack }
        }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 120; easing.type: Easing.InCubic }
    }

    // Helper function for deterministic Telegram avatar gradient
    function getAvatarGradient(name) {
        var str = (name && name.length > 0) ? name : "N";
        var code = str.charCodeAt(0);
        var gradients = [
            { c1: "#0A84FF", c2: "#00D2FF" }, // Cyan-Blue
            { c1: "#8B5CF6", c2: "#C084FC" }, // Lilac
            { c1: "#10B981", c2: "#34D399" }, // Emerald
            { c1: "#F59E0B", c2: "#FBBF24" }, // Amber
            { c1: "#EC4899", c2: "#F472B6" }  // Rose
        ];
        return gradients[code % gradients.length];
    }

    // Helper function for relative timestamp display
    function formatTime(timestamp) {
        if (!timestamp) return "now";
        var now = Math.floor(Date.now() / 1000);
        var diff = Math.max(0, now - timestamp);
        if (diff < 60) return "now";
        if (diff < 3600) return Math.floor(diff / 60) + "m";
        if (diff < 86400) return Math.floor(diff / 3600) + "h";
        return Math.floor(diff / 86400) + "d";
    }

    // ─── BACKGROUND CONTAINER & AMBIENT SHADOW ───
    background: Rectangle {
        radius: 12
        color: "#17212B" // Telegram Dark Obsidian
        border.color: Qt.rgba(255, 255, 255, 0.12)
        border.width: 1

        // Outer soft ambient drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: Qt.rgba(0, 0, 0, 0.55)
            z: -1
        }
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ══════════════════════════════════════════════════════
        // HEADER BAR: "Notifications", Badge & "Clear all"
        // ══════════════════════════════════════════════════════
        Item {
            id: headerSection
            Layout.fillWidth: true
            Layout.preferredHeight: 46

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 8

                // Header icon
                Image {
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    text: "Notifications"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.bold: true
                    font.pixelSize: 13
                }

                // New badge count if unread
                Rectangle {
                    visible: (typeof NotificationManager !== "undefined" && NotificationManager) ? (NotificationManager.unreadCount > 0) : false
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: unreadBadgeText.implicitWidth + 10
                    radius: 9
                    color: "#0A84FF"

                    Text {
                        id: unreadBadgeText
                        anchors.centerIn: parent
                        text: (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.unreadCount : "0"
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.bold: true
                        font.pixelSize: 10
                    }
                }

                Item { Layout.fillWidth: true } // Spacer

                // "Clear all" Action Button
                Rectangle {
                    id: clearBtn
                    visible: notifListView.count > 0
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: clearRow.implicitWidth + 16
                    radius: 6
                    color: clearMouse.containsMouse ? Qt.rgba(255, 69, 58, 0.18) : Qt.rgba(255, 255, 255, 0.06)
                    border.color: clearMouse.containsMouse ? Qt.rgba(255, 69, 58, 0.45) : Qt.rgba(255, 255, 255, 0.10)
                    border.width: 1

                    RowLayout {
                        id: clearRow
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            source: "qrc:/qt/qml/NeoNect/assets/icons/trash.svg"
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            fillMode: Image.PreserveAspectFit
                        }

                        Text {
                            text: "Clear all"
                            color: clearMouse.containsMouse ? "#FF453A" : "#8E9BAE"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: clearMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                NotificationManager.clearAll();
                            }
                        }
                    }
                }
            }
        }

        // Hairline Divider
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(255, 255, 255, 0.08)
        }

        // ══════════════════════════════════════════════════════
        // BODY: Notification List OR Empty State
        // ══════════════════════════════════════════════════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // EMPTY STATE (When no notifications exist)
            ColumnLayout {
                id: emptySection
                anchors.centerIn: parent
                visible: notifListView.count === 0
                spacing: 8

                Image {
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignHCenter
                    opacity: 0.30
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    text: "No notifications"
                    color: "#8E9BAE"
                    font.family: "Segoe UI"
                    font.bold: true
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "You're all caught up! New alerts will appear here."
                    color: "#5E6877"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // NOTIFICATION LIST VIEW
            ListView {
                id: notifListView
                anchors.fill: parent
                visible: count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.activeNotifications : []

                ScrollBar.vertical: ScrollBar {
                    id: vScroll
                    active: true
                    policy: notifListView.contentHeight > notifListView.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
                    width: 6
                    background: Rectangle { color: "transparent" }
                    contentItem: Rectangle {
                        radius: 3
                        color: vScroll.pressed ? "#0A84FF" : (vScroll.hovered ? Qt.rgba(255, 255, 255, 0.3) : Qt.rgba(255, 255, 255, 0.15))
                    }
                }

                delegate: Rectangle {
                    id: itemDelegate
                    width: notifListView.width
                    height: 58
                    color: itemMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.06) : "transparent"

                    property var notifData: modelData
                    property var grad: flyoutRoot.getAvatarGradient(modelData.title || modelData.sender)

                    // Hairline bottom separator
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 56
                        height: 1
                        color: Qt.rgba(255, 255, 255, 0.05)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 10
                        spacing: 10

                        // A. Circular Avatar with gradient / icon
                        Item {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 17
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: itemDelegate.grad.c1 }
                                    GradientStop { position: 1.0; color: itemDelegate.grad.c2 }
                                }
                                border.color: Qt.rgba(255, 255, 255, 0.15)
                                border.width: 1

                                Text {
                                    visible: !modelData.type || modelData.type === "message"
                                    anchors.centerIn: parent
                                    text: {
                                        if (modelData.avatar && modelData.avatar.length > 0) return modelData.avatar.charAt(0).toUpperCase();
                                        if (modelData.title && modelData.title.length > 0) return modelData.title.charAt(0).toUpperCase();
                                        return "@";
                                    }
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.bold: true
                                    font.pixelSize: 14
                                }

                                Image {
                                    visible: modelData.type && modelData.type !== "message"
                                    anchors.centerIn: parent
                                    source: {
                                        if (modelData.type === "security") return "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg";
                                        if (modelData.type === "success") return "qrc:/qt/qml/NeoNect/assets/icons/check.svg";
                                        if (modelData.type === "friend_request") return "qrc:/qt/qml/NeoNect/assets/icons/friends.svg";
                                        return "qrc:/qt/qml/NeoNect/assets/icons/bell.svg";
                                    }
                                    width: 16; height: 16
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            // Small green online dot
                            Rectangle {
                                width: 8; height: 8
                                radius: 4
                                color: "#23A55A"
                                border.color: "#17212B"
                                border.width: 1.5
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: !modelData.type || modelData.type === "message"
                            }
                        }

                        // B. Content details: Title, Channel, Time & Body preview
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 1

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: modelData.title || "NeoNect"
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.bold: true
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: flyoutRoot.formatTime(modelData.timestamp)
                                    color: "#6B7684"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 11
                                }
                            }

                            Text {
                                text: modelData.body || "New notification"
                                color: "#9FA8B4"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                Layout.fillWidth: true
                            }
                        }

                        // C. Dismiss Button "✕"
                        Rectangle {
                            id: dismissItemBtn
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            radius: 10
                            color: dismissMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"
                            opacity: itemMouse.containsMouse || dismissMouse.containsMouse ? 1.0 : 0.0
                            Behavior on opacity { NumberAnimation { duration: 120 } }

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: dismissMouse.containsMouse ? "#FFFFFF" : "#7E8794"
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: dismissMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                        NotificationManager.dismissNotification(modelData.notifId || modelData.id);
                                    }
                                }
                            }
                        }
                    }

                    // Full-Item Click Handler
                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: -1
                        onClicked: {
                            var channel = modelData.channel || "";
                            flyoutRoot.itemClicked(channel);
                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                NotificationManager.markChannelAsRead(channel);
                            }
                            flyoutRoot.close();
                        }
                    }
                }
            }
        }
    }
}
