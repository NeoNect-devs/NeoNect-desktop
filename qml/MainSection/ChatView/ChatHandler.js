// ChatHandler.js
.pragma library

// This function now returns a simple flat list of message objects.
// The grouping logic has been removed from the QML side.
function getFlatMessageList() {
    // This is your original test data, kept as a flat list.
    return [
        { "text": "Okay team, let's begin the final stress test.", "fromMe": true, "senderName": "Me", "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME" },
        { "text": "Ready when you are! I'll start with some basic messages.", "fromMe": false, "senderName": "JaneDoe", "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD" },
        { "text": "This is the second message from me, just to check grouping.", "fromMe": false, "senderName": "JaneDoe", "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD" },
        { "text": "Looks good on my end. John, how about some international flavor?", "fromMe": true, "senderName": "Me", "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME" },
        { "text": "¡Por supuesto! ¿Qué tal un poco de español para empezar?", "fromMe": false, "senderName": "JohnSmith", "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS" },
        { "text": "Esta es una frase un poco más larga para asegurar que el ajuste de línea funciona correctamente.", "fromMe": false, "senderName": "JohnSmith", "senderAvatar": "https://placehold.co/40x40/43B581/FFFFFF?text=JS" },
        { "text": "Excellent. Mon tour ! Bonjour tout le monde.", "fromMe": false, "senderName": "Yuki", "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y" },
        { "text": "J'ajoute une phrase avec des caractères spéciaux comme 'é', 'à', et 'ç'.", "fromMe": false, "senderName": "Yuki", "senderAvatar": "https://placehold.co/40x40/FAA61A/FFFFFF?text=Y" },
        { "text": "Alright, my turn for a really long message to test wrapping.", "fromMe": true, "senderName": "Me", "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME" },
        { "text": "The long message looks perfect.", "fromMe": true, "senderName": "Me", "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME" },
        { "text": "Time for the fun stuff! Emojis! 🚀🎉😂❤️🔥😊👍", "fromMe": false, "senderName": "JaneDoe", "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD" },
        { "text": "They seem to render correctly. 👍", "fromMe": false, "senderName": "JaneDoe", "senderAvatar": "https://placehold.co/40x40/7289DA/FFFFFF?text=JD" },
        { "text": "Great work everyone! Test complete. 🎉", "fromMe": true, "senderName": "Me", "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME" }
    ];
}

// --- REIMPLEMENTED sendMessage ---
// Appends a single message to the model. It also checks the previous message
// to update its `isLastInBlock` property correctly.
function sendMessage(msgText, messageModel) {
    if (msgText.trim() === "") return;

    const lastIndex = messageModel.count - 1;
    let isFirst = true;

    if (lastIndex >= 0) {
        const lastMessage = messageModel.get(lastIndex);
        // If the last message was also from me, this new one isn't the first in its block.
        if (lastMessage.fromMe) {
            isFirst = false;
            // The previous message is no longer the last in the block.
            lastMessage.isLastInBlock = false;
            messageModel.set(lastIndex, lastMessage);
        }
    }

    // Append the new message. It's always the last in its block initially.
    messageModel.append({
        "text": msgText,
        "fromMe": true,
        "senderName": "Me",
        "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME",
        "isFirstInBlock": isFirst,
        "isLastInBlock": true // A new message is always the last one.
    });
}

// --- REIMPLEMENTED loadMessages ---
// This function now processes the flat list and adds helper properties
// (`isFirstInBlock`, `isLastInBlock`) to each message object before adding it to the model.
function loadMessages(messageModel) {
    const flatMessageList = getFlatMessageList();
    messageModel.clear();

    for (let i = 0; i < flatMessageList.length; i++) {
        const currentMsg = flatMessageList[i];
        const prevMsg = i > 0 ? flatMessageList[i - 1] : null;
        const nextMsg = i < flatMessageList.length - 1 ? flatMessageList[i + 1] : null;

        // A message is the first in its block if it's the very first message
        // or if the sender is different from the previous message's sender.
        const isFirst = !prevMsg || prevMsg.senderName !== currentMsg.senderName;

        // A message is the last in its block if it's the very last message
        // or if the sender is different from the next message's sender.
        const isLast = !nextMsg || nextMsg.senderName !== currentMsg.senderName;

        messageModel.append({
            "text": currentMsg.text,
            "fromMe": currentMsg.fromMe,
            "senderName": currentMsg.senderName,
            "senderAvatar": currentMsg.senderAvatar,
            "isFirstInBlock": isFirst,
            "isLastInBlock": isLast
        });
    }
}
