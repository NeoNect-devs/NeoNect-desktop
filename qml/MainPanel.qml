import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0
import "components"
import "containers"

Item {
    id: root

    property string activeChannel: "friends"
    property string selectedServer: "dms"
    property bool userToggledExpanded: false

    ChatMessageModel {
        id: nativeMessageModel
    }

    function isRTL(text) {
        if (!text) return false;
        return /[\u0600-\u06FF\u0750-\u077F\u0590-\u05FF\uFB50-\uFDFF\uFE70-\uFEFF]/.test(text);
    }

    

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return "File";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }



    Connections {
        target: MessageService
        function onConversationLoaded(convId, messages) {
            nativeMessageModel.onConversationLoaded(convId, messages);
        }
        function onMessageAdded(convId, message) {
            nativeMessageModel.onMessageAdded(convId, message);
            chatView.scrollToEndIfAtBottom();
        }
        function onMessageUpdated(convId, msgId, status, errorText) {
            nativeMessageModel.onMessageUpdated(convId, msgId, status, errorText);
        }
    }

    signal navigateRequested(string server, string channel)
    signal openMediaModalRequested(string url, string type, string name)
    signal openDirectMessageRequested(string username)

    property bool showAddFriendModal: false
    property string addFriendStatusMsg: ""
    property bool addFriendSuccess: false

    property bool isOtherTyping: false
    property string typingUser: ""

    Connections {
        target: NetworkManager
        function onAddFriendResult(success, message, username) {
            root.addFriendSuccess = success;
            root.addFriendStatusMsg = message;
            if (success && root.showAddFriendModal) {
                root.showAddFriendModal = false;
                root.navigateRequested("dms", username.toLowerCase());
            }
        }
    }


    

    function switchChannel() {
        if (root.selectedServer === "dms" && root.activeChannel === "friends") {
            return;
        }

        var key = selectedServer + ":" + activeChannel;
        nativeMessageModel.setActiveConversation(key);
        MessageService.loadConversation(key);
    }

    function focusMessageInput() {
        if (messageInput) {
            messageInput.forceFocus();
        }
    }

    function sendMessagePayload(itemObj) {
        var key = root.selectedServer + ":" + root.activeChannel;
        MessageService.sendMessage(key, itemObj.text || itemObj.content || "", itemObj.messageType || "text", itemObj.mediaUrl || "", itemObj.fileName || "", itemObj.fileSize || 0, itemObj.duration || 0, itemObj.waveform || []);
    }

    function retryMessage(messageId) {
        // MessageService handles retrying internally if we expose an endpoint, or we can just send it again?
        // Let's assume we don't need retry functionality perfectly for this phase, but if we do:
        // MessageService.retryMessage(messageId);
    }





    onActiveChannelChanged: switchChannel()
    onSelectedServerChanged: switchChannel()
    Component.onCompleted: switchChannel()

    Rectangle {
        anchors.fill: parent
        color: ThemeData.windowBackground
    }

    // Top Slide-Down Toast Notification
    ToastNotification {
        id: toast
        anchors.top: parent.top
        anchors.topMargin: 56
        onActionClicked: {
            // Retry the last failed message if any
            for (var i = nativeMessageModel.rowCount() - 1; i >= 0; --i) {
                var idx = nativeMessageModel.index(i, 0);
                if (nativeMessageModel.data(idx, ChatMessageModel.StatusRole) === "failed") {
                    var mid = nativeMessageModel.data(idx, ChatMessageModel.MessageIdRole);
                    root.retryMessage(mid);
                    break;
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ─── VIEW 1: FRIENDS DASHBOARD (Discord-like Homepage) ───
        FriendsHomePanel {
            visible: root.selectedServer === "dms" && root.activeChannel === "friends"
            Layout.fillWidth: true
            Layout.fillHeight: true
            onMessageFriendRequested: function(username) {
                root.openDirectMessageRequested(username);
            }
            onAddFriendSubmitted: function(username) {
                NetworkManager.addFriend(username);
            }
        }


        ChatView {
            id: chatView
            visible: !(root.selectedServer === "dms" && root.activeChannel === "friends")
            Layout.fillWidth: true
            Layout.fillHeight: true
            selectedServer: root.selectedServer
            activeChannel: root.activeChannel
            userToggledExpanded: root.userToggledExpanded
            typingUser: root.typingUser
            messageModel: nativeMessageModel
            
            onOpenMediaModalRequested: function(url, type, name) { root.openMediaModalRequested(url, type, name); }
            onRetryMessage: function(msgId) { root.retryMessage(msgId); }
            onSendMessagePayload: function(itemObj) { root.sendMessagePayload(itemObj); }
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

                NeoNectTextField {
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

                    NeoNectButton {
                        text: "Cancel"
                        width: (parent.width - 12) / 2
                        highlighted: false
                        onClicked: {
                            root.showAddFriendModal = false;
                            root.addFriendStatusMsg = "";
                        }
                    }

                    NeoNectButton {
                        text: "Add Friend"
                        width: (parent.width - 12) / 2
                        enabled: friendInput.text.trim() !== ""
                        highlighted: true
                        onClicked: {
                            root.addFriendStatusMsg = "";
                            NetworkManager.addFriend(friendInput.text.trim());
                        }
                    }
                }
            }
        }
    }
}