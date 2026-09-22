// qml/components/NotificationCenterFlyout.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Popup {
    id: flyoutRoot

    readonly property int notifCount: (typeof NotificationManager !== "undefined" && NotificationManager && NotificationManager.activeNotifications) ? NotificationManager.activeNotifications.length : notifListView.count

    width: 360
    height: Math.min(460, headerSection.height + 1 + (flyoutRoot.notifCount > 0 ? Math.min(390, notifListView.contentHeight + 10) : emptySection.height) + 12)
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

    // Helper function for deterministic avatar gradient
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
        color: ThemeData.windowBackground
        border.color: ThemeData.borderColor
        border.width: 1

        // Outer soft ambient drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: parent.radius + 4
            color: Qt.rgba(0, 0, 0, 0.65)
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
                IconImage {
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    color: ThemeData.accentColor
                }

                Text {
                    text: "Notifications"
                    color: ThemeData.textPrimary
                    font.family: "Segoe UI"
                    font.bold: true
                    font.pixelSize: 13
                }

                // Notification count badge
                Rectangle {
                    visible: flyoutRoot.notifCount > 0
                    Layout.preferredHeight: 18
                    Layout.preferredWidth: Math.max(18, countBadgeText.implicitWidth + 10)
                    radius: 9
                    color: ThemeData.accentColor

                    Text {
                        id: countBadgeText
                        anchors.centerIn: parent
                        text: flyoutRoot.notifCount
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
                    visible: flyoutRoot.notifCount > 0
                    Layout.preferredHeight: 26
                    Layout.preferredWidth: clearRow.implicitWidth + 16
                    radius: 6
                    color: clearMouse.containsMouse ? Qt.rgba(255, 69, 58, 0.18) : ThemeData.itemHoverBackground
                    border.color: clearMouse.containsMouse ? Qt.rgba(255, 69, 58, 0.45) : ThemeData.borderColor
                    border.width: 1

                    RowLayout {
                        id: clearRow
                        anchors.centerIn: parent
                        spacing: 5

                        IconImage {
                            source: "qrc:/qt/qml/NeoNect/assets/icons/trash.svg"
                            Layout.preferredWidth: 12
                            Layout.preferredHeight: 12
                            color: clearMouse.containsMouse ? "#FF453A" : ThemeData.textSecondary
                        }

                        Text {
                            text: "Clear all"
                            color: clearMouse.containsMouse ? "#FF453A" : ThemeData.textSecondary
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
            color: ThemeData.borderColor
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
                visible: flyoutRoot.notifCount === 0
                spacing: 8

                IconImage {
                    source: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38
                    Layout.alignment: Qt.AlignHCenter
                    color: ThemeData.textMuted
                    opacity: 0.50
                }

                Text {
                    text: "No notifications"
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.bold: true
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "You're all caught up! New alerts will appear here."
                    color: ThemeData.textMuted
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // NOTIFICATION LIST VIEW
            ListView {
                id: notifListView
                anchors.fill: parent
                visible: flyoutRoot.notifCount > 0
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
                        color: vScroll.pressed ? ThemeData.accentColor : (vScroll.hovered ? ThemeData.scrollBarThumbHover : ThemeData.scrollBarThumb)
                    }
                }

                delegate: Rectangle {
                    id: itemDelegate
                    width: notifListView.width
                    height: 58
                    color: itemMouse.containsMouse ? ThemeData.itemHoverBackground : "transparent"

                    property var notifData: modelData
                    property var grad: flyoutRoot.getAvatarGradient(modelData.title || modelData.sender)

                    // Hairline bottom separator
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 56
                        height: 1
                        color: ThemeData.borderColor
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

                                CircularImage {
                                    id: flyoutAvatarImg
                                    anchors.fill: parent
                                    source: (typeof NetworkManager !== "undefined" && NetworkManager && modelData.channel && (!modelData.type || modelData.type === "message")) ? NetworkManager.getAvatarUrl(modelData.channel) : ""
                                    cornerRadius: 17
                                }

                                Text {
                                    visible: !flyoutAvatarImg.ready && (!modelData.type || modelData.type === "message")
                                    anchors.centerIn: parent
                                    text: {
                                        if (modelData.channel && typeof NetworkManager !== "undefined" && NetworkManager && (!modelData.type || modelData.type === "message")) return NetworkManager.getDisplayName(modelData.channel).charAt(0).toUpperCase();
                                        if (modelData.avatar && modelData.avatar.length > 0) return modelData.avatar.charAt(0).toUpperCase();
                                        if (modelData.title && modelData.title.length > 0) return modelData.title.charAt(0).toUpperCase();
                                        return "@";
                                    }
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.bold: true
                                    font.pixelSize: 14
                                }

                                IconImage {
                                    visible: !flyoutAvatarImg.ready && modelData.type && modelData.type !== "message"
                                    anchors.centerIn: parent
                                    source: {
                                        if (modelData.type === "security") return "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg";
                                        if (modelData.type === "success") return "qrc:/qt/qml/NeoNect/assets/icons/check.svg";
                                        if (modelData.type === "friend_request") return "qrc:/qt/qml/NeoNect/assets/icons/friends.svg";
                                        if (modelData.type === "media_request") return "qrc:/qt/qml/NeoNect/assets/icons/file.svg";
                                        return "qrc:/qt/qml/NeoNect/assets/icons/bell.svg";
                                    }
                                    width: 16; height: 16
                                    color: "#FFFFFF"
                                }
                            }

                            // Small green online dot
                            Rectangle {
                                width: 8; height: 8
                                radius: 4
                                color: "#23A55A"
                                border.color: ThemeData.windowBackground
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
                                    text: (modelData.channel && typeof NetworkManager !== "undefined" && NetworkManager && (!modelData.type || modelData.type === "message")) ? NetworkManager.getDisplayName(modelData.channel) : (modelData.title || "NeoNect")
                                    color: ThemeData.textPrimary
                                    font.family: "Segoe UI"
                                    font.bold: true
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: flyoutRoot.formatTime(modelData.timestamp)
                                    color: ThemeData.textMuted
                                    font.family: "Segoe UI"
                                    font.pixelSize: 11
                                }
                            }

                            Text {
                                text: modelData.body || "New notification"
                                color: ThemeData.textSecondary
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                Layout.fillWidth: true
                            }
                        }

                        // C. Actions: Accept/Decline for Friend Requests, or Dismiss for other notifications
                        RowLayout {
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 6

                            // Friend Request Buttons: Accept & Decline
                            RowLayout {
                                visible: modelData.type === "friend_request"
                                spacing: 6

                                // Accept Button
                                Rectangle {
                                    id: acceptFlyoutBtn
                                    Layout.preferredHeight: 26
                                    Layout.preferredWidth: 62
                                    radius: 13
                                    color: acceptFlyoutMouse.containsMouse ? "#23A55A" : Qt.rgba(35, 165, 90, 0.2)
                                    border.color: "#23A55A"
                                    border.width: 1

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        IconImage {
                                            source: "qrc:/qt/qml/NeoNect/assets/icons/check.svg"
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                            color: acceptFlyoutMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                        }

                                        Text {
                                            text: "Accept"
                                            color: acceptFlyoutMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                            font.family: "Segoe UI"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: acceptFlyoutMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var sender = modelData.channel || modelData.title;
                                            NetworkManager.acceptFriend(sender);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissNotification(modelData.notifId || modelData.id);
                                            }
                                        }
                                    }
                                }

                                // Decline Button
                                Rectangle {
                                    id: declineFlyoutBtn
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    radius: 13
                                    color: declineFlyoutMouse.containsMouse ? Qt.rgba(242, 63, 67, 0.3) : Qt.rgba(242, 63, 67, 0.15)
                                    border.color: "#F23F43"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        color: "#F23F43"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: declineFlyoutMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var sender = modelData.channel || modelData.title;
                                            NetworkManager.rejectFriend(sender);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissNotification(modelData.notifId || modelData.id);
                                            }
                                        }
                                    }
                                }
                            }

                            // Media Request Buttons: Accept & Decline
                            RowLayout {
                                visible: modelData.type === "media_request"
                                spacing: 6

                                // Accept Button
                                Rectangle {
                                    id: acceptMediaFlyoutBtn
                                    Layout.preferredHeight: 26
                                    Layout.preferredWidth: 62
                                    radius: 13
                                    color: acceptMediaMouse.containsMouse ? "#23A55A" : Qt.rgba(35, 165, 90, 0.2)
                                    border.color: "#23A55A"
                                    border.width: 1

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        IconImage {
                                            source: "qrc:/qt/qml/NeoNect/assets/icons/check.svg"
                                            Layout.preferredWidth: 12
                                            Layout.preferredHeight: 12
                                            color: acceptMediaMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                        }

                                        Text {
                                            text: "Accept"
                                            color: acceptMediaMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                            font.family: "Segoe UI"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }

                                    MouseArea {
                                        id: acceptMediaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var ch = (modelData.channel || modelData.title || "").toLowerCase();
                                            var convId = ch.startsWith("dms:") ? ch : ("dms:" + ch);
                                            var reqId = modelData.requestId || modelData.id;
                                            MessageService.acceptMediaRequest(convId, reqId);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissNotification(modelData.notifId || modelData.id);
                                            }
                                        }
                                    }
                                }

                                // Decline Button
                                Rectangle {
                                    id: declineMediaFlyoutBtn
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    radius: 13
                                    color: declineMediaMouse.containsMouse ? Qt.rgba(242, 63, 67, 0.3) : Qt.rgba(242, 63, 67, 0.15)
                                    border.color: "#F23F43"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        color: "#F23F43"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: declineMediaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var ch = (modelData.channel || modelData.title || "").toLowerCase();
                                            var convId = ch.startsWith("dms:") ? ch : ("dms:" + ch);
                                            var reqId = modelData.requestId || modelData.id;
                                            MessageService.declineMediaRequest(convId, reqId);
                                            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                                NotificationManager.dismissNotification(modelData.notifId || modelData.id);
                                            }
                                        }
                                    }
                                }
                            }

                            // Standard Dismiss Button for other notification types
                            Rectangle {
                                id: dismissItemBtn
                                visible: modelData.type !== "friend_request" && modelData.type !== "media_request"
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                radius: 10
                                color: dismissMouse.containsMouse ? ThemeData.itemSelectedBackground : "transparent"
                                opacity: itemMouse.containsMouse || dismissMouse.containsMouse ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 120 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: dismissMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textMuted
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
