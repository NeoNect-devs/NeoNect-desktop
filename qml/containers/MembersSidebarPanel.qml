import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0
import "../components"

Rectangle {
    id: membersRoot

    property string selectedServer: ""
    property bool expanded: false
    property bool userToggledExpanded: false

    readonly property bool isDmOrEmpty: selectedServer === "dms" || selectedServer === ""

    Layout.fillHeight: true
    Layout.preferredWidth: (!isDmOrEmpty && expanded) ? 180 : 0
    width: (!isDmOrEmpty && expanded) ? 180 : 0
    implicitWidth: width
    visible: !isDmOrEmpty && expanded && width > 0
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

    function getStatusColor(st) {
        switch(st) {
            case "online": return "#23A55A"; // Emerald Green
            case "afk":
            case "idle": return "#FAA81A";   // Amber Yellow (AFK / Idle)
            case "dnd": return "#F23F43";    // Crimson Red (Do Not Disturb)
            case "offline":
            default: return "#80848E";       // Muted Gray (Offline)
        }
    }

    property var serverMembersMap: ({})

    ListModel { id: membersModel }

    function refreshModel() {
        membersModel.clear()
        if (selectedServer === "dms") return

        // Insert current logged in user at top of online members list
        if (NetworkManager && NetworkManager.currentUsername && NetworkManager.currentUsername !== "") {
            var myDn = (NetworkManager.displayName && NetworkManager.displayName !== "") ? NetworkManager.displayName : NetworkManager.currentUsername;
            membersModel.append({
                name: myDn + " (You)",
                role: "@" + NetworkManager.currentUsername,
                status: "online",
                avatarColor: "#0A84FF"
            })
        }

        var list = serverMembersMap[selectedServer] || []
        for (var i = 0; i < list.length; i++) {
            membersModel.append(list[i])
        }
    }

    Connections {
        target: NetworkManager
        function onFriendsChanged() {
            refreshModel()
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
        spacing: 0

        // Search Box in Members Panel
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "transparent"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                radius: 6
                color: "#18191D"
                border.color: searchInput.activeFocus ? ThemeData.accentColor : "#2B2D31"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    spacing: 6

                    IconImage {
                        source: "qrc:/qt/qml/NeoNect/assets/icons/search.svg"
                        width: 14; height: 14
                        color: ThemeData.textSecondary
                    }

                    TextInput {
                        id: searchInput
                        Layout.fillWidth: true
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        clip: true
                        selectByMouse: true

                        Text {
                            text: "Search members..."
                            color: ThemeData.textMuted
                            font.family: "Segoe UI"
                            font.pixelSize: 12
                            visible: !searchInput.text && !searchInput.activeFocus
                        }
                    }
                }
            }
        }

        // Members Count Header
        Rectangle {
            Layout.fillWidth: true
            height: 24
            color: "transparent"

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "MEMBERS — " + membersModel.count
                color: ThemeData.textSecondary
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 0.5
            }
        }

        // Members ListView
        ListView {
            id: membersListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: membersModel

            ScrollBar.vertical: ScrollBar {
                id: membersScrollBar
                width: 8
                policy: ScrollBar.AsNeeded
                visible: membersScrollBar.size < 1.0
                active: membersListView.moving || membersScrollBar.hovered || membersScrollBar.pressed

                background: Rectangle {
                    color: "transparent"
                }

                contentItem: Rectangle {
                    implicitWidth: 8
                    radius: 4
                    color: membersScrollBar.pressed ? "#6E727A" : (membersScrollBar.hovered ? "#4E5058" : "#2B2D31")
                    opacity: (membersScrollBar.size < 1.0 && (membersScrollBar.active || membersScrollBar.hovered)) ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 150 } }
                }
            }

            delegate: Rectangle {
                width: membersListView.width - 6
                height: 42
                radius: 6
                color: itemMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.05) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6; anchors.rightMargin: 6
                    spacing: 8

                    Item {
                        width: 28; height: 35
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            id: memberAvatarBox
                            width: 28; height: 28
                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            radius: 7
                            color: model.avatarColor ? model.avatarColor : "#0A84FF"

                            CircularImage {
                                id: memberAvatarImg
                                anchors.fill: parent
                                source: (typeof NetworkManager !== "undefined" && NetworkManager && model.name) ? NetworkManager.getAvatarUrl(model.name) : ""
                                cornerRadius: 7
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !memberAvatarImg.ready
                                text: (typeof NetworkManager !== "undefined" && NetworkManager && model.name) ? NetworkManager.getDisplayName(model.name).charAt(0).toUpperCase() : (model.name ? model.name.charAt(0).toUpperCase() : "?")
                                color: "#FFFFFF"
                                font.bold: true
                                font.pixelSize: 12
                            }
                        }

                        // Horizontal Status Pill Under Member Avatar
                        Rectangle {
                            id: memberStatusPill
                            width: 20
                            height: 6
                            radius: 3
                            color: getStatusColor(model.status)
                            border.color: ThemeData.panelBackground
                            border.width: 1
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
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