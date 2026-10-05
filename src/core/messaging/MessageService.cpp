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
    
    // Save to DB initially, or proceed if already saved by the legacy facade
    if (!m_storage->saveMessage(msg)) {
        auto existing = m_storage->getMessage(msg.messageId);
        if (!existing) {
            qDebug() << "[CoreMessageService] No existing message found for ID:" << msg.messageId;
            return false;
        }
        if (existing->state != MessageState::CREATED) {
            qDebug() << "[CoreMessageService] Existing message has state:" << static_cast<int>(existing->state);
            return false;
        }
    }

    // Attempt state transition
    if (!isValidTransition(msg.state, MessageState::ENCRYPTING)) {
        qDebug() << "[CoreMessageService] Invalid transition to ENCRYPTING from:" << static_cast<int>(msg.state);
        return false;
    }
    msg.state = MessageState::ENCRYPTING;
    if (!m_storage->updateMessageState(msg.messageId, msg.state)) {
        qDebug() << "[CoreMessageService] updateMessageState to ENCRYPTING failed!";
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

    QString sessionId = msg.receiverId + ":default_device";
    
    if (m_sessionManager->hasSession(sessionId)) {
        auto result = m_sessionManager->sendMessage(
            sessionId,
            rawPlaintext,
            msg.messageId,
            msg.receiverId,
            "default_device"
        );

        bool queued = false;
        if (m_offlineQueue) {
            queued = m_offlineQueue->getEntry(msg.messageId).has_value();
        } else {
            queued = result.success;
        }

        if (!result.success || !queued) {
            msg.state = MessageState::FAILED;
            m_storage->updateMessageState(msg.messageId, msg.state);
            return false;
        }

        msg.state = MessageState::SENT;
        m_storage->updateMessageState(msg.messageId, msg.state);
        return true;
    } else {
        if (!m_preKeyClaimCb) {
            qDebug() << "[CoreMessageService] No session and no prekey claim callback available!";
            msg.state = MessageState::FAILED;
            m_storage->updateMessageState(msg.messageId, msg.state);
            return false;
        }

        QString msgId = msg.messageId;
        QString receiver = msg.receiverId;

        // Async path
        std::weak_ptr<MessageService> weakSelf = weak_from_this();
        
        m_preKeyClaimCb(receiver, "default_device", [weakSelf, msgId, receiver, rawPlaintext, sessionId](std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
            auto self = weakSelf.lock();
            if (!self) return;

            if (!bundle.has_value()) {
                qDebug() << "[CoreMessageService] Failed to claim prekey bundle for" << receiver;
                self->m_storage->updateMessageState(msgId, MessageState::FAILED);
                return;
            }

            auto result = self->m_sessionManager->createSession(
                receiver,
                "default_device",
                bundle.value(),
                msgId,
                rawPlaintext
            );

            bool queued = false;
            if (self->m_offlineQueue) {
                queued = self->m_offlineQueue->getEntry(msgId).has_value();
            } else {
                queued = result.success;
            }

            if (!result.success || !queued) {
                qDebug() << "[MessageService] createSession failed! success:" << result.success << "msg:" << result.message << "queued:" << queued;
                self->m_storage->updateMessageState(msgId, MessageState::FAILED);
                return;
            }

            self->m_storage->updateMessageState(msgId, MessageState::SENT);
        });

        // We return true immediately, message stays in ENCRYPTING state
        return true;
    }
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
