// qml/entrypage/entry.qml
import QtQuick
import QtQuick.Layouts
import Avila 1.0
import "apiService.js" as Api

Rectangle {
    id: entryRoot
    color: "transparent"

    property string currentSubScreen: "address_entry" // Options: "address_entry", "signup", "login"
    property string verifiedServerAddress: ""

    // Down-stream action callbacks handling root main window adjustments
    signal authenticationSuccess(string token, string serverUrl)
    signal requestTitleChange(string newTitle)

    onCurrentSubScreenChanged: {
        if (currentSubScreen === "address_entry") requestTitleChange("Server Connection");
        else if (currentSubScreen === "signup") requestTitleChange("Create Account");
        else if (currentSubScreen === "login") requestTitleChange("Welcome Back");
    }

    Rectangle {
        id: containerPanel
        color: "black"
        width: 400
        height: 480
        radius: 8
        anchors.centerIn: parent

        // -----------------------------------------------------------
        // SUB-VIEW 1: SERVER ADDRESS CONNECT & VERIFY
        // -----------------------------------------------------------
        ColumnLayout {
            id: addressView
            anchors.fill: parent; anchors.margins: 35; spacing: 12
            visible: entryRoot.currentSubScreen === "address_entry"

            property bool formatMatches: /^[a-zA-Z0-9.-]+:[0-9]{1,5}$/.test(txtServerAddr.text)
            property bool isAvilaVerified: false
            property bool isValidatingNetwork: false
            property string serverHint: "Enter server address (host:port)"
            readonly property bool connectivityPass: formatMatches && isAvilaVerified

            Image {
                Layout.alignment: Qt.AlignCenter
                Layout.topMargin: 10
                Layout.preferredWidth: 75; Layout.preferredHeight: 75
                source: "../../assets/logo.png"
                fillMode: Image.PreserveAspectFit
                antialiasing: true
                // --- THE CHEAT CLICK MOUSEAREA ---
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onDoubleClicked: {
                        console.log("🛠️ Dev Mode Active: Bypassing Server Pipeline");
                        // Manually trigger the successful auth signal with local placeholder data
                        entryRoot.authenticationSuccess("mock_dev_token_xyz123", "localhost:localdev");
                    }
                }
            }

            Text {
                Layout.alignment: Qt.AlignCenter
                text: "Avila Chat"
                color: "white"; font.pointSize: 18; font.bold: true
            }

            Item { Layout.preferredHeight: 15 }

            Text { text: "Server Address"; color: ThemeData.textColor || "#b3b3b3"; font.pointSize: 10; font.bold: true }

            Rectangle {
                Layout.fillWidth: true; implicitHeight: 40; radius: 6
                color: ThemeData.mainWindowBackground

                border.width: 1
                border.color: addressView.isValidatingNetwork ? "#ffb300" :
                              (addressView.connectivityPass ? "#4caf50" :
                              (txtServerAddr.text.length > 0 ? "#ff5252" : "transparent"))

                TextInput {
                    id: txtServerAddr
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 40
                    verticalAlignment: TextInput.AlignVCenter; color: "white"; font.pointSize: 11
                    text: "localhost:8080"; selectByMouse: true

                    onTextChanged: {
                        addressView.isAvilaVerified = false
                        if (addressView.formatMatches) {
                            addressView.isValidatingNetwork = true
                            addressView.serverHint = "Verifying Avila instance signatures..."
                            Api.verifyAvilaServer(txtServerAddr.text, function(success, message) {
                                addressView.isAvilaVerified = success;
                                addressView.serverHint = message;
                                addressView.isValidatingNetwork = false;
                                if (success) entryRoot.verifiedServerAddress = txtServerAddr.text;
                            });
                        } else {
                            addressView.isValidatingNetwork = false
                            addressView.serverHint = "Format missing port (e.g., chat.com:443)"
                        }
                    }
                }

                Text {
                    anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter; font.pointSize: 12
                    text: addressView.isValidatingNetwork ? "⏳" : (addressView.connectivityPass ? "🛡️" : "⚠️")
                    opacity: txtServerAddr.text.length > 0 ? 1.0 : 0.0
                }
            }

            Text {
                Layout.fillWidth: true; text: addressView.serverHint; font.pointSize: 9; wrapMode: Text.WordWrap
                color: addressView.isValidatingNetwork ? "#ffb300" : (addressView.connectivityPass ? "#4caf50" : "#ff5252")
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true; spacing: 15
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 42; radius: 8
                    color: !addressView.connectivityPass ? "#2c2c2c" : (sUpM.containsMouse ? "#e0e0e0" : "#ffffff")
                    opacity: addressView.connectivityPass ? 1.0 : 0.4
                    Text { anchors.centerIn: parent; text: "Sign Up"; color: addressView.connectivityPass ? "black" : "#888888"; font.bold: true }
                    MouseArea { id: sUpM; anchors.fill: parent; hoverEnabled: addressView.connectivityPass; enabled: addressView.connectivityPass; onClicked: entryRoot.currentSubScreen = "signup" }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 42; radius: 8
                    color: !addressView.connectivityPass ? "#2c2c2c" : (linM.containsMouse ? "#e0e0e0" : "#ffffff")
                    opacity: addressView.connectivityPass ? 1.0 : 0.4
                    Text { anchors.centerIn: parent; text: "Log In"; color: addressView.connectivityPass ? "black" : "#888888"; font.bold: true }
                    MouseArea { id: linM; anchors.fill: parent; hoverEnabled: addressView.connectivityPass; enabled: addressView.connectivityPass; onClicked: entryRoot.currentSubScreen = "login" }
                }
            }
        }

        // -----------------------------------------------------------
        // SUB-VIEW 2: PROFILE REGISTRATION SIGNUP
        // -----------------------------------------------------------
        ColumnLayout {
            id: signupView
            anchors.fill: parent; anchors.margins: 30; spacing: 10
            visible: entryRoot.currentSubScreen === "signup"

            property bool isUsernameValid: txtUsername.text.length >= 5 && /^[a-zA-Z0-9_]+$/.test(txtUsername.text)
            property bool isUsernameUnique: false
            property string usernameHint: "Minimum 5 letters/numbers"
            property bool isPasswordValid: txtPassword.text.length >= 8 && /[A-Z]/.test(txtPassword.text) && /[a-z]/.test(txtPassword.text) && /[0-9]/.test(txtPassword.text)
            readonly property bool formReady: isUsernameValid && isUsernameUnique && isPasswordValid

            Text { text: "Sign Up"; color: "white"; font.pointSize: 18; font.bold: true }
            Item { implicitHeight: 5 }

            Text { text: "Username"; color: "#b3b3b3"; font.pointSize: 10 }
            Rectangle {
                color: ThemeData.mainWindowBackground; Layout.fillWidth: true; implicitHeight: 36; radius: 6
                border.color: txtUsername.activeFocus ? "#ffffff" : "transparent"; border.width: 1
                TextInput {
                    id: txtUsername; anchors.fill: parent; anchors.leftMargin: 10; verticalAlignment: TextInput.AlignVCenter; color: "white"; font.pointSize: 11; maximumLength: 20
                    onTextChanged: {
                        if (signupView.isUsernameValid) {
                            Api.checkUsernameUniqueness(entryRoot.verifiedServerAddress, txtUsername.text, function(unique, msg) {
                                signupView.isUsernameUnique = unique;
                                signupView.usernameHint = msg;
                            });
                        } else {
                            signupView.usernameHint = "Minimum 5 alphanumeric characters"
                        }
                    }
                }
            }
            Text { text: signupView.usernameHint; font.pointSize: 9; color: (signupView.isUsernameValid && signupView.isUsernameUnique) ? "#4caf50" : "#ff5252" }

            Text { text: "Password"; color: "#b3b3b3"; font.pointSize: 10 }
            Rectangle {
                color: ThemeData.mainWindowBackground; Layout.fillWidth: true; implicitHeight: 36; radius: 6
                border.color: txtPassword.activeFocus ? "#ffffff" : "transparent"; border.width: 1
                TextInput { id: txtPassword; anchors.fill: parent; anchors.leftMargin: 10; verticalAlignment: TextInput.AlignVCenter; color: "white"; font.pointSize: 11; echoMode: TextInput.Password }
            }
            Text { text: "Must include 8+ characters, uppercase, lowercase, & number"; font.pointSize: 9; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: signupView.isPasswordValid ? "#4caf50" : "#ff5252" }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true; implicitHeight: 42; radius: 8
                color: !signupView.formReady ? "#424242" : (subM.containsMouse ? "#e0e0e0" : "#ffffff")
                opacity: signupView.formReady ? 1.0 : 0.5
                Text { anchors.centerIn: parent; text: "Register Account"; color: "black"; font.bold: true }
                MouseArea {
                    id: subM; anchors.fill: parent; hoverEnabled: signupView.formReady; enabled: signupView.formReady
                    onClicked: {
                        Api.registerUser(entryRoot.verifiedServerAddress, txtUsername.text, txtPassword.text, function(success) {
                            if (success) entryRoot.currentSubScreen = "login";
                            else signupView.usernameHint = "Registration failed. Server error.";
                        });
                    }
                }
            }
        }

        // -----------------------------------------------------------
        // SUB-VIEW 3: PROFILE SIGN IN
        // -----------------------------------------------------------
        ColumnLayout {
            id: loginView
            anchors.fill: parent; anchors.margins: 30; spacing: 12
            visible: entryRoot.currentSubScreen === "login"

            property bool hasUsername: loginUsername.text.trim().length > 0
            property bool hasPassword: loginPassword.text.length > 0
            readonly property bool formReady: hasUsername && hasPassword
            property bool hidePassword: true
            property string loginErrorHint: ""

            Text { text: "Sign In"; color: "white"; font.pointSize: 18; font.bold: true }
            Item { implicitHeight: 5 }

            Text { text: "Username"; color: "#b3b3b3"; font.pointSize: 10 }
            Rectangle {
                color: ThemeData.mainWindowBackground; Layout.fillWidth: true; implicitHeight: 38; radius: 6
                border.color: loginUsername.activeFocus ? "#ffffff" : "transparent"; border.width: 1
                TextInput { id: loginUsername; anchors.fill: parent; anchors.leftMargin: 10; verticalAlignment: TextInput.AlignVCenter; color: "white"; font.pointSize: 11; maximumLength: 25; onTextChanged: loginView.loginErrorHint = "" }
            }

            Text { text: "Password"; color: "#b3b3b3"; font.pointSize: 10 }
            Rectangle {
                color: ThemeData.mainWindowBackground; Layout.fillWidth: true; implicitHeight: 38; radius: 6
                border.color: loginPassword.activeFocus ? "#ffffff" : "transparent"; border.width: 1
                TextInput { id: loginPassword; anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 35; verticalAlignment: TextInput.AlignVCenter; color: "white"; font.pointSize: 11; echoMode: loginView.hidePassword ? TextInput.Password : TextInput.Normal; onTextChanged: loginView.loginErrorHint = "" }
                Rectangle {
                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.rightMargin: 6
                    width: 26; height: 26; radius: 4; color: toggleM.containsMouse ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
                    Text { anchors.centerIn: parent; text: loginView.hidePassword ? "👁" : "🙈"; color: "white"; font.pointSize: 11 }
                    MouseArea { id: toggleM; anchors.fill: parent; hoverEnabled: true; onClicked: loginView.hidePassword = !loginView.hidePassword }
                }
            }

            Text { Layout.fillWidth: true; text: loginView.loginErrorHint; font.pointSize: 9; color: "#ff5252"; visible: text.length > 0 }

            Item { Layout.fillHeight: true }

            Rectangle {
                Layout.fillWidth: true; implicitHeight: 42; radius: 8
                color: !loginView.formReady ? "#424242" : (loginSubM.containsMouse ? "#e0e0e0" : "#ffffff")
                opacity: loginView.formReady ? 1.0 : 0.5
                Text { anchors.centerIn: parent; text: "Login"; color: "black"; font.bold: true }
                MouseArea {
                    id: loginSubM; anchors.fill: parent; hoverEnabled: loginView.formReady; enabled: loginView.formReady
                    onClicked: {
                        Api.loginUser(entryRoot.verifiedServerAddress, loginUsername.text, loginPassword.text, function(success, result) {
                            if (success) {
                                entryRoot.authenticationSuccess(result, entryRoot.verifiedServerAddress);
                            } else {
                                loginView.loginErrorHint = result;
                            }
                        });
                    }
                }
            }
        }
    }
}
