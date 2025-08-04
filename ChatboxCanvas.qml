import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#101210"

    // --- Data Model for Message Groups ---
    ListModel {
        id: messageGroupModel
    }

    // --- Core Logic for the Sticky Avatar ---
    function updateStickyAvatar() {
        // Iterate through the message groups to find the topmost visible one.
        for (var i = 0; i < messageListView.count; i++) {
            const item = messageListView.itemAtIndex(i)
            if (!item)
                continue

            // Item might not be instantiated by the ListView yet.
            const modelData = messageGroupModel.get(i)
            // We only care about message groups from other users.
            if (modelData.fromMe)
                continue

            // Calculate the group's position relative to the visible area of the ScrollView.
            const itemTopInView = item.y - messageListView.contentY
            const itemBottomInView = itemTopInView + item.height

            // Check if this message group is currently visible in the viewport.
            if (itemBottomInView > 0 && itemTopInView < chatScrollView.height) {
                // This is our target group. It's the highest visible block with an avatar.

                // Calculate the avatar's Y position. It should follow the original avatar
                // until it hits the top of the view, then "stick" there.
                const avatarY = Math.max(0, itemTopInView)

                // Update the sticky avatar's properties.
                stickyAvatar.source = modelData.senderAvatar
                stickyAvatar.y = avatarY
                stickyAvatar.visible = true

                // Tell the ListView which group's avatar is now being handled by the sticky one.
                messageListView.stickyAvatarIndex = i
                return
                // Exit, we've found our target for this frame.
            }
        }

        // If the loop completes, no relevant message group is visible. Hide the sticky avatar.
        stickyAvatar.visible = false
        messageListView.stickyAvatarIndex = -1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            id: chatViewWrapper
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollView {
                id: chatScrollView
                clip: true
                anchors.fill: parent

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
                        color: chatScrollbar.pressed ? Qt.darker("grey",
                                                                 1.5) : "grey"
                        opacity: chatScrollbar.policy === ScrollBar.AlwaysOn
                                 || (chatScrollbar.active
                                     && chatScrollbar.size < 1.0) ? 0.75 : 0
                    }
                }

                ListView {
                    id: messageListView
                    width: chatScrollView.width
                    model: messageGroupModel // Bind to the new group model
                    spacing: 12 // Spacing between different users' message groups

                    // *** PLATFORM-SPECIFIC CACHE BUFFER ***
                    // This sets the cacheBuffer dynamically based on the operating system.
                    // We use a larger buffer on desktop for maximum smoothness and a
                    // smaller, more memory-friendly buffer on mobile devices.
                    cacheBuffer: {
                        switch (Qt.platform.os) {
                        case "android":
                        case "ios":
                            return 800; // A good value for mobile
                        case "windows":
                        case "linux":
                        case "osx":
                            return 1600; // A generous value for desktop
                        default:
                            return 600; // A safe default
                        }
                    }

                    // Custom property to track which delegate's avatar is being replaced.
                    property int stickyAvatarIndex: -1

                    // Trigger the update function whenever the view is scrolled.
                    onContentYChanged: root.updateStickyAvatar()

                    // The delegate is now the MessageGroup wrapper.
                    // You must have a MessageGroup.qml file for this to work.
                    delegate: MessagesGroup {
                        width: messageListView.width
                        // Bind properties from the group model
                        senderName: model.senderName
                        senderAvatar: model.senderAvatar
                        fromMe: model.fromMe
                        messages: model.messages
                    }

                    Component.onCompleted: {
                        root.loadMessages()
                        positionViewAtEnd()
                        // Run the update once on startup to set the initial state.
                        Qt.callLater(root.updateStickyAvatar)
                    }

                    function positionViewAtEnd() {
                        // Use Qt.callLater to ensure the layout has been updated before scrolling.
                        Qt.callLater(() => {
                                         if (contentHeight > chatScrollView.height) {
                                             chatScrollView.ScrollBar.vertical.position = 1.0
                                         }
                                     })
                    }
                }
            }
            // --- The Sticky Avatar ---
            // This Image sits on top of the ScrollView.
            Image {
                id: stickyAvatar
                x: 6 // Align with the original avatar's horizontal position. note: be aware of leftmargin of avatar
                y: 0
                width: 32
                height: 32
                visible: false // Initially hidden.
                source: ""
                fillMode: Image.PreserveAspectCrop

                Rectangle {
                    // Circular mask
                    anchors.fill: parent
                    radius: 16
                    color: "transparent"
                }
            }
        }

        // --- Message Input Area ---
        MessageInputCanvas {
            id: messageInput
            Layout.preferredHeight: 28
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            Layout.rightMargin: 3
            Layout.leftMargin: 3
            onSendMessage: msgText => {
                               root.sendMessage(msgText)
                           }
        }
    }

    // --- Functions ---
    function sendMessage(msgText) {
        if (msgText.trim() === "")
            return

        const lastGroupIndex = messageGroupModel.count - 1

        // Check if the last message group is also from "Me"
        if (lastGroupIndex >= 0 && messageGroupModel.get(
                    lastGroupIndex).senderName === "Me") {
            // Add to the existing group
            const lastGroup = messageGroupModel.get(lastGroupIndex)
            lastGroup.messages.push({
                                        "text": msgText
                                    })
            // Notify the model that the property has changed so the Repeater updates
            messageGroupModel.setProperty(lastGroupIndex, "messages",
                                          lastGroup.messages)
        } else {
            // Create a brand new group for this message
            messageGroupModel.append({
                                         "senderName": "Me",
                                         "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME",
                                         "fromMe": true,
                                         "messages": [{
                                                 "text": msgText
                                             }]
                                     })
        }

        messageListView.positionViewAtEnd()
    }

    function loadMessages() {
        // --- NEW TEST DATA ---
        const flatMessageList = [// Start of conversation
                                 {
                                     "text": "Okay team, let's begin the final stress test. I want to see everything you've got.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "Ready when you are! I'll start with some basic messages.",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "This is the second message from me, just to check grouping.",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "Looks good on my end. John, how about some international flavor?",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, // Spanish
                                 {
                                     "text": "¡Por supuesto! ¿Qué tal un poco de español para empezar?",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "Esta es una frase un poco más larga para asegurar que el ajuste de línea funciona correctamente en otros idiomas.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, // French
                                 {
                                     "text": "Excellent. Mon tour ! Bonjour tout le monde.",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "J'ajoute une phrase avec des caractères spéciaux comme 'é', 'à', et 'ç'.",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, // German
                                 {
                                     "text": "Sehr gut. Jetzt auf Deutsch. Wie geht's?",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Alles klar, danke! Hier sind einige Umlaute: ä, ö, ü, und das scharfe S: ß.",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, // Very long message
                                 {
                                     "text": "Alright, my turn for a really long message. This is to rigorously test the word wrapping functionality across multiple lines and to see how the layout holds up under pressure. The bubble should expand vertically but not exceed its maximum horizontal width, which we've set to a percentage of the screen. The spacing between this message and the next one in the group should be minimal, while the space between this entire group and the next person's message group should be larger. Let's add even more text just to be absolutely sure that everything flows correctly and there are no visual glitches or overlaps. This is a critical test for the UI's robustness.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "The long message looks perfect. The wrapping is clean.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, // Emojis and Special Characters
                                 {
                                     "text": "Time for the fun stuff! Emojis! 🚀🎉😂❤️🔥😊👍",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "And a mix of special characters: !@#$%^&*()_+-=[]{}|;':\",./<>?`~",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "They seem to render correctly. 👍",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, // Japanese
                                 {
                                     "text": "日本の出番です！こんにちは！",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "これは日本語の長い文章です。改行が正しく機能するかどうかを確認します。",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "はい、大丈夫です。",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, // Russian
                                 {
                                     "text": "Теперь по-русски. Привет!",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Как дела? Это тест кириллицы.",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Отлично, спасибо!",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, // Unbreakable string test
                                 {
                                     "text": "UnbreakableStringTestAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, // Arabic
                                 {
                                     "text": "والآن، اللغة العربية. مرحبا بالعالم!",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "هذه رسالة طويلة جدا باللغة العربية لاختبار التفاف النص والتأكد من أن التخطيط من اليمين إلى اليسار يعمل بشكل صحيح.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "كل شيء يبدو رائعاً.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, // More mixed content
                                 {
                                     "text": "Let's try some mixed content: 123 Main St, Apt 4B. Call (555) 123-4567. Email: test@example.com",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "The quick brown fox 🦊 jumps over the lazy dog 🐶.",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, // Long block of short messages
                                 {
                                     "text": "Okay, rapid fire test.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "1",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "2",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "3",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "4",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "5",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "Stop spamming! 😂",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Just testing the performance with many small messages in a row.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "It seems to be holding up well.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, // Final messages
                                 {
                                     "text": "I think that covers most of the edge cases.",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "Agreed. The UI feels very solid now.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "Great work everyone! Test complete. 🎉",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "They seem to render correctly. 👍",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "日本の出番です！こんにちは！",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "これは日本語の長い文章です。改行が正しく機能するかどうかを確認します。",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "はい、大丈夫です。",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "UnbreakableStringTestAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 },
                                 // Conversation 2: Code Review
                                 {
                                     "text": "Hey Alex, can you look at this code snippet? I think there's a more efficient way to write this.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "function factorial(n) {\n  if (n === 0) {\n    return 1;\n  }\n  return n * factorial(n - 1);\n}",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "Sure. You could use a ternary operator to make it more concise.",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Like this: `const factorial = n => n ? n * factorial(n - 1) : 1;`",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Ah, much cleaner! Thanks. I'll update the PR.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "No problem! Happy to help. 👨‍💻",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 },
                                 // Conversation 3: Trip Planning
                                 {
                                     "text": "Okay, trip planning! Where are we thinking for the team offsite?",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "I was thinking maybe somewhere with mountains? 🏔️",
                                     "fromMe": false,
                                     "senderName": "JaneDoe",
                                     "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD"
                                 }, {
                                     "text": "Or the beach! 🏖️☀️",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "Let's make a list of pros and cons.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "Mountains:\n- Hiking\n- Fresh Air\n- Usually cheaper\n\nBeach:\n- Swimming\n- Relaxing\n- Good food",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }, {
                                     "text": "Good points. I'm leaning towards the mountains for team-building activities.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "I'm good with either, as long as there's good Wi-Fi! 😉",
                                     "fromMe": false,
                                     "senderName": "Alex",
                                     "senderAvatar": "https://placehold.co/40x40/F04747/FFFFFF?text=A"
                                 }, {
                                     "text": "Haha, of course. That's a top priority.",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 },
                                 // Conversation 4: Final check
                                 {
                                     "text": "I think that covers most of the edge cases.",
                                     "fromMe": false,
                                     "senderName": "Yuki",
                                     "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y"
                                 }, {
                                     "text": "Agreed. The UI feels very solid now.",
                                     "fromMe": false,
                                     "senderName": "JohnSmith",
                                     "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS"
                                 }, {
                                     "text": "Great work everyone! Test complete. 🎉",
                                     "fromMe": true,
                                     "senderName": "Me",
                                     "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME"
                                 }]

        const groupedMessages = groupMessages(flatMessageList)

        messageGroupModel.clear()
        for (const group of groupedMessages) {
            messageGroupModel.append(group)
        }
    }
    // --- Data Processing Function ---
    // This function takes a flat list of messages and groups them by sender.
    function groupMessages(flatMessageList) {
        if (flatMessageList.length === 0) {
            return []
        }

        const grouped = []
        let currentGroup = null

        for (const message of flatMessageList) {
            // If there's no current group or the sender changes, create a new group.
            if (!currentGroup
                    || currentGroup.senderName !== message.senderName) {
                // If a group was in progress, push it to the results.
                if (currentGroup) {
                    grouped.push(currentGroup)
                }
                // Start a new group.
                currentGroup = {
                    "senderName": message.senderName,
                    "senderAvatar": message.senderAvatar,
                    "fromMe": message.fromMe,
                    "messages": [] // This will hold the text of the messages
                }
            }
            // Add the current message's text to the current group.
            currentGroup.messages.push({
                                           "text": message.text
                                       })
        }

        // Don't forget to push the very last group after the loop finishes.
        if (currentGroup) {
            grouped.push(currentGroup)
        }

        return grouped
    }
}
