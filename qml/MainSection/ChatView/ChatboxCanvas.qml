// ChatboxCanvas.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Avila 1.0
import "ChatHandler.js" as ChatHandler

Rectangle {
    id: root
    color: ThemeData.viewsBackground

    ListModel {
        id: messageModel
    }

    // Fixed loop lookup mechanism to protect against null component returns
    function updateStickyAvatar() {
        if (messageListView.count === 0) {
            stickyAvatar.visible = false;
            return;
        }

        for (var i = 0; i < messageListView.count; i++) {
            const item = messageListView.itemAtIndex(i);
            if (!item) continue;

            const modelData = messageModel.get(i);
            if (!modelData || modelData.fromMe) continue;

            const itemTopInView = item.y - messageListView.contentY;
            const itemBottomInView = itemTopInView + item.height - 16;

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
                rightPadding: 8
                topPadding: 5
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ScrollBar.vertical: ScrollBar {
                    id: chatScrollbar
                    active: true
                    policy: ScrollBar.AsNeeded

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: chatScrollbar.pressed ? "#50545c" : "#4f545c"
                        opacity: chatScrollbar.active ? 0.7 : 0
                        Behavior on opacity { NumberAnimation { duration: 100 } }
                    }
                }

                ListView {
                    id: messageListView
                    width: chatScrollView.availableWidth
                    model: messageModel
                    spacing: 6
                    property int stickyAvatarIndex: -1

                    onContentYChanged: Qt.callLater(root.updateStickyAvatar)

                    delegate: Item {
                        // Wraps the row layout securely so height can be safely inferred by the list engine
                        width: messageListView.width
                        height: delegateRow.implicitHeight

                        RowLayout {
                            id: delegateRow
                            anchors.fill: parent
                            spacing: 8
                            Layout.topMargin: model.isFirstInBlock ? 12 : 0

                            Item { Layout.fillWidth: true; visible: model.fromMe }

                            CircularImage {
                                id: avatar
                                source: model.senderAvatar
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                Layout.alignment: Qt.AlignTop
                                visible: !model.fromMe
                                opacity: model.isFirstInBlock && messageListView.stickyAvatarIndex !== index ? 1.0 : 0.0
                            }

                            ColumnLayout {
                                spacing: 2
                                Layout.maximumWidth: root.width * 0.70

                                Text {
                                    text: model.senderName
                                    visible: !model.fromMe && model.isFirstInBlock
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                }

                                MessageBubble {
                                    messageText: model.text
                                    sentByMe: model.fromMe
                                }
                            }

                            Item { Layout.fillWidth: true; visible: !model.fromMe }
                        }
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
                x: 0 // Adjusted margin placement rules to match row track offsets
                y: 0
                width: 32
                height: 32
                visible: false
                source: ""
            }
        }

        MessageInputCanvas {
            id: messageInput
            Layout.fillWidth: true
            Layout.margins: 12

            onSendMessage: msgText => {
                ChatHandler.sendMessage(msgText, messageModel);
                Qt.callLater(messageListView.positionViewAtEnd);
            }
            onInputHeightChanged: {
                Qt.callLater(messageListView.positionViewAtEnd);
            }
        }
    }
}
