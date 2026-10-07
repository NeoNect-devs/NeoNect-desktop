#include "MessageService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QUuid>

namespace NeoNect {
namespace Core {
namespace Messaging {

MessageService::MessageService(std::weak_ptr<IMessageStorage> storage,
                               std::shared_ptr<Crypto::Session::SessionManager> sessionManager,
                               std::weak_ptr<IMessageQueue> offlineQueue)
    : m_storage(std::move(storage)), m_sessionManager(std::move(sessionManager)), m_offlineQueue(std::move(offlineQueue))
{
}

QByteArray MessageService::serializePayload(const Message& msg) {
    QJsonObject obj;
    obj["messageId"] = msg.messageId;
    obj["conversationId"] = msg.conversationId;
    obj["senderId"] = msg.senderId;
    obj["receiverId"] = msg.receiverId;
    obj["timestamp"] = msg.timestamp;
    obj["plaintext"] = msg.plaintext;
    obj["type"] = msg.type;
    
    if (!msg.fileName.isEmpty()) obj["fileName"] = msg.fileName;
    if (msg.fileSize > 0) obj["fileSize"] = msg.fileSize;
    if (msg.duration > 0) obj["duration"] = msg.duration;
    if (msg.mediaWidth > 0) obj["mediaWidth"] = msg.mediaWidth;
    if (msg.mediaHeight > 0) obj["mediaHeight"] = msg.mediaHeight;
    if (!msg.waveform.isEmpty()) {
        obj["waveform"] = QString::fromLatin1(msg.waveform.toBase64());
    }

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}


bool MessageService::sendMessage(Message& msg) {
    auto storage = m_storage.lock();
    if (!storage) return false;
    auto offlineQueue = m_offlineQueue.lock();
    if (msg.state != MessageState::CREATED) return false;

    // Save to DB initially, or proceed if already saved by the legacy facade
    if (!storage->saveMessage(msg)) {
        auto existing = storage->getMessage(msg.messageId);
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
    if (!storage->updateMessageState(msg.messageId, msg.state)) {
        qDebug() << "[CoreMessageService] updateMessageState to ENCRYPTING failed!";
        return false;
    }

    QByteArray rawPlaintext = serializePayload(msg);

    QString msgId = msg.messageId;
    QString receiver = msg.receiverId;
    std::weak_ptr<MessageService> weakSelf = weak_from_this();

    if (!m_deviceResolverCb) {
        qDebug() << "[CoreMessageService] No device resolver callback available!";
        msg.state = MessageState::FAILED;
        storage->updateMessageState(msg.messageId, msg.state);
        return false;
    }

    m_deviceResolverCb(receiver, [weakSelf, msgId, receiver, rawPlaintext](std::optional<QString> targetDeviceOpt) {
        auto self = weakSelf.lock();
        if (!self) return;

        if (!targetDeviceOpt) {
            qDebug() << "[CoreMessageService] Failed to resolve target device for" << receiver;
            if (auto s = self->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
            return;
        }

        QString targetDevice = *targetDeviceOpt;
        QString sessionId = receiver + ":" + targetDevice;

        if (self->m_sessionManager->hasSession(sessionId)) {
            auto result = self->m_sessionManager->sendMessage(
                sessionId,
                rawPlaintext,
                msgId,
                receiver,
                targetDevice
            );

            bool queued = false;
            auto offlineQueue = self->m_offlineQueue.lock();
            if (offlineQueue) {
                queued = offlineQueue->getEntry(msgId).has_value();
            } else {
                queued = result.success;
            }

            if (!result.success || !queued) {
                if (auto s = self->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
                return;
            }

            if (auto s = self->m_storage.lock()) s->updateMessageState(msgId, MessageState::SENT);
        } else {
            if (!self->m_preKeyClaimCb) {
                qDebug() << "[CoreMessageService] No session and no prekey claim callback available!";
                if (auto s = self->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
                return;
            }

            self->m_preKeyClaimCb(receiver, targetDevice, [weakSelf, msgId, receiver, targetDevice, rawPlaintext, sessionId](std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                auto selfInner = weakSelf.lock();
                if (!selfInner) return;

                if (!bundle.has_value()) {
                    qDebug() << "[CoreMessageService] Failed to claim prekey bundle for" << receiver;
                    if (auto s = selfInner->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
                    return;
                }

                auto result = selfInner->m_sessionManager->createSession(
                    receiver,
                    targetDevice,
                    bundle.value(),
                    msgId,
                    rawPlaintext
                );

                bool queued = false;
                auto offlineQueue = selfInner->m_offlineQueue.lock();
                if (offlineQueue) {
                    queued = offlineQueue->getEntry(msgId).has_value();
                } else {
                    queued = result.success;
                }

                if (!result.success || !queued) {
                    qDebug() << "[MessageService] createSession failed! success:" << result.success << "msg:" << result.message << "queued:" << queued;
                    if (auto s = selfInner->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
                    return;
                }

                if (auto s = selfInner->m_storage.lock()) s->updateMessageState(msgId, MessageState::SENT);
            });
        }
    });

    return true;
}

void MessageService::receiveMessage(const QString& sessionId, const QByteArray& plaintext) {
    auto storage = m_storage.lock();
    if (!storage) return;
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
    if (obj.contains("type")) msg.type = obj["type"].toString();

    if (obj.contains("fileName")) {
        QString f = obj["fileName"].toString();
        if (f.length() <= 1024) msg.fileName = f;
    }
    if (obj.contains("fileSize")) {
        qint64 size = obj["fileSize"].toVariant().toLongLong();
        if (size > 0) msg.fileSize = size;
    }
    if (obj.contains("duration")) {
        qint64 duration = obj["duration"].toVariant().toLongLong();
        if (duration > 0) msg.duration = duration;
    }
    if (obj.contains("mediaWidth")) {
        int width = obj["mediaWidth"].toInt();
        if (width > 0 && width <= 32768) msg.mediaWidth = width;
    }
    if (obj.contains("mediaHeight")) {
        int height = obj["mediaHeight"].toInt();
        if (height > 0 && height <= 32768) msg.mediaHeight = height;
    }
    if (obj.contains("waveform")) {
        QString b64 = obj["waveform"].toString();
        if (b64.length() <= 65536) {
            auto options = QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors;
            auto result = QByteArray::fromBase64Encoding(b64.toLatin1(), options);
            if (result.decodingStatus == QByteArray::Base64DecodingStatus::Ok) {
                if (!result.decoded.isEmpty()) {
                    msg.waveform = result.decoded;
                }
            }
        }
    }

    if (msg.messageId.isEmpty() || msg.senderId.isEmpty()) {
        return; // reject missing mandatory fields
    }

    // Prevent spoofing by verifying senderId matches sessionId prefix
    if (!sessionId.startsWith(msg.senderId)) {
        return; // reject spoofed sender
    }

    msg.state = MessageState::DELIVERED;

    // Check if it already exists
    auto existing = storage->getMessage(msg.messageId);
    if (existing) {
        // Duplicate handling: return existing message state
        return;
    }

    // Save locally
    storage->saveMessage(msg);

    if (m_onMessageReceived) {
        m_onMessageReceived(msg);
    }
}

bool MessageService::updateDeliveryState(const QString& messageId, MessageState newState) {
    auto storage = m_storage.lock();
    if (!storage) return false;
    auto optMsg = storage->getMessage(messageId);
    if (!optMsg) return false;

    Message msg = optMsg.value();
    if (!isValidTransition(msg.state, newState)) {
        return false;
    }

    msg.state = newState;
    return storage->updateMessageState(messageId, newState);
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
