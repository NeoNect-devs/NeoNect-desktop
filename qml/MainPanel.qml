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

    function isRTL(text) {
        if (!text) return false;
        return /[\u0600-\u06FF\u0750-\u077F\u0590-\u05FF\uFB50-\uFDFF\uFE70-\uFEFF]/.test(text);
    }

    function formatTime(secs) {
        if (!secs || secs <= 0) return Qt.formatTime(new Date(), "hh:mm AP");
        var d = new Date(secs * 1000);
        return Qt.formatTime(d, "hh:mm AP");
    }

    function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return "File";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }

    function isPlayableVideo(url, fileName) {
        var path = (fileName || url || "").toLowerCase();
        return path.endsWith(".mp4") || path.endsWith(".webm") || path.endsWith(".mov") || path.endsWith(".m4v") || path.endsWith(".avi");
    }

    function getFileExtension(fileName, url) {
        var name = fileName || url || "FILE";
        var idx = name.lastIndexOf('.');
        if (idx !== -1 && idx < name.length - 1) {
            return name.substring(idx + 1).toUpperCase();
        }
        return "FILE";
    }

    function detectMediaType(url, fileName) {
        var path = (fileName || url || "").toLowerCase();
        if (path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".jpeg") ||
            path.endsWith(".webp") || path.endsWith(".gif") || path.endsWith(".bmp") ||
            path.endsWith(".svg") || path.endsWith(".ico") || path.endsWith(".tiff")) {
            return "image";
        }
        if (path.endsWith(".mp4") || path.endsWith(".webm") || path.endsWith(".mov") ||
            path.endsWith(".mkv") || path.endsWith(".avi") || path.endsWith(".m4v") ||
            path.endsWith(".flv") || path.endsWith(".wmv") || path.endsWith(".3gp")) {
            return "video";
        }
        if (path.endsWith(".mp3") || path.endsWith(".wav") || path.endsWith(".ogg") ||
            path.endsWith(".flac") || path.endsWith(".m4a") || path.endsWith(".aac") ||
            path.endsWith(".opus") || path.endsWith(".wma")) {
            return "audio";
        }
        return "file";
    }

    Connections {
        target: NetworkManager

        function onIncomingRichMessageReceived(messageData) {
            var fromUsername = messageData.sender || "Anonymous";
            var target = messageData.target || "general";
            var senderLower = fromUsername.toLowerCase();
            var targetLower = (target || "").toLowerCase();

            var myUsername = (NetworkManager && NetworkManager.currentUsername) ? NetworkManager.currentUsername.toLowerCase() : "";
            if (myUsername !== "" && senderLower === myUsername) {
                // Ignore self-echoes from relay server
                return;
            }

            var isChannelMsg = (targetLower === "general" || targetLower.startsWith("channel:"));
            var targetChannelName = targetLower.startsWith("channel:") ? targetLower.replace("channel:", "") : targetLower;
            var key = isChannelMsg ? ("server1:" + targetChannelName) : ("dms:" + senderLower);

            if (!chatHistories[key]) {
                chatHistories[key] = [];
            }

            var itemObj = {
                messageId: messageData.messageId || ("msg_" + Date.now()),
                text: messageData.text || messageData.content || "",
                fromMe: false,
                senderName: fromUsername,
                senderAvatar: fromUsername.charAt(0).toUpperCase(),
                messageType: messageData.type || "text",
                mediaUrl: messageData.mediaUrl || "",
                fileName: messageData.fileName || "",
                fileSize: messageData.fileSize || 0,
                duration: messageData.duration || 0,
                waveform: messageData.waveform || [],
                status: "seen",
                timestamp: messageData.timestamp || Math.floor(Date.now() / 1000)
            };

            chatHistories[key].push(itemObj);

            // Auto add friend to Direct Messages list ONLY for direct messages from others
            if (!isChannelMsg && NetworkManager && NetworkManager.friends && NetworkManager.friends.indexOf(senderLower) === -1) {
                NetworkManager.addFriend(senderLower);
            }

            // Insert into active model if currently viewing this channel/DM
            if (isChannelMsg) {
                if (root.selectedServer === "server1" && root.activeChannel.toLowerCase() === targetChannelName) {
                    nativeMessageModel.insertMessageItem(itemObj);
                    scrollTimer.restart();
                }
            } else {
                if (root.selectedServer === "dms" && root.activeChannel.toLowerCase() === senderLower) {
                    nativeMessageModel.insertMessageItem(itemObj);
                    scrollTimer.restart();
                }
            }
        }

        function onIncomingRelayMessageReceived(fromUsername, target, text, timestamp) {
            // Handled via onIncomingRichMessageReceived if structured, but fallback here if needed
        }

        function onMessageTransmissionStatus(targetUser, messageId, success, errorMessage) {
            var newStatus = success ? "sent" : "failed";
            nativeMessageModel.updateMessageStatus(messageId, newStatus, errorMessage);

            if (!success) {
                var targetName = root.selectedServer === "dms" ? ("@" + root.activeChannel) : ("#" + root.activeChannel);
                toast.show("Message delivery failed to " + targetName + ": " + (errorMessage || "Network error"), "error", "Retry");
            }
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

    property var defaultHistories: ({
        "server1:welcome-rules": [
            { messageId: "d1", text: "Welcome to Danisa / Avila secure E2EE node server!", fromMe: false, senderName: "System", senderAvatar: "🛠️", messageType: "text", status: "sent", timestamp: 1620000000 },
            { messageId: "d2", text: "All conversations here are relayed client-side using OpenSSL 4.0.0 AES-256-GCM.", fromMe: false, senderName: "System", senderAvatar: "🛠️", messageType: "text", status: "sent", timestamp: 1620000010 }
        ],
        "server1:general": [
            { messageId: "d3", text: "Hello! Is anyone online?", fromMe: false, senderName: "Alex", senderAvatar: "A", messageType: "text", status: "sent", timestamp: 1620000020 },
            { messageId: "d4", text: "Hey Alex! Yes, testing live E2EE relay messaging.", fromMe: false, senderName: "Beatrice", senderAvatar: "B", messageType: "text", status: "sent", timestamp: 1620000030 },
            { messageId: "d5", text: "", fromMe: false, senderName: "Alex", senderAvatar: "A", messageType: "sticker", mediaUrl: "qrc:/qt/qml/Avila/assets/stickers/duck/duck_happy.svg", fileName: "Happy", status: "sent", timestamp: 1620000040 }
        ],
        "dms:alex": [
            { messageId: "d6", text: "Hey Alex, are you available for a quick sync later today?", fromMe: true, senderName: "Me", senderAvatar: "", messageType: "text", status: "seen", timestamp: 1620000050 },
            { messageId: "d7", text: "Sure! Just ping me here when you're ready.", fromMe: false, senderName: "Alex", senderAvatar: "A", messageType: "text", status: "seen", timestamp: 1620000060 },
            { messageId: "d8", text: "", fromMe: false, senderName: "Alex", senderAvatar: "A", messageType: "voice", duration: 8, waveform: [0.3, 0.5, 0.8, 0.9, 0.6, 0.4, 0.7, 0.85, 0.95, 0.6, 0.3, 0.5, 0.8, 0.9, 0.6, 0.4, 0.7, 0.85, 0.95, 0.6, 0.4, 0.5, 0.6, 0.7, 0.8, 0.5, 0.3, 0.2], status: "seen", timestamp: 1620000070 }
        ],
        "dms:saved-messages": [
            { messageId: "s1", text: "Welcome to Saved Messages! 🔖\n\n• Forward messages here to save them\n• Send notes, media, audio, and files to store in your private cloud\n• Everything is end-to-end encrypted with your local keys", fromMe: false, senderName: "Saved Messages Cloud", senderAvatar: "🔖", messageType: "text", status: "seen", timestamp: 1620000000 }
        ]
    })

    property var chatHistories: ({})

    function getAvatarColor(sender) {
        if (sender === "Alex") return "#0A84FF"
        if (sender === "Beatrice") return "#06B6D4"
        if (sender === "Charlie") return "#10B981"
        if (sender === "David") return "#F59E0B"
        if (sender === "System") return "#14B8A6"
        return "#4F545C"
    }

    function switchChannel() {
        if (root.selectedServer === "dms" && root.activeChannel === "friends") {
            return;
        }

        var key = selectedServer + ":" + activeChannel;
        nativeMessageModel.clearActiveViewportStore();

        if (!chatHistories[key]) {
            var defaults = defaultHistories[key] || [];
            chatHistories[key] = JSON.parse(JSON.stringify(defaults));
        }

        var history = chatHistories[key];
        for (var i = 0; i < history.length; ++i) {
            nativeMessageModel.insertMessageItem(history[i]);
        }

        scrollTimer.restart();
    }

    function sendMessagePayload(itemObj) {
        var key = root.selectedServer + ":" + root.activeChannel;
        if (!root.chatHistories[key]) {
            root.chatHistories[key] = [];
        }
        root.chatHistories[key].push(itemObj);
        nativeMessageModel.insertMessageItem(itemObj);
        messageListView.positionViewAtEnd();

        var target = root.selectedServer === "dms" ? root.activeChannel : "general";
        NetworkManager.sendRichRelayMessage(target, itemObj);

        // Transition: Sending -> Sent after 350ms
        statusSentTimer.targetChannel = target;
        statusSentTimer.messageId = itemObj.messageId;
        statusSentTimer.restart();
    }

    function retryMessage(messageId) {
        var item = nativeMessageModel.getMessageById(messageId);
        if (!item || !item.messageId) return;

        nativeMessageModel.updateMessageStatus(messageId, "sending", "");
        var target = root.selectedServer === "dms" ? root.activeChannel : "general";
        NetworkManager.sendRichRelayMessage(target, item);
        toast.show("Retrying message transmission to " + (root.selectedServer === "dms" ? "@" + root.activeChannel : "#" + root.activeChannel), "info", "");
    }

    Timer {
        id: scrollTimer
        interval: 50
        repeat: false
        onTriggered: messageListView.positionViewAtEnd()
    }

    // ─── REAL-TIME MESSAGE DELIVERY & TYPING SIMULATION TIMERS ───
    Timer {
        id: statusSentTimer
        property string targetChannel: ""
        property string messageId: ""
        interval: 350
        repeat: false
        onTriggered: {
            nativeMessageModel.updateMessageStatus(messageId, "sent", "");
            statusDeliveredTimer.targetChannel = targetChannel;
            statusDeliveredTimer.messageId = messageId;
            statusDeliveredTimer.restart();
        }
    }

    Timer {
        id: statusDeliveredTimer
        property string targetChannel: ""
        property string messageId: ""
        interval: 700
        repeat: false
        onTriggered: {
            nativeMessageModel.updateMessageStatus(messageId, "delivered", "");
            if (root.selectedServer === "dms" && targetChannel !== "saved-messages") {
                statusSeenTimer.targetChannel = targetChannel;
                statusSeenTimer.messageId = messageId;
                statusSeenTimer.restart();
            }
        }
    }

    Timer {
        id: statusSeenTimer
        property string targetChannel: ""
        property string messageId: ""
        interval: 1200
        repeat: false
        onTriggered: {
            nativeMessageModel.updateMessageStatus(messageId, "seen", "");
            // Friend starts typing after message is seen
            root.typingUser = targetChannel.replace(/^\w/, c => c.toUpperCase());
            root.isOtherTyping = true;
            mockReplyTimer.targetChannel = targetChannel;
            mockReplyTimer.restart();
        }
    }

    Timer {
        id: mockReplyTimer
        property string targetChannel: ""
        interval: 2500
        repeat: false
        onTriggered: {
            root.isOtherTyping = false;
            var replies = [
                "Received your message loud and clear! The new UI looks fantastic 🚀",
                "Testing end-to-end encrypted relay transmission — works flawlessly!",
                "Got the file/message, everything looks crisp.",
                "Awesome! Thanks for the update 👍"
            ];
            var replyText = replies[Math.floor(Math.random() * replies.length)];
            var replyObj = {
                messageId: "msg_reply_" + Date.now(),
                text: replyText,
                fromMe: false,
                senderName: targetChannel.replace(/^\w/, c => c.toUpperCase()),
                senderAvatar: targetChannel.charAt(0).toUpperCase(),
                messageType: "text",
                mediaUrl: "",
                fileName: "",
                fileSize: 0,
                duration: 0,
                waveform: [],
                status: "seen",
                errorText: "",
                timestamp: Math.floor(Date.now() / 1000)
            };
            var key = "dms:" + targetChannel.toLowerCase();
            if (!root.chatHistories[key]) root.chatHistories[key] = [];
            root.chatHistories[key].push(replyObj);
            if (root.selectedServer === "dms" && root.activeChannel.toLowerCase() === targetChannel.toLowerCase()) {
                nativeMessageModel.insertMessageItem(replyObj);
                messageListView.positionViewAtEnd();
            }
        }
    }

    // ─── PERIODIC HEARTBEAT ONLINE CHECKER ───
    Timer {
        id: heartbeatOnlineTimer
        interval: 12000
        repeat: true
        running: true
        onTriggered: {
            if (NetworkManager) {
                NetworkManager.checkFriendsStatus();
            }
        }
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

        // ─── VIEW 2: CHAT INTERFACE (Server Channels, DMs, Saved Messages) ───
        ColumnLayout {
            visible: !(root.selectedServer === "dms" && root.activeChannel === "friends")
            Layout.fillWidth: true
            Layout.fillHeight: true
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

                    // Server Channel Icon
                    IconImage {
                        visible: root.selectedServer !== "dms"
                        source: "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                        width: 18; height: 18
                        color: ThemeData.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // Saved Messages Vector Icon
                    IconImage {
                        visible: root.selectedServer === "dms" && root.activeChannel === "saved-messages"
                        source: "qrc:/qt/qml/Avila/assets/icons/bookmark.svg"
                        width: 20; height: 20
                        color: "#00E5FF"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    // DM Contact @ Prefix
                    Text {
                        visible: root.selectedServer === "dms" && root.activeChannel !== "saved-messages"
                        text: "@"
                        color: ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 20
                        font.weight: Font.Light
                    }

                    // Channel / Contact Title
                    Text {
                        text: {
                            if (root.selectedServer === "dms") {
                                if (root.activeChannel === "saved-messages") return "Saved Messages";
                                return root.activeChannel.replace(/^\w/, c => c.toUpperCase());
                            }
                            return root.activeChannel;
                        }
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.bold: true
                    }

                    Rectangle {
                        width: 1; height: 16
                        color: ThemeData.textSecondary
                        opacity: 0.3
                        Layout.leftMargin: 4; Layout.rightMargin: 4
                    }

                    // Channel / DM Subtitle
                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (root.selectedServer === "dms") {
                                if (root.activeChannel === "saved-messages") return "Your Personal Cloud Storage & Notes";
                                return "Danisa Zero-Knowledge E2EE Direct Messages";
                            }
                            return "Secure Workspace Channel";
                        }
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
                        spacing: root.selectedServer === "dms" ? 6 : 2

                        ScrollBar.vertical: ScrollBar {
                            id: chatScrollBar
                            width: 8
                            policy: ScrollBar.AsNeeded
                            visible: chatScrollBar.size < 1.0
                            active: messageListView.moving || chatScrollBar.hovered || chatScrollBar.pressed

                            background: Rectangle {
                                color: "transparent"
                            }

                            contentItem: Rectangle {
                                implicitWidth: 8
                                radius: 4
                                color: chatScrollBar.pressed ? ThemeData.accentColor : (chatScrollBar.hovered ? ThemeData.accentHover : "#4E5058")
                                opacity: (chatScrollBar.size < 1.0 && (chatScrollBar.active || chatScrollBar.hovered)) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 150 } }
                            }
                        }

                        // Message Item Delegate
                        delegate: Item {
                            id: delegateRoot
                            width: messageListView.width - 12
                            height: messageContentColumn.implicitHeight + (model.isFirstInBlock ? 12 : 4)

                            readonly property bool isDM: root.selectedServer === "dms"
                            readonly property bool isMe: model.fromMe
                            readonly property bool isText: model.messageType === "text"
                            readonly property bool isSticker: model.messageType === "sticker"
                            readonly property bool isImage: model.messageType === "image"
                            readonly property bool isVideo: model.messageType === "video"
                            readonly property bool isAudio: model.messageType === "audio"
                            readonly property bool isVoice: model.messageType === "voice"
                            readonly property bool isFile: model.messageType === "file"
                            readonly property bool isMediaWidget: isSticker || isImage || isVideo || isAudio || isVoice || isFile
                            readonly property bool isFailed: model.status === "failed"

                            // Hover background for server stream
                            Rectangle {
                                anchors.fill: parent
                                visible: !delegateRoot.isDM
                                color: itemMouseArea.containsMouse ? Qt.rgba(255, 255, 255, 0.04) : "transparent"
                            }

                            MouseArea {
                                id: itemMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                            }

                            // ─── FLOATING / STICKY AVATAR (on the left for others in DMs and all in Servers) ───
                            Item {
                                id: avatarContainer
                                width: 38; height: 38
                                visible: (!delegateRoot.isDM || !delegateRoot.isMe) && model.isFirstInBlock
                                anchors.left: parent.left
                                anchors.leftMargin: 16

                                // Floating sticky calculation: smooth reactive offset tracking scroll position within message block
                                y: {
                                    if (!messageListView) return 6;
                                    var topInView = delegateRoot.y - messageListView.contentY;
                                    if (topInView < 0) {
                                        var maxOffset = delegateRoot.height - avatarContainer.height - 6;
                                        return Math.max(6, Math.min(maxOffset, 6 - topInView));
                                    }
                                    return 6;
                                }

                                Behavior on y {
                                    NumberAnimation { duration: 40; easing.type: Easing.OutQuad }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 8
                                    color: getAvatarColor(model.senderName)

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.senderAvatar !== "" ? model.senderAvatar : model.senderName.charAt(0).toUpperCase()
                                        color: "#FFFFFF"
                                        font.bold: true
                                    }
                                }
                            }

                            // ─── MESSAGE CONTENT COLUMN ───
                            ColumnLayout {
                                id: messageContentColumn
                                anchors.top: parent.top
                                anchors.topMargin: model.isFirstInBlock ? 6 : 2
                                anchors.left: (!delegateRoot.isDM || !delegateRoot.isMe) ? parent.left : undefined
                                anchors.leftMargin: (!delegateRoot.isDM || !delegateRoot.isMe) ? 66 : 0
                                anchors.right: (delegateRoot.isDM && delegateRoot.isMe) ? parent.right : (!delegateRoot.isDM ? parent.right : undefined)
                                anchors.rightMargin: (delegateRoot.isDM && delegateRoot.isMe) ? 16 : (!delegateRoot.isDM ? 16 : 0)
                                width: (!delegateRoot.isDM) ? (delegateRoot.width - 82) : Math.min(delegateRoot.width - 90, bubbleRow.implicitWidth)
                                spacing: 4

                                // Sender Name & Timestamp (Server view OR first in block)
                                RowLayout {
                                    visible: !delegateRoot.isDM && model.isFirstInBlock
                                    spacing: 8
                                    Layout.fillWidth: true

                                    Text {
                                        text: model.senderName
                                        color: model.fromMe ? ThemeData.accentColor : ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.bold: true
                                        font.pixelSize: 14
                                    }

                                    Text {
                                        text: "Today at " + root.formatTime(model.timestamp)
                                        color: ThemeData.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 11
                                    }
                                }

                                // ─── MESSAGE BODY (BUBBLE IN DM VS FLAT STREAM IN SERVER) ───
                                RowLayout {
                                    id: bubbleRow
                                    Layout.alignment: (delegateRoot.isDM && delegateRoot.isMe) ? Qt.AlignRight : Qt.AlignLeft
                                    Layout.fillWidth: !delegateRoot.isDM
                                    Layout.maximumWidth: delegateRoot.isDM ? Math.min(540, Math.max(260, delegateRoot.width * 0.72)) : (delegateRoot.width - 82)
                                    spacing: 6

                                    // Retry button on left of sent failed bubble
                                    Rectangle {
                                        visible: delegateRoot.isDM && delegateRoot.isMe && delegateRoot.isFailed
                                        width: 26; height: 26
                                        radius: 13
                                        color: retryMouse1.containsMouse ? "#E53935" : Qt.rgba(229, 57, 53, 0.2)

                                        IconImage {
                                            anchors.centerIn: parent
                                            source: "qrc:/qt/qml/Avila/assets/icons/refresh.svg"
                                            width: 14; height: 14
                                            color: "#FFFFFF"
                                        }

                                        MouseArea {
                                            id: retryMouse1
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.retryMessage(model.messageId)
                                        }
                                    }

                                    // Main Bubble / Media Container
                                    Rectangle {
                                        id: bubbleBox
                                        readonly property real maxContentWidth: delegateRoot.isDM ? (bubbleRow.Layout.maximumWidth - (delegateRoot.isFailed ? 32 : 0)) : (delegateRoot.width - 82)

                                        width: bubbleInnerContent.implicitWidth + (delegateRoot.isDM && delegateRoot.isText ? 24 : 0)
                                        height: bubbleInnerContent.implicitHeight + (delegateRoot.isDM && delegateRoot.isText ? 16 : 0)
                                        implicitWidth: width
                                        implicitHeight: height

                                        // Background: only for DM text bubbles (stickers, media, widgets have their own styling)
                                        color: {
                                            if (!delegateRoot.isDM || !delegateRoot.isText) return "transparent";
                                            return delegateRoot.isMe ? "#3C3F46" : "#232528";
                                        }

                                        // Subtle sleek white-grayish gradient for Sent DM bubbles
                                        gradient: (delegateRoot.isDM && delegateRoot.isMe && delegateRoot.isText) ? bubbleGrad : null

                                        Gradient {
                                            id: bubbleGrad
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: delegateRoot.isFailed ? "#992D22" : "#383A40" }
                                            GradientStop { position: 1.0; color: delegateRoot.isFailed ? "#C0392B" : "#484B54" }
                                        }

                                        border.color: {
                                            if (!delegateRoot.isDM || !delegateRoot.isText) return "transparent";
                                            if (delegateRoot.isFailed) return "#E53935";
                                            return delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.18) : Qt.rgba(255, 255, 255, 0.08);
                                        }
                                        border.width: (delegateRoot.isDM && delegateRoot.isText) ? 1 : 0

                                        // Tailored corner radii for Telegram/iMessage style bubbles
                                        radius: (delegateRoot.isDM && delegateRoot.isText) ? 16 : 0

                                        ColumnLayout {
                                            id: bubbleInnerContent
                                            anchors.left: parent.left
                                            anchors.top: parent.top
                                            anchors.margins: (delegateRoot.isDM && delegateRoot.isText) ? 8 : 0
                                            spacing: 6

                                            // 1. TEXT MESSAGE
                                            Text {
                                                id: chatTextItem
                                                visible: delegateRoot.isText && model.text !== ""
                                                width: visible ? Math.min(bubbleBox.maxContentWidth - (delegateRoot.isDM ? 24 : 0), implicitWidth) : 0
                                                Layout.preferredWidth: width
                                                Layout.maximumWidth: bubbleBox.maxContentWidth - (delegateRoot.isDM ? 24 : 0)
                                                text: model.text
                                                color: (delegateRoot.isDM && delegateRoot.isMe) ? "#FFFFFF" : ThemeData.textPrimary
                                                font.family: "Segoe UI"
                                                font.pixelSize: 14
                                                wrapMode: Text.WrapAnywhere
                                                textFormat: Text.PlainText
                                                // Dynamic LTR vs RTL alignment
                                                horizontalAlignment: root.isRTL(model.text) ? Text.AlignRight : Text.AlignLeft
                                            }

                                            // 2. STICKER MESSAGE (Telegram Style)
                                            Item {
                                                visible: model.messageType === "sticker"
                                                implicitWidth: 130; implicitHeight: 130
                                                Layout.preferredWidth: 130; Layout.preferredHeight: 130
                                                width: 130; height: 130

                                                Image {
                                                    anchors.centerIn: parent
                                                    width: 120; height: 120
                                                    source: model.messageType === "sticker" ? (model.mediaUrl || "") : ""
                                                    sourceSize: Qt.size(240, 240)
                                                    fillMode: Image.PreserveAspectFit
                                                    smooth: true
                                                }
                                            }

                                            // 3. IMAGE MESSAGE (Dynamically Scaled)
                                            Item {
                                                id: imgDelegateItem
                                                visible: model.messageType === "image"

                                                readonly property real naturalW: (chatImg.sourceSize && chatImg.sourceSize.width > 0) ? chatImg.sourceSize.width : (chatImg.implicitWidth > 0 ? chatImg.implicitWidth : 320)
                                                readonly property real naturalH: (chatImg.sourceSize && chatImg.sourceSize.height > 0) ? chatImg.sourceSize.height : (chatImg.implicitHeight > 0 ? chatImg.implicitHeight : 200)
                                                readonly property real ratio: (naturalW > 0 && naturalH > 0) ? (naturalW / naturalH) : 1.6

                                                readonly property real calcWidth: {
                                                    var maxW = 420;
                                                    var maxH = 340;
                                                    var minW = 160;
                                                    var w = naturalW;
                                                    var h = naturalH;
                                                    if (w > maxW) {
                                                        h = maxW / ratio;
                                                        w = maxW;
                                                    }
                                                    if (h > maxH) {
                                                        w = maxH * ratio;
                                                        h = maxH;
                                                    }
                                                    return Math.max(minW, Math.min(maxW, w));
                                                }
                                                readonly property real calcHeight: Math.max(100, Math.min(340, calcWidth / ratio))

                                                implicitWidth: calcWidth
                                                implicitHeight: calcHeight
                                                Layout.preferredWidth: calcWidth
                                                Layout.preferredHeight: calcHeight
                                                width: calcWidth
                                                height: calcHeight

                                                Rectangle {
                                                    anchors.fill: parent
                                                    radius: 10
                                                    color: "#18191D"
                                                    clip: true

                                                    Image {
                                                        id: chatImg
                                                        anchors.fill: parent
                                                        source: model.messageType === "image" ? (model.mediaUrl || "") : ""
                                                        fillMode: Image.PreserveAspectFit
                                                        smooth: true
                                                        asynchronous: true
                                                    }

                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: root.openMediaModalRequested(model.mediaUrl, "image", model.fileName || "Image")
                                                    }
                                                }
                                            }

                                            // 4. VIDEO MESSAGE (In-App Player for supported formats)
                                            VideoPlayerItem {
                                                visible: model.messageType === "video" && root.isPlayableVideo(model.mediaUrl, model.fileName)
                                                messageId: model.messageId
                                                videoUrl: model.mediaUrl
                                                fileName: model.fileName || "Video"
                                                fileSize: model.fileSize
                                                duration: model.duration || 30
                                                fromMe: model.fromMe
                                                onOpenFullscreenRequested: (url, name) => root.openMediaModalRequested(url, "video", name)
                                            }

                                            // 5. MUSIC AUDIO MESSAGE
                                            AudioMusicPlayer {
                                                visible: model.messageType === "audio"
                                                messageId: model.messageId
                                                audioUrl: model.mediaUrl
                                                fileName: model.fileName || "Audio"
                                                fileSize: model.fileSize
                                                duration: model.duration || 180
                                                fromMe: model.fromMe
                                            }

                                            // 6. VOICE NOTE MESSAGE (Telegram Style)
                                            VoiceMessagePlayer {
                                                visible: model.messageType === "voice"
                                                messageId: model.messageId
                                                audioUrl: model.mediaUrl
                                                duration: model.duration || 6
                                                waveform: model.waveform
                                                fromMe: model.fromMe
                                            }

                                            // 7. FILE / DOCUMENT / NON-PLAYABLE MEDIA MESSAGE (Rich Card with External Open)
                                            Rectangle {
                                                visible: model.messageType === "file" || (model.messageType === "video" && !root.isPlayableVideo(model.mediaUrl, model.fileName))
                                                implicitWidth: 320; implicitHeight: 64
                                                Layout.preferredWidth: 320; Layout.preferredHeight: 64
                                                width: 320; height: 64
                                                radius: 10
                                                color: delegateRoot.isMe ? Qt.rgba(0, 0, 0, 0.25) : Qt.rgba(255, 255, 255, 0.06)
                                                border.color: fileCardMouse.containsMouse ? ThemeData.accentColor : (delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.2) : Qt.rgba(255, 255, 255, 0.1))
                                                border.width: 1

                                                Behavior on border.color { ColorAnimation { duration: 150 } }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 10
                                                    spacing: 12

                                                    // File Extension Badge Box
                                                    Rectangle {
                                                        width: 44; height: 44
                                                        radius: 8
                                                        color: delegateRoot.isMe ? "#FFFFFF" : Qt.rgba(10, 132, 255, 0.15)
                                                        border.color: delegateRoot.isMe ? "transparent" : Qt.rgba(10, 132, 255, 0.35)
                                                        border.width: 1

                                                        ColumnLayout {
                                                            anchors.centerIn: parent
                                                            spacing: 1

                                                            IconImage {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                source: "qrc:/qt/qml/Avila/assets/icons/file.svg"
                                                                width: 16; height: 16
                                                                color: delegateRoot.isMe ? ThemeData.accentColor : ThemeData.accentColor
                                                            }

                                                            Text {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                text: root.getFileExtension(model.fileName, model.mediaUrl)
                                                                color: delegateRoot.isMe ? ThemeData.accentColor : "#FFFFFF"
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 9
                                                                font.bold: true
                                                            }
                                                        }
                                                    }

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2

                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: model.fileName || "File Attachment"
                                                            color: delegateRoot.isMe ? "#FFFFFF" : ThemeData.textPrimary
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 13
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }

                                                        RowLayout {
                                                            spacing: 6

                                                            Text {
                                                                text: root.formatBytes(model.fileSize)
                                                                color: delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.75) : ThemeData.textSecondary
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 10
                                                            }

                                                            Text {
                                                                text: "•  Click to open ↗"
                                                                color: delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.6) : ThemeData.accentColor
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 10
                                                            }
                                                        }
                                                    }

                                                    // Open Icon
                                                    Rectangle {
                                                        width: 28; height: 28
                                                        radius: 14
                                                        color: fileCardMouse.containsMouse ? (delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.3) : Qt.rgba(255, 255, 255, 0.15)) : "transparent"

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "↗"
                                                            color: delegateRoot.isMe ? "#FFFFFF" : ThemeData.textSecondary
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 14
                                                            font.bold: true
                                                        }
                                                    }
                                                }

                                                MouseArea {
                                                    id: fileCardMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: AudioManager.openMediaFile(model.mediaUrl)
                                                }
                                            }

                                            // Caption for media messages
                                            Text {
                                                visible: (model.messageType === "image" || model.messageType === "video" || model.messageType === "file") && model.text !== ""
                                                Layout.fillWidth: true
                                                text: model.text
                                                color: (delegateRoot.isDM && delegateRoot.isMe) ? "#FFFFFF" : ThemeData.textPrimary
                                                font.family: "Segoe UI"
                                                font.pixelSize: 13
                                                wrapMode: Text.Wrap
                                                horizontalAlignment: root.isRTL(model.text) ? Text.AlignRight : Text.AlignLeft
                                            }

                                            // DM Bubble Footer: Timestamp & Delivery Status
                                            RowLayout {
                                                visible: delegateRoot.isDM
                                                Layout.alignment: delegateRoot.isMe ? Qt.AlignRight : Qt.AlignLeft
                                                spacing: 4

                                                Text {
                                                    text: root.formatTime(model.timestamp)
                                                    color: delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.7) : ThemeData.textSecondary
                                                    font.family: "Segoe UI"
                                                    font.pixelSize: 10
                                                }

                                                // Status Icon for Sent Messages
                                                Row {
                                                    visible: delegateRoot.isMe
                                                    spacing: 3
                                                    Layout.alignment: Qt.AlignVCenter

                                                    // 1. Sending Progress Dot
                                                    Rectangle {
                                                        visible: model.status === "sending"
                                                        width: 8; height: 8; radius: 4
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        color: "transparent"
                                                        border.color: Qt.rgba(255, 255, 255, 0.7)
                                                        border.width: 1.5

                                                        SequentialAnimation on opacity {
                                                            loops: Animation.Infinite
                                                            running: model.status === "sending"
                                                            NumberAnimation { from: 0.3; to: 1.0; duration: 400 }
                                                            NumberAnimation { from: 1.0; to: 0.3; duration: 400 }
                                                        }
                                                    }

                                                    // 2. Sent (Unseen) Single Check
                                                    IconImage {
                                                        visible: model.status === "sent"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        source: "qrc:/qt/qml/Avila/assets/icons/check.svg"
                                                        width: 12; height: 12
                                                        color: Qt.rgba(255, 255, 255, 0.7)
                                                    }

                                                    // 3. Delivered Double Check
                                                    IconImage {
                                                        visible: model.status === "delivered"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        source: "qrc:/qt/qml/Avila/assets/icons/check-check.svg"
                                                        width: 13; height: 13
                                                        color: "#FFFFFF"
                                                    }

                                                    // 4. Seen Double Check + Seen Badge
                                                    Row {
                                                        visible: model.status === "seen"
                                                        spacing: 2
                                                        anchors.verticalCenter: parent.verticalCenter

                                                        IconImage {
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            source: "qrc:/qt/qml/Avila/assets/icons/check-check.svg"
                                                            width: 13; height: 13
                                                            color: "#00E5FF"
                                                        }

                                                        Text {
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            text: "Seen"
                                                            color: "#00E5FF"
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 9
                                                            font.bold: true
                                                        }
                                                    }

                                                    // 5. Error / Failed Alert Icon
                                                    IconImage {
                                                        visible: model.status === "failed"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        source: "qrc:/qt/qml/Avila/assets/icons/alert-circle.svg"
                                                        width: 12; height: 12
                                                        color: "#FF5252"
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Retry button on right of server failed message
                                    Rectangle {
                                        visible: !delegateRoot.isDM && delegateRoot.isFailed
                                        width: 24; height: 24
                                        radius: 12
                                        color: retryMouse2.containsMouse ? "#E53935" : Qt.rgba(229, 57, 53, 0.2)

                                        IconImage {
                                            anchors.centerIn: parent
                                            source: "qrc:/qt/qml/Avila/assets/icons/refresh.svg"
                                            width: 14; height: 14
                                            color: "#FFFFFF"
                                        }

                                        MouseArea {
                                            id: retryMouse2
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.retryMessage(model.messageId)
                                        }
                                    }
                                }
                            }
                        }

                        // Empty State Placeholder
                        Item {
                            visible: nativeMessageModel.count === 0
                            anchors.centerIn: parent
                            width: parent.width * 0.7
                            height: 120

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 8

                                IconImage {
                                    Layout.alignment: Qt.AlignHCenter
                                    source: root.selectedServer === "dms" ? "qrc:/qt/qml/Avila/assets/icons/friends.svg" : "qrc:/qt/qml/Avila/assets/icons/hash.svg"
                                    width: 32; height: 32
                                    color: "#4E5058"
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.selectedServer === "dms" ? ("This is the beginning of your direct message history with @" + (root.activeChannel.replace(/^\w/, c => c.toUpperCase()))) : ("Welcome to #" + root.activeChannel + "!")
                                    color: ThemeData.textPrimary
                                    font.family: "Segoe UI"
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "Send a message or media to kick off the conversation."
                                    color: "#949BA4"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                }
                            }
                        }
                    }

                    // ─── DRAG & DROP FOR ATTACHING FILES & MEDIA ───
                    DropArea {
                        id: chatDropArea
                        anchors.fill: parent
                        keys: ["text/uri-list"]

                        onEntered: (drag) => {
                            if (drag.hasUrls) {
                                drag.acceptProposedAction();
                            }
                        }

                        onDropped: (drop) => {
                            if (drop.hasUrls && drop.urls.length > 0) {
                                if (drop.urls.length === 1) {
                                    // Single file: stage as draft attachment so user can write an optional description
                                    messageInput.handleSelectedFileUrl(drop.urls[0].toString());
                                } else {
                                    // Multiple files: send earlier files and stage the last one with focus for captioning
                                    for (var i = 0; i < drop.urls.length - 1; ++i) {
                                        var url = drop.urls[i].toString();
                                        var path = url.replace("file:///", "").replace("file://", "");
                                        var fileName = path.substring(path.lastIndexOf('/') + 1);
                                        if (!fileName || fileName.indexOf('\\') !== -1) {
                                            fileName = path.substring(path.lastIndexOf('\\') + 1);
                                        }
                                        var detectedType = root.detectMediaType(url, fileName);
                                        var itemObj = {
                                            messageId: "msg_" + Date.now() + "_" + i,
                                            text: "",
                                            fromMe: true,
                                            senderName: "Me",
                                            senderAvatar: "",
                                            messageType: detectedType,
                                            mediaUrl: url,
                                            fileName: fileName,
                                            fileSize: (detectedType === "image" ? 1540000 : (detectedType === "video" ? 8500000 : (detectedType === "audio" ? 4200000 : 2500000))),
                                            duration: (detectedType === "video" ? 30 : (detectedType === "audio" ? 180 : 0)),
                                            waveform: (detectedType === "voice" ? [0.3, 0.6, 0.9, 0.5, 0.2] : []),
                                            status: "sending",
                                            timestamp: Math.floor(Date.now() / 1000)
                                        };
                                        root.sendMessagePayload(itemObj);
                                    }
                                    messageInput.handleSelectedFileUrl(drop.urls[drop.urls.length - 1].toString());
                                }
                                drop.acceptProposedAction();
                            }
                        }
                    }

                    // Drag & Drop visual feedback overlay (Deep Obsidian Frosted Glass)
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: "#DD060709"
                        border.color: "#00E5FF"
                        border.width: 2
                        visible: chatDropArea.containsDrag
                        z: 9999

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 14

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 68; height: 68
                                radius: 16
                                color: Qt.rgba(10, 132, 255, 0.2)
                                border.color: "#00E5FF"
                                border.width: 2

                                IconImage {
                                    anchors.centerIn: parent
                                    source: "qrc:/qt/qml/Avila/assets/icons/download.svg"
                                    width: 32; height: 32
                                    color: "#00E5FF"
                                    rotation: 180
                                }
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Drop to Attach & Add Caption"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "File will be staged in the message bar for optional description"
                                color: "#949BA4"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                // ─── TYPING INDICATOR BANNER ───
                Rectangle {
                    visible: root.isOtherTyping && root.selectedServer === "dms" && root.activeChannel !== "saved-messages"
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    height: 20
                    color: "transparent"

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        // 3 Pulsing Animated Typing Dots
                        Row {
                            spacing: 3
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                width: 5; height: 5; radius: 2.5
                                color: "#00E5FF"
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: root.isOtherTyping
                                    NumberAnimation { from: 0.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 1.0; to: 0.2; duration: 400; easing.type: Easing.InOutQuad }
                                }
                            }
                            Rectangle {
                                width: 5; height: 5; radius: 2.5
                                color: "#00E5FF"
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: root.isOtherTyping
                                    PauseAnimation { duration: 200 }
                                    NumberAnimation { from: 0.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 1.0; to: 0.2; duration: 400; easing.type: Easing.InOutQuad }
                                }
                            }
                            Rectangle {
                                width: 5; height: 5; radius: 2.5
                                color: "#00E5FF"
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: root.isOtherTyping
                                    PauseAnimation { duration: 400 }
                                    NumberAnimation { from: 0.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 1.0; to: 0.2; duration: 400; easing.type: Easing.InOutQuad }
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: (root.typingUser || (root.activeChannel.replace(/^\w/, c => c.toUpperCase()))) + " is typing..."
                            color: "#00E5FF"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.italic: true
                        }
                    }
                }

                // ─── MESSAGE INPUT SECTION ───
                AvilaMessageInput {
                    id: messageInput
                    Layout.fillWidth: true
                    Layout.margins: 12
                    channelName: root.selectedServer === "dms" ? root.activeChannel.replace("dm-", "").replace(/^\w/, c => c.toUpperCase()) : root.activeChannel
                    isDM: root.selectedServer === "dms"

                    onMessageSent: function(msgText) {
                        var itemObj = {
                            messageId: "msg_" + Date.now(),
                            text: msgText,
                            fromMe: true,
                            senderName: "Me",
                            senderAvatar: "",
                            messageType: "text",
                            status: "sending",
                            timestamp: Math.floor(Date.now() / 1000)
                        };
                        root.sendMessagePayload(itemObj);
                    }

                    onStickerSent: function(stickerUrl, packId, stickerName) {
                        var itemObj = {
                            messageId: "msg_" + Date.now(),
                            text: "",
                            fromMe: true,
                            senderName: "Me",
                            senderAvatar: "",
                            messageType: "sticker",
                            mediaUrl: stickerUrl,
                            fileName: stickerName,
                            status: "sending",
                            timestamp: Math.floor(Date.now() / 1000)
                        };
                        root.sendMessagePayload(itemObj);
                    }

                    onVoiceSent: function(voiceData) {
                        var itemObj = {
                            messageId: "msg_" + Date.now(),
                            text: "",
                            fromMe: true,
                            senderName: "Me",
                            senderAvatar: "",
                            messageType: "voice",
                            mediaUrl: voiceData.audioUrl,
                            duration: voiceData.duration,
                            waveform: voiceData.waveform,
                            fileSize: voiceData.fileSize,
                            status: "sending",
                            timestamp: Math.floor(Date.now() / 1000)
                        };
                        root.sendMessagePayload(itemObj);
                    }

                    onMediaSent: function(mediaData) {
                        var itemObj = {
                            messageId: "msg_" + Date.now(),
                            text: mediaData.caption || mediaData.text || "",
                            fromMe: true,
                            senderName: "Me",
                            senderAvatar: "",
                            messageType: mediaData.type,
                            mediaUrl: mediaData.mediaUrl,
                            fileName: mediaData.fileName,
                            fileSize: mediaData.fileSize,
                            duration: mediaData.duration || 0,
                            status: "sending",
                            timestamp: Math.floor(Date.now() / 1000)
                        };
                        root.sendMessagePayload(itemObj);
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
                            root.showAddFriendModal = false;
                            root.addFriendStatusMsg = "";
                        }
                    }

                    AvilaButton {
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