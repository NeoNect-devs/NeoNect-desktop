import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: toastRoot

    property string message: ""
    property string type: "error" // "error", "warning", "info", "success"
    property string actionText: ""
    property bool active: false

    signal actionClicked()
    signal dismissed()

    function show(msg, toastType, action) {
        toastRoot.message = msg;
        toastRoot.type = toastType || "error";
        toastRoot.actionText = action || "";
        toastRoot.active = true;
        dismissTimer.restart();
    }

    function hide() {
        toastRoot.active = false;
        toastRoot.dismissed();
    }

    Timer {
        id: dismissTimer
        interval: 6000
        repeat: false
        onTriggered: toastRoot.hide()
    }

    visible: opacity > 0
    opacity: active ? 1.0 : 0.0
    y: active ? 16 : -60
    z: 9999

    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }
    Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutBack } }

    anchors.horizontalCenter: parent.horizontalCenter
    width: Math.min(parent.width - 32, Math.max(340, contentRow.implicitWidth + 32))
    height: 48
    radius: 10

    color: type === "error" ? "#1E1214" : (type === "warning" ? "#1E1A12" : (type === "success" ? "#121E15" : "#12151E"))
    border.color: type === "error" ? "#E53935" : (type === "warning" ? "#FFA000" : (type === "success" ? "#23A55A" : "#0A84FF"))
    border.width: 1

    RowLayout {
        id: contentRow
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            width: 28; height: 28
            radius: 14
            color: type === "error" ? Qt.rgba(229, 57, 53, 0.2) : (type === "warning" ? Qt.rgba(255, 160, 0, 0.2) : (type === "success" ? Qt.rgba(35, 165, 90, 0.2) : Qt.rgba(10, 132, 255, 0.2)))

            IconImage {
                anchors.centerIn: parent
                source: type === "error" ? "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg" : "qrc:/qt/qml/Avila/assets/icons/refresh.svg"
                width: 16; height: 16
                color: type === "error" ? "#E53935" : (type === "warning" ? "#FFA000" : (type === "success" ? "#23A55A" : "#0A84FF"))
            }
        }

        Text {
            Layout.fillWidth: true
            text: toastRoot.message
            color: ThemeData.textPrimary
            font.family: "Segoe UI"
            font.pixelSize: 13
            elide: Text.ElideRight
        }

        Rectangle {
            visible: toastRoot.actionText !== ""
            height: 28
            implicitWidth: actionLabel.implicitWidth + 16
            radius: 6
            color: actionMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1)

            Text {
                id: actionLabel
                anchors.centerIn: parent
                text: toastRoot.actionText
                color: ThemeData.textPrimary
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.bold: true
            }

            MouseArea {
                id: actionMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    toastRoot.actionClicked();
                    toastRoot.hide();
                }
            }
        }

        Text {
            text: "✕"
            color: ThemeData.textSecondary
            font.pixelSize: 14
            opacity: closeMouse.containsMouse ? 1.0 : 0.6

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: toastRoot.hide()
            }
        }
    }
}
