// qml/MainSection/TopView/TopbarCanvas.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0

Rectangle {
    id: root
    property bool memberlistVisibility: true
    height: 50
    clip: true
    color: ThemeData.viewsBackground

    // Structural separating underline border box
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: "#232523"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 0

        // SERVER STATUS ICON / LOGO INDICATOR
        Rectangle {
            id: btnServerSetting
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter
            color: serverSettingMouse.containsMouse ? "#2A2C2A" : "#1E201E"
            radius: 8

            Behavior on color { ColorAnimation { duration: 100 } }

            Text {
                anchors.centerIn: parent
                text: "📡"
                font.pixelSize: 14
            }

            MouseArea {
                id: serverSettingMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }

        // DYNAMIC CONVERSATION CHANNEL TITLE
        Text {
            id: lblServerDesc
            // Dynamically prints active context based on selection matrix state rules
            text: (root.parent && currentActiveChannel.startsWith("dm-"))
                  ? "💬 " + currentActiveChannel.substring(3).toUpperCase()
                  : "#️⃣ " + currentActiveChannel

            font.family: "Segoe UI"
            font.pixelSize: 15
            font.weight: Font.Bold
            color: "#FFFFFF"

            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.alignment: Qt.AlignVCenter
        }

        // TOGGLE MEMBER LIST BUTTON ACTION WINDOW
        Rectangle {
            id: btnShowMemberlist
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Layout.alignment: Qt.AlignVCenter

            color: root.memberlistVisibility
                   ? Qt.rgba(0, 163, 108, 0.15)
                   : (showMemberMouseArea.containsMouse ? "#2A2C2A" : "transparent")

            border.color: root.memberlistVisibility ? "#00A36C" : "transparent"
            border.width: 1
            radius: 8

            Behavior on color { ColorAnimation { duration: 100 } }

            Text {
                anchors.centerIn: parent
                text: "👥"
                font.pixelSize: 14
                opacity: root.memberlistVisibility ? 1.0 : 0.7
            }

            MouseArea {
                id: showMemberMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.memberlistVisibility = !root.memberlistVisibility;
                }
            }
        }
    }
}
