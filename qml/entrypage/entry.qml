// qml/entrypage/entry.qml
import QtQuick
import QtQuick.Controls
import Avila.Core 1.0
import "../containers"
import "../components"

Item {
    id: entryRoot
    anchors.fill: parent

    readonly property bool showTitleBackButton: currentScreen !== "server"
    property string currentScreen: "server" // "server", "login", "signup"
    property bool isServerReady: false
    property string serverStatusText: ""

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
        if (currentScreen !== "server") {
            currentScreen = "server";
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ThemeData.windowBackground

        // Background subtle ambient radial glow
        Rectangle {
            width: 500; height: 500
            radius: 250
            anchors.centerIn: parent
            color: ThemeData.accentColor
            opacity: 0.04
        }
    }

    Rectangle {
        id: formCard
        width: 420
        height: currentScreen === "signup" ? 640 : (currentScreen === "login" ? 530 : 500)
        radius: 20
        color: ThemeData.panelBackground
        anchors.centerIn: parent
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }

        Column {
            id: mainColumn
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            // ─── BRAND HEADER ──────────────────────────────────────────────
            Column {
                width: parent.width
                spacing: 6

                Image {
                    source: "qrc:/qt/qml/Avila/assets/logo.png"
                    width: 56
                    height: 56
                    fillMode: Image.PreserveAspectFit
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "DANISA / AVILA"
                    color: ThemeData.textPrimary
                    font.pointSize: ThemeData.fontSizeHeader + 2
                    font.bold: true
                    font.letterSpacing: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: entryRoot.currentScreen === "server" ? "Zero-Knowledge Relay Server Connection" :
                          entryRoot.currentScreen === "login" ? "Welcome back! Sign in to continue" : "Create your secure end-to-end encrypted account"
                    color: ThemeData.textSecondary
                    font.pointSize: ThemeData.fontSizeNormal - 3
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // ─── NAVIGATION TAB SEGMENT (LOGIN vs SIGNUP) ─────────────────
            Rectangle {
                width: parent.width
                height: 38
                radius: 10
                color: Qt.rgba(0, 0, 0, 0.25)
                border.color: Qt.rgba(1, 1, 1, 0.05)
                visible: entryRoot.currentScreen !== "server"

                Row {
                    anchors.fill: parent
                    anchors.margins: 3

                    Rectangle {
                        width: parent.width / 2
                        height: parent.height
                        radius: 8
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
                        radius: 8
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
                               (entryRoot.currentScreen === "login" ? loginView.implicitHeight : signupView.implicitHeight)
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    active: true
                }

                // ─── SCREEN 1: SERVER NODE ROUTING ────────────────────────
                Column {
                    id: serverView
                    width: parent.width
                    spacing: 14
                    visible: entryRoot.currentScreen === "server"
                    anchors.top: parent.top

                    Label {
                        text: "SERVER NODE ADDRESS"
                        color: ThemeData.textSecondary
                        font.pointSize: ThemeData.fontSizeNormal - 3
                        font.bold: true
                        font.letterSpacing: 1
                    }

                    AvilaTextField {
                        id: serverInput
                        width: parent.width
                        placeholderText: "e.g., http://localhost:8080 or avila.chat"

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
                        height: 36
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
                            font.pointSize: ThemeData.fontSizeNormal - 2
                            anchors.centerIn: parent
                        }
                    }

                    Item { width: 1; height: 6 }

                    Row {
                        width: parent.width
                        spacing: 12

                        AvilaButton {
                            text: "Log In"
                            width: (parent.width - parent.spacing) / 2
                            enabled: entryRoot.isServerReady && !NetworkManager.isLoading
                            highlighted: true
                            onClicked: entryRoot.currentScreen = "login"
                        }

                        AvilaButton {
                            text: "Register"
                            width: (parent.width - parent.spacing) / 2
                            enabled: entryRoot.isServerReady && !NetworkManager.isLoading
                            highlighted: false
                            onClicked: entryRoot.currentScreen = "signup"
                        }
                    }

                    AvilaButton {
                        width: parent.width
                        height: 42
                        text: "⚡ Quick Connect (@" + (typeof appProfile !== "undefined" && appProfile !== "" ? appProfile : "demo") + ")"
                        enabled: entryRoot.isServerReady && !NetworkManager.isLoading
                        highlighted: true
                        onClicked: {
                            var u = (typeof appProfile !== "undefined" && appProfile !== "") ? appProfile : "alice";
                            entryRoot.quickConnectUser = u;
                            NetworkManager.loginUser(u, "password123");
                        }
                    }
                }



                // ─── SCREEN 2: USER LOGIN ──────────────────────────────────
                Column {
                    id: loginView
                    width: parent.width
                    spacing: 12
                    visible: entryRoot.currentScreen === "login"
                    anchors.top: parent.top

                    // Error Notification Chip
                    Rectangle {
                        width: parent.width
                        height: 36
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
                        spacing: 4
                        Label {
                            text: "USERNAME"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        AvilaTextField {
                            id: loginUser
                            width: parent.width
                            placeholderText: "Enter your username"
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: 4
                        Label {
                            text: "PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        Item {
                            width: parent.width
                            height: 46
                            AvilaTextField {
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
                                anchors.rightMargin: 14
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

                    Item { width: 1; height: 6 }

                    AvilaButton {
                        text: NetworkManager.isLoading ? "Signing In..." : "Sign In"
                        width: parent.width
                        enabled: loginUser.text.trim() !== "" && loginPass.text !== "" && !NetworkManager.isLoading
                        highlighted: true
                        onClicked: {
                            entryRoot.loginErrorText = ""
                            NetworkManager.loginUser(loginUser.text, loginPass.text)
                        }
                    }
                }

                // ─── SCREEN 3: USER SIGNUP ─────────────────────────────────
                Column {
                    id: signupView
                    width: parent.width
                    spacing: 10
                    visible: entryRoot.currentScreen === "signup"
                    anchors.top: parent.top

                    // Error / Success Chip
                    Rectangle {
                        width: parent.width
                        height: 34
                        radius: 8
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

                    // Username Field with Live Availability
                    Column {
                        width: parent.width
                        spacing: 4
                        Label {
                            text: "USERNAME"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        AvilaTextField {
                            id: regUser
                            width: parent.width
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
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            visible: text !== ""
                        }
                    }

                    // Password Field & Strength Indicator
                    Column {
                        width: parent.width
                        spacing: 4
                        Label {
                            text: "PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        Item {
                            width: parent.width
                            height: 46
                            AvilaTextField {
                                id: regPass
                                anchors.fill: parent
                                placeholderText: "Choose a password"
                                echoMode: entryRoot.showRegPassword ? TextInput.Normal : TextInput.Password
                            }
                            Text {
                                text: entryRoot.showRegPassword ? "Hide" : "Show"
                                color: ThemeData.accentColor
                                font.pointSize: ThemeData.fontSizeNormal - 3
                                font.bold: true
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                z: 10
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: entryRoot.showRegPassword = !entryRoot.showRegPassword
                                }
                            }
                        }

                        // Strength Progress Bar
                        Row {
                            width: parent.width
                            spacing: 4
                            visible: regPass.text.length > 0
                            property int score: (regPass.text.length >= 6 ? 1 : 0) +
                                                (/[0-9]/.test(regPass.text) ? 1 : 0) +
                                                (/[A-Z]/.test(regPass.text) || /[^a-zA-Z0-9]/.test(regPass.text) ? 1 : 0)

                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 4
                                radius: 2
                                color: parent.score >= 1 ? (parent.score === 1 ? "#ef5350" : parent.score === 2 ? "#ffa726" : "#23a55a") : Qt.rgba(1,1,1,0.1)
                            }
                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 4
                                radius: 2
                                color: parent.score >= 2 ? (parent.score === 2 ? "#ffa726" : "#23a55a") : Qt.rgba(1,1,1,0.1)
                            }
                            Rectangle {
                                width: (parent.width - 8) / 3
                                height: 4
                                radius: 2
                                color: parent.score >= 3 ? "#23a55a" : Qt.rgba(1,1,1,0.1)
                            }
                        }
                    }

                    // Confirm Password Field
                    Column {
                        width: parent.width
                        spacing: 4
                        Label {
                            text: "CONFIRM PASSWORD"
                            color: ThemeData.textSecondary
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            font.bold: true
                        }
                        AvilaTextField {
                            id: regConfirmPass
                            width: parent.width
                            placeholderText: "Re-enter your password"
                            echoMode: entryRoot.showRegPassword ? TextInput.Normal : TextInput.Password
                        }
                        Text {
                            text: regConfirmPass.text === "" ? "" :
                                  (regConfirmPass.text === regPass.text ? "✔ Passwords match" : "❌ Passwords do not match")
                            color: regConfirmPass.text === regPass.text ? "#23a55a" : "#ef5350"
                            font.pointSize: ThemeData.fontSizeNormal - 3
                            visible: text !== ""
                        }
                    }

                    Item { width: 1; height: 4 }

                    AvilaButton {
                        text: NetworkManager.isLoading ? "Creating Account..." : "Create Account"
                        width: parent.width
                        enabled: regUser.text.trim().length >= 3 &&
                                 regPass.text !== "" &&
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
    property string quickConnectUser: ""

    Connections {
        target: NetworkManager

        function onVerificationResult(success, message) {
            entryRoot.isServerReady = success;
            entryRoot.serverStatusText = success ? "🟢 " + message : "🔴 " + message;
        }

        function onAvailabilityResult(username, available, message) {
            if (regUser.text.trim() === username) {
                entryRoot.isCheckingAvailability = false;
                entryRoot.isUsernameAvailable = available;
                entryRoot.usernameStatusText = available ? "✔ Username is available" : "❌ " + message;
            }
        }

        function onRegistrationResult(success, message) {
            if (success) {
                entryRoot.regSuccessText = "Account created! Signing in...";
                var u = regUser.text.trim() !== "" ? regUser.text.trim() : (entryRoot.quickConnectUser !== "" ? entryRoot.quickConnectUser : "alice");
                var p = regPass.text !== "" ? regPass.text : "password123";
                NetworkManager.loginUser(u, p);
            } else {
                entryRoot.regErrorText = message;
            }
        }

        function onLoginResult(success, tokenOrError) {
            if (success) {
                entryRoot.quickConnectUser = "";
            } else {
                if (entryRoot.quickConnectUser !== "") {
                    var u = entryRoot.quickConnectUser;
                    entryRoot.quickConnectUser = "";
                    NetworkManager.registerUser(u, "password123");
                    return;
                }
                if (entryRoot.currentScreen === "signup") {
                    entryRoot.regErrorText = tokenOrError ? tokenOrError : "Registration error";
                } else {
                    entryRoot.loginErrorText = tokenOrError ? tokenOrError : "Login error";
                }
            }
        }
    }


    Component.onCompleted: {
        if (typeof root !== "undefined" && root.devDeepLink !== "") {
            entryRoot.currentScreen = root.devDeepLink;
        }
    }
}

