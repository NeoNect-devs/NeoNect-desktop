// qml/entrypage/entry.qml
import QtQuick
import QtQuick.Controls
import Avila.Core 1.0
import "../containers"

Item {
    id: entryRoot
    anchors.fill: parent

    readonly property bool showTitleBackButton: currentScreen !== "server"
    property string currentScreen: "server"
    property bool isServerReady: false
    property string serverStatusText: ""

    function goBack() {
        if (currentScreen !== "server") {
            currentScreen = "server";
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ThemeData.windowBackground
    }

    Rectangle {
        id: formCard
        width: 380
        height: 540
        radius: 16
        color: ThemeData.panelBackground
        anchors.centerIn: parent
        border.color: "#1affffff"
        border.width: 1

        Column {
            id: headerArea
            width: parent.width
            topPadding: 45
            spacing: 16

            Image {
                source: "qrc:/qt/qml/Avila/assets/logo.png"
                width: 96
                height: 96
                fillMode: Image.PreserveAspectFit
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: "AVILA"
                color: ThemeData.textPrimary
                font.pointSize: ThemeData.fontSizeHeader + 4
                font.bold: true
                font.letterSpacing: 3
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        Item {
            id: dynamicContent
            width: parent.width - 64
            anchors.top: headerArea.bottom
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            clip: true

            // ─── SCREEN 1: SERVER ROUTING ────────────────────────────
            Column {
                id: serverView
                width: parent.width
                spacing: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: entryRoot.currentScreen === "server"

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
                    placeholderText: "host:port (e.g., avila.chat:443)"

                    property bool hasValidFormat: /^([a-zA-Z0-9.-]+):([0-9]+)$/.test(text)

                    onTextChanged: {
                        entryRoot.isServerReady = false;
                        if (hasValidFormat) {
                            entryRoot.serverStatusText = "🔍 Resolving node...";
                            verifyDebounce.restart();
                        } else if (text !== "") {
                            entryRoot.serverStatusText = "⚠️ Required format: host:port";
                        } else {
                            entryRoot.serverStatusText = "";
                        }
                    }
                }

                Text {
                    text: entryRoot.serverStatusText
                    color: entryRoot.isServerReady ? "#4caf50" : "#ef5350"
                    font.pointSize: ThemeData.fontSizeNormal - 3
                    wrapMode: Text.WordWrap
                    width: parent.width
                }

                Item {
                    width: 1
                    height: 14
                }

                // ➔ SIDE-BY-SIDE BUTTON ARRANGEMENT
                Row {
                    width: parent.width
                    spacing: 12

                    AvilaButton {
                        text: "Log In"
                        // Divide available space cleanly between both components
                        width: (parent.width - parent.spacing) / 2
                        enabled: entryRoot.isServerReady
                        highlighted: true
                        onClicked: entryRoot.currentScreen = "login"
                    }

                    AvilaButton {
                        text: "Create Account"
                        width: (parent.width - parent.spacing) / 2
                        enabled: entryRoot.isServerReady
                        highlighted: false
                        onClicked: entryRoot.currentScreen = "signup"
                    }
                }
            }

            // ─── SCREEN 2: USER LOGIN ────────────────────────────────
            Column {
                id: loginView
                width: parent.width
                spacing: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: entryRoot.currentScreen === "login"

                AvilaTextField {
                    id: loginUser
                    width: parent.width
                    placeholderText: "Username"
                }

                AvilaTextField {
                    id: loginPass
                    width: parent.width
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                }

                Item {
                    width: 1
                    height: 10
                }

                AvilaButton {
                    text: "Sign In"
                    width: parent.width
                    enabled: loginUser.text !== "" && loginPass.text !== ""
                    onClicked: NetworkManager.loginUser(loginUser.text, loginPass.text)
                }
            }

            // ─── SCREEN 3: USER SIGNUP ───────────────────────────────
            Column {
                id: signupView
                width: parent.width
                spacing: 14
                anchors.verticalCenter: parent.verticalCenter
                visible: entryRoot.currentScreen === "signup"

                AvilaTextField {
                    id: regUser
                    width: parent.width
                    placeholderText: "Username"
                }

                AvilaTextField {
                    id: regPass
                    width: parent.width
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                }

                Item {
                    width: 1
                    height: 10
                }

                AvilaButton {
                    text: "Create Secure Account"
                    width: parent.width
                    enabled: regUser.text !== "" && regPass.text !== ""
                    onClicked: console.log("Account initialization targeted:", regUser.text)
                }
            }
        }
    }

    Timer {
        id: verifyDebounce
        interval: 500
        repeat: false
        onTriggered: NetworkManager.verifyServer(serverInput.text)
    }

    Connections {
        target: NetworkManager

        function onVerificationResult(success, message) {
            entryRoot.isServerReady = success;
            entryRoot.serverStatusText = success ? "🟢 " + message : "🔴 " + message;
        }

        function onLoginResult(success, tokenOrError) {
            if (success) {
                root.appState = "authenticated";
            } else {
                entryRoot.serverStatusText = "❌ " + tokenOrError;
                entryRoot.currentScreen = "server";
            }
        }
    }

    Component.onCompleted: {
        if (root && root.devDeepLink !== "") {
            entryRoot.currentScreen = root.devDeepLink;
        }
    }
}
