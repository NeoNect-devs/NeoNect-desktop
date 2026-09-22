import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs
import NeoNect.Core 1.0

Rectangle {
    id: modalRoot
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.75)
    visible: opacity > 0
    opacity: 0.0
    z: 99999

    FileDialog {
        id: avatarFileDialog
        title: "Select Profile Picture"
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.webp *.bmp *.gif)", "All files (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            if (selectedFile && NetworkManager) {
                NetworkManager.setAvatar(selectedFile.toString());
            }
        }
    }

    signal logoutRequested
    signal sendTestNotificationRequested
    signal profileUpdated(string displayName, string bio, string status)
    signal closed()

    property string currentTab: "profile" // "profile", "appearance", "notifications", "privacy", "logout"
    property string currentStatus: "online"
    property string userBio: "Decentralized E2EE NeoNect Communicator"
    property string customDisplayName: (NetworkManager && NetworkManager.displayName && NetworkManager.displayName.length > 0) ? NetworkManager.displayName : ((NetworkManager && NetworkManager.currentUsername && NetworkManager.currentUsername.length > 0) ? NetworkManager.currentUsername.replace(/^\w/, c => c.toUpperCase()) : "NeoNect User")
    property int selectedThemeIndex: 0
    property string selectedAccentColor: "#0A84FF"

    function open() {
        modalRoot.opacity = 1.0;
    }

    function close() {
        modalRoot.opacity = 0.0;
        modalRoot.closed();
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    // Dismiss modal on clicking backdrop
    MouseArea {
        anchors.fill: parent
        onClicked: modalRoot.close()
    }

    // Main Modal Dialog Window
    Rectangle {
        id: dialogFrame
        width: Math.min(parent.width - 48, 760)
        height: Math.min(parent.height - 48, 540)
        radius: 16
        color: "#0F1013"
        border.color: Qt.rgba(255, 255, 255, 0.12)
        border.width: 1
        anchors.centerIn: parent

        // Prevent clicks inside modal from closing
        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ─── MODAL HEADER ───
            Rectangle {
                Layout.fillWidth: true
                height: 52
                color: "#14161A"
                topLeftRadius: 16
                topRightRadius: 16
                bottomLeftRadius: 0
                bottomRightRadius: 0

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 16
                    spacing: 12

                    Image {
                        source: "qrc:/qt/qml/NeoNect/assets/NeoNect/icon.png"
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    Text {
                        text: "NeoNect Settings & Profile"
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    // Close ✕ Button
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: closeMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.15) : "transparent"

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: closeMouse.containsMouse ? "#FFFFFF" : "#949BA4"
                            font.pixelSize: 13
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: modalRoot.close()
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Qt.rgba(255, 255, 255, 0.08)
            }

            // ─── MODAL BODY (LEFT SIDEBAR + RIGHT CONTENT) ───
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // ─── LEFT NAVIGATION SIDEBAR ───
                Rectangle {
                    Layout.preferredWidth: 210
                    Layout.fillHeight: true
                    color: "#121417"
                    bottomLeftRadius: 16

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        NavTabButton {
                            tabId: "profile"
                            tabTitle: "My Profile"
                            tabIconSource: "qrc:/qt/qml/NeoNect/assets/icons/user.svg"
                        }

                        NavTabButton {
                            tabId: "appearance"
                            tabTitle: "Appearance"
                            tabIconSource: "qrc:/qt/qml/NeoNect/assets/icons/palette.svg"
                        }

                        NavTabButton {
                            tabId: "notifications"
                            tabTitle: "Notifications"
                            tabIconSource: "qrc:/qt/qml/NeoNect/assets/icons/bell.svg"
                        }

                        NavTabButton {
                            tabId: "privacy"
                            tabTitle: "Privacy & E2EE"
                            tabIconSource: "qrc:/qt/qml/NeoNect/assets/icons/lock.svg"
                        }

                        Item {
                            width: 1
                            height: 16
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Qt.rgba(255, 255, 255, 0.06)
                        }

                        Item {
                            width: 1
                            height: 8
                        }

                        NavTabButton {
                            tabId: "logout"
                            tabTitle: "Log Out"
                            tabIconSource: "qrc:/qt/qml/NeoNect/assets/icons/log-out.svg"
                            isDanger: true
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: Qt.rgba(255, 255, 255, 0.08)
                }

                // ─── RIGHT CONTENT AREA ───
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0F1013"
                    bottomRightRadius: 16
                    clip: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 20
                        clip: true

                        // ══════════════════════════════════════════════════════
                        // VIEW 1: MY PROFILE
                        // ══════════════════════════════════════════════════════
                        ColumnLayout {
                            visible: modalRoot.currentTab === "profile"
                            width: parent.width - 24
                            spacing: 16

                            // Profile Hero Header
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 16

                                // Avatar with Status Pill & Change Hover
                                Item {
                                    width: 72
                                    height: 72

                                    Rectangle {
                                        id: profileAvatarBox
                                        width: 72
                                        height: 72
                                        radius: 18
                                        color: ThemeData.accentColor
                                        border.color: Qt.rgba(255, 255, 255, 0.2)
                                        border.width: 1.5

                                        CircularImage {
                                            id: profileAvatarImg
                                            anchors.fill: parent
                                            source: (NetworkManager && NetworkManager.avatarUrl) ? NetworkManager.avatarUrl : ""
                                            cornerRadius: 18
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: !profileAvatarImg.ready
                                            text: {
                                                var n = (nameInput && nameInput.text && nameInput.text.trim().length > 0) ? nameInput.text.trim() : modalRoot.customDisplayName;
                                                return (n && n.length > 0) ? n.charAt(0).toUpperCase() : "A";
                                            }
                                            color: "#FFFFFF"
                                            font.family: "Segoe UI"
                                            font.bold: true
                                            font.pixelSize: 28
                                        }

                                        // Hover Overlay to Change Avatar
                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 18
                                            color: Qt.rgba(0, 0, 0, 0.6)
                                            visible: avatarHoverArea.containsMouse

                                            Text {
                                                anchors.centerIn: parent
                                                text: "Change\nAvatar"
                                                horizontalAlignment: Text.AlignHCenter
                                                color: "#FFFFFF"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 11
                                                font.bold: true
                                            }
                                        }

                                        MouseArea {
                                            id: avatarHoverArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: avatarFileDialog.open()
                                        }
                                    }

                                    // Online Status Pill Under Avatar
                                    Rectangle {
                                        width: 32
                                        height: 9
                                        radius: 4.5
                                        color: {
                                            var st = (NetworkManager && NetworkManager.userStatus) ? NetworkManager.userStatus : modalRoot.currentStatus;
                                            if (st === "online")
                                                return "#23A55A";
                                            if (st === "afk")
                                                return "#FAA81A";
                                            if (st === "dnd")
                                                return "#F23F43";
                                            return "#80848E";
                                        }
                                        border.color: "#0F1013"
                                        border.width: 1.5
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: (nameInput && nameInput.text && nameInput.text.trim().length > 0) ? nameInput.text.trim() : modalRoot.customDisplayName
                                        color: "#FFFFFF"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 18
                                        font.bold: true
                                    }

                                    Text {
                                        text: "@" + ((NetworkManager && NetworkManager.currentUsername) ? NetworkManager.currentUsername : "user")
                                        color: "#00E5FF"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 13
                                    }

                                    RowLayout {
                                        spacing: 8
                                        Layout.topMargin: 4

                                        Rectangle {
                                            height: 26
                                            width: uploadBtnText.implicitWidth + 16
                                            radius: 6
                                            color: Qt.rgba(10, 132, 255, uploadMouse.containsMouse ? 0.35 : 0.2)
                                            border.color: "#0A84FF"
                                            border.width: 1

                                            Text {
                                                id: uploadBtnText
                                                anchors.centerIn: parent
                                                text: "Change Avatar"
                                                color: "#0A84FF"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 11
                                                font.bold: true
                                            }

                                            MouseArea {
                                                id: uploadMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: avatarFileDialog.open()
                                            }
                                        }

                                        Rectangle {
                                            visible: NetworkManager && NetworkManager.avatarUrl !== ""
                                            height: 26
                                            width: removeBtnText.implicitWidth + 16
                                            radius: 6
                                            color: Qt.rgba(242, 63, 67, removeMouse.containsMouse ? 0.35 : 0.2)
                                            border.color: "#F23F43"
                                            border.width: 1

                                            Text {
                                                id: removeBtnText
                                                anchors.centerIn: parent
                                                text: "Remove"
                                                color: "#F23F43"
                                                font.family: "Segoe UI"
                                                font.pixelSize: 11
                                                font.bold: true
                                            }

                                            MouseArea {
                                                id: removeMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (NetworkManager) NetworkManager.clearAvatar();
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Qt.rgba(255, 255, 255, 0.08)
                            }

                            // Display Name Editor
                            Text {
                                text: "Display Name"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 38
                                radius: 8
                                color: "#14161A"
                                border.color: nameInput.activeFocus ? "#0A84FF" : Qt.rgba(255, 255, 255, 0.1)
                                border.width: 1

                                TextInput {
                                    id: nameInput
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    verticalAlignment: TextInput.AlignVCenter
                                    text: modalRoot.customDisplayName
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    selectByMouse: true
                                    onTextEdited: {
                                        var trimmed = text.trim();
                                        if (NetworkManager) {
                                            NetworkManager.setDisplayName(trimmed.length > 0 ? trimmed : (NetworkManager.currentUsername || ""));
                                        }
                                    }
                                    onEditingFinished: {
                                        var trimmed = text.trim();
                                        if (NetworkManager) {
                                            NetworkManager.setDisplayName(trimmed.length > 0 ? trimmed : (NetworkManager.currentUsername || ""));
                                        }
                                    }
                                }
                            }
                        }

                        // ══════════════════════════════════════════════════════
                        // VIEW 2: APPEARANCE & THEME
                        // ══════════════════════════════════════════════════════
                        ColumnLayout {
                            visible: modalRoot.currentTab === "appearance"
                            width: parent.width - 24
                            spacing: 16

                            Text {
                                text: "Theme Preset"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                ThemeCard {
                                    tIndex: 0
                                    tName: "Cyber Dark"
                                    tBgColor: "#0E0F12"
                                    tPanelColor: "#18191D"
                                }
                                ThemeCard {
                                    tIndex: 1
                                    tName: "Soft Charcoal"
                                    tBgColor: "#1E1F22"
                                    tPanelColor: "#2B2D31"
                                }
                                ThemeCard {
                                    tIndex: 2
                                    tName: "OLED Black"
                                    tBgColor: "#000000"
                                    tPanelColor: "#08080A"
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Qt.rgba(255, 255, 255, 0.08)
                            }

                            // Accent Colors
                            Text {
                                text: "Accent Color"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                AccentColorCircle {
                                    hexColor: "#00E5FF"
                                    colorName: "Electric Cyan"
                                }
                                AccentColorCircle {
                                    hexColor: "#0A84FF"
                                    colorName: "Royal Blue"
                                }
                                AccentColorCircle {
                                    hexColor: "#23A55A"
                                    colorName: "Emerald Green"
                                }
                                AccentColorCircle {
                                    hexColor: "#FAA81A"
                                    colorName: "Amber Gold"
                                }
                                AccentColorCircle {
                                    hexColor: "#FF5252"
                                    colorName: "Crimson Red"
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Qt.rgba(255, 255, 255, 0.08)
                            }

                            // Chat Font Sizing
                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "Chat Font Size"
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: ThemeData.fontSizeNormal + "px"
                                    color: "#00E5FF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 12
                                to: 18
                                stepSize: 1
                                value: ThemeData.fontSizeNormal
                                onMoved: ThemeData.fontSizeNormal = Math.round(value)
                            }
                        }

                        // ══════════════════════════════════════════════════════
                        // VIEW 3: NOTIFICATIONS & SOUNDS
                        // ══════════════════════════════════════════════════════
                        ColumnLayout {
                            visible: modalRoot.currentTab === "notifications"
                            width: parent.width - 24
                            spacing: 16

                            Text {
                                text: "In-App Notifications"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Show Floating Notification Pills"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: typeof NotificationManager !== "undefined" && NotificationManager ? NotificationManager.notificationsEnabled : true
                                    onToggled: {
                                        if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                            NotificationManager.setNotificationsEnabled(checked);
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Sound on Incoming Messages"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: typeof NotificationManager !== "undefined" && NotificationManager ? NotificationManager.soundEnabled : true
                                    onToggled: {
                                        if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                            NotificationManager.setSoundEnabled(checked);
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Show Message Text Preview"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: typeof NotificationManager !== "undefined" && NotificationManager ? NotificationManager.previewEnabled : true
                                    onToggled: {
                                        if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                            NotificationManager.setPreviewEnabled(checked);
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Do Not Disturb (Silence All)"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: typeof NotificationManager !== "undefined" && NotificationManager ? NotificationManager.dndEnabled : false
                                    onToggled: {
                                        if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                            NotificationManager.setDndEnabled(checked);
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Screen Corner Position"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                ComboBox {
                                    id: cornerCombo
                                    Layout.preferredWidth: 170
                                    model: [
                                        {
                                            text: "Bottom Right (Default)",
                                            value: "bottom-right"
                                        },
                                        {
                                            text: "Top Right",
                                            value: "top-right"
                                        },
                                        {
                                            text: "Bottom Left",
                                            value: "bottom-left"
                                        },
                                        {
                                            text: "Top Left",
                                            value: "top-left"
                                        }
                                    ]
                                    textRole: "text"
                                    valueRole: "value"
                                    currentIndex: {
                                        var cur = (typeof NotificationManager !== "undefined" && NotificationManager) ? NotificationManager.screenCorner : "bottom-right";
                                        for (var i = 0; i < model.length; ++i) {
                                            if (model[i].value === cur)
                                                return i;
                                        }
                                        return 0;
                                    }
                                    onActivated: {
                                        var selected = model[currentIndex].value;
                                        if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                            NotificationManager.setScreenCorner(selected);
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Qt.rgba(255, 255, 255, 0.08)
                            }

                            Text {
                                text: "Notification Test & Diagnostics"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Text {
                                text: "Test the notification pill system by sending a live test notification."
                                color: "#80848E"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                            }

                            NeoNectButton {
                                text: "⚡ Send NeoNect Notification Pill"
                                highlighted: true
                                Layout.preferredWidth: 260
                                Layout.preferredHeight: 36
                                onClicked: {
                                    if (typeof NotificationManager !== "undefined" && NotificationManager) {
                                        NotificationManager.showNotification("NeoNect System", "Hey! Testing the new Telegram-style notification pill ⚡", "message", "system", "N", 5000);
                                    } else {
                                        modalRoot.sendTestNotificationRequested();
                                    }
                                }
                            }
                        }

                        // ══════════════════════════════════════════════════════
                        // VIEW 4: PRIVACY & E2EE
                        // ══════════════════════════════════════════════════════
                        ColumnLayout {
                            visible: modalRoot.currentTab === "privacy"
                            width: parent.width - 24
                            spacing: 16

                            Text {
                                text: "End-to-End Cryptography"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 72
                                radius: 10
                                color: Qt.rgba(35, 165, 90, 0.1)
                                border.color: Qt.rgba(35, 165, 90, 0.3)
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 10

                                    IconImage {
                                        source: "qrc:/qt/qml/NeoNect/assets/icons/shield.svg"
                                        width: 24
                                        height: 24
                                        color: "#23A55A"
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Double Ratchet + X3DH Active"
                                            color: "#23A55A"
                                            font.family: "Segoe UI"
                                            font.pixelSize: 13
                                            font.bold: true
                                        }

                                        Text {
                                            text: "All relay packets are encrypted locally before transmission."
                                            color: "#B5BAC1"
                                            font.family: "Segoe UI"
                                            font.pixelSize: 11
                                        }
                                    }
                                }
                            }

                            Text {
                                text: "Identity Key Fingerprint"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 38
                                radius: 8
                                color: "#14161A"
                                border.color: Qt.rgba(255, 255, 255, 0.1)

                                Text {
                                    anchors.centerIn: parent
                                    text: "4F9A : B301 : C882 : EF71 : 09A2 : 55D4 : EE12 : 99B8"
                                    color: "#00E5FF"
                                    font.family: "Consolas"
                                    font.pixelSize: 12
                                }
                            }
                        }

                        // ══════════════════════════════════════════════════════
                        // VIEW 5: LOG OUT
                        // ══════════════════════════════════════════════════════
                        ColumnLayout {
                            visible: modalRoot.currentTab === "logout"
                            width: parent.width - 24
                            spacing: 16

                            Text {
                                text: "Log Out of NeoNect"
                                color: "#FF5252"
                                font.family: "Segoe UI"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                text: "Are you sure you want to log out? Your local session token will be cleared and you will return to the gateway connection screen."
                                color: "#B5BAC1"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                spacing: 12
                                Layout.topMargin: 12

                                NeoNectButton {
                                    text: "Cancel"
                                    highlighted: false
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 36
                                    onClicked: modalRoot.currentTab = "profile"
                                }

                                Rectangle {
                                    width: 140
                                    height: 36
                                    radius: 8
                                    color: logoutMouse.containsMouse ? "#D32F2F" : "#FF5252"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "Yes, Log Out"
                                        color: "#FFFFFF"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 13
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: logoutMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            modalRoot.close();
                                            modalRoot.logoutRequested();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Helper Navigation Item Component
    component NavTabButton: Rectangle {
        property string tabId: ""
        property string tabTitle: ""
        property string tabIconSource: ""
        property bool isDanger: false

        width: parent ? parent.width : 190
        height: 38
        radius: 8
        color: modalRoot.currentTab === tabId ? (isDanger ? Qt.rgba(255, 82, 82, 0.2) : Qt.rgba(10, 132, 255, 0.2)) : (navMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.06) : "transparent")

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            IconImage {
                source: tabIconSource
                width: 17
                height: 17
                color: isDanger ? "#FF5252" : (modalRoot.currentTab === tabId ? (tabId === "appearance" ? "#00E5FF" : (tabId === "notifications" ? "#FAA81A" : (tabId === "privacy" ? "#23A55A" : "#0A84FF"))) : (navMouse.containsMouse ? "#FFFFFF" : "#949BA4"))
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: tabTitle
                color: isDanger ? "#FF5252" : (modalRoot.currentTab === tabId ? "#FFFFFF" : (navMouse.containsMouse ? "#FFFFFF" : "#949BA4"))
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.weight: modalRoot.currentTab === tabId ? Font.DemiBold : Font.Normal
                Layout.fillWidth: true
            }
        }

        MouseArea {
            id: navMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: modalRoot.currentTab = tabId
        }
    }

    component ThemeCard: Rectangle {
        property int tIndex: 0
        property string tName: ""
        property string tBgColor: ""
        property string tPanelColor: ""

        Layout.fillWidth: true
        height: 70
        radius: 10
        color: tBgColor
        border.color: modalRoot.selectedThemeIndex === tIndex ? "#00E5FF" : Qt.rgba(255, 255, 255, 0.15)
        border.width: modalRoot.selectedThemeIndex === tIndex ? 2 : 1

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 4

            Rectangle {
                width: 24
                height: 24
                radius: 6
                color: tPanelColor
                border.color: Qt.rgba(255, 255, 255, 0.2)
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: tName
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 11
                font.bold: modalRoot.selectedThemeIndex === tIndex
                Layout.alignment: Qt.AlignHCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                modalRoot.selectedThemeIndex = tIndex;
                if (tIndex === 0)
                    ThemeData.loadOledPreset();
                else if (tIndex === 1)
                    ThemeData.loadSoftDarkPreset();
            }
        }
    }

    component AccentColorCircle: Rectangle {
        property string hexColor: ""
        property string colorName: ""

        width: 32
        height: 32
        radius: 16
        color: hexColor
        border.color: modalRoot.selectedAccentColor === hexColor ? "#FFFFFF" : "transparent"
        border.width: 2.5

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                modalRoot.selectedAccentColor = hexColor;
                ThemeData.accentColor = hexColor;
            }
        }
    }
}
