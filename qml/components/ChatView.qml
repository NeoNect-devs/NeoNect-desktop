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
    property bool isRestoringPosition: false
    property string currentChatId: ""
    property alias currentConvKey: chatViewRoot.currentChatId
    property real preFetchContentHeight: 0
    property real preFetchContentY: 0
    property bool isPrependingHistory: false
    
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

    Connections {
        target: messageModel
        ignoreUnknownSignals: true
        function onConversationReady(convId) {
            if (convId && chatViewRoot.currentChatId && convId.toLowerCase() === chatViewRoot.currentChatId.toLowerCase()) {
                var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
                chatViewRoot.restorePositionForChat(convId, unread);
            }
        }
        function onCountChanged() {
            if (chatViewRoot.isRestoringPosition || chatViewRoot.isPrependingHistory) return;
            if (chatViewRoot.visible && messageModel && messageModel.count > 0 && !chatViewRoot.hasInitialPositioned && chatViewRoot.currentChatId) {
                var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
                chatViewRoot.restorePositionForChat(chatViewRoot.currentChatId, unread);
            }
        }
    }

    onVisibleChanged: {
        if (visible) {
            if (currentChatId) {
                hasInitialPositioned = false;
                isRestoringPosition = true;
                messageListView.forceLayout();
                if (messageModel && messageModel.count > 0) {
                    var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
                    restorePositionForChat(currentChatId, unread);
                }
            }
        } else {
            if (currentChatId) {
                saveCurrentPosition(currentChatId);
            }
        }
    }

    Component.onDestruction: {
        if (currentChatId) {
            saveCurrentPosition(currentChatId);
        }
    }

    function isNearBottom(threshold) {
        if (!messageListView) return true;
        if (messageListView.contentHeight <= messageListView.height) return true;
        var thresh = (threshold !== undefined) ? threshold : 120;
        var dist = (messageListView.contentHeight - messageListView.height) - messageListView.contentY;
        return dist <= thresh;
    }

    function calculateScrollMemory() {
        if (!messageListView || !messageModel || messageModel.count === 0) {
            return { lastVisibleMessageId: "", pixelOffset: 0, wasAtEnd: true };
        }

        // 1. Detect if the user was snapped to the very end/bottom of the feed
        var atBottom = messageListView.atYEnd || isNearBottom(60) || (messageListView.contentItem && messageListView.contentItem.height <= messageListView.height);
        if (atBottom) {
            return {
                lastVisibleMessageId: "",
                pixelOffset: 0,
                wasAtEnd: true
            };
        }

        // 2. Identify top-most visible message in content coordinates
        var topY = messageListView.contentY;
        var idx = messageListView.indexAt(messageListView.width / 2, topY + 2);
        if (idx < 0) {
            for (var offset = 6; offset <= 60; offset += 6) {
                idx = messageListView.indexAt(messageListView.width / 2, topY + offset);
                if (idx >= 0) break;
            }
        }

        if (idx < 0 || idx >= messageModel.count) {
            idx = 0;
        }

        var item = messageListView.itemAtIndex(idx);
        var offset = 0;
        if (item) {
            offset = topY - item.y;
        }

        var msgId = messageModel.getMessageIdAt(idx);

        return {
            lastVisibleMessageId: msgId,
            pixelOffset: offset,
            wasAtEnd: false
        };
    }

    function saveCurrentPosition(chatId) {
        var targetId = (chatId !== undefined && chatId !== "") ? chatId : currentChatId;
        targetId = (targetId ? targetId.trim().toLowerCase() : "");
        if (!targetId || !chatViewRoot.visible || !messageModel || isRestoringPosition) return;

        // 1. Calculate robust anchor scroll memory from the current ListView items
        var mem = calculateScrollMemory();
        if (messageModel.saveChatPosition) {
            messageModel.saveChatPosition(targetId, mem.lastVisibleMessageId, mem.pixelOffset, mem.wasAtEnd);
        }
        if (messageModel.saveScrollMemory) {
            messageModel.saveScrollMemory(targetId, mem.lastVisibleMessageId, mem.pixelOffset, mem.wasAtEnd);
        }
        if (messageModel.saveScrollPosition) {
            messageModel.saveScrollPosition(targetId, messageListView.contentY, mem.wasAtEnd);
        }

        // 2. Save draft text and attachment to C++ model
        if (messageInput) {
            var draft = messageInput.getDraftState();
            if (messageModel.saveDraft) {
                messageModel.saveDraft(targetId, draft.text, draft.attachment ? draft.attachment : ({}));
            }
        }
    }

    function saveConversationState() {
        saveCurrentPosition(currentChatId);
    }

    function saveCurrentScrollPosition() {
        saveCurrentPosition(currentChatId);
    }

    function restorePositionForChat(chatId, unread) {
        var targetId = (chatId !== undefined && chatId !== "") ? chatId : currentChatId;
        targetId = (targetId ? targetId.trim().toLowerCase() : "");
        if (!chatViewRoot.visible || !messageModel || messageModel.count === 0 || !targetId) {
            isRestoringPosition = false;
            return;
        }

        isRestoringPosition = true;

        // 1. Restore draft text and attachment
        if (messageInput) {
            var draftText = messageModel.getDraftText ? messageModel.getDraftText(targetId) : "";
            var draftAtt = messageModel.getDraftAttachment ? messageModel.getDraftAttachment(targetId) : ({});
            messageInput.setDraftState(draftText, draftAtt);
        }

        // 2. Retrieve scroll position memory from C++
        var hasMem = messageModel.hasChatPosition ? messageModel.hasChatPosition(targetId)
                   : (messageModel.hasScrollMemory ? messageModel.hasScrollMemory(targetId) : false);
        var mem = messageModel.getChatPosition ? messageModel.getChatPosition(targetId)
                : (messageModel.getScrollMemory ? messageModel.getScrollMemory(targetId) : null);

        var wasAtEnd = mem ? mem.wasAtEnd : true;
        var anchorId = mem ? (mem.messageId ? mem.messageId : mem.lastVisibleMessageId) : "";
        var pixelOffset = mem ? (mem.offset !== undefined ? mem.offset : mem.pixelOffset) : 0;

        // Determine unread count
        var unreadCount = (unread !== undefined && unread > 0) ? unread : 0;
        if (unreadCount === 0 && selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager && activeChannel) {
            unreadCount = NetworkManager.unreadCount(activeChannel);
        }

        // 3. Force ListView layout to measure items
        messageListView.forceLayout();

        // 4. Case A: User was reading history at an anchor message
        if (hasMem && !wasAtEnd && anchorId && anchorId !== "") {
            var idx = messageModel.indexOfMessageId ? messageModel.indexOfMessageId(anchorId) : -1;
            if (idx >= 0 && idx < messageModel.count) {
                if (unreadCount > 0 && scrollToBottomBtn) {
                    scrollToBottomBtn.unreadCount = unreadCount;
                }
                messageListView.positionViewAtIndex(idx, ListView.Beginning);
                var item = messageListView.itemAtIndex(idx);
                if (item) {
                    messageListView.contentY = Math.max(0, Math.min(item.y + pixelOffset, messageListView.contentHeight - messageListView.height));
                } else {
                    messageListView.contentY = Math.max(0, Math.min(messageListView.contentY + pixelOffset, messageListView.contentHeight - messageListView.height));
                }

                // Layout pass: let delegates settle and then re-anchor exactly
                Qt.callLater(function() {
                    if (!chatViewRoot.visible || !messageListView || idx >= messageModel.count) {
                        chatViewRoot.isRestoringPosition = false;
                        return;
                    }
                    messageListView.forceLayout();
                    var lateItem = messageListView.itemAtIndex(idx);
                    if (lateItem) {
                        messageListView.contentY = Math.max(0, Math.min(lateItem.y + pixelOffset, messageListView.contentHeight - messageListView.height));
                    }
                    chatViewRoot.hasInitialPositioned = true;
                    chatViewRoot.isRestoringPosition = false;
                });
                return;
            }
        }

        // 5. Case B: Unread messages present and user was at bottom or fresh visit
        if (unreadCount > 0 && messageModel.count > 0) {
            var unreadIdx = Math.max(0, messageModel.count - unreadCount);
            if (messageModel.setFirstUnreadIndex) {
                messageModel.setFirstUnreadIndex(unreadIdx);
            }
            if (scrollToBottomBtn) {
                scrollToBottomBtn.unreadCount = unreadCount;
            }
            messageListView.positionViewAtIndex(unreadIdx, ListView.Beginning);
            Qt.callLater(function() {
                if (!chatViewRoot.visible || !messageListView) {
                    chatViewRoot.isRestoringPosition = false;
                    return;
                }
                if (messageModel && messageModel.count > 0) {
                    messageListView.forceLayout();
                    messageListView.positionViewAtIndex(unreadIdx, ListView.Beginning);
                    chatViewRoot.hasInitialPositioned = true;
                    chatViewRoot.isRestoringPosition = false;
                } else {
                    chatViewRoot.isRestoringPosition = false;
                }
            });
            return;
        }

        // 6. Case C: Bottom pinning (User was at end or default view)
        messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
        Qt.callLater(function() {
            if (!chatViewRoot.visible || !messageListView) {
                chatViewRoot.isRestoringPosition = false;
                return;
            }
            if (messageModel && messageModel.count > 0) {
                messageListView.forceLayout();
                messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
                chatViewRoot.hasInitialPositioned = true;
                chatViewRoot.isRestoringPosition = false;
                chatViewRoot.checkAndSendSeenReceipt();
            } else {
                chatViewRoot.isRestoringPosition = false;
            }
        });
    }

    function executeRestorationSequence(unread) {
        restorePositionForChat(currentChatId, unread);
    }

    function restoreConversationState(unread) {
        restorePositionForChat(currentChatId, unread);
    }

    function restoreScrollPosition(unread) {
        restorePositionForChat(currentChatId, unread);
    }

    function scrollToBottomCompletely() {
        if (chatViewRoot.isRestoringPosition || !messageModel || messageModel.count === 0) return;
        messageListView.forceLayout();
        messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
        Qt.callLater(function() {
            if (chatViewRoot.isRestoringPosition) return;
            if (messageModel && messageModel.count > 0) {
                messageListView.forceLayout();
                messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
            }
            Qt.callLater(function() {
                if (chatViewRoot.isRestoringPosition) return;
                if (messageModel && messageModel.count > 0) {
                    messageListView.forceLayout();
                    messageListView.positionViewAtIndex(messageModel.count - 1, ListView.End);
                    chatViewRoot.hasInitialPositioned = true;
                }
            });
        });
    }

    function onConversationLoaded(convId, messages) {
        if (convId && chatViewRoot.currentChatId && convId.toLowerCase() === chatViewRoot.currentChatId.toLowerCase()) {
            var unread = (selectedServer === "dms" && typeof NetworkManager !== "undefined" && NetworkManager) ? NetworkManager.unreadCount(activeChannel) : 0;
            if (unread > 0 && messageModel && messageModel.count > 0) {
                var unreadIdx = Math.max(0, messageModel.count - unread);
                messageModel.setFirstUnreadIndex(unreadIdx);
            }
            restorePositionForChat(convId, unread);
            if (selectedServer === "dms" && activeChannel && activeChannel !== "friends" && activeChannel !== "saved-messages") {
                if (typeof NetworkManager !== "undefined" && NetworkManager) {
                    NetworkManager.markConversationAsRead(activeChannel);
                }
            }
        }
    }

    onActiveChannelChanged: {
        if (messageModel) messageModel.clearFirstUnread();
        isFetchingMore = false;
        isPrependingHistory = false;
        if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
    }

    onSelectedServerChanged: {
        if (messageModel) messageModel.clearFirstUnread();
        isFetchingMore = false;
        isPrependingHistory = false;
        if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
    }

    Timer {
        id: debounceSaveScrollTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (!chatViewRoot.isRestoringPosition && chatViewRoot.currentChatId && chatViewRoot.hasInitialPositioned) {
                chatViewRoot.saveCurrentPosition(chatViewRoot.currentChatId);
            }
        }
    }

    Timer {
        id: scrollTimer
        interval: 50
        repeat: false
        onTriggered: {
            if (chatViewRoot.isRestoringPosition) return;
            chatViewRoot.scrollToBottomCompletely();
            chatViewRoot.checkAndSendSeenReceipt();
        }
    }

    function checkAndSendSeenReceipt() {
        if (chatViewRoot.isRestoringPosition) return;
        if ((!messageListView.atYEnd && !isNearBottom(60)) || messageListView.moving || messageListView.dragging) return;
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
        if (chatViewRoot.isRestoringPosition) return;
        if ((messageListView.atYEnd || isNearBottom(80)) && !messageListView.moving && !messageListView.dragging) scrollTimer.restart();
    }

    function handleIncomingMessage(isFromMe, msgId) {
        if (chatViewRoot.isRestoringPosition) return;
        if (isFromMe) {
            if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
            if (messageModel) messageModel.clearFirstUnread();
            scrollTimer.restart();
        } else {
            var nearBottom = isNearBottom(120) && !messageListView.moving && !messageListView.dragging && !messageListView.flicking;
            if (nearBottom) {
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
        if (isPrependingHistory) {
            messageListView.forceLayout();
            var delta = messageListView.contentHeight - preFetchContentHeight;
            if (delta > 0) {
                messageListView.contentY = preFetchContentY + delta;
            }
            Qt.callLater(function() {
                messageListView.forceLayout();
                var lateDelta = messageListView.contentHeight - preFetchContentHeight;
                if (lateDelta > 0) {
                    messageListView.contentY = preFetchContentY + lateDelta;
                }
                isPrependingHistory = false;
            });
        }
    }

    function checkFetchMore() {
        if (!chatViewRoot.hasInitialPositioned || !messageModel || !messageModel.canFetchMore || isFetchingMore) return;
        if (messageListView.contentHeight > messageListView.height && messageListView.contentY <= 100 && (messageListView.moving || messageListView.dragging || messageListView.flicking)) {
            var oldest = messageModel.oldestTimestamp();
            if (oldest > 0) {
                isFetchingMore = true;
                isPrependingHistory = true;
                preFetchContentHeight = messageListView.contentHeight;
                preFetchContentY = messageListView.contentY;
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
                        flickDeceleration: 1500
                        reuseItems: false
                        cacheBuffer: 1200
                        spacing: selectedServer === "dms" ? 6 : 2
                        pixelAligned: true

                        onMovementEnded: {
                            if (chatViewRoot.isRestoringPosition || !chatViewRoot.currentChatId) return;
                            chatViewRoot.saveCurrentPosition(chatViewRoot.currentChatId);
                            chatViewRoot.checkFetchMore();
                            if (atYEnd || chatViewRoot.isNearBottom(60)) {
                                if (scrollToBottomBtn) scrollToBottomBtn.unreadCount = 0;
                                chatViewRoot.checkAndSendSeenReceipt();
                            }
                        }
                        onContentHeightChanged: {
                            if (chatViewRoot.isRestoringPosition || chatViewRoot.isPrependingHistory || !chatViewRoot.currentChatId) return;
                            if (!chatViewRoot.hasInitialPositioned) {
                                var key = chatViewRoot.currentChatId.toLowerCase();
                                if (messageModel && messageModel.hasChatPosition && messageModel.hasChatPosition(key)) {
                                    var mem = messageModel.getChatPosition(key);
                                    if (mem && !mem.wasAtEnd) {
                                        return; // DO NOT snap to end when user has an anchor!
                                    }
                                }
                                positionViewAtEnd();
                            }
                        }
                        onContentYChanged: {
                            if (chatViewRoot.isRestoringPosition || !chatViewRoot.currentChatId || !chatViewRoot.hasInitialPositioned) return;
                            debounceSaveScrollTimer.restart();
                            if (moving && contentY <= 100) {
                                chatViewRoot.checkFetchMore();
                            }
                            if ((atYEnd || chatViewRoot.isNearBottom(60)) && !moving) {
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
                                    text: selectedServer === "dms" ? ("This is the beginning of your direct message history with " + (typeof NetworkManager !== "undefined" && NetworkManager ? NetworkManager.getDisplayName(activeChannel) : activeChannel) + " (@" + activeChannel + ")") : ("Welcome to #" + activeChannel + "!")
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
                                            messageId: (messageModel && messageModel.generateUuid) ? messageModel.generateUuid() : ("msg_" + Date.now() + "_" + i),
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

                    // Drag & Drop visual feedback overlay (Deep Obsidian Frosted Glass - Loaded on-demand only during drag)
                    Loader {
                        id: dragFeedbackLoader
                        anchors.fill: parent
                        active: chatDropArea.containsDrag
                        z: 9999
                        sourceComponent: Component {
                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: "#DD060709"
                                border.color: "#00E5FF"
                                border.width: 2

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
                            text: (chatViewRoot.typingUser ? (typeof NetworkManager !== "undefined" && NetworkManager ? NetworkManager.getDisplayName(chatViewRoot.typingUser) : chatViewRoot.typingUser) : (typeof NetworkManager !== "undefined" && NetworkManager ? NetworkManager.getDisplayName(activeChannel) : activeChannel)) + " is typing..."
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
                    channelName: selectedServer === "dms" ? (typeof NetworkManager !== "undefined" && NetworkManager ? NetworkManager.getDisplayName(activeChannel) : activeChannel) : activeChannel
                    isDM: selectedServer === "dms"
                    onTypingStarted: chatViewRoot.typingStarted()
                    onTypingStopped: chatViewRoot.typingStopped()

                    onMessageSent: function(msgText) {
                        var itemObj = {
                            messageId: (messageModel && messageModel.generateUuid) ? messageModel.generateUuid() : ("msg_" + Date.now()),
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
                            messageId: (messageModel && messageModel.generateUuid) ? messageModel.generateUuid() : ("msg_" + Date.now()),
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
                            messageId: (messageModel && messageModel.generateUuid) ? messageModel.generateUuid() : ("msg_" + Date.now()),
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
                            messageId: (messageModel && messageModel.generateUuid) ? messageModel.generateUuid() : ("msg_" + Date.now()),
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
