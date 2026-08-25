import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import Avila.Core 1.0
import Avila 1.0
import "components"
import "containers"

Item {
    id: root

    property string activeChannel: "general"
    property string selectedServer: "server1"

    ChatMessageModel {
        id: nativeMessageModel
    }

    Connections {
        target: NetworkManager

        function onIncomingRelayMessageReceived(fromUsername, text, timestamp) {
            var senderLower = fromUsername.toLowerCase()
            var dmKey = "dms:" + senderLower
            if (!chatHistories[dmKey]) {
                chatHistories[dmKey] = []
            }
            chatHistories[dmKey].push({ text: text, fromMe: false, sender: fromUsername, avatar: fromUsername.charAt(0).toUpperCase() })

            // Auto add friend to Direct Messages list if not already in friends list
            if (NetworkManager && NetworkManager.friends && NetworkManager.friends.indexOf(senderLower) === -1) {
                NetworkManager.addFriend(senderLower)
            }

            // Insert message into active model if viewing this DM or general channel
            if ((root.selectedServer === "dms" && root.activeChannel.toLowerCase() === senderLower) ||
                (root.selectedServer !== "dms" && root.activeChannel === "general")) {
                nativeMessageModel.insertMessage(text, false, fromUsername, fromUsername.charAt(0).toUpperCase())
                scrollTimer.restart()
            }
        }

    }

    property bool showAddFriendModal: false
    property string addFriendStatusMsg: ""
    property bool addFriendSuccess: false

    Connections {
        target: NetworkManager
        function onAddFriendResult(success, message, username) {
            root.addFriendSuccess = success;
            root.addFriendStatusMsg = message;
            if (success) {
                root.selectedServer = "dms";
                root.activeChannel = username.toLowerCase();
            }
        }
    }

    property var defaultHistories: ({
        "server1:welcome-rules": [
            { text: "Welcome to Danisa / Avila secure E2EE node server!", fromMe: false, sender: "System", avatar: "🛠️" },
            { text: "All conversations here are relayed client-side using OpenSSL 4.0.0 AES-256-GCM.", fromMe: false, sender: "System", avatar: "🛠️" }
        ],
        "server1:general": [
            { text: "Hello! Is anyone online?", fromMe: false, sender: "Alex", avatar: "A" },
            { text: "Hey Alex! Yes, testing live E2EE relay messaging.", fromMe: false, sender: "Beatrice", avatar: "B" }
        ],
        "dms:alex": [
            { text: "Hey Alex, are you available for a quick sync later today?", fromMe: true, sender: "Me", avatar: "" },
            { text: "Sure! Just ping me here when you're ready.", fromMe: false, sender: "Alex", avatar: "A" }
        ]
    })

    property var chatHistories: ({})

    function getAvatarColor(sender) {
        if (sender === "Alex") return "#5865F2"
        if (sender === "Beatrice") return "#EB459E"
        if (sender === "Charlie") return "#9B59B6"
        if (sender === "David") return "#F1C40F"
        if (sender === "System") return "#1ABC9C"
        return "#4F545C"
    }

    function switchChannel() {
        var key = selectedServer + ":" + activeChannel
        nativeMessageModel.clearActiveViewportStore()

        if (!chatHistories[key]) {
            var defaults = defaultHistories[key] || []
            chatHistories[key] = JSON.parse(JSON.stringify(defaults))
        }

        var history = chatHistories[key]
        for (var i = 0; i < history.length; ++i) {
            var msg = history[i]
            nativeMessageModel.insertMessage(msg.text, msg.fromMe, msg.sender, msg.avatar)
        }

        scrollTimer.restart()
    }

    Timer {
        id: scrollTimer
        interval: 50
        repeat: false
        onTriggered: messageListView.positionViewAtEnd()
    }

    onActiveChannelChanged: switchChannel()
    onSelectedServerChanged: switchChannel()
    Component.onCompleted: switchChannel()

    Rectangle {
        anchors.fill: parent
        color: ThemeData.windowBackground
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Full-width Header Bar
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: ThemeData.panelBackground
            border.color: Qt.darker(ThemeData.panelBackground, 1.25)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                IconImage {
                    visible: root.selectedServer !== "dms"
                    source: "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                    width: 18; height: 18
                    color: ThemeData.textSecondary
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    visible: root.selectedServer === "dms"
                    text: "@"
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 20
                    font.weight: Font.Light
                }

                Text {
                    text: root.selectedServer === "dms" ? root.activeChannel.replace(/^\w/, c => c.toUpperCase()) : root.activeChannel
                    color: ThemeData.textPrimary
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                Rectangle {
                    width: 1; height: 16
                    color: ThemeData.textSecondary
                    opacity: 0.3
                    Layout.leftMargin: 4; Layout.rightMargin: 4
                }

                Text {
                    Layout.fillWidth: true
                    text: root.selectedServer === "dms" ? "Danisa Zero-Knowledge E2EE Direct Messages" : "Secure Workspace Channel"
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Rectangle {
                    width: 32; height: 32
                    radius: 6
                    visible: root.selectedServer !== "dms"
                    color: membersToggleMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"

                    IconImage {
                        anchors.centerIn: parent
                        source: "qrc:/qt/qml/Avila/assets/icons/users.svg"
                        width: 20; height: 20
                        color: membersPanel.expanded ? ThemeData.textPrimary : ThemeData.textSecondary
                    }

                    MouseArea {
                        id: membersToggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: membersPanel.userToggledExpanded = !membersPanel.userToggledExpanded
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: messageListView
                        anchors.fill: parent
                        model: nativeMessageModel
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar {
                            id: chatScrollBar
                            parent: messageListView
                            anchors.top: messageListView.top
                            anchors.right: messageListView.right
                            anchors.bottom: messageListView.bottom
                            width: 8
                            policy: ScrollBar.AsNeeded
                            active: true
                            palette.window: "transparent"
                            palette.base: "transparent"

                            contentItem: Rectangle {
                                implicitWidth: 8
                                radius: 4
                                color: chatScrollBar.pressed ? ThemeData.accentColor : (chatScrollBar.hovered ? "#7289DA" : "#4E5058")
                            }
                            background: Item {}
                        }

                        delegate: Rectangle {
                            width: messageListView.width - 12
                            height: model.isFirstInBlock ? Math.max(50, messageTextLabel.implicitHeight + 36) : Math.max(22, messageTextLabel.implicitHeight + 6)
                            color: itemMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.04) : "transparent"

                            MouseArea {
                                id: itemMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16; anchors.rightMargin: 16
                                spacing: 12

                                Item {
                                    Layout.preferredWidth: 38; Layout.preferredHeight: 38
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: model.isFirstInBlock ? 6 : 0
                                    visible: model.isFirstInBlock

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 19
                                        color: getAvatarColor(model.senderName)

                                        Text {
                                            anchors.centerIn: parent
                                            text: model.senderAvatar !== "" ? model.senderAvatar : model.senderName.charAt(0).toUpperCase()
                                            color: "#ffffff"
                                            font.bold: true
                                        }
                                    }
                                }

                                Item {
                                    Layout.preferredWidth: 38; Layout.preferredHeight: 1
                                    visible: !model.isFirstInBlock
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: model.isFirstInBlock ? 4 : 2

                                    RowLayout {
                                        spacing: 8
                                        visible: model.isFirstInBlock

                                        Text {
                                            text: model.senderName
                                            color: model.fromMe ? ThemeData.accentColor : ThemeData.textPrimary
                                            font.bold: true
                                        }

                                        Text {
                                            text: "Today at " + Qt.formatTime(new Date(), "hh:mm AP")
                                            color: ThemeData.textSecondary
                                            font.pixelSize: 11
                                        }
                                    }

                                    Text {
                                        id: messageTextLabel
                                        Layout.fillWidth: true
                                        text: model.text
                                        color: ThemeData.textPrimary
                                        font.pixelSize: 14
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                AvilaMessageInput {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    channelName: root.selectedServer === "dms" ? root.activeChannel.replace("dm-", "").replace(/^\w/, c => c.toUpperCase()) : root.activeChannel
                    isDM: root.selectedServer === "dms"

                    onMessageSent: function(msgText) {
                        var key = root.selectedServer + ":" + root.activeChannel
                        if (!root.chatHistories[key]) {
                            root.chatHistories[key] = []
                        }
                        root.chatHistories[key].push({ text: msgText, fromMe: true, sender: "Me", avatar: "" })
                        nativeMessageModel.insertMessage(msgText, true, "Me", "")
                        messageListView.positionViewAtEnd()

                        // Send E2EE Relay Message via Danisa REST API
                        var target = root.selectedServer === "dms" ? root.activeChannel : "general"
                        NetworkManager.sendRelayMessage(target, msgText)
                    }
                }
            }

            MembersSidebarPanel {
                id: membersPanel
                Layout.fillHeight: true
                selectedServer: root.selectedServer
                expanded: root.selectedServer !== "dms" && userToggledExpanded
            }
        }
    }

    // ─── ADD FRIEND / DIRECT CHAT MODAL OVERLAY ───────────────────────────
    Rectangle {
        id: addFriendOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)
        visible: root.showAddFriendModal
        z: 9999

        MouseArea {
            anchors.fill: parent
            onClicked: root.showAddFriendModal = false
        }

        Rectangle {
            width: 380; height: 260
            radius: 16
            color: ThemeData.panelBackground
            border.color: Qt.rgba(1, 1, 1, 0.1)
            anchors.centerIn: parent

            MouseArea { anchors.fill: parent } // Block clicks from closing modal

            Column {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                RowLayout {
                    width: parent.width

                    Text {
                        text: "Add Friend / Start Direct Chat"
                        color: ThemeData.textPrimary
                        font.bold: true
                        font.pixelSize: 16
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "✕"
                        color: ThemeData.textSecondary
                        font.pixelSize: 16
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.showAddFriendModal = false
                        }
                    }
                }

                Text {
                    text: "Enter the username of a user on the Danisa network to add them to your Direct Messages."
                    color: ThemeData.textSecondary
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                AvilaTextField {
                    id: friendInput
                    width: parent.width
                    placeholderText: "Enter username (e.g. alex)"
                }

                Text {
                    text: root.addFriendStatusMsg
                    color: root.addFriendSuccess ? "#23a55a" : "#ef5350"
                    font.pixelSize: 12
                    visible: text !== ""
                }

                Row {
                    width: parent.width
                    spacing: 12

                    AvilaButton {
                        text: "Cancel"
                        width: (parent.width - 12) / 2
                        highlighted: false
                        onClicked: {
                            root.showAddFriendModal = false
                            root.addFriendStatusMsg = ""
                        }
                    }

                    AvilaButton {
                        text: "Add Friend"
                        width: (parent.width - 12) / 2
                        enabled: friendInput.text.trim() !== ""
                        highlighted: true
                        onClicked: {
                            root.addFriendStatusMsg = ""
                            NetworkManager.addFriend(friendInput.text.trim())
                        }
                    }
                }
            }
        }
    }
}