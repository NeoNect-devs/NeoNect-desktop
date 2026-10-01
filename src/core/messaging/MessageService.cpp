#include "MessageService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QUuid>

namespace NeoNect {
namespace Core {
namespace Messaging {

MessageService::MessageService(std::shared_ptr<IMessageStorage> storage, 
                               std::shared_ptr<Crypto::Session::SessionManager> sessionManager,
                               std::shared_ptr<IMessageQueue> offlineQueue)
    : m_storage(std::move(storage)), m_sessionManager(std::move(sessionManager)), m_offlineQueue(std::move(offlineQueue))
{
}

bool MessageService::sendMessage(Message& msg) {
    if (msg.state != MessageState::CREATED) return false;
    
    // Save to DB initially
    if (!m_storage->saveMessage(msg)) {
        return false;
    }

    // Attempt state transition
    if (!isValidTransition(msg.state, MessageState::ENCRYPTING)) return false;
    msg.state = MessageState::ENCRYPTING;
    if (!m_storage->updateMessageState(msg.messageId, msg.state)) {
        return false;
    }

    QJsonObject obj;
    obj["messageId"] = msg.messageId;
    obj["conversationId"] = msg.conversationId;
    obj["senderId"] = msg.senderId;
    obj["receiverId"] = msg.receiverId;
    obj["timestamp"] = msg.timestamp;
    obj["plaintext"] = msg.plaintext;
    QByteArray rawPlaintext = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    // We assume SessionManager internally handles the RelayService call via its m_sendCb.
    QString sessionId = "sess_" + msg.receiverId; 
    
    // If transport fails during SessionManager's m_sendCb, how do we know?
    // SessionManager::sendMessage returns ServiceResult.
    auto result = m_sessionManager->sendMessage(
        sessionId,
        rawPlaintext,
        msg.messageId,
        msg.receiverId,
        "default_device" // Do NOT implement multi-device, mock device id
    );

    bool queued = false;
    if (m_offlineQueue) {
        queued = m_offlineQueue->getEntry(msg.messageId).has_value();
    } else {
        queued = result.success;
    }

    if (!result.success || !queued) {
        // Encryption or transport failed.
        // Task: "If encryption fails: message is not marked SENT"
        // Task: "If transport fails: message becomes FAILED"
        // Both can be handled by moving to FAILED.
        msg.state = MessageState::FAILED;
        m_storage->updateMessageState(msg.messageId, msg.state);
        return false;
    }

    // Success
    msg.state = MessageState::SENT;
    m_storage->updateMessageState(msg.messageId, msg.state);
    return true;
}

void MessageService::receiveMessage(const QString& sessionId, const QByteArray& plaintext) {
    Message msg;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(plaintext, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return; // reject invalid JSON
    }
    
    QJsonObject obj = doc.object();
    msg.messageId = obj["messageId"].toString();
    msg.conversationId = obj["conversationId"].toString();
    msg.senderId = obj["senderId"].toString();
    msg.receiverId = obj["receiverId"].toString();
    msg.timestamp = obj["timestamp"].toVariant().toLongLong();
    msg.plaintext = obj["plaintext"].toString();
    
    if (msg.messageId.isEmpty() || msg.senderId.isEmpty()) {
        return; // reject missing mandatory fields
    }
    
    // Prevent spoofing by verifying senderId matches sessionId prefix
    if (!sessionId.startsWith(msg.senderId)) {
        return; // reject spoofed sender
    }
    
    msg.state = MessageState::DELIVERED;
    
    // Check if it already exists
    auto existing = m_storage->getMessage(msg.messageId);
    if (existing) {
        // Duplicate handling: return existing message state
        return;
    }
    
    // Save locally
    m_storage->saveMessage(msg);
    
    if (m_onMessageReceived) {
        m_onMessageReceived(msg);
    }
}

bool MessageService::updateDeliveryState(const QString& messageId, MessageState newState) {
    auto optMsg = m_storage->getMessage(messageId);
    if (!optMsg) return false;
    
    Message msg = optMsg.value();
    if (!isValidTransition(msg.state, newState)) {
        return false;
    }
    
    msg.state = newState;
    return m_storage->updateMessageState(messageId, newState);
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
