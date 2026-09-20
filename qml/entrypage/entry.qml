// qml/entrypage/entry.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import NeoNect.Core 1.0
import "../containers"
import "../components"

Item {
    id: entryRoot
    anchors.fill: parent

    readonly property bool showTitleBackButton: currentScreen !== "server"
    property string currentScreen: "server" // "server", "login", "signup", "bookmarks", "bookmark_edit"
    property bool isServerReady: false
    property string serverStatusText: ""

    // Bookmark states & properties
    property bool isEditingBookmark: false
    property string editBmId: ""
    property string editBmName: ""
    property string editBmServer: ""
    property string editBmUser: ""
    property string editBmPass: ""
    property bool showBmPassword: false
    property string bookmarkErrorText: ""
    property string connectingBookmarkId: ""
    property bool saveAsBookmark: false

    // Form status states
    property string loginErrorText: ""
    property string regErrorText: ""
    property string regSuccessText: ""

    // Username availability state
    property bool isCheckingAvailability: false
    property bool isUsernameAvailable: true
    property string usernameStatusText: ""

    // Password visibility states
    property bool showLoginPassword: false
    property bool showRegPassword: false

    function goBack() {
        if (currentScreen === "bookmark_edit") {
            currentScreen = "bookmarks";
        } else if (currentScreen !== "server") {
            currentScreen = "server";
        }
    }

    // ─── RESPONSIVE GEOMETRY BASED ON WINDOW SIZE ──────────────────────────
    readonly property real responsiveCardWidth: {
        var base = (currentScreen === "bookmarks" || currentScreen === "bookmark_edit") ? 460 : 420;
        if (entryRoot.width > 1200) {
            base = Math.min(entryRoot.width * 0.42, 520);
        } else if (entryRoot.width > 900) {
            base = Math.min(entryRoot.width * 0.48, 470);
        }
        return Math.min(base, entryRoot.width - 32);
    }

    readonly property real maxCardHeight: Math.max(280, entryRoot.height - (entryRoot.height < 600 ? 70 : 90))

    readonly property real targetCardHeight: {
        var preferred = 355;
        if (currentScreen === "signup") {
            preferred = 445;
        } else if (currentScreen === "login") {
            preferred = 375;
        } else if (currentScreen === "bookmarks") {
            preferred = NetworkManager.bookmarks.length === 0 ? 380 : 430;
        } else if (currentScreen === "bookmark_edit") {
            preferred = 430;
        } else {
            preferred = 355; // "server" - spacious vertical budget so scrollbar never appears in initial state
        }
        return Math.min(maxCardHeight, preferred);
    }

    readonly property real heroLogoSize: entryRoot.height < 600 ? 84 : (entryRoot.height < 700 ? 104 : 116)
    readonly property real heroSectionHeight: heroLogoSize + 4 + (entryRoot.height < 600 ? 28 : 36)

    Rectangle {
        anchors.fill: parent
        color: ThemeData.windowBackground
    }

    // ─── BRAND HEADER (ON TOP OF FORM, CLOSE TO FORM CARD) ───────────────────
    Column {
        id: heroHeader
        anchors.bottom: formCard.top
        anchors.bottomMargin: 8
        anchors.horizontalCenter: formCard.horizontalCenter
        spacing: 4

        // Bigger Hero Logo
        Item {
            width: entryRoot.heroLogoSize
            height: entryRoot.heroLogoSize
            anchors.horizontalCenter: parent.horizontalCenter

            // Ambient Radial Glow Backdrop
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 1.35
                height: parent.height * 1.35
                radius: width / 2
                color: "transparent"
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(0.35, 0.42, 0.95, 0.14) }
                    GradientStop { position: 0.55; color: Qt.rgba(0.20, 0.28, 0.70, 0.05) }
                    GradientStop { position: 1.0; color: "transparent" }
                }
            }

            Image {
                id: heroLogo
                source: "qrc:/qt/qml/NeoNect/assets/NeoNect/icon.png"
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true

                scale: logoHoverArea.containsMouse ? 1.05 : 1.0
                Behavior on scale {
                    NumberAnimation { duration: 180; easing.type: Easing.OutBack }
                }

                MouseArea {
                    id: logoHoverArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }

        // Bigger NeoNect Typography
        Text {
            text: "NEONECT"
            color: ThemeData.textPrimary
            font.family: "Segoe UI"
            font.pointSize: entryRoot.height < 600 ? (ThemeData.fontSizeHeader + 4) : (ThemeData.fontSizeHeader + 7)
            font.bold: true
            font.letterSpacing: 5.0
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Rectangle {
        id: formCard
        width: entryRoot.responsiveCardWidth
        height: entryRoot.targetCardHeight
        radius: 16
        color: ThemeData.panelBackground
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: ThemeData.borderColor
        border.width: 1

        y: {
            var totalHeight = entryRoot.heroSectionHeight + 8 + height;
            var topMargin = Math.max(8, Math.round((entryRoot.height - totalHeight) / 2));
            return topMargin + entryRoot.heroSectionHeight + 8;
        }

        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
        Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

        Column {
            id: mainColumn
            anchors.fill: parent
            anchors.margins: entryRoot.height < 600 ? 14 : 16
            spacing: 8

            // ─── CARD SCREEN HEADER (for server & bookmarks) ────────────
            Column {
                width: parent.width
                spacing: 2
                visible: entryRoot.currentScreen === "server" || entryRoot.currentScreen === "bookmarks" || entryRoot.currentScreen === "bookmark_edit"

                Text {
                    text: entryRoot.currentScreen === "server" ? "Zero-Knowledge Relay Server Connection" :
                          entryRoot.currentScreen === "bookmarks" ? "TeamSpeak-Style Server Bookmarks" :
                          (entryRoot.isEditingBookmark ? "Edit Server Connection Profile" : "Create New Server Bookmark")
                    color: ThemeData.textSecondary
                    font.family: "Segoe UI"
                    font.pointSize: ThemeData.fontSizeNormal - 2
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // ─── NAVIGATION TAB SEGMENT (LOGIN vs SIGNUP) ─────────────────
            Rectangle {
                width: parent.width
                height: 34
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.25)
                border.color: Qt.rgba(1, 1, 1, 0.05)
                visible: entryRoot.currentScreen === "login" || entryRoot.currentScreen === "signup"

                Row {
                    anchors.fill: parent
                    anchors.margins: 2

                    Rectangle {
                        width: parent.width / 2
                        height: parent.height
                        radius: 6
                        color: entryRoot.currentScreen === "login" ? ThemeData.accentColor : "transparent"

                        Text {
                            text: "Sign In"
                            color: entryRoot.currentScreen === "login" ? "#ffffff" : ThemeData.textSecondary
                            font.bold: true
                            font.pointSize: ThemeData.fontSizeNormal - 2
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                entryRoot.currentScreen = "login"
                                entryRoot.loginErrorText = ""
                            }
                        }

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }

                    Rectangle {
                        width: parent.width / 2
                        height: parent.height
                        radius: 6
                        color: entryRoot.currentScreen === "signup" ? ThemeData.accentColor : "transparent"

                        Text {
                            text: "Create Account"
                            color: entryRoot.currentScreen === "signup" ? "#ffffff" : ThemeData.textSecondary
                            font.bold: true
                            font.pointSize: ThemeData.fontSizeNormal - 2
                            anchors.centerIn: parent
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                entryRoot.currentScreen = "signup"
                                entryRoot.regErrorText = ""
                            }
                        }

                        Behavior on color { ColorAnimation { duration: 150 } }
                    }
                }
            }

            // ─── DYNAMIC SCREEN STACK WITH SCROLL SUPPORT ─────────────────
            Flickable {
                width: parent.width
                height: parent.height - y
                contentHeight: entryRoot.currentScreen === "server" ? serverView.implicitHeight :
                               (entryRoot.currentScreen === "login" ? loginView.implicitHeight :
                               (entryRoot.currentScreen === "bookmarks" ? bookmarksView.implicitHeight :
                               (entryRoot.currentScreen === "bookmark_edit" ? bookmarkEditView.implicitHeight : signupView.implicitHeight)))
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: (entryRoot.currentScreen === "server" && serverView.implicitHeight <= height) ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
                }

                // ─── SCREEN 1: SERVER NODE ROUTING ────────────────────────
                Column {
                    id: serverView
                    width: parent.width
                    spacing: 11
                    visible: entryRoot.currentScreen === "server"
                    anchors.top: parent.top

                    Label {
                        text: "SERVER NODE ADDRESS"
                        color: ThemeData.textSecondary
                        font.pointSize: ThemeData.fontSizeNormal - 3
                        font.bold: true
                        font.letterSpacing: 1
                    }

                    NeoNectTextField {
                        id: serverInput
                        width: parent.width
                        height: 42
                        placeholderText: "e.g., http://localhost:8080 or neonect.chat"
                        text: NetworkManager.serverUrl

                        onTextChanged: {
                            entryRoot.isServerReady = false;
                            if (text.trim() !== "") {
                                entryRoot.serverStatusText = "🔍 Resolving node...";
                                verifyDebounce.restart();
                            } else {
                                entryRoot.serverStatusText = "";
                            }
                        }
                    }

                    // Status Badge Chip
                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: entryRoot.isServerReady ? Qt.rgba(0.14, 0.65, 0.35, 0.15) :
                               serverInput.text.trim() === "" ? "transparent" : Qt.rgba(0.93, 0.32, 0.32, 0.15)
                        border.color: entryRoot.isServerReady ? "#23a55a" :
                                      serverInput.text.trim() === "" ? "transparent" : "#ef5350"
                        border.width: 1
                        visible: entryRoot.serverStatusText !== ""

                        Text {
                            text: entryRoot.serverStatusText
                            color: entryRoot.isServerReady ? "#23a55a" : "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            anchors.centerIn: parent
                        }
                    }

                    Item { width: 1; height: 2 }

                    Row {
                        width: parent.width
                        spacing: 10

                        NeoNectButton {
                            text: "Log In"
                            width: (parent.width - parent.spacing) / 2
                            height: 40
                            enabled: entryRoot.isServerReady && !NetworkManager.isLoading
                            highlighted: true
                            onClicked: entryRoot.currentScreen = "login"
                        }

                        NeoNectButton {
                            text: "Register"
                            width: (parent.width - parent.spacing) / 2
                            height: 40
                            enabled: entryRoot.isServerReady && !NetworkManager.isLoading
                            highlighted: false
                            onClicked: entryRoot.currentScreen = "signup"
                        }
                    }

                    // Bookmarks launcher button
                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 8
                        color: bmMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.04)
                        border.color: Qt.rgba(1, 1, 1, 0.12)
                        border.width: 1

                        Row {
                            anchors.centerIn: parent
                            spacing: 8

                            IconImage {
                                source: "qrc:/qt/qml/NeoNect/assets/icons/bookmark.svg"
                                width: 15
                                height: 15
                                color: ThemeData.accentColor
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: "Server Bookmarks" + (NetworkManager.bookmarks.length > 0 ? " (" + NetworkManager.bookmarks.length + ")" : "")
                                color: ThemeData.textPrimary
                                font.bold: true
                                font.pointSize: ThemeData.fontSizeNormal - 2
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: bmMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                entryRoot.bookmarkErrorText = "";
                                entryRoot.currentScreen = "bookmarks";
                            }
                        }
                    }
                }

                // ─── SCREEN 2: USER LOGIN ──────────────────────────────────
                Column {
                    id: loginView
                    width: parent.width
                    spacing: 10
                    visible: entryRoot.currentScreen === "login"
                    anchors.top: parent.top

                    // Error Notification Chip
                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: Qt.rgba(0.93, 0.32, 0.32, 0.15)
                        border.color: "#ef5350"
                        border.width: 1
                        visible: entryRoot.loginErrorText !== ""

                        Text {
                            text: "⚠️ " + entryRoot.loginErrorText
                            color: "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            anchors.centerIn: parent
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 3
                        Label {
                            text: "USERNAME"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: loginUser
                            width: parent.width
                            height: 42
                            placeholderText: "Enter your username"
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 3
                        Label {
                            text: "PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        Item {
                            width: parent.width
                            height: 42
                            NeoNectTextField {
                                id: loginPass
                                anchors.fill: parent
                                placeholderText: "Enter your password"
                                echoMode: entryRoot.showLoginPassword ? TextInput.Normal : TextInput.Password
                            }
                            Text {
                                text: entryRoot.showLoginPassword ? "Hide" : "Show"
                                color: ThemeData.accentColor
                                font.pointSize: ThemeData.fontSizeNormal - 3
                                font.bold: true
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                z: 10
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: entryRoot.showLoginPassword = !entryRoot.showLoginPassword
                                }
                            }
                        }
                    }

                    // Save as Bookmark Checkbox Row
                    Row {
                        width: parent.width
                        spacing: 8

                        Rectangle {
                            width: 16; height: 16; radius: 4
                            color: entryRoot.saveAsBookmark ? ThemeData.accentColor : "transparent"
                            border.color: entryRoot.saveAsBookmark ? ThemeData.accentColor : Qt.rgba(1, 1, 1, 0.3)
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                color: "#ffffff"
                                font.pointSize: 9
                                font.bold: true
                                visible: entryRoot.saveAsBookmark
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: entryRoot.saveAsBookmark = !entryRoot.saveAsBookmark
                            }
                        }

                        Text {
                            text: "Save as Bookmark for easier login"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            anchors.verticalCenter: parent.verticalCenter

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: entryRoot.saveAsBookmark = !entryRoot.saveAsBookmark
                            }
                        }
                    }

                    Item { width: 1; height: 2 }

                    NeoNectButton {
                        text: NetworkManager.isLoading ? "Signing In..." : "Sign In"
                        width: parent.width
                        height: 40
                        enabled: loginUser.text.trim() !== "" && loginPass.text !== "" && !NetworkManager.isLoading
                        highlighted: true
                        onClicked: {
                            entryRoot.loginErrorText = ""
                            NetworkManager.loginUser(loginUser.text, loginPass.text)
                        }
                    }
                }

                // ─── SCREEN 3: USER REGISTRATION ───────────────────────────
                Column {
                    id: signupView
                    width: parent.width
                    spacing: 8
                    visible: entryRoot.currentScreen === "signup"
                    anchors.top: parent.top

                    // Error / Success Chip
                    Rectangle {
                        width: parent.width
                        height: 30
                        radius: 6
                        color: entryRoot.regSuccessText !== "" ? Qt.rgba(0.14, 0.65, 0.35, 0.15) : Qt.rgba(0.93, 0.32, 0.32, 0.15)
                        border.color: entryRoot.regSuccessText !== "" ? "#23a55a" : "#ef5350"
                        border.width: 1
                        visible: entryRoot.regErrorText !== "" || entryRoot.regSuccessText !== ""

                        Text {
                            text: entryRoot.regSuccessText !== "" ? "✔ " + entryRoot.regSuccessText : "⚠️ " + entryRoot.regErrorText
                            color: entryRoot.regSuccessText !== "" ? "#23a55a" : "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            anchors.centerIn: parent
                        }
                    }

                    // Username Field
                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "USERNAME"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: regUser
                            width: parent.width
                            height: 40
                            placeholderText: "Choose a username"
                            onTextChanged: {
                                if (text.trim().length >= 3) {
                                    entryRoot.isUsernameAvailable = true;
                                    entryRoot.isCheckingAvailability = true;
                                    entryRoot.usernameStatusText = "🔍 Checking availability...";
                                    availDebounce.restart();
                                } else if (text.trim().length > 0) {
                                    entryRoot.isUsernameAvailable = false;
                                    entryRoot.isCheckingAvailability = false;
                                    entryRoot.usernameStatusText = "⚠️ Min 3 characters required";
                                } else {
                                    entryRoot.isUsernameAvailable = true;
                                    entryRoot.isCheckingAvailability = false;
                                    entryRoot.usernameStatusText = "";
                                }
                            }
                        }
                        Text {
                            text: entryRoot.usernameStatusText
                            color: entryRoot.isUsernameAvailable ? "#23a55a" :
                                   entryRoot.isCheckingAvailability ? ThemeData.textSecondary : "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 4
                            visible: text !== ""
                        }
                    }

                    // Password Field & Strength
                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        Item {
                            width: parent.width
                            height: 40
                            NeoNectTextField {
                                id: regPass
                                anchors.fill: parent
                                placeholderText: "Min 8 chars, letters & numbers"
                                echoMode: entryRoot.showRegPassword ? TextInput.Normal : TextInput.Password
                            }

                            Text {
                                text: entryRoot.showRegPassword ? "Hide" : "Show"
                                color: ThemeData.accentColor
                                font.pointSize: ThemeData.fontSizeNormal - 3
                                font.bold: true
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                z: 10
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: entryRoot.showRegPassword = !entryRoot.showRegPassword
                                }
                            }
                        }

                        // Password Complexity Helper
                        Text {
                            text: (regPass.text.length > 0 && (regPass.text.length < 8 || !/[a-zA-Z]/.test(regPass.text) || !/[0-9]/.test(regPass.text))) ?
                                  "❌ Must be at least 8 chars with letters & numbers" : ""
                            color: "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 4
                            visible: text !== ""
                        }

                        // Strength Progress Bar
                        Row {
                            width: parent.width
                            spacing: 4
                            visible: regPass.text.length > 0
                            property int score: (regPass.text.length >= 8 ? 1 : 0) +
                                                (/[0-9]/.test(regPass.text) && /[a-zA-Z]/.test(regPass.text) ? 1 : 0) +
                                                (/[A-Z]/.test(regPass.text) && /[^a-zA-Z0-9]/.test(regPass.text) ? 1 : 0)

                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 3
                                radius: 1.5
                                color: parent.score >= 1 ? (parent.score === 1 ? "#ef5350" : parent.score === 2 ? "#ffa726" : "#23a55a") : Qt.rgba(1,1,1,0.1)
                            }
                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 3
                                radius: 1.5
                                color: parent.score >= 2 ? (parent.score === 2 ? "#ffa726" : "#23a55a") : Qt.rgba(1,1,1,0.1)
                            }
                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 3
                                radius: 1.5
                                color: parent.score >= 3 ? "#23a55a" : Qt.rgba(1,1,1,0.1)
                            }
                        }
                    }

                    // Confirm Password Field
                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "CONFIRM PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: regConfirmPass
                            width: parent.width
                            height: 40
                            placeholderText: "Re-enter your password"
                            echoMode: entryRoot.showRegPassword ? TextInput.Normal : TextInput.Password
                        }
                        Text {
                            text: regConfirmPass.text === "" ? "" :
                                  (regConfirmPass.text === regPass.text ? "✔ Passwords match" : "❌ Passwords do not match")
                            color: regConfirmPass.text === regPass.text ? "#23a55a" : "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 4
                            visible: text !== ""
                        }
                    }

                    Item { width: 1; height: 2 }

                    NeoNectButton {
                        text: NetworkManager.isLoading ? "Creating Account..." : "Create Account"
                        width: parent.width
                        height: 40
                        enabled: regUser.text.trim().length >= 3 &&
                                 regPass.text.length >= 8 &&
                                 /[a-zA-Z]/.test(regPass.text) &&
                                 /[0-9]/.test(regPass.text) &&
                                 regPass.text === regConfirmPass.text &&
                                 !NetworkManager.isLoading &&
                                 !entryRoot.usernameStatusText.startsWith("❌")
                        highlighted: true
                        onClicked: {
                            entryRoot.regErrorText = ""
                            entryRoot.regSuccessText = ""
                            NetworkManager.registerUser(regUser.text.trim(), regPass.text)
                        }
                    }
                }

                // ─── SCREEN 4: BOOKMARKS LIST (TEAMSPEAK STYLE) ───────────
                Column {
                    id: bookmarksView
                    width: parent.width
                    spacing: 10
                    visible: entryRoot.currentScreen === "bookmarks"
                    anchors.top: parent.top

                    // Header Row with Title and "+ New Bookmark"
                    Row {
                        width: parent.width
                        Item {
                            width: parent.width - newBmBtn.width
                            height: 30
                            Text {
                                text: "SAVED SERVERS"
                                color: ThemeData.textSecondary
                                font.pointSize: ThemeData.fontSizeNormal - 3
                                font.bold: true
                                font.letterSpacing: 1
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        NeoNectButton {
                            id: newBmBtn
                            text: "+ New Bookmark"
                            height: 28
                            fontSize: ThemeData.fontSizeNormal - 3
                            highlighted: false
                            onClicked: {
                                entryRoot.editBmId = "";
                                entryRoot.editBmName = "";
                                entryRoot.editBmServer = NetworkManager.serverUrl !== "" ? NetworkManager.serverUrl : "http://localhost:8080";
                                entryRoot.editBmUser = "";
                                entryRoot.editBmPass = "";
                                entryRoot.isEditingBookmark = false;
                                entryRoot.bookmarkErrorText = "";
                                entryRoot.currentScreen = "bookmark_edit";
                            }
                        }
                    }

                    // Error Notification Chip if connecting failed
                    Rectangle {
                        width: parent.width
                        height: 32
                        radius: 8
                        color: Qt.rgba(0.93, 0.32, 0.32, 0.15)
                        border.color: "#ef5350"
                        border.width: 1
                        visible: entryRoot.bookmarkErrorText !== ""

                        Text {
                            text: "⚠️ " + entryRoot.bookmarkErrorText
                            color: "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            anchors.centerIn: parent
                        }
                    }

                    // Empty State
                    Column {
                        width: parent.width
                        spacing: 8
                        visible: NetworkManager.bookmarks.length === 0
                        topPadding: 12
                        bottomPadding: 12

                        Rectangle {
                            width: 44; height: 44
                            radius: 22
                            color: Qt.rgba(1, 1, 1, 0.05)
                            border.color: Qt.rgba(1, 1, 1, 0.1)
                            border.width: 1
                            anchors.horizontalCenter: parent.horizontalCenter

                            IconImage {
                                source: "qrc:/qt/qml/NeoNect/assets/icons/bookmark.svg"
                                width: 22; height: 22
                                color: ThemeData.accentColor
                                anchors.centerIn: parent
                            }
                        }

                        Text {
                            text: "No Bookmarks Saved"
                            color: ThemeData.textPrimary
                            font.bold: true
                            font.pointSize: ThemeData.fontSizeNormal - 1
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Text {
                            text: "Save your favorite servers, usernames, and passwords\nfor instant 1-click connection, like TeamSpeak."
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            horizontalAlignment: Text.AlignHCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                        }

                        Item { width: 1; height: 4 }

                        NeoNectButton {
                            text: "+ Add Your First Bookmark"
                            height: 34
                            fontSize: ThemeData.fontSizeNormal - 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            highlighted: true
                            onClicked: {
                                entryRoot.editBmId = "";
                                entryRoot.editBmName = "";
                                entryRoot.editBmServer = NetworkManager.serverUrl !== "" ? NetworkManager.serverUrl : "http://localhost:8080";
                                entryRoot.editBmUser = "";
                                entryRoot.editBmPass = "";
                                entryRoot.isEditingBookmark = false;
                                entryRoot.bookmarkErrorText = "";
                                entryRoot.currentScreen = "bookmark_edit";
                            }
                        }
                    }

                    // Bookmarks Scrollable Area
                    Item {
                        width: parent.width
                        height: Math.min(NetworkManager.bookmarks.length * 58 + Math.max(0, NetworkManager.bookmarks.length - 1) * 6, 230)
                        visible: NetworkManager.bookmarks.length > 0
                        clip: true

                        ListView {
                            anchors.fill: parent
                            spacing: 6
                            model: NetworkManager.bookmarks
                            boundsBehavior: Flickable.StopAtBounds

                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                                active: true
                            }

                            delegate: Rectangle {
                                id: bmItem
                                width: parent.width
                                height: 52
                                radius: 8
                                color: bmItemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.25)
                                border.color: bmItemMouse.containsMouse ? ThemeData.accentColor : Qt.rgba(1, 1, 1, 0.08)
                                border.width: 1

                                MouseArea {
                                    id: bmItemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: 0
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    z: 1

                                    IconImage {
                                        source: "qrc:/qt/qml/NeoNect/assets/icons/bookmark.svg"
                                        width: 16; height: 16
                                        color: ThemeData.accentColor
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Column {
                                        width: parent.width - 16 - 8 - actionRow.width - 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 2

                                        Text {
                                            width: parent.width
                                            text: modelData.name || modelData.serverUrl
                                            color: ThemeData.textPrimary
                                            font.bold: true
                                            font.pointSize: ThemeData.fontSizeNormal - 2
                                            elide: Text.ElideRight
                                        }

                                        Row {
                                            spacing: 4
                                            Text {
                                                text: modelData.username ? ("@" + modelData.username) : ""
                                                color: ThemeData.accentColor
                                                font.bold: true
                                                font.pointSize: ThemeData.fontSizeNormal - 4
                                                visible: text !== ""
                                            }
                                            Text {
                                                text: "• " + modelData.serverUrl
                                                color: ThemeData.textSecondary
                                                font.pointSize: ThemeData.fontSizeNormal - 4
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    Row {
                                        id: actionRow
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 5

                                        NeoNectButton {
                                            text: (entryRoot.connectingBookmarkId === modelData.id && NetworkManager.isLoading) ? "Connecting..." : "Connect"
                                            height: 28
                                            width: 76
                                            fontSize: ThemeData.fontSizeNormal - 3.5
                                            highlighted: true
                                            enabled: !NetworkManager.isLoading
                                            onClicked: {
                                                entryRoot.connectingBookmarkId = modelData.id;
                                                entryRoot.bookmarkErrorText = "";
                                                NetworkManager.connectBookmark(modelData.id);
                                            }
                                        }

                                        Rectangle {
                                            width: 28; height: 28
                                            radius: 6
                                            color: editItemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                                            anchors.verticalCenter: parent.verticalCenter

                                            IconImage {
                                                source: "qrc:/qt/qml/NeoNect/assets/icons/settings.svg"
                                                width: 14; height: 14
                                                color: editItemMouse.containsMouse ? ThemeData.textPrimary : ThemeData.textSecondary
                                                anchors.centerIn: parent
                                            }

                                            MouseArea {
                                                id: editItemMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    entryRoot.editBmId = modelData.id;
                                                    entryRoot.editBmName = modelData.name || "";
                                                    entryRoot.editBmServer = modelData.serverUrl || "";
                                                    entryRoot.editBmUser = modelData.username || "";
                                                    entryRoot.editBmPass = modelData.password || "";
                                                    entryRoot.isEditingBookmark = true;
                                                    entryRoot.bookmarkErrorText = "";
                                                    entryRoot.currentScreen = "bookmark_edit";
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: 28; height: 28
                                            radius: 6
                                            color: delItemMouse.containsMouse ? Qt.rgba(0.93, 0.32, 0.32, 0.25) : Qt.rgba(1, 1, 1, 0.05)
                                            anchors.verticalCenter: parent.verticalCenter

                                            IconImage {
                                                source: "qrc:/qt/qml/NeoNect/assets/icons/trash.svg"
                                                width: 14; height: 14
                                                color: delItemMouse.containsMouse ? "#ef5350" : ThemeData.textSecondary
                                                anchors.centerIn: parent
                                            }

                                            MouseArea {
                                                id: delItemMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    NetworkManager.deleteBookmark(modelData.id);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 2 }

                    NeoNectButton {
                        text: "🌐 Direct Server Connection"
                        width: parent.width
                        height: 38
                        fontSize: ThemeData.fontSizeNormal - 2
                        highlighted: false
                        onClicked: {
                            entryRoot.currentScreen = "server";
                        }
                    }
                }

                // ─── SCREEN 5: BOOKMARK ADD / EDIT ────────────────────────
                Column {
                    id: bookmarkEditView
                    width: parent.width
                    spacing: 8
                    visible: entryRoot.currentScreen === "bookmark_edit"
                    anchors.top: parent.top

                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "BOOKMARK LABEL"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: editBmNameInput
                            width: parent.width
                            height: 40
                            placeholderText: "e.g., My Clan Server, Dev Local Relay"
                            text: entryRoot.editBmName
                            onTextChanged: entryRoot.editBmName = text
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "SERVER NODE ADDRESS"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: editBmServerInput
                            width: parent.width
                            height: 40
                            placeholderText: "e.g., http://localhost:8080"
                            text: entryRoot.editBmServer
                            onTextChanged: entryRoot.editBmServer = text
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "USERNAME"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        NeoNectTextField {
                            id: editBmUserInput
                            width: parent.width
                            height: 40
                            placeholderText: "Username on this server"
                            text: entryRoot.editBmUser
                            onTextChanged: entryRoot.editBmUser = text
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 2
                        Label {
                            text: "PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        Item {
                            width: parent.width
                            height: 40
                            NeoNectTextField {
                                id: editBmPassInput
                                anchors.fill: parent
                                placeholderText: "Account password"
                                echoMode: entryRoot.showBmPassword ? TextInput.Normal : TextInput.Password
                                text: entryRoot.editBmPass
                                onTextChanged: entryRoot.editBmPass = text
                            }
                            Text {
                                text: entryRoot.showBmPassword ? "Hide" : "Show"
                                color: ThemeData.accentColor
                                font.pointSize: ThemeData.fontSizeNormal - 3
                                font.bold: true
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                z: 10
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: entryRoot.showBmPassword = !entryRoot.showBmPassword
                                }
                            }
                        }
                    }

                    Item { width: 1; height: 4 }

                    Row {
                        width: parent.width
                        spacing: 10

                        NeoNectButton {
                            text: entryRoot.isEditingBookmark ? "Update Bookmark" : "Save Bookmark"
                            width: (parent.width - parent.spacing) / 2
                            height: 40
                            highlighted: true
                            enabled: editBmServerInput.text.trim() !== "" && editBmUserInput.text.trim() !== ""
                            onClicked: {
                                var label = editBmNameInput.text.trim() !== "" ? editBmNameInput.text.trim() : editBmServerInput.text.trim();
                                NetworkManager.saveBookmark(label, editBmServerInput.text.trim(), editBmUserInput.text.trim(), editBmPassInput.text, entryRoot.isEditingBookmark ? entryRoot.editBmId : "");
                                entryRoot.currentScreen = "bookmarks";
                            }
                        }

                        NeoNectButton {
                            text: "Cancel"
                            width: (parent.width - parent.spacing) / 2
                            height: 40
                            highlighted: false
                            onClicked: entryRoot.currentScreen = "bookmarks"
                        }
                    }
                }
            }
        }
    }

    // ─── TIMERS ────────────────────────────────────────────────────────────
    Timer {
        id: verifyDebounce
        interval: 500
        repeat: false
        onTriggered: NetworkManager.verifyServer(serverInput.text)
    }

    Timer {
        id: availDebounce
        interval: 400
        repeat: false
        onTriggered: {
            if (regUser.text.trim().length >= 3 && NetworkManager.serverUrl !== "") {
                NetworkManager.checkUsernameAvailability(regUser.text.trim())
            }
        }
    }

    // ─── SIGNAL CONNECTIONS ────────────────────────────────────────────────
    Connections {
        target: NetworkManager
        ignoreUnknownSignals: true

        function onVerificationResult(success, message) {
            if (entryRoot) {
                entryRoot.isServerReady = success;
                entryRoot.serverStatusText = success ? "🟢 " + message : "🔴 " + message;
            }
        }

        function onAvailabilityResult(username, available, message) {
            if (entryRoot && regUser.text.trim() === username) {
                entryRoot.isCheckingAvailability = false;
                entryRoot.isUsernameAvailable = available;
                entryRoot.usernameStatusText = available ? "✔ Username is available" : "❌ " + message;
            }
        }

        function onRegistrationResult(success, message) {
            if (!entryRoot) return;
            if (success) {
                entryRoot.regSuccessText = "Account created! Signing in...";
                var u = regUser.text.trim();
                var p = regPass.text;
                NetworkManager.loginUser(u, p);
            } else {
                entryRoot.regErrorText = message;
            }
        }

        function onLoginResult(success, tokenOrError) {
            if (!entryRoot) return;
            if (success) {
                if (entryRoot.saveAsBookmark) {
                    var srv = NetworkManager.serverUrl !== "" ? NetworkManager.serverUrl : "http://localhost:8080";
                    NetworkManager.saveBookmark(srv, srv, loginUser.text.trim(), loginPass.text);
                    entryRoot.saveAsBookmark = false;
                }
                entryRoot.connectingBookmarkId = "";
            } else {
                var errStr = tokenOrError ? tokenOrError : "Error occurred";
                entryRoot.connectingBookmarkId = "";
                if (entryRoot.currentScreen === "signup") {
                    entryRoot.regErrorText = errStr;
                } else if (entryRoot.currentScreen === "bookmarks") {
                    entryRoot.bookmarkErrorText = errStr;
                } else {
                    entryRoot.loginErrorText = errStr;
                }
            }
        }
    }

    Component.onCompleted: {
        if (typeof root !== "undefined" && typeof root.devDeepLink === "string" && root.devDeepLink !== "") {
            entryRoot.currentScreen = root.devDeepLink;
        }
    }
}


