// ChatboxCanvas.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
// No longer need GraphicalEffects
import Avila 1.0
import "ChatHandler.js" as ChatHandler

Rectangle {
    id: root
    color: ThemeData.viewsBackground

    ListModel {
        id: messageModel
    }

    function updateStickyAvatar() {
        for (var i = 0; i < messageListView.count; i++) {
            const item = messageListView.itemAtIndex(i);
            if (!item) continue;

            const modelData = messageModel.get(i);
            if (modelData.fromMe) continue;

            const itemTopInView = item.y - messageListView.contentY;
            const itemBottomInView = itemTopInView + item.height - 26;
            if (itemBottomInView > 0 && itemTopInView < chatScrollView.height) {
                const avatarY = Math.max(0, itemTopInView);
                stickyAvatar.source = modelData.senderAvatar;
                stickyAvatar.y = avatarY;
                stickyAvatar.visible = true;
                messageListView.stickyAvatarIndex = i;
                return;
            }
        }
        stickyAvatar.visible = false;
        messageListView.stickyAvatarIndex = -1;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: chatScrollView
                clip: true
                anchors.fill: parent
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ScrollBar.vertical: ScrollBar {
                    id: chatScrollbar
                    size: 0.6
                    position: 0.2
                    active: true
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    orientation: Qt.Vertical
                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 5
                        implicitHeight: chatScrollView.height
                        color: chatScrollbar.pressed ? Qt.darker("grey", 1.5) : "grey"
                        opacity: chatScrollbar.policy === ScrollBar.AlwaysOn || (chatScrollbar.active && chatScrollbar.size < 1.0) ? 0.75 : 0
                    }
                }

                ListView {
                    id: messageListView
                    width: chatScrollView.width
                    model: messageModel
                    // --- FIX: Set a smaller base spacing for all messages ---
                    spacing: 4
                    property int stickyAvatarIndex: -1

                    onContentYChanged: root.updateStickyAvatar()

                    delegate: RowLayout {
                        width: messageListView.width
                        spacing: 6

                        // --- FIX: Add extra top margin for the first message in a block ---
                        Layout.topMargin: model.isFirstInBlock ? 12 : 0

                        Item { Layout.fillWidth: true; visible: model.fromMe }

                        CircularImage {
                            id: avatar
                            source: model.senderAvatar
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.leftMargin: 4
                            Layout.alignment: Qt.AlignTop
                            visible: !model.fromMe
                            opacity: model.isFirstInBlock && messageListView.stickyAvatarIndex !== index ? 1 : 0
                        }

                        ColumnLayout {
                            spacing: 4
                            Layout.maximumWidth: root.width * 0.70
                            Layout.rightMargin: model.fromMe ? 6 : 0

                            Text {
                                text: model.senderName
                                visible: !model.fromMe && model.isFirstInBlock
                                color: "#8e9297"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MessageBubble {
                                messageText: model.text
                                sentByMe: model.fromMe
                            }
                        }

                        Item { Layout.fillWidth: true; visible: !model.fromMe }
                    }

                    Component.onCompleted: {
                        ChatHandler.loadMessages(messageModel);
                        Qt.callLater(root.updateStickyAvatar);
                        Qt.callLater(messageListView.positionViewAtEnd);
                    }
                }
            }

            CircularImage {
                id: stickyAvatar
                x: 4; y: 0; width: 32; height: 32
                visible: false
                source: ""
            }
        }

        MessageInputCanvas {
            id: messageInput
            Layout.preferredHeight: 28
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            Layout.rightMargin: 3
            Layout.leftMargin: 3
            onSendMessage: msgText => {
                ChatHandler.sendMessage(msgText, messageModel);
                Qt.callLater(messageListView.positionViewAtEnd);
            }
        }
    }
}
