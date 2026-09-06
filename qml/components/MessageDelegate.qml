import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "qrc:/qt/qml/NeoNect/qml/components"
import "UIHelpers.js" as UIHelpers

                        Item {
                            id: delegateRoot
    property string selectedServer: ""
    signal openMediaModalRequested(string url, string type, string name)
    signal retryMessage(string msgId)
                            width: messageListView.width - 12
                            height: messageContentColumn.implicitHeight + (model.isFirstInBlock ? 12 : 4)

                            readonly property bool isDM: selectedServer === "dms"
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
                                    color: UIHelpers.getAvatarColor(model.senderName)

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
                                        text: "Today at " + UIHelpers.formatTime(model.timestamp)
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
                                            source: "qrc:/qt/qml/NeoNect/assets/icons/refresh.svg"
                                            width: 14; height: 14
                                            color: "#FFFFFF"
                                        }

                                        MouseArea {
                                            id: retryMouse1
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: delegateRoot.retryMessage(model.messageId)
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
                                                        onClicked: delegateRoot.openMediaModalRequested(model.mediaUrl, "image", model.fileName || "Image")
                                                    }
                                                }
                                            }

                                            // 4. VIDEO MESSAGE (In-App Player for supported formats)
                                            VideoPlayerItem {
                                                visible: model.messageType === "video" && UIHelpers.isPlayableVideo(model.mediaUrl, model.fileName)
                                                messageId: model.messageId
                                                videoUrl: model.mediaUrl
                                                fileName: model.fileName || "Video"
                                                fileSize: model.fileSize
                                                duration: model.duration || 30
                                                fromMe: model.fromMe
                                                onOpenFullscreenRequested: (url, name) => delegateRoot.openMediaModalRequested(url, "video", name)
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
                                                visible: model.messageType === "file" || (model.messageType === "video" && !UIHelpers.isPlayableVideo(model.mediaUrl, model.fileName))
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
                                                                source: "qrc:/qt/qml/NeoNect/assets/icons/file.svg"
                                                                width: 16; height: 16
                                                                color: delegateRoot.isMe ? ThemeData.accentColor : ThemeData.accentColor
                                                            }

                                                            Text {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                text: UIHelpers.getFileExtension(model.fileName, model.mediaUrl)
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
                                                    text: UIHelpers.formatTime(model.timestamp)
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
                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/check.svg"
                                                        width: 12; height: 12
                                                        color: Qt.rgba(255, 255, 255, 0.7)
                                                    }

                                                    // 3. Delivered Double Check
                                                    IconImage {
                                                        visible: model.status === "delivered"
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/check-check.svg"
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
                                                            source: "qrc:/qt/qml/NeoNect/assets/icons/check-check.svg"
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
                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg"
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
                                            source: "qrc:/qt/qml/NeoNect/assets/icons/refresh.svg"
                                            width: 14; height: 14
                                            color: "#FFFFFF"
                                        }

                                        MouseArea {
                                            id: retryMouse2
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: delegateRoot.retryMessage(model.messageId)
                                        }
                                    }
                                }
                            }
                        }
