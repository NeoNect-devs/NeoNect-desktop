// qml/components/NotificationStackView.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0

Window {
    id: notifWindow

    // Native frameless floating tool window on top of all desktop windows
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    property string screenCorner: (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.screenCorner : "bottom-right"

    // Primary screen dimensions queried directly from C++ QGuiApplication::primaryScreen()->availableGeometry()
    readonly property real screenX: (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.screenAvailableX : 0
    readonly property real screenY: (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.screenAvailableY : 0
    readonly property real screenW: (typeof NotificationManager !== "undefined" && NotificationManager && NotificationManager.screenAvailableWidth > 0) ? NotificationManager.screenAvailableWidth : 1920
    readonly property real screenH: (typeof NotificationManager !== "undefined" && NotificationManager && NotificationManager.screenAvailableHeight > 0) ? NotificationManager.screenAvailableHeight : 1080

    width: 380
    height: Math.max(72, notifColumn.implicitHeight + 20)

    function updatePosition() {
        if (screenCorner === "top-left" || screenCorner === "bottom-left") {
            notifWindow.x = screenX + 20;
        } else {
            notifWindow.x = (screenX + screenW) - notifWindow.width - 20;
        }

        if (screenCorner === "top-left" || screenCorner === "top-right") {
            notifWindow.y = screenY + 20;
        } else {
            notifWindow.y = (screenY + screenH) - notifWindow.height - 20;
        }
    }

    onScreenCornerChanged: updatePosition()
    onScreenWChanged: updatePosition()
    onScreenHChanged: updatePosition()
    onHeightChanged: updatePosition()
    Component.onCompleted: {
        updatePosition();
        if (typeof NotificationManager !== "undefined" && NotificationManager) {
            NotificationManager.setupFramelessTransparentWindow(notifWindow);
        }
    }
    onVisibleChanged: {
        if (visible) {
            updatePosition();
            if (typeof NotificationManager !== "undefined" && NotificationManager) {
                NotificationManager.setupFramelessTransparentWindow(notifWindow);
            }
        }
    }

    // Automatically show window on the screen only when active notifications exist
    visible: notifModel.count > 0

    signal actionTriggered(string notifId, string action, string channel)
    signal dismissed(string notifId)

    ListModel {
        id: notifModel
    }

    Connections {
        target: NotificationManager
        ignoreUnknownSignals: true
        function onNotificationTriggered(notification) {
            notifWindow.showNotification(notification);
        }
        function onNotificationDismissed(id) {
            notifWindow.removeNotification(id);
        }
        function onNotificationsCleared() {
            notifWindow.clearAll();
        }
    }

    function showNotification(options) {
        if (!options) return;
        var id = options.id || options.notifId || ("notif_" + Date.now() + "_" + Math.floor(Math.random() * 1000));
        var duration = options.duration || 4500;
        var notifObj = {
            notifId: id,
            title: options.title || "NeoNect",
            body: options.body || "",
            type: options.type || "message", // "message", "friend_request", "security", "system", "success", "error"
            avatar: options.avatar || "",
            actionText: options.actionText || (options.type === "message" ? "Reply" : ""),
            channel: options.channel || "",
            duration: duration
        };

        // If duplicate id exists, update it, otherwise prepend
        for (var i = 0; i < notifModel.count; ++i) {
            if (notifModel.get(i).notifId === id) {
                notifModel.set(i, notifObj);
                return;
            }
        }

        // Limit stack to maximum 3 pills on screen
        if (notifModel.count >= 3) {
            notifModel.remove(notifModel.count - 1);
        }

        notifModel.insert(0, notifObj);

        updatePosition();
        notifWindow.visible = true;
        notifWindow.show();
        notifWindow.raise();

        console.log("--> [NotificationPill] Screen pill displayed at: (" + notifWindow.x + ", " + notifWindow.y + ") size: " + notifWindow.width + "x" + notifWindow.height + " | Title: " + notifObj.title);
    }

    function removeNotification(id) {
        for (var i = 0; i < notifModel.count; ++i) {
            if (notifModel.get(i).notifId === id) {
                notifModel.remove(i);
                notifWindow.dismissed(id);
                if (typeof NotificationManager !== "undefined" && NotificationManager) {
                    NotificationManager.dismissNotification(id);
                }
                break;
            }
        }
        if (notifModel.count === 0) {
            notifWindow.visible = false;
        }
    }

    function clearAll() {
        notifModel.clear();
        notifWindow.visible = false;
    }

    // Helper function for deterministic Telegram avatar gradient
    function getAvatarGradient(name) {
        var str = (name && name.length > 0) ? name : "N";
        var code = str.charCodeAt(0);
        var gradients = [
            { c1: "#0A84FF", c2: "#00D2FF" }, // Telegram Cyan-Blue
            { c1: "#8B5CF6", c2: "#C084FC" }, // Violet / Lilac
            { c1: "#10B981", c2: "#34D399" }, // Emerald
            { c1: "#F59E0B", c2: "#FBBF24" }, // Amber Warm
            { c1: "#EC4899", c2: "#F472B6" }  // Rose
        ];
        return gradients[code % gradients.length];
    }

    Column {
        id: notifColumn
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: (screenCorner === "bottom-left" || screenCorner === "bottom-right") ? parent.bottom : undefined
        anchors.top: (screenCorner === "top-left" || screenCorner === "top-right") ? parent.top : undefined
        anchors.margins: 10
        width: 360
        spacing: 10

        Repeater {
            model: notifModel

            delegate: Item {
                id: pillWrapper
                width: 360
                height: 52

                property bool isHovered: false
                property real remainingTime: model.duration
                property real totalTime: model.duration
                property var grad: notifWindow.getAvatarGradient(model.title)

                // Entrance micro-animation
                opacity: 1.0
                scale: 1.0
                x: 0

                NumberAnimation on opacity {
                    from: 0.0; to: 1.0; duration: 220; easing.type: Easing.OutCubic
                }
                NumberAnimation on scale {
                    from: 0.88; to: 1.0; duration: 260; easing.type: Easing.OutBack
                }
                NumberAnimation on x {
                    from: (notifWindow.screenCorner === "top-left" || notifWindow.screenCorner === "bottom-left") ? -80 : 80
                    to: 0
                    duration: 260
                    easing.type: Easing.OutBack
                }

                // Timer for auto-dismiss with pause-on-hover support
                Timer {
                    id: dismissTimer
                    interval: 100
                    running: !pillWrapper.isHovered
                    repeat: true
                    onTriggered: {
                        pillWrapper.remainingTime -= 100;
                        if (pillWrapper.remainingTime <= 0) {
                            stop();
                            notifWindow.removeNotification(model.notifId);
                        }
                    }
                }

                // ─── 1. OUTER SOFT AMBIENT DROP SHADOW ───
                Rectangle {
                    anchors.fill: telegramPill
                    anchors.margins: -3
                    radius: telegramPill.radius + 3
                    color: Qt.rgba(0, 0, 0, 0.55)
                    z: 0
                }

                // ─── 2. TELEGRAM-STYLE NOTIFICATION PILL CONTAINER ───
                Rectangle {
                    id: telegramPill
                    anchors.fill: parent
                    radius: 26 // Full Telegram pill curve
                    color: "#17212B" // Solid Telegram dark slate obsidian
                    border.color: {
                        if (model.type === "error") return Qt.rgba(255, 82, 82, 0.45);
                        if (model.type === "success") return Qt.rgba(35, 165, 90, 0.45);
                        if (model.type === "security") return Qt.rgba(250, 168, 26, 0.45);
                        return pillWrapper.isHovered ? Qt.rgba(0, 210, 255, 0.4) : Qt.rgba(255, 255, 255, 0.14);
                    }
                    border.width: 1
                    clip: true
                    z: 1

                    // Subtle inner gradient glass highlight
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(255, 255, 255, 0.06) }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    // ─── 3. HAIRLINE PROGRESS INDICATOR ALONG BOTTOM ───
                    Rectangle {
                        id: progressIndicator
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.bottomMargin: 1
                        height: 2
                        radius: 1
                        width: Math.max(0, (telegramPill.width - 40) * (pillWrapper.remainingTime / pillWrapper.totalTime))
                        color: {
                            if (model.type === "error") return "#FF5252";
                            if (model.type === "success") return "#23A55A";
                            if (model.type === "security") return "#FAA81A";
                            return "#0A84FF";
                        }
                        opacity: pillWrapper.isHovered ? 0.3 : 0.85
                        Behavior on width { NumberAnimation { duration: 100 } }
                    }

                    // ─── 4. PILL CONTENT ROW ───
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 12
                        spacing: 10

                        // A. CIRCULAR SENDER AVATAR WITH GRADIENT
                        Item {
                            width: 36; height: 36
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                id: avatarCircle
                                anchors.fill: parent
                                radius: 18
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: pillWrapper.grad.c1 }
                                    GradientStop { position: 1.0; color: pillWrapper.grad.c2 }
                                }
                                border.color: Qt.rgba(255, 255, 255, 0.2)
                                border.width: 1

                                Text {
                                    visible: model.type === "message" || model.avatar !== ""
                                    anchors.centerIn: parent
                                    text: {
                                        if (model.avatar && model.avatar.length > 0) return model.avatar.charAt(0).toUpperCase();
                                        if (model.title && model.title.length > 0) return model.title.charAt(0).toUpperCase();
                                        return "@";
                                    }
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.bold: true
                                    font.pixelSize: 15
                                }

                                IconImage {
                                    visible: model.type !== "message" && model.avatar === ""
                                    anchors.centerIn: parent
                                    source: {
                                        if (model.type === "security") return "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg";
                                        if (model.type === "success") return "qrc:/qt/qml/NeoNect/assets/icons/check.svg";
                                        if (model.type === "friend_request") return "qrc:/qt/qml/NeoNect/assets/icons/friends.svg";
                                        return "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg";
                                    }
                                    width: 16; height: 16
                                    color: "#FFFFFF"
                                }
                            }

                            // Small vibrant online indicator dot on avatar rim
                            Rectangle {
                                width: 9; height: 9
                                radius: 4.5
                                color: "#23A55A"
                                border.color: "#17212B"
                                border.width: 1.5
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: model.type === "message"
                            }
                        }

                        // B. SENDER NAME + PREVIEW TYPOGRAPHY
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 1

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: model.title
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    font.bold: true
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: "now"
                                    color: "#7E8794"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                text: model.body !== "" ? model.body : "New message"
                                color: "#9FA8B4"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                Layout.fillWidth: true
                            }
                        }

                        // C. QUICK ACTION PILL BUTTON (e.g., "Reply")
                        Rectangle {
                            visible: model.actionText !== ""
                            height: 24
                            implicitWidth: actionTextElem.implicitWidth + 14
                            radius: 12
                            color: replyMouse.containsMouse ? "#0A84FF" : Qt.rgba(10, 132, 255, 0.18)
                            border.color: Qt.rgba(10, 132, 255, 0.45)
                            border.width: 1
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                id: actionTextElem
                                anchors.centerIn: parent
                                text: model.actionText
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            MouseArea {
                                id: replyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    notifWindow.actionTriggered(model.notifId, "reply", model.channel);
                                    notifWindow.removeNotification(model.notifId);
                                }
                            }
                        }

                        // D. CIRCULAR DISMISS BUTTON "✕"
                        Rectangle {
                            id: closeBtn
                            width: 22; height: 22
                            radius: 11
                            color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: closeMouse.containsMouse ? "#FFFFFF" : "#7E8794"
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: notifWindow.removeNotification(model.notifId)
                            }
                        }
                    }

                    // ─── 5. FULL-PILL HOVER & CLICK AREA ───
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        z: -1
                        onEntered: pillWrapper.isHovered = true
                        onExited: pillWrapper.isHovered = false
                        onClicked: {
                            if (model.channel && model.channel !== "") {
                                notifWindow.actionTriggered(model.notifId, "open", model.channel);
                            }
                            notifWindow.removeNotification(model.notifId);
                        }
                    }
                }
            }
        }
    }
}
