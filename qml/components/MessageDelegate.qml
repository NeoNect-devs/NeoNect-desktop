import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0
import "qrc:/qt/qml/NeoNect/qml/components"
import "UIHelpers.js" as UIHelpers

                        Item {
                            id: delegateRoot
    property string selectedServer: ""
    property string activeChannel: ""
    signal openMediaModalRequested(string url, string type, string name, int startPosMs, bool isPlaying)
    signal retryMessage(string msgId)
    signal acceptMediaRequested(string convId, string reqId)
    signal declineMediaRequested(string convId, string reqId)
    visible: true
    width: messageListView.width - 12
    height: visible ? (messageContentColumn.implicitHeight + (model.isFirstInBlock ? 12 : 4) + (model.isFirstUnread ? 32 : 0)) : 0

    readonly property string effectiveType: {
        var t = (model.messageType || "").toLowerCase();
        if (t === "media_request") return "media_request";
        if (t === "sticker" || t === "voice") return t;
        if (t === "image" || t === "video" || t === "audio") return t;
        if (model.mediaUrl || model.fileName) {
            var detected = UIHelpers.detectMediaType(model.mediaUrl, model.fileName);
            if (detected && detected !== "file") return detected;
            if (t === "file") return "file";
        }
        if (t === "text" || !t) return "text";
        return t;
    }

    readonly property bool isDM: selectedServer === "dms"
    readonly property bool isMe: model.fromMe
    readonly property bool isText: effectiveType === "text"
    readonly property bool isSticker: effectiveType === "sticker"
    readonly property bool isImage: effectiveType === "image"
    readonly property bool isVideo: effectiveType === "video"
    readonly property bool isAudio: effectiveType === "audio"
    readonly property bool isVoice: effectiveType === "voice"
    readonly property bool isFile: effectiveType === "file"
    readonly property bool isMediaRequest: effectiveType === "media_request"
    readonly property bool isMediaWidget: isSticker || isImage || isVideo || isAudio || isVoice || isFile || isMediaRequest
    readonly property bool isFailed: model.status === "failed"

    function getConversationId() {
        if (delegateRoot.selectedServer && delegateRoot.activeChannel) {
            return delegateRoot.selectedServer + ":" + delegateRoot.activeChannel;
        }
        if (typeof chatViewRoot !== "undefined" && chatViewRoot && chatViewRoot.selectedServer && chatViewRoot.activeChannel) {
            return chatViewRoot.selectedServer + ":" + chatViewRoot.activeChannel;
        }
        if (model.senderName && !model.fromMe) {
            return "dms:" + model.senderName.toLowerCase();
        }
        return "";
    }

    function formatMediaRequestSize(bytes) {
        return UIHelpers.formatSize(bytes);
    }

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

    // ─── NEW MESSAGES SEPARATOR BAR ───
    Rectangle {
        id: unreadSeparator
        visible: model.isFirstUnread
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 4
        height: 24
        color: "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#F23F43"
                opacity: 0.8
            }

            Rectangle {
                Layout.preferredWidth: unreadBadgeText.implicitWidth + 16
                Layout.preferredHeight: 18
                radius: 9
                color: "#F23F43"

                Text {
                    id: unreadBadgeText
                    anchors.centerIn: parent
                    text: "NEW MESSAGES"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 0.5
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#F23F43"
                opacity: 0.8
            }
        }
    }

    // ─── FLOATING / STICKY AVATAR (on the left for others in DMs and all in Servers) ───
    Item {
        id: avatarContainer
        width: 38; height: 38
        anchors.left: parent.left
        anchors.leftMargin: 16

        readonly property real topOffset: model.isFirstUnread ? 32 : 0
        readonly property real topInView: messageListView ? (delegateRoot.y + topOffset - messageListView.contentY) : 0
        readonly property real bottomInView: topInView + (delegateRoot.height - topOffset)

        visible: (!delegateRoot.isDM || !delegateRoot.isMe) && (
            (model.isFirstInBlock && topInView >= 0) ||
            (topInView < 0 && bottomInView > 0)
        )

        y: {
            if (!messageListView) return topOffset + 6;
            if (topInView >= 0) {
                return topOffset + 6;
            }
            var targetY = topOffset + (-topInView) + 6;
            if (model.isLastInBlock) {
                var maxOffset = delegateRoot.height - avatarContainer.height - 6;
                return Math.max(topOffset + 6, Math.min(maxOffset, targetY));
            }
            return targetY;
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
        anchors.top: model.isFirstUnread ? unreadSeparator.bottom : parent.top
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
                                                horizontalAlignment: UIHelpers.isRTL(model.text) ? Text.AlignRight : Text.AlignLeft
                                            }

                                            // 2. STICKER MESSAGE (Telegram Style)
                                            Item {
                                                visible: delegateRoot.isSticker
                                                implicitWidth: 130; implicitHeight: 130
                                                Layout.preferredWidth: 130; Layout.preferredHeight: 130
                                                width: 130; height: 130

                                                Image {
                                                    anchors.centerIn: parent
                                                    width: 120; height: 120
                                                    source: delegateRoot.isSticker ? (model.mediaUrl || "") : ""
                                                    sourceSize: Qt.size(240, 240)
                                                    fillMode: Image.PreserveAspectFit
                                                    smooth: true
                                                }
                                            }

                                            // 3. IMAGE MESSAGE (Dynamically Scaled & GIF Hover Play)
                                            Item {
                                                id: imgDelegateItem
                                                visible: delegateRoot.isImage

                                                readonly property bool isGif: {
                                                    var url = (model.mediaUrl || "").toLowerCase();
                                                    var fn = (model.fileName || "").toLowerCase();
                                                    return url.endsWith(".gif") || fn.endsWith(".gif");
                                                }
                                                readonly property var activeImgObj: isGif ? chatGif : chatImg
                                                readonly property real naturalW: (activeImgObj && activeImgObj.sourceSize && activeImgObj.sourceSize.width > 0) ? activeImgObj.sourceSize.width : (activeImgObj && activeImgObj.implicitWidth > 0 ? activeImgObj.implicitWidth : 320)
                                                readonly property real naturalH: (activeImgObj && activeImgObj.sourceSize && activeImgObj.sourceSize.height > 0) ? activeImgObj.sourceSize.height : (activeImgObj && activeImgObj.implicitHeight > 0 ? activeImgObj.implicitHeight : 200)
                                                readonly property real ratio: (naturalW > 0 && naturalH > 0) ? (naturalW / naturalH) : 1.6

                                                readonly property real maxAllowedWidth: Math.min(420, Math.max(160, bubbleBox.maxContentWidth))
                                                readonly property real calcWidth: {
                                                    var maxW = maxAllowedWidth;
                                                    var maxH = 340;
                                                    var minW = Math.min(160, maxW);
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
                                                        visible: delegateRoot.isImage && !imgDelegateItem.isGif
                                                        anchors.fill: parent
                                                        source: visible ? UIHelpers.formatMediaSource(model.mediaUrl) : ""
                                                        fillMode: Image.PreserveAspectFit
                                                        smooth: true
                                                        asynchronous: true
                                                    }

                                                    AnimatedImage {
                                                        id: chatGif
                                                        visible: delegateRoot.isImage && imgDelegateItem.isGif
                                                        anchors.fill: parent
                                                        source: visible ? UIHelpers.formatMediaSource(model.mediaUrl) : ""
                                                        fillMode: Image.PreserveAspectFit
                                                        smooth: true
                                                        asynchronous: true
                                                        playing: imgMouseArea.containsMouse
                                                        paused: !imgMouseArea.containsMouse
                                                        currentFrame: !imgMouseArea.containsMouse ? 0 : currentFrame
                                                    }

                                                    // GIF Indicator Badge
                                                    Rectangle {
                                                        visible: imgDelegateItem.isGif && !imgMouseArea.containsMouse
                                                        anchors.left: parent.left
                                                        anchors.top: parent.top
                                                        anchors.margins: 8
                                                        width: 32; height: 20
                                                        radius: 4
                                                        color: Qt.rgba(0, 0, 0, 0.65)
                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: "GIF"
                                                            color: "#FFFFFF"
                                                            font.pixelSize: 10
                                                            font.bold: true
                                                            font.family: "Segoe UI"
                                                        }
                                                    }

                                                    // Loading state
                                                    Rectangle {
                                                        anchors.fill: parent
                                                        visible: (imgDelegateItem.isGif ? chatGif.status : chatImg.status) === Image.Loading
                                                        color: "#16171A"
                                                        BusyIndicator {
                                                            anchors.centerIn: parent
                                                            running: (imgDelegateItem.isGif ? chatGif.status : chatImg.status) === Image.Loading
                                                            width: 28; height: 28
                                                        }
                                                    }

                                                    // Failed to load / download / upload overlay
                                                    Rectangle {
                                                        anchors.fill: parent
                                                        visible: (imgDelegateItem.isGif ? chatGif.status : chatImg.status) === Image.Error || (model.messageType === "image" && delegateRoot.isFailed && (!imgDelegateItem.activeImgObj.source || imgDelegateItem.activeImgObj.source == ""))
                                                        color: "#1B1C20"
                                                        border.color: Qt.rgba(242, 63, 67, 0.4)
                                                        border.width: 1
                                                        radius: 10

                                                        ColumnLayout {
                                                            anchors.centerIn: parent
                                                            spacing: 8

                                                            IconImage {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                source: "qrc:/qt/qml/NeoNect/assets/icons/alert-circle.svg"
                                                                width: 26; height: 26
                                                                color: "#F23F43"
                                                            }

                                                            Text {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                text: delegateRoot.isFailed ? "Image upload failed" : "Failed to load image"
                                                                color: "#FFFFFF"
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 12
                                                                font.bold: true
                                                            }

                                                            Rectangle {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                width: 84; height: 26
                                                                radius: 13
                                                                color: imgRetryMouse.containsMouse ? "#E53935" : Qt.rgba(229, 57, 53, 0.25)
                                                                border.color: "#E53935"
                                                                border.width: 1

                                                                RowLayout {
                                                                    anchors.centerIn: parent
                                                                    spacing: 4
                                                                    IconImage {
                                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/refresh.svg"
                                                                        width: 12; height: 12
                                                                        color: "#FFFFFF"
                                                                    }
                                                                    Text {
                                                                        text: "Retry"
                                                                        color: "#FFFFFF"
                                                                        font.family: "Segoe UI"
                                                                        font.pixelSize: 11
                                                                        font.bold: true
                                                                    }
                                                                }

                                                                MouseArea {
                                                                    id: imgRetryMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        if (delegateRoot.isFailed) {
                                                                            delegateRoot.retryMessage(model.messageId);
                                                                        }
                                                                        if (imgDelegateItem.isGif) {
                                                                            var sg = chatGif.source;
                                                                            chatGif.source = "";
                                                                            chatGif.source = sg;
                                                                        } else {
                                                                            var s = chatImg.source;
                                                                            chatImg.source = "";
                                                                            chatImg.source = s;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }

                                                    // Transferring overlay (Uploading / Downloading)
                                                    Rectangle {
                                                        id: imgTransferOverlay
                                                        anchors.fill: parent
                                                        visible: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                        color: Qt.rgba(0, 0, 0, 0.78)
                                                        radius: 10
                                                        z: 5

                                                        ColumnLayout {
                                                            anchors.centerIn: parent
                                                            spacing: 8

                                                            Item {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                width: 52; height: 52

                                                                Canvas {
                                                                    id: imgProgressCanvas
                                                                    anchors.fill: parent
                                                                    property real progressVal: Math.max(0.05, Math.min(1.0, (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05))
                                                                    onProgressValChanged: requestPaint()
                                                                    Component.onCompleted: requestPaint()

                                                                    onPaint: {
                                                                        var ctx = getContext("2d");
                                                                        ctx.reset();
                                                                        var cx = width / 2;
                                                                        var cy = height / 2;
                                                                        var r = width / 2 - 4;

                                                                        // Background track
                                                                        ctx.beginPath();
                                                                        ctx.arc(cx, cy, r, 0, 2 * Math.PI, false);
                                                                        ctx.lineWidth = 3.5;
                                                                        ctx.strokeStyle = "rgba(255, 255, 255, 0.2)";
                                                                        ctx.stroke();

                                                                        // Progress arc
                                                                        var startAngle = -Math.PI / 2;
                                                                        var endAngle = startAngle + (2 * Math.PI * progressVal);
                                                                        ctx.beginPath();
                                                                        ctx.arc(cx, cy, r, startAngle, endAngle, false);
                                                                        ctx.lineWidth = 3.5;
                                                                        ctx.strokeStyle = ThemeData.accentColor;
                                                                        ctx.lineCap = "round";
                                                                        ctx.stroke();
                                                                    }
                                                                }

                                                                // Cancel button in center
                                                                Rectangle {
                                                                    id: imgCancelCircle
                                                                    anchors.centerIn: parent
                                                                    width: 30; height: 30
                                                                    radius: 15
                                                                    color: imgCancelMouse.containsMouse ? "#E53935" : "rgba(0, 0, 0, 0.65)"
                                                                    border.color: imgCancelMouse.containsMouse ? "#E53935" : "rgba(255, 255, 255, 0.35)"
                                                                    border.width: 1

                                                                    Text {
                                                                        anchors.centerIn: parent
                                                                        text: "✕"
                                                                        color: "#FFFFFF"
                                                                        font.family: "Segoe UI"
                                                                        font.pixelSize: 11
                                                                        font.bold: true
                                                                    }

                                                                    MouseArea {
                                                                        id: imgCancelMouse
                                                                        anchors.fill: parent
                                                                        hoverEnabled: true
                                                                        cursorShape: Qt.PointingHandCursor
                                                                        onClicked: {
                                                                            if (typeof MessageService !== "undefined" && MessageService) {
                                                                                MessageService.cancelMediaTransfer(delegateRoot.getConversationId(), model.messageId);
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }

                                                            ColumnLayout {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                spacing: 2

                                                                Text {
                                                                    Layout.alignment: Qt.AlignHCenter
                                                                    text: model.status === "downloading" ? "Downloading..." : "Uploading..."
                                                                    color: ThemeData.textPrimary
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 11
                                                                    font.bold: true
                                                                }

                                                                Text {
                                                                    Layout.alignment: Qt.AlignHCenter
                                                                    property real currentProg: (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05
                                                                    property real totalBytes: (model.fileSize && model.fileSize > 0) ? model.fileSize : ((model.transferBytes && model.transferBytes > 0 && currentProg > 0) ? Math.round(model.transferBytes / currentProg) : 2500000)
                                                                    property real effectiveBytes: Math.min(totalBytes, (model.transferBytes && model.transferBytes > 0) ? model.transferBytes : Math.round(currentProg * totalBytes))
                                                                    text: UIHelpers.formatSize(effectiveBytes) + " / " + UIHelpers.formatSize(totalBytes) + " (" + Math.round(currentProg * 100) + "%)"
                                                                    color: ThemeData.textSecondary
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 10
                                                                }
                                                            }
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: imgMouseArea
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        visible: ((imgDelegateItem.isGif ? chatGif.status : chatImg.status) === Image.Ready) && model.status !== "sending" && model.status !== "downloading"
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: delegateRoot.openMediaModalRequested(UIHelpers.formatMediaSource(model.mediaUrl), "image", model.fileName || "Image", 0, false)
                                                    }
                                                }
                                            }

                                            // 4. VIDEO MESSAGE (In-App Player for supported formats)
                                            Item {
                                                id: videoDelegateWrapper
                                                visible: delegateRoot.isVideo && UIHelpers.isPlayableVideo(model.mediaUrl, model.fileName)
                                                implicitWidth: videoPlayerInstance.implicitWidth
                                                implicitHeight: videoPlayerInstance.implicitHeight
                                                Layout.preferredWidth: videoPlayerInstance.width
                                                Layout.preferredHeight: videoPlayerInstance.height
                                                width: videoPlayerInstance.width
                                                height: videoPlayerInstance.height

                                                VideoPlayerItem {
                                                    id: videoPlayerInstance
                                                    anchors.fill: parent
                                                    messageId: model.messageId
                                                    videoUrl: UIHelpers.formatMediaSource(model.mediaUrl)
                                                    fileName: model.fileName || "Video"
                                                    fileSize: model.fileSize
                                                    duration: model.duration || 30
                                                    fromMe: model.fromMe
                                                    onOpenFullscreenRequested: (url, name, pos, playing) => delegateRoot.openMediaModalRequested(UIHelpers.formatMediaSource(url), "video", name, pos, playing)
                                                }

                                                // Video Transfer Progress Overlay
                                                Rectangle {
                                                    anchors.fill: parent
                                                    radius: 12
                                                    visible: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                    color: Qt.rgba(0, 0, 0, 0.82)
                                                    z: 10

                                                    ColumnLayout {
                                                        anchors.centerIn: parent
                                                        spacing: 8

                                                        Item {
                                                            Layout.alignment: Qt.AlignHCenter
                                                            width: 52; height: 52

                                                            Canvas {
                                                                anchors.fill: parent
                                                                property real progressVal: Math.max(0.05, Math.min(1.0, (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05))
                                                                onProgressValChanged: requestPaint()
                                                                Component.onCompleted: requestPaint()

                                                                onPaint: {
                                                                    var ctx = getContext("2d");
                                                                    ctx.reset();
                                                                    var cx = width / 2;
                                                                    var cy = height / 2;
                                                                    var r = width / 2 - 4;

                                                                    ctx.beginPath();
                                                                    ctx.arc(cx, cy, r, 0, 2 * Math.PI, false);
                                                                    ctx.lineWidth = 3.5;
                                                                    ctx.strokeStyle = "rgba(255, 255, 255, 0.2)";
                                                                    ctx.stroke();

                                                                    var startAngle = -Math.PI / 2;
                                                                    var endAngle = startAngle + (2 * Math.PI * progressVal);
                                                                    ctx.beginPath();
                                                                    ctx.arc(cx, cy, r, startAngle, endAngle, false);
                                                                    ctx.lineWidth = 3.5;
                                                                    ctx.strokeStyle = ThemeData.accentColor;
                                                                    ctx.lineCap = "round";
                                                                    ctx.stroke();
                                                                }
                                                            }

                                                            Rectangle {
                                                                anchors.centerIn: parent
                                                                width: 30; height: 30
                                                                radius: 15
                                                                color: vidCancelMouse.containsMouse ? "#E53935" : "rgba(0, 0, 0, 0.65)"
                                                                border.color: vidCancelMouse.containsMouse ? "#E53935" : "rgba(255, 255, 255, 0.35)"
                                                                border.width: 1

                                                                Text {
                                                                    anchors.centerIn: parent
                                                                    text: "✕"
                                                                    color: "#FFFFFF"
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 11
                                                                    font.bold: true
                                                                }

                                                                MouseArea {
                                                                    id: vidCancelMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        if (typeof MessageService !== "undefined" && MessageService) {
                                                                            MessageService.cancelMediaTransfer(delegateRoot.getConversationId(), model.messageId);
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        ColumnLayout {
                                                            Layout.alignment: Qt.AlignHCenter
                                                            spacing: 2

                                                            Text {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                text: model.status === "downloading" ? "Downloading Video..." : "Uploading Video..."
                                                                color: ThemeData.textPrimary
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 11
                                                                font.bold: true
                                                            }

                                                            Text {
                                                                Layout.alignment: Qt.AlignHCenter
                                                                property real currentProg: (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05
                                                                property real totalBytes: (model.fileSize && model.fileSize > 0) ? model.fileSize : ((model.transferBytes && model.transferBytes > 0 && currentProg > 0) ? Math.round(model.transferBytes / currentProg) : 5000000)
                                                                property real effectiveBytes: Math.min(totalBytes, (model.transferBytes && model.transferBytes > 0) ? model.transferBytes : Math.round(currentProg * totalBytes))
                                                                text: UIHelpers.formatSize(effectiveBytes) + " / " + UIHelpers.formatSize(totalBytes) + " (" + Math.round(currentProg * 100) + "%)"
                                                                color: ThemeData.textSecondary
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 10
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // 5. MUSIC AUDIO MESSAGE
                                            Item {
                                                id: audioDelegateWrapper
                                                visible: delegateRoot.isAudio
                                                implicitWidth: audioPlayerInstance.implicitWidth
                                                implicitHeight: audioPlayerInstance.implicitHeight
                                                Layout.preferredWidth: audioPlayerInstance.width
                                                Layout.preferredHeight: audioPlayerInstance.height
                                                width: audioPlayerInstance.width
                                                height: audioPlayerInstance.height

                                                AudioMusicPlayer {
                                                    id: audioPlayerInstance
                                                    anchors.fill: parent
                                                    messageId: model.messageId
                                                    audioUrl: UIHelpers.formatMediaSource(model.mediaUrl)
                                                    fileName: model.fileName || "Audio"
                                                    fileSize: model.fileSize
                                                    duration: model.duration || 180
                                                    fromMe: model.fromMe
                                                }

                                                // Audio Transfer Overlay
                                                Rectangle {
                                                    anchors.fill: parent
                                                    radius: 12
                                                    visible: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                    color: Qt.rgba(0, 0, 0, 0.85)
                                                    z: 10

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.margins: 12
                                                        spacing: 12

                                                        // Cancel button
                                                        Rectangle {
                                                            Layout.preferredWidth: 32
                                                            Layout.preferredHeight: 32
                                                            radius: 16
                                                            color: audCancelMouse.containsMouse ? "#E53935" : Qt.rgba(255, 255, 255, 0.15)
                                                            border.color: audCancelMouse.containsMouse ? "#E53935" : Qt.rgba(255, 255, 255, 0.3)
                                                            border.width: 1

                                                            Text {
                                                                anchors.centerIn: parent
                                                                text: "✕"
                                                                color: "#FFFFFF"
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 12
                                                                font.bold: true
                                                            }

                                                            MouseArea {
                                                                id: audCancelMouse
                                                                anchors.fill: parent
                                                                hoverEnabled: true
                                                                cursorShape: Qt.PointingHandCursor
                                                                onClicked: {
                                                                    if (typeof MessageService !== "undefined" && MessageService) {
                                                                        MessageService.cancelMediaTransfer(delegateRoot.getConversationId(), model.messageId);
                                                                    }
                                                                }
                                                            }
                                                        }

                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 4

                                                            RowLayout {
                                                                Layout.fillWidth: true
                                                                Text {
                                                                    text: model.status === "downloading" ? "Downloading Audio..." : "Uploading Audio..."
                                                                    color: ThemeData.textPrimary
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 12
                                                                    font.bold: true
                                                                }
                                                                Item { Layout.fillWidth: true }
                                                                Text {
                                                                    property real currentProg: (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05
                                                                    property real totalBytes: (model.fileSize && model.fileSize > 0) ? model.fileSize : ((model.transferBytes && model.transferBytes > 0 && currentProg > 0) ? Math.round(model.transferBytes / currentProg) : 2000000)
                                                                    property real effectiveBytes: Math.min(totalBytes, (model.transferBytes && model.transferBytes > 0) ? model.transferBytes : Math.round(currentProg * totalBytes))
                                                                    text: UIHelpers.formatSize(effectiveBytes) + " / " + UIHelpers.formatSize(totalBytes) + " (" + Math.round(currentProg * 100) + "%)"
                                                                    color: ThemeData.accentColor
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 10
                                                                }
                                                            }

                                                            Rectangle {
                                                                Layout.fillWidth: true
                                                                Layout.preferredHeight: 3
                                                                radius: 1.5
                                                                color: Qt.rgba(255, 255, 255, 0.1)
                                                                clip: true

                                                                Rectangle {
                                                                    anchors.left: parent.left
                                                                    anchors.top: parent.top
                                                                    anchors.bottom: parent.bottom
                                                                    width: parent.width * Math.max(0.05, Math.min(1.0, (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05))
                                                                    color: ThemeData.accentColor
                                                                    radius: 1.5
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // 6. VOICE NOTE MESSAGE (Telegram Style)
                                            VoiceMessagePlayer {
                                                visible: delegateRoot.isVoice
                                                messageId: model.messageId
                                                audioUrl: UIHelpers.formatMediaSource(model.mediaUrl)
                                                duration: model.duration || 6
                                                waveform: model.waveform
                                                fromMe: model.fromMe
                                            }

                                            // 7. FILE / DOCUMENT / NON-PLAYABLE MEDIA MESSAGE (Rich Card with External Open)
                                            Rectangle {
                                                visible: delegateRoot.isFile || (delegateRoot.isVideo && !UIHelpers.isPlayableVideo(model.mediaUrl, model.fileName))
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
                                                                text: UIHelpers.formatSize(model.fileSize)
                                                                color: delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.75) : ThemeData.textSecondary
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 10
                                                            }

                                                            Text {
                                                                readonly property bool isTransferring: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                                property real currentProg: (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05
                                                                property real totalBytes: (model.fileSize && model.fileSize > 0) ? model.fileSize : ((model.transferBytes && model.transferBytes > 0 && currentProg > 0) ? Math.round(model.transferBytes / currentProg) : 2500000)
                                                                property real effectiveBytes: Math.min(totalBytes, (model.transferBytes && model.transferBytes > 0) ? model.transferBytes : Math.round(currentProg * totalBytes))

                                                                text: isTransferring ? 
                                                                    ("•  " + (model.status === "downloading" ? "Downloading " : "Uploading ") + UIHelpers.formatSize(effectiveBytes) + " / " + UIHelpers.formatSize(totalBytes) + " (" + Math.round(currentProg * 100) + "%)") :
                                                                    "•  Click to open ↗"
                                                                color: isTransferring ? ThemeData.accentColor : (delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.6) : ThemeData.accentColor)
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 10
                                                            }
                                                        }
                                                    }

                                                    // Open / Cancel Icon
                                                    Rectangle {
                                                        width: 28; height: 28
                                                        radius: 14
                                                        readonly property bool isTransferring: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                        color: isTransferring ? 
                                                            (fileCancelMouse.containsMouse ? "#E53935" : Qt.rgba(255, 255, 255, 0.15)) : 
                                                            (fileCardMouse.containsMouse ? (delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.3) : Qt.rgba(255, 255, 255, 0.15)) : "transparent")

                                                        Text {
                                                            anchors.centerIn: parent
                                                            text: parent.isTransferring ? "✕" : "↗"
                                                            color: delegateRoot.isMe ? "#FFFFFF" : (parent.isTransferring ? "#FFFFFF" : ThemeData.textSecondary)
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: parent.isTransferring ? 12 : 14
                                                            font.bold: true
                                                        }

                                                        MouseArea {
                                                            id: fileCancelMouse
                                                            anchors.fill: parent
                                                            enabled: parent.isTransferring
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (typeof MessageService !== "undefined" && MessageService) {
                                                                    MessageService.cancelMediaTransfer(delegateRoot.getConversationId(), model.messageId);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                // Bottom progress bar when transferring
                                                Rectangle {
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.bottom: parent.bottom
                                                    height: 3
                                                    radius: 1.5
                                                    color: Qt.rgba(255, 255, 255, 0.1)
                                                    visible: (model.status === "sending" || model.status === "downloading") && !delegateRoot.isFailed
                                                    clip: true

                                                    Rectangle {
                                                        anchors.left: parent.left
                                                        anchors.top: parent.top
                                                        anchors.bottom: parent.bottom
                                                        width: parent.width * Math.max(0.05, Math.min(1.0, (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05))
                                                        color: ThemeData.accentColor
                                                        radius: 1.5
                                                    }
                                                }

                                                MouseArea {
                                                    id: fileCardMouse
                                                    anchors.fill: parent
                                                    enabled: model.status !== "sending" && model.status !== "downloading"
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: AudioManager.openMediaFile(model.mediaUrl)
                                                }
                                            }

                                            // 6. MEDIA REQUEST CARD (Two-Phase Transfer Flow)
                                            Rectangle {
                                                id: mediaRequestCard
                                                visible: delegateRoot.isMediaRequest
                                                readonly property real cardAvailableWidth: Math.max(220, bubbleBox.maxContentWidth - (delegateRoot.isDM ? 12 : 0))
                                                readonly property real responsiveCardWidth: Math.min(380, cardAvailableWidth)
                                                Layout.preferredWidth: responsiveCardWidth
                                                implicitWidth: responsiveCardWidth
                                                implicitHeight: requestContentCol.implicitHeight + 24
                                                width: responsiveCardWidth
                                                height: implicitHeight
                                                radius: 12
                                                color: delegateRoot.isMe ? "#2F3136" : "#202225"
                                                border.color: {
                                                    if (model.status === "accepted") return "#23A55A";
                                                    if (model.status === "declined" || model.status === "failed") return Qt.rgba(242, 63, 67, 0.4);
                                                    return delegateRoot.isMe ? Qt.rgba(255, 255, 255, 0.15) : Qt.rgba(88, 101, 242, 0.35);
                                                }
                                                border.width: 1

                                                readonly property string mediaCategory: {
                                                    var t = (model.errorText || "").toLowerCase();
                                                    if (!t || t === "pending" || t === "accepted" || t === "declined" || t === "sent" || t === "failed") {
                                                        t = (UIHelpers.detectMediaType(model.mediaUrl, model.fileName) || "file").toLowerCase();
                                                    }
                                                    return t;
                                                }

                                                ColumnLayout {
                                                    id: requestContentCol
                                                    anchors.left: parent.left
                                                    anchors.right: parent.right
                                                    anchors.top: parent.top
                                                    anchors.margins: 12
                                                    spacing: 8

                                                    // Header Row: Media Icon, Filename & Type/Size Badges
                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 10

                                                        Rectangle {
                                                            Layout.preferredWidth: 40
                                                            Layout.preferredHeight: 40
                                                            radius: 8
                                                            color: {
                                                                var t = mediaRequestCard.mediaCategory;
                                                                if (t === "image") return Qt.rgba(88, 101, 242, 0.2);
                                                                if (t === "video") return Qt.rgba(235, 69, 158, 0.2);
                                                                if (t === "audio") return Qt.rgba(0, 163, 108, 0.2);
                                                                return Qt.rgba(250, 166, 26, 0.2);
                                                            }
                                                            border.color: {
                                                                var t = mediaRequestCard.mediaCategory;
                                                                if (t === "image") return "#5865F2";
                                                                if (t === "video") return "#EB459E";
                                                                if (t === "audio") return "#00A36C";
                                                                return "#FAA61A";
                                                            }
                                                            border.width: 1

                                                            IconImage {
                                                                anchors.centerIn: parent
                                                                source: {
                                                                    var t = mediaRequestCard.mediaCategory;
                                                                    if (t === "image") return "qrc:/qt/qml/NeoNect/assets/icons/image.svg";
                                                                    if (t === "video") return "qrc:/qt/qml/NeoNect/assets/icons/video.svg";
                                                                    if (t === "audio") return "qrc:/qt/qml/NeoNect/assets/icons/music.svg";
                                                                    return "qrc:/qt/qml/NeoNect/assets/icons/file.svg";
                                                                }
                                                                width: 18; height: 18
                                                                color: parent.border.color
                                                            }
                                                        }

                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 3

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: model.fileName !== "" ? model.fileName : "Media File"
                                                                color: ThemeData.textPrimary
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 13
                                                                font.bold: true
                                                                elide: Text.ElideMiddle
                                                            }

                                                            RowLayout {
                                                                spacing: 6

                                                                Rectangle {
                                                                    Layout.preferredHeight: 16
                                                                    Layout.preferredWidth: catLabel.implicitWidth + 8
                                                                    radius: 3
                                                                    color: Qt.rgba(255, 255, 255, 0.1)

                                                                    Text {
                                                                        id: catLabel
                                                                        anchors.centerIn: parent
                                                                        text: mediaRequestCard.mediaCategory.toUpperCase()
                                                                        color: ThemeData.textSecondary
                                                                        font.family: "Segoe UI"
                                                                        font.pixelSize: 9
                                                                        font.bold: true
                                                                    }
                                                                }

                                                                Text {
                                                                    text: "•"
                                                                    color: ThemeData.textMuted
                                                                    font.pixelSize: 10
                                                                }

                                                                Text {
                                                                    text: delegateRoot.formatMediaRequestSize(model.fileSize)
                                                                    color: ThemeData.textSecondary
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 11
                                                                    font.bold: true
                                                                }
                                                            }
                                                        }
                                                    }

                                                    Text {
                                                        visible: model.text !== ""
                                                        Layout.fillWidth: true
                                                        text: model.text
                                                        color: delegateRoot.isMe ? "#FFFFFF" : ThemeData.textPrimary
                                                        font.family: "Segoe UI"
                                                        font.pixelSize: 12
                                                        wrapMode: Text.Wrap
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        height: 1
                                                        color: Qt.rgba(255, 255, 255, 0.08)
                                                    }

                                                    // Sender Status Row (when delegateRoot.isMe)
                                                    ColumnLayout {
                                                        visible: delegateRoot.isMe
                                                        Layout.fillWidth: true
                                                        spacing: 6

                                                        RowLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 8

                                                            Text {
                                                                Layout.fillWidth: true
                                                                text: {
                                                                    if (model.status === "accepted" || model.status === "sending") return "✓ Request accepted — Sending media...";
                                                                    if (model.status === "declined") return "✕ Request declined by recipient";
                                                                    if (model.status === "failed") return "⚠ Transfer failed — Tap retry to re-send";
                                                                    return "⏳ Media request sent — Awaiting approval...";
                                                                }
                                                                color: {
                                                                    if (model.status === "accepted" || model.status === "sending") return "#23A55A";
                                                                    if (model.status === "declined" || model.status === "failed") return "#F23F43";
                                                                    return "#FAA61A";
                                                                }
                                                                font.family: "Segoe UI"
                                                                font.pixelSize: 11
                                                                font.bold: true
                                                                wrapMode: Text.WordWrap
                                                            }

                                                            // Retry Button inside media request card when failed
                                                            Rectangle {
                                                                visible: model.status === "failed"
                                                                Layout.preferredWidth: 68
                                                                Layout.preferredHeight: 26
                                                                radius: 13
                                                                color: reqRetryMouse.containsMouse ? "#E53935" : Qt.rgba(229, 57, 53, 0.25)
                                                                border.color: "#E53935"
                                                                border.width: 1

                                                                RowLayout {
                                                                    anchors.centerIn: parent
                                                                    spacing: 4
                                                                    IconImage {
                                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/refresh.svg"
                                                                        width: 11; height: 11
                                                                        color: "#FFFFFF"
                                                                    }
                                                                    Text {
                                                                        text: "Retry"
                                                                        color: "#FFFFFF"
                                                                        font.family: "Segoe UI"
                                                                        font.pixelSize: 10
                                                                        font.bold: true
                                                                    }
                                                                }

                                                                MouseArea {
                                                                    id: reqRetryMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: delegateRoot.retryMessage(model.messageId)
                                                                }
                                                            }
                                                        }

                                                        // Sender Transfer Progress when accepted or sending
                                                        ColumnLayout {
                                                            visible: model.status === "accepted" || model.status === "sending"
                                                            Layout.fillWidth: true
                                                            spacing: 4

                                                            RowLayout {
                                                                Layout.fillWidth: true
                                                                Text {
                                                                    text: "Transmitting..."
                                                                    color: ThemeData.textSecondary
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 10
                                                                    font.bold: true
                                                                }
                                                                Item { Layout.fillWidth: true }
                                                                Text {
                                                                    property real currentProg: (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05
                                                                    property real totalBytes: (model.fileSize && model.fileSize > 0) ? model.fileSize : ((model.transferBytes && model.transferBytes > 0 && currentProg > 0) ? Math.round(model.transferBytes / currentProg) : 2500000)
                                                                    property real effectiveBytes: Math.min(totalBytes, (model.transferBytes && model.transferBytes > 0) ? model.transferBytes : Math.round(currentProg * totalBytes))
                                                                    text: UIHelpers.formatSize(effectiveBytes) + " / " + UIHelpers.formatSize(totalBytes) + " (" + Math.round(currentProg * 100) + "%)"
                                                                    color: ThemeData.accentColor
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 10
                                                                    font.bold: true
                                                                }
                                                            }

                                                            Rectangle {
                                                                Layout.fillWidth: true
                                                                Layout.preferredHeight: 4
                                                                radius: 2
                                                                color: Qt.rgba(255, 255, 255, 0.1)
                                                                clip: true

                                                                Rectangle {
                                                                    anchors.left: parent.left
                                                                    anchors.top: parent.top
                                                                    anchors.bottom: parent.bottom
                                                                    width: parent.width * Math.max(0.05, Math.min(1.0, (model.transferProgress !== undefined && !isNaN(model.transferProgress)) ? model.transferProgress : 0.05))
                                                                    color: ThemeData.accentColor
                                                                    radius: 2
                                                                }
                                                            }
                                                        }
                                                    }

                                                    // Recipient Action Row (when !delegateRoot.isMe)
                                                    RowLayout {
                                                        visible: !delegateRoot.isMe
                                                        Layout.fillWidth: true
                                                        spacing: 8

                                                        Text {
                                                            visible: model.status !== "accepted" && model.status !== "declined"
                                                            Layout.fillWidth: true
                                                            text: "Incoming transfer request"
                                                            color: ThemeData.textSecondary
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }

                                                        Text {
                                                            visible: model.status === "accepted" || model.status === "declined"
                                                            Layout.fillWidth: true
                                                            text: model.status === "accepted" ? "✓ Accepted — Receiving media..." : "✕ Request declined"
                                                            color: model.status === "accepted" ? "#23A55A" : "#F23F43"
                                                            font.family: "Segoe UI"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                            wrapMode: Text.WordWrap
                                                        }

                                                        RowLayout {
                                                            visible: model.status !== "accepted" && model.status !== "declined"
                                                            spacing: 8

                                                            Rectangle {
                                                                Layout.preferredHeight: 28
                                                                Layout.preferredWidth: 70
                                                                radius: 14
                                                                color: declineBtnMouse.containsMouse ? Qt.rgba(242, 63, 67, 0.3) : Qt.rgba(242, 63, 67, 0.12)
                                                                border.color: "#F23F43"
                                                                border.width: 1

                                                                Text {
                                                                    anchors.centerIn: parent
                                                                    text: "Decline"
                                                                    color: "#F23F43"
                                                                    font.family: "Segoe UI"
                                                                    font.pixelSize: 11
                                                                    font.bold: true
                                                                }

                                                                MouseArea {
                                                                    id: declineBtnMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        var convId = delegateRoot.getConversationId();
                                                                        if (typeof MessageService !== "undefined" && MessageService) {
                                                                            MessageService.declineMediaRequest(convId, model.messageId);
                                                                        } else {
                                                                            delegateRoot.declineMediaRequested(convId, model.messageId);
                                                                        }
                                                                    }
                                                                }
                                                            }

                                                            Rectangle {
                                                                Layout.preferredHeight: 28
                                                                Layout.preferredWidth: 80
                                                                radius: 14
                                                                color: acceptBtnMouse.containsMouse ? "#23A55A" : Qt.rgba(35, 165, 90, 0.25)
                                                                border.color: "#23A55A"
                                                                border.width: 1

                                                                RowLayout {
                                                                    anchors.centerIn: parent
                                                                    spacing: 4

                                                                    IconImage {
                                                                        source: "qrc:/qt/qml/NeoNect/assets/icons/check.svg"
                                                                        Layout.preferredWidth: 12
                                                                        Layout.preferredHeight: 12
                                                                        color: acceptBtnMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                                                    }

                                                                    Text {
                                                                        text: "Accept"
                                                                        color: acceptBtnMouse.containsMouse ? "#FFFFFF" : "#23A55A"
                                                                        font.family: "Segoe UI"
                                                                        font.pixelSize: 11
                                                                        font.bold: true
                                                                    }
                                                                }

                                                                MouseArea {
                                                                    id: acceptBtnMouse
                                                                    anchors.fill: parent
                                                                    hoverEnabled: true
                                                                    cursorShape: Qt.PointingHandCursor
                                                                    onClicked: {
                                                                        var convId = delegateRoot.getConversationId();
                                                                        if (typeof MessageService !== "undefined" && MessageService) {
                                                                            MessageService.acceptMediaRequest(convId, model.messageId);
                                                                        } else {
                                                                            delegateRoot.acceptMediaRequested(convId, model.messageId);
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            // Caption for media messages
                                            Text {
                                                visible: (delegateRoot.isImage || delegateRoot.isVideo || delegateRoot.isAudio || delegateRoot.isFile) && model.text !== ""
                                                Layout.fillWidth: true
                                                text: model.text
                                                color: (delegateRoot.isDM && delegateRoot.isMe) ? "#FFFFFF" : ThemeData.textPrimary
                                                font.family: "Segoe UI"
                                                font.pixelSize: 13
                                                wrapMode: Text.Wrap
                                                horizontalAlignment: UIHelpers.isRTL(model.text) ? Text.AlignRight : Text.AlignLeft
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
                                                            color: ThemeData.accentColor
                                                        }

                                                        Text {
                                                            anchors.verticalCenter: parent.verticalCenter
                                                            text: "Seen"
                                                            color: ThemeData.accentColor
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
