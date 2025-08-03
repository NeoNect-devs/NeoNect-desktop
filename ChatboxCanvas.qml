import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    //color: Qt.darker("white", 6)
    color: Qt.darker("grey",4)
    anchors {
        top: parent.top
        left: parent.left
        right: parent.right
        bottom: parent.bottom
        //rightMargin: 7
    }
    // --- Data Model ---
        // A ListModel stores all the messages for the chat.
        // In a real app, this might be populated from a network request or database.
    ListModel {
        id: messageModel

    }

    ColumnLayout{
        anchors.fill: parent
        spacing: 0
        // Rectangle{
        //     color:"green"
        //     Layout.fillWidth:true
        //     Layout.fillHeight:true
        // }

        ScrollView {
            id: chatScrollView
            clip: true
            //color: Qt.ligher("red",4)
            Layout.fillWidth:true
            Layout.fillHeight:true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: ScrollBar {
                id: chatScrollbar
                size: 0.6
                position: 0.2
                active: true
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                orientation: Qt.Vertical
                contentItem: Rectangle {

                    implicitWidth: 6
                    radius: 5
                    implicitHeight: chatScrollView.height
                    color: chatScrollbar.pressed ? Qt.darker("grey", 1.5) : "grey"
                    opacity: chatScrollbar.policy === ScrollBar.AlwaysOn
                             || (chatScrollbar.active
                                 && chatScrollbar.size < 1.0) ? 0.75 : 0
                }
            }//ScrollBar.vertical


            // ListView is much more efficient than a static ColumnLayout for dynamic content.
            //ScrollView>>ListView
            ListView {
                id: messageListView
                width: chatScrollView.width
                model: messageModel
                spacing: 8
                // The delegate is the template for each item in the model.
                delegate: MessageBubble {
                    width: messageListView.width
                    messageText: text // 'text' comes from the model role
                    sentByMe: fromMe  // 'fromMe' comes from the model role
                    avatarVisible: showAvatar
                    avatarSource: senderAvatar
                }
                Component.onCompleted: {
                    root.loadMessages()
                    positionViewAtEnd()
                }

                onContentHeightChanged: positionViewAtEnd()

                function positionViewAtEnd() {
                    if (contentHeight > chatScrollView.height) {
                        chatScrollView.ScrollBar.vertical.position = 1.0
                        }
                }
            }//ListView

                // Text {
                //     text: qsTr("ABC\nDEF\nGHI\nDEF\nGHI\nGHI\nDEF\nABC\n\nDEF\nABC")
                //     font.pointSize: 80
                // }


        }//ScrollView

        // --- Message Input Area ---
        //ColumnLayout >> MessageInputCanvas
        MessageInputCanvas {
            id: messageInput
            // height: 40
            Layout.preferredHeight: 40
            Layout.fillWidth:true
            // The onSendMessage signal from the input component is connected here
             onSendMessage: (msgText) => { root.sendMessage(msgText) }

        }//MessageInputCanvas

    }//ColumnLayout


    // --- Functions ---
    function updateAvatarVisibility() {
        for (let i = 0; i < messageModel.count; ++i) {
            const currentSender = messageModel.get(i).senderName
            const nextSender = (i + 1 < messageModel.count) ? messageModel.get(i + 1).senderName : ""
            // Show avatar if the next message is from a different sender
            const show = (currentSender !== nextSender)
            messageModel.setProperty(i, "showAvatar", show)
        }
    }

    function sendMessage(msgText) {
        if (msgText.trim() === "") return;

        messageModel.append({
            text: msgText,
            fromMe: true,
            senderName: "Me",
            senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME", // Placeholder for your avatar
            showAvatar: true
        })

        updateAvatarVisibility()
    }

    function loadMessages() {
        messageModel.clear()
        // Sample data demonstrating the avatar logic
        messageModel.append({ text: "OK, let's start the test.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Ready!", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "Short message.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "This is a slightly longer message to see how the bubble aligns.", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "This is a very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very long message to rigorously test the text wrapping inside the message bubble component. It must not overflow.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "It works!", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "Now for the unbroken string test.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "WrapAnywhere is handling it correctly.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Time for different languages. ¿Qué tal?", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "¡Hola! Todo bien por aquí.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Bonjour, comment allez-vous ?", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "Très bien, merci. Et vous ?", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Wie geht's?", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "Gut, danke!", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "こんにちは", fromMe: false, senderName: "Yuki", senderAvatar: "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"});
               messageModel.append({ text: "元気ですか？", fromMe: false, senderName: "Yuki", senderAvatar: "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"});
               messageModel.append({ text: "はい、元気です。", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Привет!", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "Как дела?", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "Хорошо, спасибо.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Special characters test: !@#$%^&*()_+-=[]{}|;':\",./<>?`~", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "They render fine.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Emojis: 👍🎉🚀😂❤️🔥😊", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "Looks good! 👍", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Numbers: 1234567890", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "Mixed: 1a2b3c4d5e", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "مرحبا", fromMe: false, senderName: "Yuki", senderAvatar: "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"});
               messageModel.append({ text: "كيف حالك؟", fromMe: false, senderName: "Yuki", senderAvatar: "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"});
               messageModel.append({ text: "أنا بخير، شكراً لك.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               for (let i = 31; i <= 90; ++i) {
                   const sender = i % 3 === 0 ? "JaneDoe" : (i % 3 === 1 ? "JohnSmith" : "Yuki");
                   const avatar = sender === "JaneDoe" ? "https://placehold.co/40x40/7289DA/FFFFFF?text=JD" : (sender === "JohnSmith" ? "https://placehold.co/40x40/43B581/FFFFFF?text=JS" : "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y");
                   messageModel.append({ text: "Filler message " + i, fromMe: false, senderName: sender, senderAvatar: avatar});
               }
               messageModel.append({ text: "Almost done.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "Just a few more.", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "93", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "94", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
               messageModel.append({ text: "95", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "96", fromMe: false, senderName: "Yuki", senderAvatar: "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"});
               messageModel.append({ text: "97", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "98", fromMe: false, senderName: "JaneDoe", senderAvatar: "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"});
               messageModel.append({ text: "This is the second to last message of this very long test.", fromMe: true, senderName: "Me", senderAvatar: "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"});
               messageModel.append({ text: "And this is the final one! Test complete. Scrolling and rendering look good.", fromMe: false, senderName: "JohnSmith", senderAvatar: "https://placehold.co/40x40/43B581/FFFFFF?text=JS"});
        updateAvatarVisibility()
    }
}//root




