import QtQuick
import QtQuick.Layouts
import Avila 1.0
import "../components"

Rectangle {
    id: root
    property Window windowTarget
    property string appState: "gateway"
    property string titleText: ""
    property bool showBackButton: false
    signal backClicked
    signal brandClicked

    height: 45
    color: root.appState === "gateway" ? Qt.darker(ThemeData.mainWindowBackground, 1.15) : "transparent"
    z: 100

    // ─── TOP-LEFT OVERLAPPING BRAND BUTTON ────────────────────────────
    AvilaBrandButton {
        id: topBrandButton
        anchors.left: parent.left
        anchors.top: parent.top
        visible: root.appState === "authenticated"
        z: 101 // Floats above the window drag MouseArea
        onClicked: root.brandClicked()
    }

    // Window Drag Handler
    MouseArea {
        anchors.fill: parent
        property point clickPos: Qt.point(0, 0)
        onPressed: mouse => {
            clickPos = Qt.point(mouse.x, mouse.y);
        }
        onPositionChanged: mouse => {
            if (root.windowTarget.visibility === Window.Maximized) {
                let ratioX = mouse.x / root.windowTarget.width;
                root.windowTarget.visibility = Window.Windowed;
                clickPos.x = root.windowTarget.width * ratioX;
                clickPos.y = mouse.y;
                return;
            }
            let delta = Qt.point(mouse.x - clickPos.x, mouse.y - clickPos.y);
            root.windowTarget.x += delta.x;
            root.windowTarget.y += delta.y;
        }
        onDoubleClicked: {
            if (root.appState === "authenticated")
                root.windowTarget.visibility = (root.windowTarget.visibility === Window.Maximized) ? Window.Windowed : Window.Maximized;
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
        Rectangle {
            Layout.rightMargin: 10
            Layout.alignment: Qt.AlignVCenter
            width: 35
            height: 35
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
            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 32
                radius: 4
                color: minM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "—"
                    color: "white"
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
                    color: "white"
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
                    color: "white"
                }
                MouseArea {
                    id: authCloseM
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.windowTarget.close()
                }
            }
        }
    }
}
