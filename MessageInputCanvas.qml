import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#40444B" // A darker, more modern background color

    // Signal to notify the parent (ChatboxCanvas) to send a message.
    signal sendMessage(string msgText)

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        // The text input field where the user types their message.
        TextField {
            id: messageInput
            Layout.fillWidth: true
            placeholderText: "Type a message..."
            color: "#DCDDDE"
            placeholderTextColor:"#444444"

            // Custom background for the text field
            background: Rectangle {
                color: "#202225"
                radius: 18
            }

            // When the user presses Enter, emit the sendMessage signal.
            onAccepted: {
                if (text.trim() !== "") {
                    root.sendMessage(text)
                    text = "" // Clear the input field
                }
            }
        }

        // The send button.
        Rectangle {
            id: btnSend
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            color: "#5865F2" // A nice blue for the button
            radius: 18 // Make it circular

            // Text or an Icon for the button
            Text {
                text: "➤" // Using a simple arrow character as an icon
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 18
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (messageInput.text.trim() !== "") {
                        root.sendMessage(messageInput.text)
                        messageInput.text = "" // Clear the input field
                    }
                }
            }
        }
    }
}
