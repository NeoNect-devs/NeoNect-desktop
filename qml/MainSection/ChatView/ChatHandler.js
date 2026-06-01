// ChatHandler.js
.pragma library

function getFlatMessageList() {
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

function sendMessage(msgText, messageModel) {
    if (!msgText || msgText.trim() === "") return;

    const lastIndex = messageModel.count - 1;
    let isFirst = true;

    if (lastIndex >= 0) {
        const lastMessage = messageModel.get(lastIndex);
        if (lastMessage && lastMessage.fromMe) {
            isFirst = false;
            lastMessage.isLastInBlock = false;
            messageModel.set(lastIndex, lastMessage);
        }
    }

    messageModel.append({
        "text": msgText.trim(),
        "fromMe": true,
        "senderName": "Me",
        "senderAvatar": "https://placehold.co/40x40/5865F2/FFFFFF?text=ME",
        "isFirstInBlock": isFirst,
        "isLastInBlock": true
    });
}

function loadMessages(messageModel) {
    const flatMessageList = getFlatMessageList();
    messageModel.clear();

    for (let i = 0; i < flatMessageList.length; i++) {
        const currentMsg = flatMessageList[i];
        const prevMsg = i > 0 ? flatMessageList[i - 1] : null;
        const nextMsg = i < flatMessageList.length - 1 ? flatMessageList[i + 1] : null;

        const isFirst = !prevMsg || prevMsg.senderName !== currentMsg.senderName;
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

function loadMembers(memberModel) {
    const messages = getFlatMessageList();
    const uniqueUsers = {};

    messages.forEach(msg => {
        if (!msg.fromMe && !uniqueUsers[msg.senderName]) {
            uniqueUsers[msg.senderName] = {
                name: msg.senderName,
                avatar: msg.senderAvatar
            };
        }
    });

    memberModel.clear();
    Object.values(uniqueUsers).forEach(user => {
        memberModel.append(user);
    });
}
