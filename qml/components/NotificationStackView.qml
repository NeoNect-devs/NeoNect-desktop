// qml/components/NotificationStackView.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Item {
    id: notifRoot
    anchors.top: parent.top
    anchors.right: parent.right
    anchors.topMargin: 56
    anchors.rightMargin: 16
    width: 360
    height: notifColumn.implicitHeight
    z: 10000

    signal actionTriggered(string notifId, string action, string channel)
    signal dismissed(string notifId)

    ListModel {
        id: notifModel
    }

    function showNotification(options) {
        // options: { id, title, body, type, avatar, actionText, channel, duration }
        var id = options.id || ("notif_" + Date.now() + "_" + Math.floor(Math.random() * 1000));
        var duration = options.duration || 5000;
        var notifObj = {
            notifId: id,
            title: options.title || "Avila Notification",
            body: options.body || "",
            type: options.type || "message", // "message", "friend_request", "security", "system", "success", "error"
            avatar: options.avatar || "",
            actionText: options.actionText || "",
            channel: options.channel || "",
            duration: duration,
            progress: 1.0
        };

        // If duplicate id exists, update it, otherwise prepend
        for (var i = 0; i < notifModel.count; ++i) {
            if (notifModel.get(i).notifId === id) {
                notifModel.set(i, notifObj);
                return;
            }
        }

        // Limit stack to 4 toasts
        if (notifModel.count >= 4) {
            notifModel.remove(notifModel.count - 1);
        }

        notifModel.insert(0, notifObj);
    }

    function removeNotification(id) {
        for (var i = 0; i < notifModel.count; ++i) {
            if (notifModel.get(i).notifId === id) {
                notifModel.remove(i);
                notifRoot.dismissed(id);
                break;
            }
        }
    }

    function clearAll() {
        notifModel.clear();
    }

    Column {
        id: notifColumn
        width: parent.width
        spacing: 8

        Repeater {
            model: notifModel

            delegate: Rectangle {
                id: toastCard
                width: notifRoot.width
                height: Math.max(68, cardLayout.implicitHeight + 16)
                radius: 12
                color: "#121418"
                border.color: {
                    if (model.type === "error") return "#FF5252";
                    if (model.type === "success") return "#23A55A";
                    if (model.type === "friend_request") return "#00E5FF";
                    if (model.type === "security") return "#FAA81A";
                    return Qt.rgba(255, 255, 255, 0.12);
                }
                border.width: 1

                // Enter / Exit animation
                opacity: 0.0
                x: 80
                Component.onCompleted: {
                    opacity = 1.0;
                    x = 0;
                }

                Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                // Dismiss timer & progress
                Timer {
                    id: toastTimer
                    interval: model.duration
                    running: true
                    repeat: false
                    onTriggered: notifRoot.removeNotification(model.notifId)
                }

                // Glassmorphism background subtle glow
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: {
                        if (model.type === "error") return Qt.rgba(255, 82, 82, 0.08);
                        if (model.type === "success") return Qt.rgba(35, 165, 90, 0.08);
                        if (model.type === "friend_request") return Qt.rgba(0, 229, 255, 0.08);
                        if (model.type === "security") return Qt.rgba(250, 168, 26, 0.08);
                        return Qt.rgba(10, 132, 255, 0.06);
                    }
                }

                // Progress Bar at Bottom
                Rectangle {
                    id: progressBar
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 4
                    anchors.bottomMargin: 2
                    height: 2.5
                    radius: 1.25
                    width: parent.width - 8
                    color: {
                        if (model.type === "error") return "#FF5252";
                        if (model.type === "success") return "#23A55A";
                        if (model.type === "friend_request") return "#00E5FF";
                        if (model.type === "security") return "#FAA81A";
                        return "#0A84FF";
                    }

                    NumberAnimation on width {
                        from: toastCard.width - 8
                        to: 0
                        duration: model.duration
                        running: true
                    }
                }

                RowLayout {
                    id: cardLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    anchors.bottomMargin: 8
                    spacing: 10

                    // 1. Avatar / Category Icon
                    Rectangle {
                        width: 38; height: 38
                        radius: 10
                        color: {
                            if (model.type === "error") return Qt.rgba(255, 82, 82, 0.2);
                            if (model.type === "success") return Qt.rgba(35, 165, 90, 0.2);
                            if (model.type === "friend_request") return Qt.rgba(0, 229, 255, 0.2);
                            if (model.type === "security") return Qt.rgba(250, 168, 26, 0.2);
                            return Qt.rgba(10, 132, 255, 0.2);
                        }
                        border.color: Qt.rgba(255, 255, 255, 0.1)
                        border.width: 1
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            visible: model.avatar !== "" || model.type === "message"
                            anchors.centerIn: parent
                            text: model.avatar !== "" ? model.avatar.charAt(0).toUpperCase() : (model.title ? model.title.charAt(0).toUpperCase() : "@")
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.bold: true
                            font.pixelSize: 16
                        }

                        IconImage {
                            visible: model.avatar === "" && model.type !== "message"
                            anchors.centerIn: parent
                            source: {
                                if (model.type === "friend_request") return "qrc:/qt/qml/Avila/assets/icons/friends.svg";
                                if (model.type === "security") return "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg";
                                if (model.type === "success") return "qrc:/qt/qml/Avila/assets/icons/check.svg";
                                return "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg";
                            }
                            width: 18; height: 18
                            color: {
                                if (model.type === "error") return "#FF5252";
                                if (model.type === "success") return "#23A55A";
                                if (model.type === "friend_request") return "#00E5FF";
                                if (model.type === "security") return "#FAA81A";
                                return "#0A84FF";
                            }
                        }
                    }

                    // 2. Title & Body Content
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Layout.alignment: Qt.AlignVCenter

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
                                color: "#80848E"
                                font.family: "Segoe UI"
                                font.pixelSize: 10
                            }
                        }

                        Text {
                            text: model.body
                            color: "#B5BAC1"
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    // 3. Action Button (Optional e.g. "Reply" or "Open")
                    Rectangle {
                        visible: model.actionText !== ""
                        height: 26
                        implicitWidth: actionBtnText.implicitWidth + 16
                        radius: 6
                        color: actionBtnMouse.containsMouse ? "#0A84FF" : Qt.rgba(10, 132, 255, 0.25)
                        border.color: Qt.rgba(10, 132, 255, 0.5)
                        border.width: 1
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            id: actionBtnText
                            anchors.centerIn: parent
                            text: model.actionText
                            color: "#FFFFFF"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: actionBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                notifRoot.actionTriggered(model.notifId, model.actionText, model.channel);
                                notifRoot.removeNotification(model.notifId);
                            }
                        }
                    }

                    // 4. Close Button
                    Rectangle {
                        width: 22; height: 22
                        radius: 11
                        color: closeBtnMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: closeBtnMouse.containsMouse ? "#FFFFFF" : "#80848E"
                            font.pixelSize: 10
                        }

                        MouseArea {
                            id: closeBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: notifRoot.removeNotification(model.notifId)
                        }
                    }
                }

                // Click toast body to trigger default action
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (model.channel !== "") {
                            notifRoot.actionTriggered(model.notifId, "open", model.channel);
                        }
                        notifRoot.removeNotification(model.notifId);
                    }
                }
            }
        }
    }
}
