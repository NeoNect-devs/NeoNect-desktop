import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs
import Avila.Core 1.0

Rectangle {
    id: inputRoot

    property string channelName: "general"
    property bool isDM: false

    // Signals
    signal messageSent(string text)
    signal stickerSent(string stickerUrl, string packId, string stickerName)
    signal voiceSent(var voiceData)
    signal mediaSent(var mediaData) // { type, mediaUrl, fileName, fileSize, duration, caption }
    signal typingStarted()
    signal typingStopped()

    Timer {
        id: typingDebounceTimer
        interval: 2000
        repeat: false
        onTriggered: inputRoot.typingStopped()
    }

    // Draft Attachment State
    property var draftAttachment: null // { type, url, name, size }
    property bool showStickerPicker: false
    property bool isRecordingMode: false

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

    function handleSelectedFileUrl(fileUrlStr) {
        if (!fileUrlStr) return;
        var url = fileUrlStr.toString();
        var path = url.replace("file:///", "").replace("file://", "");
        var fileName = path.substring(path.lastIndexOf('/') + 1);
        if (!fileName || fileName.indexOf('\\') !== -1) {
            fileName = path.substring(path.lastIndexOf('\\') + 1);
        }
        var detectedType = detectMediaType(url, fileName);
        inputRoot.draftAttachment = {
            type: detectedType,
            url: url,
            name: fileName,
            size: (detectedType === "image" ? 1540000 : (detectedType === "video" ? 8500000 : (detectedType === "audio" ? 4200000 : 2500000))),
            duration: (detectedType === "video" ? 30 : (detectedType === "audio" ? 180 : 0))
        };
        inputArea.forceActiveFocus();
    }

    implicitHeight: isRecordingMode ? 52 : (draftAttachment ? Math.min(Math.max(104, inputArea.contentHeight + 76), 220) : Math.min(Math.max(52, inputArea.contentHeight + 24), 160))
    color: ThemeData.inputBackgroundInactive
    radius: 10

    // Dynamic Gradient Outer Border
    Rectangle {
        id: borderContainer
        anchors.fill: parent
        radius: parent.radius
        color: inputArea.activeFocus ? "transparent" : ThemeData.inputSolidBorder

        // Animated Color-Shifting Gradient Frame
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: inputArea.activeFocus && !inputRoot.isRecordingMode

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { id: gradStop1; position: 0.0; color: "#5865F2" }
                GradientStop { id: gradStop2; position: 0.5; color: "#EB459E" }
                GradientStop { id: gradStop3; position: 1.0; color: "#00A36C" }
            }

            ParallelAnimation {
                running: inputArea.activeFocus && !inputRoot.isRecordingMode
                loops: Animation.Infinite

                SequentialAnimation {
                    ColorAnimation { target: gradStop1; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop1; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop1; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                }

                SequentialAnimation {
                    ColorAnimation { target: gradStop2; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop2; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop2; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                }

                SequentialAnimation {
                    ColorAnimation { target: gradStop3; property: "color"; to: "#5865F2"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop3; property: "color"; to: "#EB459E"; duration: 2500; easing.type: Easing.InOutSine }
                    ColorAnimation { target: gradStop3; property: "color"; to: "#00A36C"; duration: 2500; easing.type: Easing.InOutSine }
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: inputArea.activeFocus ? 2 : 1
            radius: parent.radius > 2 ? parent.radius - 2 : 0
            color: ThemeData.inputBackgroundInactive
        }
    }

    // Voice Recorder Bar View
    VoiceRecorderBar {
        anchors.fill: parent
        visible: inputRoot.isRecordingMode

        onVoiceSent: function(voiceData) {
            inputRoot.isRecordingMode = false;
            inputRoot.voiceSent(voiceData);
        }

        onVoiceCancelled: {
            inputRoot.isRecordingMode = false;
        }
    }

    // Main Input Bar Container
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        visible: !inputRoot.isRecordingMode

        // Draft Attachment Preview Bar
        Rectangle {
            visible: inputRoot.draftAttachment !== null
            Layout.fillWidth: true
            height: 44
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.4)
            border.color: Qt.rgba(255, 255, 255, 0.1)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8
                spacing: 10

                // Media Thumbnail or Icon
                Rectangle {
                    width: 32; height: 32
                    radius: 6
                    color: ThemeData.accentColor

                    IconImage {
                        anchors.centerIn: parent
                        source: {
                            if (!inputRoot.draftAttachment) return "qrc:/qt/qml/Avila/assets/icons/file.svg";
                            var t = inputRoot.draftAttachment.type;
                            if (t === "image") return "qrc:/qt/qml/Avila/assets/icons/image.svg";
                            if (t === "video") return "qrc:/qt/qml/Avila/assets/icons/video.svg";
                            if (t === "audio") return "qrc:/qt/qml/Avila/assets/icons/music.svg";
                            return "qrc:/qt/qml/Avila/assets/icons/file.svg";
                        }
                        width: 18; height: 18
                        color: "#FFFFFF"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: inputRoot.draftAttachment ? inputRoot.draftAttachment.name : ""
                        color: ThemeData.textPrimary
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: inputRoot.draftAttachment ? inputRoot.formatBytes(inputRoot.draftAttachment.size) : ""
                        color: ThemeData.textSecondary
                        font.family: "Segoe UI"
                        font.pixelSize: 10
                    }
                }

                // Remove Draft Attachment Button
                Rectangle {
                    width: 24; height: 24
                    radius: 12
                    color: removeDraftMouse.containsMouse ? Qt.rgba(255, 50, 50, 0.3) : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: ThemeData.textSecondary
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: removeDraftMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: inputRoot.draftAttachment = null
                    }
                }
            }
        }

        // Text input & buttons row
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Attachment '+' Button (Directly opens system file picker with automatic content type detection)
            Rectangle {
                width: 32; height: 32
                radius: 16
                color: attachMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                Layout.alignment: Qt.AlignBottom

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/plus-circle.svg"
                    width: 20; height: 20
                    color: attachMouse.containsMouse ? ThemeData.accentColor : ThemeData.textSecondary
                }

                MouseArea {
                    id: attachMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: universalFileDialog.open()
                }
            }

            // Text Input ScrollArea
            ScrollView {
                id: inputScrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                background: null

                ScrollBar.vertical: ScrollBar {
                    id: inputScrollBar
                    parent: inputScrollView
                    anchors.top: inputScrollView.top
                    anchors.right: inputScrollView.right
                    anchors.bottom: inputScrollView.bottom
                    width: 6
                    policy: ScrollBar.AsNeeded
                    palette.window: "transparent"
                    palette.base: "transparent"

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: inputScrollBar.pressed ? ThemeData.accentColor : (inputScrollBar.hovered ? "#7289DA" : "#4E5058")
                    }
                    background: Item {}
                }

                TextArea {
                    id: inputArea
                    width: inputScrollView.width
                    placeholderText: inputRoot.draftAttachment ? "Add a caption..." : ("Message " + (inputRoot.isDM ? "@" : "#") + inputRoot.channelName)
                    placeholderTextColor: ThemeData.placeholderColor
                    color: ThemeData.textPrimary
                    font.family: "Segoe UI"
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    selectByMouse: true
                    leftPadding: 4; rightPadding: 14
                    topPadding: 6; bottomPadding: 6
                    background: null

                    // Auto RTL/LTR alignment based on input text
                    horizontalAlignment: inputRoot.isRTL(inputArea.text) ? Text.AlignRight : Text.AlignLeft

                    onTextChanged: {
                        if (text.trim() !== "") {
                            inputRoot.typingStarted();
                            typingDebounceTimer.restart();
                        } else {
                            inputRoot.typingStopped();
                        }
                    }

                    Keys.onReturnPressed: function(event) {
                        typingDebounceTimer.stop();
                        inputRoot.typingStopped();
                        if (event.modifiers & Qt.ShiftModifier) {
                            event.accepted = false;
                            return;
                        }
                        sendTriggered();
                        event.accepted = true;
                    }
                }
            }

            // Sticker Button
            Rectangle {
                width: 32; height: 32
                radius: 6
                color: inputRoot.showStickerPicker ? Qt.rgba(88, 101, 242, 0.3) : (stickerMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent")
                Layout.alignment: Qt.AlignBottom

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/sticker.svg"
                    width: 20; height: 20
                    color: inputRoot.showStickerPicker ? ThemeData.accentColor : (stickerMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary)
                }

                MouseArea {
                    id: stickerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: inputRoot.showStickerPicker = !inputRoot.showStickerPicker
                }
            }

            // Emoji / Smile Button
            Rectangle {
                width: 32; height: 32
                radius: 6
                color: emojiMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                Layout.alignment: Qt.AlignBottom

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/smile.svg"
                    width: 20; height: 20
                    color: emojiMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                }

                MouseArea {
                    id: emojiMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        inputRoot.showStickerPicker = true;
                    }
                }
            }

            // Voice Record Button (when input text and draft attachment are empty)
            Rectangle {
                visible: inputArea.text.trim() === "" && inputRoot.draftAttachment === null
                width: 32; height: 32
                radius: 6
                color: micMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.1) : "transparent"
                Layout.alignment: Qt.AlignBottom

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/mic.svg"
                    width: 18; height: 18
                    color: micMouse.containsMouse ? ThemeData.accentColor : ThemeData.textSecondary
                }

                MouseArea {
                    id: micMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        inputRoot.isRecordingMode = true;
                        AudioManager.startRecording();
                    }
                }
            }

            // Send Button (when input text or draft attachment is present)
            Rectangle {
                visible: inputArea.text.trim() !== "" || inputRoot.draftAttachment !== null
                width: 32; height: 32
                radius: 6
                color: ThemeData.accentColor
                Layout.alignment: Qt.AlignBottom

                IconImage {
                    anchors.centerIn: parent
                    source: "qrc:/qt/qml/Avila/assets/icons/send.svg"
                    width: 18; height: 18
                    color: "#FFFFFF"
                }

                MouseArea {
                    id: sendMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sendTriggered()
                }
            }
        }
    }

    function sendTriggered() {
        var captionText = inputArea.text.trim();

        if (inputRoot.draftAttachment !== null) {
            var att = inputRoot.draftAttachment;
            var mediaPayload = {
                type: att.type,
                mediaUrl: att.url,
                fileName: att.name,
                fileSize: att.size,
                duration: att.duration || 0,
                caption: captionText,
                text: captionText
            };
            inputRoot.mediaSent(mediaPayload);
            inputRoot.draftAttachment = null;
            inputArea.clear();
            return;
        }

        if (captionText !== "") {
            inputRoot.messageSent(captionText);
            inputArea.clear();
        }
    }

    // Floating Telegram-style Sticker Picker Panel
    StickerPickerPanel {
        id: stickerPicker
        anchors.bottom: inputRoot.top
        anchors.right: inputRoot.right
        anchors.bottomMargin: 8
        visible: inputRoot.showStickerPicker
        z: 99999

        onStickerSelected: function(url, pack, name) {
            inputRoot.showStickerPicker = false;
            inputRoot.stickerSent(url, pack, name);
        }

        onEmojiSelected: function(emoji) {
            inputArea.insert(inputArea.cursorPosition, emoji);
        }

        onCloseRequested: {
            inputRoot.showStickerPicker = false;
        }
    }

    // Universal System File Picker (Auto-detects content type on selection)
    FileDialog {
        id: universalFileDialog
        title: "Select File or Media to Send"
        nameFilters: [
            "All Files (*.*)",
            "Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.svg)",
            "Videos (*.mp4 *.webm *.mkv *.mov *.avi *.m4v)",
            "Audio & Music (*.mp3 *.wav *.ogg *.flac *.m4a *.aac *.opus)",
            "Documents & Archives (*.pdf *.zip *.rar *.7z *.txt *.docx *.xlsx *.pptx)"
        ]
        onAccepted: {
            inputRoot.handleSelectedFileUrl(selectedFile.toString());
        }
    }

    // Drag & Drop Area on Input Box
    DropArea {
        id: inputDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onEntered: (drag) => {
            if (drag.hasUrls) {
                drag.acceptProposedAction();
            }
        }
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                inputRoot.handleSelectedFileUrl(drop.urls[0].toString());
                drop.acceptProposedAction();
            }
        }
    }
}