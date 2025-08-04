import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#202225" // A darker, more modern background color
    // anchors{
    //     leftMargin: 10
    //     bottomMargin: 10
    //     rightMargin: 10
    // }
    // Signal to notify the parent (ChatboxCanvas) to send a message.
    signal sendMessage(string msgText)
    radius: 8
    RowLayout {
        anchors.fill: parent
        // anchors.leftMargin: 10
        // anchors.rightMargin: 10
        //spacing: 10

        // The text input field where the user types their message.
        TextField {
            id: messageInput
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Layout.minimumHeight: root.height
            // Layout.maximumHeight: root.height
            placeholderText: "Type a message..."
            color: "#DCDDDE"
            placeholderTextColor:"#444444"
            font.pixelSize:14
            // Custom background for the text field
            background: Rectangle {
                color: "#00000000"
                //radius: 18
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
            Layout.preferredWidth: 30
            // Layout.preferredHeight: 26
            //Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#00000000" // A nice blue for the button
            //radius: 18 // Make it circular

            // Text or an Icon for the button
            Text {
                text: "➤" // Using a simple arrow character as an icon
                anchors.centerIn: parent
                color: "white"
                font.pixelSize: 22
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
