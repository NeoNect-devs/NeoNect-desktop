import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0
import "qrc:/qt/qml/NeoNect/qml/components"
import "qrc:/qt/qml/NeoNect/qml/containers"
import "UIHelpers.js" as UIHelpers

ColumnLayout {
    id: chatViewRoot
    property string selectedServer: ""
    property string activeChannel: ""
    property bool userToggledExpanded: false
    property string typingUser: ""
    property bool isOtherTyping: false
    property QtObject messageModel: null
    property bool isFetchingMore: false
    property bool hasInitialPositioned: false
    property var savedScrollPositions: ({})
    property string currentConvKey: ""
    
    signal typingStarted()
    signal typingStopped()
    signal openMediaModalRequested(string url, string type, string name, int startPosMs, bool isPlaying)
    signal retryMessage(string msgId)
    signal sendMessagePayload(var itemObj)
    signal acceptMediaRequested(string convId, string reqId)
    signal declineMediaRequested(string convId, string reqId)

    visible: !(selectedServer === "dms" && activeChannel === "friends") && activeChannel !== ""
    Layout.fillWidth: true
    Layout.fillHeight: true
    spacing: 0

    function saveCurrentScrollPosition() {
        if (!currentConvKey || !chatViewRoot.visible || !hasInitialPositioned || !messageModel || messageModel.count === 0) return;
        if (messageListView.atYEnd || (messageListView.contentItem && messageListView.contentItem.height <= messageListView.height)) {
            savedScrollPositions[currentConvKey] = "BOTTOM";
        } else {
            savedScrollPositions[currentConvKey] = messageListView.contentY;
        }
    }

    function restoreScrollPosition(unread) {
        if (!chatViewRoot.visible) return;
        var key = (selectedServer + ":" + activeChannel).toLowerCase();
        var unreadCount = (unread !== undefined) ? unread : ((selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0);
        var saved = savedScrollPositions[key];
        if (unreadCount > 0 || saved === undefined || saved === "BOTTOM") {
            scrollToBottomCompletely();
        } else if (typeof saved === "number") {
            messageListView.contentY = saved;
            Qt.callLater(function() {
                if (messageListView.contentItem && messageListView.contentItem.height > messageListView.height) {
                    messageListView.contentY = Math.max(0, Math.min(saved, messageListView.contentItem.height - messageListView.height));
                }
                if (messageModel && messageModel.count > 0) {
                    chatViewRoot.hasInitialPositioned = true;
                }
            });
        }
    }

    function scrollToBottomCompletely() {
        if (!messageModel || messageModel.count === 0) return;
        messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
        Qt.callLater(function() {
            if (messageModel && messageModel.count > 0) {
                messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
            }
            Qt.callLater(function() {
                if (messageModel && messageModel.count > 0) {
                    messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
                    chatViewRoot.hasInitialPositioned = true;
                }
            });
        });
    }

    function onConversationLoaded(convId, messages) {
        var currentConv = (selectedServer + ":" + (activeChannel ? activeChannel.trim() : "")).toLowerCase();
        if (convId && convId.toLowerCase() === currentConv) {
            hasInitialPositioned = false;
            var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
            if (unread > 0 && messageModel && messageModel.count > 0) {
                var unreadIdx = Math.max(0, messageModel.count - unread);
                messageModel.setFirstUnreadIndex(unreadIdx);
            }
            Qt.callLater(function() {
                chatViewRoot.restoreScrollPosition(unread);
                chatViewRoot.checkAndSendSeenReceipt();
                if (selectedServer === "dms" && activeChannel && activeChannel !== "friends" && activeChannel !== "saved-messages") {
                    if (typeof NetworkManager !== "undefined" && NetworkManager) {
                        NetworkManager.markConversationAsRead(activeChannel);
                    }
                }
            });
        }
    }

    onActiveChannelChanged: {
        saveCurrentScrollPosition();
        if (messageModel) messageModel.clearFirstUnread();
        currentConvKey = (selectedServer + ":" + activeChannel).toLowerCase();
        hasInitialPositioned = false;
        isFetchingMore = false;
        if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
        Qt.callLater(function() {
            chatViewRoot.restoreScrollPosition();
        });
    }

    onSelectedServerChanged: {
        saveCurrentScrollPosition();
        if (messageModel) messageModel.clearFirstUnread();
        currentConvKey = (selectedServer + ":" + activeChannel).toLowerCase();
        hasInitialPositioned = false;
        isFetchingMore = false;
        if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
        Qt.callLater(function() {
            chatViewRoot.restoreScrollPosition();
        });
    }

    onVisibleChanged: {
        if (visible) {
            hasInitialPositioned = false;
            Qt.callLater(function() {
                chatViewRoot.restoreScrollPosition();
                chatViewRoot.checkAndSendSeenReceipt();
            });
        }
    }

    Connections {
        target: messageModel
        function onCountChanged() {
            if (messageModel && messageModel.count > 0) {
                if (!chatViewRoot.hasInitialPositioned) {
                    var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
                    if (unread > 0) {
                        var unreadIdx = Math.max(0, messageModel.count - unread);
                        messageModel.setFirstUnreadIndex(unreadIdx);
                    }

                    chatViewRoot.restoreScrollPosition(unread);
                    chatViewRoot.checkAndSendSeenReceipt();
                }
            }
        }
    }

    Timer {
        id: scrollTimer
        interval: 50
        repeat: false
        onTriggered: {
            chatViewRoot.scrollToBottomCompletely();
            chatViewRoot.checkAndSendSeenReceipt();
        }
    }

    function checkAndSendSeenReceipt() {
        if (!messageListView.atYEnd || messageListView.moving || messageListView.dragging) return;
        var chan = activeChannel ? activeChannel.trim() : "";
        if (selectedServer === "dms" && chan && chan !== "saved-messages" && chan !== "friends") {
            var key = selectedServer + ":" + chan;
            if (typeof NetworkManager === "undefined" || !NetworkManager || (!NetworkManager.isInvisible && NetworkManager.effectiveStatus !== "offline")) {
                MessageService.sendSeenReceipt(key, "all");
            }
        }
        if (typeof NotificationManager !== "undefined" && NotificationManager) {
            if (chan) NotificationManager.markChannelAsRead(chan);
            if (selectedServer && chan) NotificationManager.markChannelAsRead(selectedServer + ":" + chan);
        }
    }

    function onUserScrolledToBottom() {
        if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
        if (messageModel) messageModel.clearFirstUnread();
        scrollToBottomCompletely();
        checkAndSendSeenReceipt();
    }

    function scrollToEndIfAtBottom() {
        if (messageListView.atYEnd && !messageListView.moving && !messageListView.dragging) scrollTimer.restart();
    }

    function handleIncomingMessage(isFromMe, msgId) {
        if (isFromMe) {
            if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
            if (messageModel) messageModel.clearFirstUnread();
            scrollTimer.restart();
        } else {
            if (messageListView.atYEnd && !messageListView.moving && !messageListView.dragging && !messageListView.flicking) {
                if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
                scrollTimer.restart();
            } else {
                if (scrollToBottomBtn) {
                    scrollToBottomBtn.unreadCount++;
                }
                if (messageModel) {
                    messageModel.setFirstUnreadMessageId(msgId);
                }
            }
        }
    }

    function onMoreMessagesLoaded(convId, messages) {
        isFetchingMore = false;
    }

    function checkFetchMore() {
        if (!chatViewRoot.hasInitialPositioned || !messageModel || !messageModel.canFetchMore || isFetchingMore) return;
        if (messageListView.contentHeight > messageListView.height && messageListView.contentY <= 80 && (messageListView.moving || messageListView.dragging || messageListView.flicking)) {
            var oldest = messageModel.oldestTimestamp();
            if (oldest > 0) {
                isFetchingMore = true;
                var chan = activeChannel ? activeChannel.trim() : "";
                var convId = selectedServer + ":" + chan;
                MessageService.loadMoreMessages(convId, oldest, 30);
            }
        }
    }

    ChatHeader {
        Layout.fillWidth: true
        selectedServer: chatViewRoot.selectedServer
        activeChannel: chatViewRoot.activeChannel
        membersPanelExpanded: chatViewRoot.userToggledExpanded
        onToggleMembersPanel: chatViewRoot.userToggledExpanded = !chatViewRoot.userToggledExpanded
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
                        model: messageModel
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        spacing: selectedServer === "dms" ? 6 : 2

                        onMovementEnded: {
                            chatViewRoot.saveCurrentScrollPosition();
                            chatViewRoot.checkFetchMore();
                            if (atYEnd) {
                                if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
                                chatViewRoot.checkAndSendSeenReceipt();
                            }
                        }
                        onContentHeightChanged: {
                            if (!chatViewRoot.hasInitialPositioned) {
                                positionViewAtEnd();
                                Qt.callLater(function() {
                                    positionViewAtEnd();
                                });
                            }
                        }
                        onContentYChanged: {
                            if (moving && contentY <= 80) {
                                chatViewRoot.checkFetchMore();
                            }
                            if (atYEnd && !moving) {
                                if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
                                chatViewRoot.checkAndSendSeenReceipt();
                            }
                        }

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
                        delegate: MessageDelegate {
                            selectedServer: chatViewRoot.selectedServer
                            activeChannel: chatViewRoot.activeChannel
                            onOpenMediaModalRequested: function(url, type, name, startPosMs, isPlaying) { chatViewRoot.openMediaModalRequested(url, type, name, startPosMs, isPlaying); }
                            onRetryMessage: function(msgId) { chatViewRoot.retryMessage(msgId); }
                            onAcceptMediaRequested: function(convId, reqId) {
                                chatViewRoot.acceptMediaRequested(convId, reqId);
                                MessageService.acceptMediaRequest(convId, reqId);
                            }
                            onDeclineMediaRequested: function(convId, reqId) {
                                chatViewRoot.declineMediaRequested(convId, reqId);
                                MessageService.declineMediaRequest(convId, reqId);
                            }
                        }

                        // Empty State Placeholder
                        Item {
                            visible: messageModel.count === 0
                            anchors.centerIn: parent
                            width: parent.width * 0.7
                            height: 120

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 8

                                IconImage {
                                    Layout.alignment: Qt.AlignHCenter
                                    source: selectedServer === "dms" ? "qrc:/qt/qml/NeoNect/assets/icons/friends.svg" : "qrc:/qt/qml/NeoNect/assets/icons/hash.svg"
                                    width: 32; height: 32
                                    color: "#4E5058"
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: selectedServer === "dms" ? ("This is the beginning of your direct message history with @" + (activeChannel.replace(/^\w/, c => c.toUpperCase()))) : ("Welcome to #" + activeChannel + "!")
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

                    // Top Loading Indicator when fetching older messages
                    Rectangle {
                        anchors.top: parent.top
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.topMargin: 8
                        width: 32; height: 32
                        radius: 16
                        color: "#2B2D31"
                        border.color: Qt.rgba(255, 255, 255, 0.12)
                        border.width: 1
                        visible: chatViewRoot.isFetchingMore
                        z: 90

                        BusyIndicator {
                            anchors.centerIn: parent
                            running: chatViewRoot.isFetchingMore
                            width: 20; height: 20
                        }
                    }

                    // Floating Scroll-To-Bottom Button with Unread Counter Badge
                    ScrollToBottomButton {
                        id: scrollToBottomBtn
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 16
                        anchors.bottomMargin: 16
                        targetListView: messageListView
                        z: 100
                        onClicked: {
                            chatViewRoot.onUserScrolledToBottom();
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
                                        var detectedType = UIHelpers.detectMediaType(url, fileName);
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
                                        chatViewRoot.sendMessagePayload(itemObj);
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
                                    source: "qrc:/qt/qml/NeoNect/assets/icons/download.svg"
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
                    visible: chatViewRoot.isOtherTyping && selectedServer === "dms" && activeChannel !== "saved-messages"
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
                                    running: chatViewRoot.isOtherTyping
                                    NumberAnimation { from: 0.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 1.0; to: 0.2; duration: 400; easing.type: Easing.InOutQuad }
                                }
                            }
                            Rectangle {
                                width: 5; height: 5; radius: 2.5
                                color: "#00E5FF"
                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: chatViewRoot.isOtherTyping
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
                                    running: chatViewRoot.isOtherTyping
                                    PauseAnimation { duration: 400 }
                                    NumberAnimation { from: 0.2; to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
                                    NumberAnimation { from: 1.0; to: 0.2; duration: 400; easing.type: Easing.InOutQuad }
                                }
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: (chatViewRoot.typingUser || (activeChannel.replace(/^\w/, c => c.toUpperCase()))) + " is typing..."
                            color: "#00E5FF"
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.italic: true
                        }
                    }
                }

                // ─── MESSAGE INPUT SECTION ───
                NeoNectMessageInput {
                    id: messageInput
                    Layout.fillWidth: true
                    Layout.margins: 12
                    channelName: selectedServer === "dms" ? activeChannel.replace("dm-", "").replace(/^\w/, c => c.toUpperCase()) : activeChannel
                    isDM: selectedServer === "dms"
                    onTypingStarted: chatViewRoot.typingStarted()
                    onTypingStopped: chatViewRoot.typingStopped()

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
                        chatViewRoot.sendMessagePayload(itemObj);
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
                        chatViewRoot.sendMessagePayload(itemObj);
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
                        chatViewRoot.sendMessagePayload(itemObj);
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
                        chatViewRoot.sendMessagePayload(itemObj);
                    }
                }
            }

            MembersSidebarPanel {
                id: membersPanel
                Layout.fillHeight: true
                selectedServer: chatViewRoot.selectedServer
                visible: chatViewRoot.selectedServer !== "dms" && chatViewRoot.selectedServer !== "" && membersPanel.expanded
                expanded: chatViewRoot.selectedServer !== "dms" && chatViewRoot.selectedServer !== "" && chatViewRoot.userToggledExpanded
            }
        }
    }
