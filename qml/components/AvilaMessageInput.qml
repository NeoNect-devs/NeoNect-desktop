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

            // Attachment '+' Button
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
                    onClicked: attachPopup.open()
                }

                Popup {
                    id: attachPopup
                    y: -224
                    x: 0
                    width: 240
                    padding: 6
                    modal: true
                    focus: true
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                    background: Rectangle {
                        radius: 12
                        color: ThemeData.panelBackground
                        border.color: Qt.rgba(255, 255, 255, 0.12)
                        border.width: 1
                    }

                    contentItem: ColumnLayout {
                        spacing: 4

                        // 1. Photo / Image
                        Rectangle {
                            Layout.fillWidth: true
                            height: 48
                            radius: 8
                            color: imgOptMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    width: 32; height: 32
                                    radius: 8
                                    color: Qt.rgba(88, 101, 242, 0.2)

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/image.svg"
                                        width: 16; height: 16
                                        color: ThemeData.accentColor
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: "Photo or Image"
                                        color: ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Text {
                                        text: "PNG, JPG, WebP, GIF"
                                        color: ThemeData.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                id: imgOptMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    attachPopup.close();
                                    imageFileDialog.open();
                                }
                            }
                        }

                        // 2. Video File
                        Rectangle {
                            Layout.fillWidth: true
                            height: 48
                            radius: 8
                            color: vidOptMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    width: 32; height: 32
                                    radius: 8
                                    color: Qt.rgba(235, 69, 158, 0.2)

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/video.svg"
                                        width: 16; height: 16
                                        color: "#EB459E"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: "Video File"
                                        color: ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Text {
                                        text: "MP4, MKV, WebM"
                                        color: ThemeData.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                id: vidOptMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    attachPopup.close();
                                    videoFileDialog.open();
                                }
                            }
                        }

                        // 3. Audio & Music
                        Rectangle {
                            Layout.fillWidth: true
                            height: 48
                            radius: 8
                            color: audOptMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    width: 32; height: 32
                                    radius: 8
                                    color: Qt.rgba(35, 165, 90, 0.2)

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/music.svg"
                                        width: 16; height: 16
                                        color: "#23A55A"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: "Audio & Music"
                                        color: ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Text {
                                        text: "MP3, WAV, FLAC, OGG"
                                        color: ThemeData.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                id: audOptMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    attachPopup.close();
                                    audioFileDialog.open();
                                }
                            }
                        }

                        // 4. Document / File
                        Rectangle {
                            Layout.fillWidth: true
                            height: 48
                            radius: 8
                            color: docOptMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    width: 32; height: 32
                                    radius: 8
                                    color: Qt.rgba(0, 168, 252, 0.2)

                                    IconImage {
                                        anchors.centerIn: parent
                                        source: "qrc:/qt/qml/Avila/assets/icons/file.svg"
                                        width: 16; height: 16
                                        color: "#00A8FC"
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: "Document or File"
                                        color: ThemeData.textPrimary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Text {
                                        text: "PDF, ZIP, Archives, Code"
                                        color: ThemeData.textSecondary
                                        font.family: "Segoe UI"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            MouseArea {
                                id: docOptMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    attachPopup.close();
                                    docFileDialog.open();
                                }
                            }
                        }
                    }
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

                    Keys.onReturnPressed: function(event) {
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

    // File Pickers
    FileDialog {
        id: imageFileDialog
        title: "Select Image / Photo"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "");
            var fileName = path.substring(path.lastIndexOf('/') + 1);
            inputRoot.draftAttachment = {
                type: "image",
                url: selectedFile.toString(),
                name: fileName,
                size: 1540000
            };
        }
    }

    FileDialog {
        id: videoFileDialog
        title: "Select Video File"
        nameFilters: ["Video files (*.mp4 *.webm *.mkv *.avi *.mov)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "");
            var fileName = path.substring(path.lastIndexOf('/') + 1);
            inputRoot.draftAttachment = {
                type: "video",
                url: selectedFile.toString(),
                name: fileName,
                size: 8500000,
                duration: 45
            };
        }
    }

    FileDialog {
        id: audioFileDialog
        title: "Select Music / Audio File"
        nameFilters: ["Audio files (*.mp3 *.wav *.ogg *.flac *.m4a *.aac)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "");
            var fileName = path.substring(path.lastIndexOf('/') + 1);
            inputRoot.draftAttachment = {
                type: "audio",
                url: selectedFile.toString(),
                name: fileName,
                size: 4200000,
                duration: 180
            };
        }
    }

    FileDialog {
        id: docFileDialog
        title: "Select Document / File"
        nameFilters: ["All files (*.*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "");
            var fileName = path.substring(path.lastIndexOf('/') + 1);
            inputRoot.draftAttachment = {
                type: "file",
                url: selectedFile.toString(),
                name: fileName,
                size: 2800000
            };
        }
    }
}