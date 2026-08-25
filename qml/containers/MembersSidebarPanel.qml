import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0

Rectangle {
    id: membersRoot

    property string selectedServer: "server1"
    property bool expanded: true
    property bool userToggledExpanded: true

    Layout.fillHeight: true
    Layout.preferredWidth: expanded ? 180 : 0
    width: expanded ? 180 : 0
    implicitWidth: width
    visible: width > 0
    clip: true
    color: ThemeData.panelBackground
    border.color: Qt.darker(ThemeData.panelBackground, 1.25)
    border.width: 1

    Behavior on width {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }
    Behavior on Layout.preferredWidth {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
    }

    property var serverMembersMap: ({
        "server1": [
            { name: "Alex (Admin)", role: "ADMIN", status: "online", avatarColor: "#5865F2" },
            { name: "Beatrice", role: "DEVELOPER", status: "online", avatarColor: "#EB459E" },
            { name: "Charlie", role: "MEMBER", status: "online", avatarColor: "#9B59B6" },
            { name: "David", role: "MEMBER", status: "offline", avatarColor: "#F1C40F" },
            { name: "Eva", role: "MODERATOR", status: "online", avatarColor: "#E91E63" },
            { name: "Frank", role: "MEMBER", status: "offline", avatarColor: "#3498DB" },
            { name: "Grace", role: "MEMBER", status: "online", avatarColor: "#2ECC71" },
            { name: "Henry", role: "MEMBER", status: "offline", avatarColor: "#E67E22" },
            { name: "Ivy", role: "DESIGNER", status: "online", avatarColor: "#1ABC9C" },
            { name: "Avila Bot", role: "BOT", status: "online", avatarColor: "#7289DA" }
        ],
        "server2": [
            { name: "Hannah", role: "OWNER", status: "online", avatarColor: "#FF5722" },
            { name: "Ian", role: "LEAD", status: "online", avatarColor: "#607D8B" },
            { name: "Jack", role: "MEMBER", status: "offline", avatarColor: "#9C27B0" },
            { name: "Karen", role: "MEMBER", status: "online", avatarColor: "#8BC34A" },
            { name: "Leo", role: "MEMBER", status: "offline", avatarColor: "#00BCD4" }
        ]
    })

    ListModel { id: membersModel }

    function refreshModel() {
        membersModel.clear()
        if (selectedServer === "dms") return

        var list = serverMembersMap[selectedServer] || serverMembersMap["server1"] || []
        for (var i = 0; i < list.length; i++) {
            membersModel.append(list[i])
        }
    }

    function addMember(serverId, memberObj) {
        if (!serverMembersMap[serverId]) {
            serverMembersMap[serverId] = []
        }
        serverMembersMap[serverId].push(memberObj)
        if (serverId === selectedServer) {
            membersModel.append(memberObj)
        }
    }

    onSelectedServerChanged: refreshModel()
    Component.onCompleted: refreshModel()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Text {
            text: "MEMBERS — " + membersModel.count
            color: ThemeData.textSecondary
            font.family: "Segoe UI"
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 1
            Layout.fillWidth: true
        }

        ListView {
            id: membersListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: membersModel
            spacing: 6
            clip: true

            ScrollBar.vertical: ScrollBar {
                id: membersScrollBar
                parent: membersListView
                anchors.top: membersListView.top
                anchors.right: membersListView.right
                anchors.bottom: membersListView.bottom
                width: 6
                policy: ScrollBar.AsNeeded
                active: true
                palette.window: "transparent"
                palette.base: "transparent"

                contentItem: Rectangle {
                    implicitWidth: 6
                    radius: 3
                    color: membersScrollBar.pressed ? ThemeData.accentColor : (membersScrollBar.hovered ? "#7289DA" : "#4E5058")
                }
                background: Item {}
            }

            delegate: Rectangle {
                width: membersListView.width - 6
                height: 36
                radius: 6
                color: itemMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4; anchors.rightMargin: 4
                    spacing: 8

                    Item {
                        width: 28; height: 28
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            anchors.fill: parent
                            radius: 14
                            color: model.avatarColor ? model.avatarColor : "#5865F2"

                            Text {
                                anchors.centerIn: parent
                                text: model.name ? model.name.charAt(0) : "?"
                                color: "#FFFFFF"
                                font.bold: true
                                font.pixelSize: 12
                            }
                        }

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: model.status === "online" ? "#23A55A" : "#80848E"
                            border.color: ThemeData.panelBackground
                            border.width: 1.5
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Text {
                            text: model.name || ""
                            color: ThemeData.textPrimary
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: model.role || ""
                            color: ThemeData.textSecondary
                            font.family: "Segoe UI"
                            font.pixelSize: 8
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }
}