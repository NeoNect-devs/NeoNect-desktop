#pragma once
#include <QString>
#include <QByteArray>

namespace NeoNect {
namespace Core {
namespace Messaging {

enum class MessageState {
    CREATED,
    ENCRYPTING,
    SENT,
    DELIVERED,
    FAILED,
    READ
};

struct Message {
    QString messageId;
    qint64 serverId = 0;
    QString conversationId;
    QString senderId;
    QString senderDeviceId;
    QString senderDisplayName;
    QString receiverId;
    QString receiverDeviceId;
    qint64 timestamp = 0;
    QString plaintext;
    QString type = "text"; // plaintext availability only at local application boundary
    QString mediaUrl;
    QString fileName;
    qint64 fileSize = 0;
    int duration = 0;
    QByteArray waveform;
    int mediaWidth = 0;
    int mediaHeight = 0;
    QString errorText;
    MessageState state = MessageState::CREATED;
};

inline bool isValidTransition(MessageState from, MessageState to) {
    if (from == to) return true;
    switch(from) {
        case MessageState::CREATED: return to == MessageState::ENCRYPTING || to == MessageState::FAILED;
        case MessageState::ENCRYPTING: return to == MessageState::SENT || to == MessageState::FAILED;
        case MessageState::SENT: return to == MessageState::DELIVERED || to == MessageState::READ || to == MessageState::FAILED;
        case MessageState::DELIVERED: return to == MessageState::READ;
        case MessageState::FAILED: return to == MessageState::CREATED || to == MessageState::ENCRYPTING;
        case MessageState::READ: return false; // terminal
    }
    return false;
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
