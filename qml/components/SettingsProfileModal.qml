// qml/components/SettingsProfileModal.qml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Avila.Core 1.0

Rectangle {
    id: modalRoot
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.75)
    visible: opacity > 0
    opacity: 0.0
    z: 99999

    signal logoutRequested()
    signal sendTestNotificationRequested()
    signal profileUpdated(string displayName, string bio, string status)

    property string currentTab: "profile" // "profile", "appearance", "notifications", "privacy", "logout"
    property string currentStatus: "online"
    property string userBio: "Decentralized E2EE Avila Communicator"
    property string customDisplayName: (NetworkManager && NetworkManager.currentUsername) ? NetworkManager.currentUsername.replace(/^\w/, c => c.toUpperCase()) : "Avila User"
    property int selectedThemeIndex: 0
    property string selectedAccentColor: "#0A84FF"

    function open() {
        modalRoot.opacity = 1.0;
    }

    function close() {
        modalRoot.opacity = 0.0;
    }

    Behavior on opacity {
        NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
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
        MouseArea { anchors.fill: parent }

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
                        source: "../../assets/logo.png"
                        Layout.preferredWidth: 26
                        Layout.preferredHeight: 26
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        text: "Avila Settings & Profile"
                        color: "#FFFFFF"
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    // Close ✕ Button
                    Rectangle {
                        width: 28; height: 28
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

                        // Helper Navigation Item Component
                        component NavTabButton: Rectangle {
                            property string tabId: ""
                            property string tabTitle: ""
                            property string tabIcon: ""
                            property bool isDanger: false

                            width: parent ? parent.width : 190
                            height: 38
                            radius: 8
                            color: modalRoot.currentTab === tabId ? (isDanger ? Qt.rgba(255, 82, 82, 0.2) : Qt.rgba(10, 132, 255, 0.2)) : (navMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.06) : "transparent")

                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    text: tabIcon
                                    font.pixelSize: 15
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

                        NavTabButton {
                            tabId: "profile"
                            tabTitle: "My Profile"
                            tabIcon: "👤"
                        }

                        NavTabButton {
                            tabId: "appearance"
                            tabTitle: "Appearance"
                            tabIcon: "🎨"
                        }

                        NavTabButton {
                            tabId: "notifications"
                            tabTitle: "Notifications"
                            tabIcon: "🔔"
                        }

                        NavTabButton {
                            tabId: "privacy"
                            tabTitle: "Privacy & E2EE"
                            tabIcon: "🔒"
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
                            tabIcon: "🚪"
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

                                // Avatar with Status Pill
                                Item {
                                    width: 72; height: 72

                                    Rectangle {
                                        width: 72; height: 72
                                        radius: 18
                                        color: ThemeData.accentColor
                                        border.color: Qt.rgba(255, 255, 255, 0.2)
                                        border.width: 1.5

                                        Text {
                                            anchors.centerIn: parent
                                            text: (NetworkManager && NetworkManager.currentUsername) ? NetworkManager.currentUsername.charAt(0).toUpperCase() : "A"
                                            color: "#FFFFFF"
                                            font.family: "Segoe UI"
                                            font.bold: true
                                            font.pixelSize: 28
                                        }
                                    }

                                    // Online Status Pill Under Avatar
                                    Rectangle {
                                        width: 32; height: 9
                                        radius: 4.5
                                        color: {
                                            if (modalRoot.currentStatus === "online") return "#23A55A";
                                            if (modalRoot.currentStatus === "afk") return "#FAA81A";
                                            if (modalRoot.currentStatus === "dnd") return "#F23F43";
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
                                        text: modalRoot.customDisplayName
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

                                    Text {
                                        text: "Node: Relay Connected • E2EE Active"
                                        color: "#80848E"
                                        font.family: "Segoe UI"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true; height: 1
                                color: Qt.rgba(255, 255, 255, 0.08)
                            }

                            // Online Status Selector
                            Text {
                                text: "Online Status"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                component StatusPillOption: Rectangle {
                                    property string statusKey: ""
                                    property string statusLabel: ""
                                    property string statusColor: ""

                                    Layout.fillWidth: true
                                    height: 34
                                    radius: 8
                                    color: modalRoot.currentStatus === statusKey ? Qt.rgba(255, 255, 255, 0.12) : (statusMouse.containsMouse ? Qt.rgba(255, 255, 255, 0.06) : Qt.rgba(255, 255, 255, 0.03))
                                    border.color: modalRoot.currentStatus === statusKey ? statusColor : "transparent"
                                    border.width: 1

                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 6

                                        Rectangle {
                                            width: 14; height: 6; radius: 3
                                            color: statusColor
                                        }

                                        Text {
                                            text: statusLabel
                                            color: modalRoot.currentStatus === statusKey ? "#FFFFFF" : "#949BA4"
                                            font.family: "Segoe UI"
                                            font.pixelSize: 12
                                            font.weight: modalRoot.currentStatus === statusKey ? Font.DemiBold : Font.Normal
                                        }
                                    }

                                    MouseArea {
                                        id: statusMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: modalRoot.currentStatus = statusKey
                                    }
                                }

                                StatusPillOption {
                                    statusKey: "online"
                                    statusLabel: "Online"
                                    statusColor: "#23A55A"
                                }
                                StatusPillOption {
                                    statusKey: "afk"
                                    statusLabel: "Idle / Away"
                                    statusColor: "#FAA81A"
                                }
                                StatusPillOption {
                                    statusKey: "dnd"
                                    statusLabel: "Do Not Disturb"
                                    statusColor: "#F23F43"
                                }
                                StatusPillOption {
                                    statusKey: "offline"
                                    statusLabel: "Invisible"
                                    statusColor: "#80848E"
                                }
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
                                Layout.fillWidth: true; height: 38
                                radius: 8
                                color: "#14161A"
                                border.color: nameInput.activeFocus ? "#0A84FF" : Qt.rgba(255, 255, 255, 0.1)
                                border.width: 1

                                TextInput {
                                    id: nameInput
                                    anchors.fill: parent
                                    anchors.leftMargin: 12; anchors.rightMargin: 12
                                    verticalAlignment: TextInput.AlignVCenter
                                    text: modalRoot.customDisplayName
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    onTextChanged: modalRoot.customDisplayName = text
                                }
                            }

                            // Bio / Description
                            Text {
                                text: "About / Bio"
                                color: "#FFFFFF"
                                font.family: "Segoe UI"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Rectangle {
                                Layout.fillWidth: true; height: 60
                                radius: 8
                                color: "#14161A"
                                border.color: bioInput.activeFocus ? "#0A84FF" : Qt.rgba(255, 255, 255, 0.1)
                                border.width: 1

                                TextEdit {
                                    id: bioInput
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    text: modalRoot.userBio
                                    color: "#FFFFFF"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    wrapMode: TextEdit.Wrap
                                    onTextChanged: modalRoot.userBio = text
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
                                            width: 24; height: 24; radius: 6
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
                                            if (tIndex === 0) ThemeData.loadOledPreset();
                                            else if (tIndex === 1) ThemeData.loadSoftDarkPreset();
                                        }
                                    }
                                }

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
                                Layout.fillWidth: true; height: 1
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

                                component AccentColorCircle: Rectangle {
                                    property string hexColor: ""
                                    property string colorName: ""

                                    width: 32; height: 32; radius: 16
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

                                AccentColorCircle { hexColor: "#00E5FF"; colorName: "Electric Cyan" }
                                AccentColorCircle { hexColor: "#0A84FF"; colorName: "Royal Blue" }
                                AccentColorCircle { hexColor: "#23A55A"; colorName: "Emerald Green" }
                                AccentColorCircle { hexColor: "#FAA81A"; colorName: "Amber Gold" }
                                AccentColorCircle { hexColor: "#FF5252"; colorName: "Crimson Red" }
                            }

                            Rectangle {
                                Layout.fillWidth: true; height: 1
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
                                from: 12; to: 18; stepSize: 1
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
                                    text: "Show Floating Toast Notifications"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Sound on Incoming Direct Message"
                                    color: "#B5BAC1"
                                    font.family: "Segoe UI"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                }
                                Switch {
                                    checked: true
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true; height: 1
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
                                text: "Test the custom notification stack system by sending a live in-app toast."
                                color: "#80848E"
                                font.family: "Segoe UI"
                                font.pixelSize: 12
                            }

                            AvilaButton {
                                text: "⚡ Send Live Test Notification"
                                highlighted: true
                                Layout.preferredWidth: 240
                                Layout.preferredHeight: 36
                                onClicked: modalRoot.sendTestNotificationRequested()
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

                                    Text {
                                        text: "🛡️"
                                        font.pixelSize: 22
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
                                Layout.fillWidth: true; height: 38
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
                                text: "Log Out of Avila"
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

                                AvilaButton {
                                    text: "Cancel"
                                    highlighted: false
                                    Layout.preferredWidth: 120
                                    Layout.preferredHeight: 36
                                    onClicked: modalRoot.currentTab = "profile"
                                }

                                Rectangle {
                                    width: 140; height: 36
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
}
