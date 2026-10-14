#include "MessageService.h"
#include <QCryptographicHash>
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
    : m_lifeToken(std::make_shared<MessageService*>(this)),
      m_storage(std::move(storage)),
      m_sessionManager(std::move(sessionManager)),
      m_offlineQueue(std::move(offlineQueue))
{
    if (m_sessionManager) {
        std::weak_ptr<MessageService*> weakSelf = m_lifeToken;
        m_sessionManager->setMessageCommitVerifier([weakSelf](const QString& sessionId, const QByteArray& plaintext) -> bool {
            if (auto locked = weakSelf.lock(); locked && *locked) {
                return (*locked)->persistIncomingMessage(sessionId, plaintext);
            }
            return true;
        });
    }
}

MessageService::~MessageService() {
    if (m_lifeToken) {
        *m_lifeToken = nullptr;
    }
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
    
    if (!msg.senderDisplayName.isEmpty()) obj["senderDisplayName"] = msg.senderDisplayName;
    if (!msg.errorText.isEmpty()) obj["errorText"] = msg.errorText;
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


void MessageService::dispatchWithEstablishedSession(const PendingOutboundMessage& item) {
    auto result = m_sessionManager->sendMessage(
        item.sessionId,
        item.rawPlaintext,
        item.msgId,
        item.receiver,
        item.targetDevice
    );

    bool queued = false;
    bool alreadyDeliveredToRelay = false;
    auto offlineQueue = m_offlineQueue.lock();
    if (offlineQueue) {
        auto entryOpt = offlineQueue->getEntry(item.msgId);
        queued = entryOpt.has_value();
        alreadyDeliveredToRelay = entryOpt.has_value() && entryOpt->state == QueueState::DELIVERED;
    } else {
        queued = result.success;
        alreadyDeliveredToRelay = result.success;
    }

    if (!result.success || !queued) {
        if (!item.isEphemeralControl) {
            if (auto s = m_storage.lock()) s->updateMessageState(item.msgId, MessageState::FAILED);
        }
        if (!item.msgId.contains('#') && m_onDeliveryStateChanged) {
            m_onDeliveryStateChanged(item.msgId, MessageState::FAILED, result.message);
        }
        return;
    }

    if (!alreadyDeliveredToRelay) {
        return;
    }

    bool stayedHealthy = true;
    if (!item.isEphemeralControl) {
        if (auto s = m_storage.lock()) {
            auto cur = s->getMessage(item.msgId);
            if (!cur || cur->state != MessageState::FAILED) {
                s->updateMessageState(item.msgId, MessageState::SENT);
            } else {
                stayedHealthy = false;
            }
        }
    }
    if (!item.msgId.contains('#') && stayedHealthy && m_onDeliveryStateChanged) {
        m_onDeliveryStateChanged(item.msgId, MessageState::SENT, QString());
    }
}

bool MessageService::sendMessage(Message& msg) {
    auto storage = m_storage.lock();
    if (!storage) return false;
    auto offlineQueue = m_offlineQueue.lock();
    if (msg.state != MessageState::CREATED) return false;

    const bool isEphemeralControl =
        (msg.type == "typing_start" || msg.type == "typing_stop" ||
         msg.type == "presence_status" || msg.type == "message_seen" ||
         msg.type == "avatar_update" || msg.type == "media_accept" ||
         msg.type == "media_decline" || msg.type == "file_start" ||
         msg.type == "file_ack" || msg.type == "file_nack" ||
         msg.type == "file_cancel");

    if (!isEphemeralControl) {
        if (!storage->saveMessage(msg)) {
            auto existing = storage->getMessage(msg.messageId);
            if (!existing) {
                qDebug() << "[CoreMessageService] No existing message found for ID:" << msg.messageId;
                return false;
            }
            if (existing->state != MessageState::CREATED && existing->state != MessageState::FAILED) {
                qDebug() << "[CoreMessageService] Existing message has state:" << static_cast<int>(existing->state);
                return false;
            }
        }

        if (!isValidTransition(msg.state, MessageState::ENCRYPTING)) {
            qDebug() << "[CoreMessageService] Invalid transition to ENCRYPTING from:" << static_cast<int>(msg.state);
            return false;
        }
        msg.state = MessageState::ENCRYPTING;
        if (!storage->updateMessageState(msg.messageId, msg.state)) {
            qDebug() << "[CoreMessageService] updateMessageState to ENCRYPTING failed!";
            return false;
        }
    } else {
        msg.state = MessageState::ENCRYPTING;
    }

    if (offlineQueue) {
        auto existingEntry = offlineQueue->getEntry(msg.messageId);
        if (existingEntry && existingEntry->state != QueueState::DELIVERED && !existingEntry->envelopeBytes.isEmpty()) {
            bool dispatched = m_sessionManager && m_sessionManager->dispatchEnvelope(
                existingEntry->recipientUsername,
                existingEntry->recipientDeviceId,
                existingEntry->messageId,
                existingEntry->envelopeBytes);
            if (!dispatched) {
                msg.state = MessageState::FAILED;
                if (!isEphemeralControl) storage->updateMessageState(msg.messageId, msg.state);
                if (m_onDeliveryStateChanged) m_onDeliveryStateChanged(msg.messageId, MessageState::FAILED, "Failed to re-dispatch queued envelope");
                return false;
            }
            auto updatedEntry = offlineQueue->getEntry(msg.messageId);
            if (updatedEntry && updatedEntry->state == QueueState::DELIVERED) {
                bool stayedHealthy = true;
                if (!isEphemeralControl) {
                    auto cur = storage->getMessage(msg.messageId);
                    if (!cur || cur->state != MessageState::FAILED) {
                        storage->updateMessageState(msg.messageId, MessageState::SENT);
                    } else {
                        stayedHealthy = false;
                    }
                }
                if (stayedHealthy && m_onDeliveryStateChanged) {
                    m_onDeliveryStateChanged(msg.messageId, MessageState::SENT, QString());
                }
            }
            return true;
        }
    }

    QByteArray rawPlaintext = serializePayload(msg);

    QString msgId = msg.messageId;
    QString receiver = msg.receiverId;
    QString explicitTargetDevice = msg.receiverDeviceId.trimmed();
    std::weak_ptr<MessageService> weakSelf = weak_from_this();

    auto dispatchTargetDeviceItem = [weakSelf](const PendingOutboundMessage& item) {
        auto self = weakSelf.lock();
        if (!self) return;

        if (self->m_sessionManager->hasSession(item.sessionId)) {
            self->dispatchWithEstablishedSession(item);
        } else {
            if (!self->m_preKeyClaimCb) {
                qDebug() << "[CoreMessageService] No session and no prekey claim callback available!";
                if (!item.isEphemeralControl) {
                    if (auto s = self->m_storage.lock()) s->updateMessageState(item.msgId, MessageState::FAILED);
                }
                if (!item.isEphemeralControl && self->m_onDeliveryStateChanged) {
                    self->m_onDeliveryStateChanged(item.msgId, MessageState::FAILED, "PreKey claim service unavailable");
                }
                return;
            }

            if (self->m_pendingSessionCreations.contains(item.sessionId)) {
                self->m_pendingSessionWaiters[item.sessionId].push_back(item);
                return;
            }

            self->m_pendingSessionCreations.insert(item.sessionId);

            self->m_preKeyClaimCb(item.receiver, item.targetDevice, [weakSelf, item](std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                auto selfInner = weakSelf.lock();
                if (!selfInner) return;

                selfInner->m_pendingSessionCreations.remove(item.sessionId);
                auto waiters = selfInner->m_pendingSessionWaiters.take(item.sessionId);

                auto failAll = [&](const QString& reason) {
                    if (!item.isEphemeralControl) {
                        if (auto s = selfInner->m_storage.lock()) s->updateMessageState(item.msgId, MessageState::FAILED);
                        if (selfInner->m_onDeliveryStateChanged) selfInner->m_onDeliveryStateChanged(item.msgId, MessageState::FAILED, reason);
                    }
                    for (const auto& w : waiters) {
                        if (!w.isEphemeralControl) {
                            if (auto s = selfInner->m_storage.lock()) s->updateMessageState(w.msgId, MessageState::FAILED);
                            if (selfInner->m_onDeliveryStateChanged) selfInner->m_onDeliveryStateChanged(w.msgId, MessageState::FAILED, reason);
                        }
                    }
                };

                if (selfInner->m_sessionManager->hasSession(item.sessionId)) {
                    selfInner->dispatchWithEstablishedSession(item);
                    for (const auto& w : waiters) {
                        selfInner->dispatchWithEstablishedSession(w);
                    }
                    return;
                }

                if (!bundle.has_value()) {
                    qDebug() << "[CoreMessageService] Failed to claim prekey bundle for" << item.receiver;
                    failAll("Failed to claim recipient prekey bundle");
                    return;
                }

                auto result = selfInner->m_sessionManager->createSession(
                    item.receiver,
                    item.targetDevice,
                    bundle.value(),
                    item.msgId,
                    item.rawPlaintext
                );

                bool queued = false;
                bool alreadyDeliveredToRelay = false;
                auto offlineQueue = selfInner->m_offlineQueue.lock();
                if (offlineQueue) {
                    auto entryOpt = offlineQueue->getEntry(item.msgId);
                    queued = entryOpt.has_value();
                    alreadyDeliveredToRelay = entryOpt.has_value() && entryOpt->state == QueueState::DELIVERED;
                } else {
                    queued = result.success;
                    alreadyDeliveredToRelay = result.success;
                }

                if (!result.success || !queued) {
                    qDebug() << "[MessageService] createSession failed! success:" << result.success << "msg:" << result.message << "queued:" << queued;
                    failAll(result.message);
                    return;
                }

                if (alreadyDeliveredToRelay) {
                    bool stayedHealthy = true;
                    if (!item.isEphemeralControl) {
                        if (auto s = selfInner->m_storage.lock()) {
                            auto cur = s->getMessage(item.msgId);
                            if (!cur || cur->state != MessageState::FAILED) {
                                s->updateMessageState(item.msgId, MessageState::SENT);
                            } else {
                                stayedHealthy = false;
                            }
                        }
                    }
                    if (!item.isEphemeralControl && stayedHealthy && selfInner->m_onDeliveryStateChanged) {
                        selfInner->m_onDeliveryStateChanged(item.msgId, MessageState::SENT, QString());
                    }
                }

                for (const auto& w : waiters) {
                    selfInner->dispatchWithEstablishedSession(w);
                }
            });
        }
    };

    auto handleTargetDevice = [weakSelf, msgId, receiver, rawPlaintext, isEphemeralControl, dispatchTargetDeviceItem](std::optional<QString> targetDeviceOpt) {
        auto self = weakSelf.lock();
        if (!self) return;

        if (!targetDeviceOpt || targetDeviceOpt->isEmpty()) {
            qDebug() << "[CoreMessageService] Failed to resolve target device for" << receiver;
            if (!isEphemeralControl) {
                if (auto s = self->m_storage.lock()) s->updateMessageState(msgId, MessageState::FAILED);
            }
            if (self->m_onDeliveryStateChanged) self->m_onDeliveryStateChanged(msgId, MessageState::FAILED, "Recipient has no active registered devices");
            return;
        }

        QString targetDevice = *targetDeviceOpt;
        QString sessionId = receiver + ":" + targetDevice;
        PendingOutboundMessage item{msgId, receiver, targetDevice, sessionId, rawPlaintext, isEphemeralControl};
        dispatchTargetDeviceItem(item);
    };

    if (!explicitTargetDevice.isEmpty() && explicitTargetDevice != "*") {
        handleTargetDevice(explicitTargetDevice);
        return true;
    }

    if (m_multiDeviceResolverCb) {
        m_multiDeviceResolverCb(receiver, [msgId, receiver, rawPlaintext, handleTargetDevice, dispatchTargetDeviceItem](QStringList devices) {
            QStringList uniqueDevices;
            for (const QString& d : devices) {
                const QString clean = d.trimmed();
                if (!clean.isEmpty() && !uniqueDevices.contains(clean)) {
                    uniqueDevices.append(clean);
                }
            }
            if (uniqueDevices.isEmpty()) {
                handleTargetDevice(std::nullopt);
                return;
            }
            handleTargetDevice(uniqueDevices.first());
            for (int i = 1; i < uniqueDevices.size(); ++i) {
                const QString& devId = uniqueDevices[i];
                const QString devHash = QString::fromLatin1(
                    QCryptographicHash::hash(devId.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
                const QString secMsgId = msgId + QStringLiteral("#") + devHash;
                const QString secSessionId = receiver + QStringLiteral(":") + devId;
                PendingOutboundMessage secItem{secMsgId, receiver, devId, secSessionId, rawPlaintext, true};
                dispatchTargetDeviceItem(secItem);
            }
        });
        return true;
    }

    if (!m_deviceResolverCb) {
        qDebug() << "[CoreMessageService] No device resolver callback available!";
        msg.state = MessageState::FAILED;
        if (!isEphemeralControl) storage->updateMessageState(msg.messageId, msg.state);
        if (m_onDeliveryStateChanged) m_onDeliveryStateChanged(msg.messageId, MessageState::FAILED, "No device resolver available");
        return false;
    }

    m_deviceResolverCb(receiver, handleTargetDevice);

    return true;
}

bool MessageService::parseAndValidateIncoming(const QString& sessionId, const QByteArray& plaintext, Message& msg, bool& outIsEphemeralControl) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(plaintext, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    msg.messageId = obj["messageId"].toString();
    msg.conversationId = obj["conversationId"].toString();
    msg.senderId = obj["senderId"].toString();
    msg.receiverId = obj["receiverId"].toString();
    msg.timestamp = obj["timestamp"].toVariant().toLongLong();
    msg.plaintext = obj["plaintext"].toString();
    if (obj.contains("type")) msg.type = obj["type"].toString();

    if (obj.contains("senderDisplayName")) {
        QString dn = obj["senderDisplayName"].toString().trimmed();
        if (dn.length() <= 256) msg.senderDisplayName = dn;
    }
    if (obj.contains("errorText")) {
        QString et = obj["errorText"].toString();
        if (et.length() <= 1024) msg.errorText = et;
    }
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
        return false;
    }

    if (sessionId.contains(':')) {
        QString boundSender = sessionId.section(':', 0, 0);
        if (boundSender.compare(msg.senderId, Qt::CaseInsensitive) != 0) {
            return false;
        }
        msg.senderDeviceId = sessionId.section(':', 1);
    } else {
        if (sessionId.compare(msg.senderId, Qt::CaseInsensitive) != 0) {
            return false;
        }
        msg.senderDeviceId = sessionId;
    }

    if (msg.conversationId.startsWith("dms:", Qt::CaseInsensitive) &&
        msg.conversationId.compare("dms:saved-messages", Qt::CaseInsensitive) != 0) {
        msg.conversationId = "dms:" + msg.senderId.trimmed().toLower();
    }

    msg.state = MessageState::DELIVERED;

    outIsEphemeralControl =
        (msg.type == "typing_start" || msg.type == "typing_stop" ||
         msg.type == "presence_status" || msg.type == "message_seen" ||
         msg.type == "avatar_update" || msg.type == "media_accept" ||
         msg.type == "media_decline" || msg.type == "file_start" ||
         msg.type == "file_ack" || msg.type == "file_nack" ||
         msg.type == "file_cancel");

    return true;
}

bool MessageService::persistIncomingMessage(const QString& sessionId, const QByteArray& plaintext) {
    Message msg;
    bool isEphemeralControl = false;
    if (!parseAndValidateIncoming(sessionId, plaintext, msg, isEphemeralControl)) {
        return true;
    }

    if (isEphemeralControl) {
        return true;
    }

    auto storage = m_storage.lock();
    if (!storage) {
        return false;
    }

    auto existing = storage->getMessage(msg.messageId);
    if (existing) {
        m_alreadyDeliveredIds.insert(msg.messageId);
        return true;
    }

    if (!storage->saveMessage(msg)) {
        return false;
    }

    m_freshlyPersistedIds.insert(msg.messageId);
    return true;
}

bool MessageService::receiveMessage(const QString& sessionId, const QByteArray& plaintext) {
    Message msg;
    bool isEphemeralControl = false;
    if (!parseAndValidateIncoming(sessionId, plaintext, msg, isEphemeralControl)) {
        return false;
    }

    if (!isEphemeralControl) {
        if (m_alreadyDeliveredIds.remove(msg.messageId)) {
            return true;
        }
        if (!m_freshlyPersistedIds.remove(msg.messageId)) {
            auto storage = m_storage.lock();
            if (!storage) {
                return false;
            }
            auto existing = storage->getMessage(msg.messageId);
            if (existing) {
                return true;
            }
            if (!storage->saveMessage(msg)) {
                return false;
            }
        }
    }

    if (m_onMessageReceived) {
        m_onMessageReceived(msg);
    }
    return true;
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
