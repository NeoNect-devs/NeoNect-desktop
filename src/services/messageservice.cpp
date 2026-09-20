#include "messageservice.h"
#include <QUuid>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace NeoNect {
namespace Services {

MessageService::MessageService(std::shared_ptr<Storage::IMessageRepository> repository, QObject* parent)
    : QObject(parent), m_repository(std::move(repository))
{
}

void MessageService::setCurrentUserId(const QString &userId) {
    m_currentUserId = userId;
}

void MessageService::sendMessage(const QString &conversationId, const QString &text, const QString &type, 
                                 const QString &mediaUrl, const QString &fileName, qint64 fileSize, 
                                 int duration, const QVariantList &waveform) 
{
    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = type;
    msg.text = text;
    msg.mediaUrl = mediaUrl;
    msg.fileName = fileName;
    msg.fileSize = fileSize;
    msg.duration = duration;
    
    QJsonArray waveArray;
    for (const QVariant &v : waveform) {
        waveArray.append(v.toInt());
    }
    msg.waveform = QJsonDocument(waveArray).toJson(QJsonDocument::Compact);
    
    msg.status = Domain::MessageStatus::Sending;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    QVariantMap msgMap = domainToVariantMap(msg);
    emit messageAdded(msg.conversationId, msgMap);

    m_repository->saveMessageAsync(msg, this, [this, msg, msgMap](bool success) {
        if (!success) {
            qWarning() << "[MessageService] Failed to save outgoing message locally.";
            emit messageUpdated(msg.conversationId, msg.id, "failed", "Local DB Error");
            return;
        }

        emit transmitMessage(msg);
    });
}

void MessageService::sendTyping(const QString &conversationId, bool isTyping) {
    if (conversationId.isEmpty() || !conversationId.startsWith("dms:")) return;
    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = isTyping ? "typing_start" : "typing_stop";
    msg.status = Domain::MessageStatus::Sent;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    emit transmitMessage(msg);
}

void MessageService::sendSeenReceipt(const QString &conversationId, const QString &messageId) {
    if (conversationId.isEmpty() || !conversationId.startsWith("dms:")) return;
    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = "message_seen";
    msg.text = messageId.isEmpty() ? "all" : messageId;
    msg.status = Domain::MessageStatus::Sent;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    emit transmitMessage(msg);
}

void MessageService::sendMediaRequest(const QString &conversationId, const QString &text, const QString &mediaType,
                                      const QString &mediaUrl, const QString &fileName, qint64 fileSize)
{
    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = "media_request";
    msg.text = text;
    msg.mediaUrl = mediaUrl;
    msg.fileName = fileName;
    msg.fileSize = fileSize;
    msg.errorText = mediaType;
    msg.status = Domain::MessageStatus::Pending;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_pendingMediaRequests.insert(msg.id, msg);

    QVariantMap msgMap = domainToVariantMap(msg);
    emit messageAdded(msg.conversationId, msgMap);

    m_repository->saveMessageAsync(msg, this, [this, msg](bool success) {
        if (!success) {
            qWarning() << "[MessageService] Failed to save media_request locally.";
            emit messageUpdated(msg.conversationId, msg.id, "failed", "Local DB Error");
            return;
        }
        emit transmitMessage(msg);
    });
}

void MessageService::acceptMediaRequest(const QString &conversationId, const QString &requestId) {
    QString convId = conversationId;
    if ((convId.isEmpty() || convId == ":") && m_receivedMediaRequests.contains(requestId)) {
        convId = m_receivedMediaRequests.value(requestId).conversationId;
    }
    if ((convId.isEmpty() || convId == ":") && m_pendingMediaRequests.contains(requestId)) {
        convId = m_pendingMediaRequests.value(requestId).conversationId;
    }

    m_repository->updateMessageStatusAsync(requestId, Domain::MessageStatus::Accepted, "", this, [this, convId, requestId](bool) {
        emit messageUpdated(convId, requestId, "accepted", "");
    });

    // Remove the accepted card from local chat and delete from DB
    emit messageRemoved(convId, requestId);
    m_repository->deleteMessageAsync(requestId);
    m_receivedMediaRequests.remove(requestId);

    Domain::Message acceptMsg;
    acceptMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    acceptMsg.conversationId = convId;
    acceptMsg.senderId = m_currentUserId;
    acceptMsg.type = "media_accept";
    acceptMsg.text = requestId;
    acceptMsg.status = Domain::MessageStatus::Sent;
    acceptMsg.timestamp = QDateTime::currentMSecsSinceEpoch();

    emit transmitMessage(acceptMsg);
}

void MessageService::declineMediaRequest(const QString &conversationId, const QString &requestId) {
    QString convId = conversationId;
    if ((convId.isEmpty() || convId == ":") && m_receivedMediaRequests.contains(requestId)) {
        convId = m_receivedMediaRequests.value(requestId).conversationId;
    }
    if ((convId.isEmpty() || convId == ":") && m_pendingMediaRequests.contains(requestId)) {
        convId = m_pendingMediaRequests.value(requestId).conversationId;
    }

    m_repository->updateMessageStatusAsync(requestId, Domain::MessageStatus::Declined, "", this, [this, convId, requestId](bool) {
        emit messageUpdated(convId, requestId, "declined", "");
    });

    Domain::Message declineMsg;
    declineMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    declineMsg.conversationId = convId;
    declineMsg.senderId = m_currentUserId;
    declineMsg.type = "media_decline";
    declineMsg.text = requestId;
    declineMsg.status = Domain::MessageStatus::Sent;
    declineMsg.timestamp = QDateTime::currentMSecsSinceEpoch();

    emit transmitMessage(declineMsg);
}

void MessageService::loadConversation(const QString &conversationId) {
    m_repository->getMessagesAsync(conversationId, 50, 0, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
        QVariantList msgList;
        for (const auto& msg : messages) {
            // Do not load old accepted media_requests from history
            if (msg.type == "media_request" && msg.status == Domain::MessageStatus::Accepted) {
                m_repository->deleteMessageAsync(msg.id);
                continue;
            }
            msgList.append(domainToVariantMap(msg));
        }
        emit conversationLoaded(conversationId, msgList);
    });
}

void MessageService::handleIncomingMessage(const Domain::Message &msg) {
    handleIncomingMessages({msg});
}

void MessageService::handleIncomingMessages(const std::vector<Domain::Message> &msgs) {
    if (msgs.empty()) return;
    
    std::vector<Domain::Message> msgsToSave;
    msgsToSave.reserve(msgs.size());

    for (const auto& msg : msgs) {
        if (msg.type == "typing_start" || msg.type == "typing_stop") {
            bool isTyping = (msg.type == "typing_start");
            emit peerTypingStatusChanged(msg.conversationId, msg.senderId, isTyping);
            continue;
        }

        if (msg.type == "message_seen") {
            QString targetId = msg.text.trimmed();
            QString convId = msg.conversationId;
            qDebug() << "[MessageService] Received message_seen receipt from" << msg.senderId << "for:" << targetId << "conv:" << convId;
            if (!targetId.isEmpty() && targetId != "all") {
                m_repository->updateMessageStatusAsync(targetId, Domain::MessageStatus::Seen, "", this, nullptr);
            }
            m_repository->markMessagesSeenAsync(convId, m_currentUserId, this, [this, convId, targetId](bool) {
                if (!targetId.isEmpty() && targetId != "all") {
                    emit messageUpdated(convId, targetId, "seen", "");
                }
                emit messageUpdated(convId, "all", "seen", "");
            });
            continue;
        }

        if (msg.type == "media_accept") {
            QString reqId = msg.text.trimmed();
            qDebug() << "[MessageService] Received media_accept for request:" << reqId;
            m_repository->updateMessageStatusAsync(reqId, Domain::MessageStatus::Accepted, "", this, [this, convId = msg.conversationId, reqId](bool) {
                emit messageUpdated(convId, reqId, "accepted", "");
            });

            emit messageRemoved(msg.conversationId, reqId);
            m_repository->deleteMessageAsync(reqId);

            if (m_pendingMediaRequests.contains(reqId)) {
                Domain::Message orig = m_pendingMediaRequests.take(reqId);
                emit messageRemoved(orig.conversationId, reqId);
                QString realType = orig.errorText.isEmpty() ? "file" : orig.errorText;
                qDebug() << "[MessageService] Auto-sending media payload:" << orig.fileName << "type:" << realType << "size:" << orig.fileSize;
                
                Domain::Message payloadMsg;
                payloadMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                payloadMsg.conversationId = orig.conversationId;
                payloadMsg.senderId = m_currentUserId;
                payloadMsg.type = realType;
                payloadMsg.text = orig.text;
                payloadMsg.mediaUrl = orig.mediaUrl;
                payloadMsg.fileName = orig.fileName;
                payloadMsg.fileSize = orig.fileSize;
                payloadMsg.errorText = reqId; // Carry requestId to clear on receiver side
                payloadMsg.status = Domain::MessageStatus::Sending;
                payloadMsg.timestamp = QDateTime::currentMSecsSinceEpoch();

                QVariantMap msgMap = domainToVariantMap(payloadMsg);
                emit messageAdded(payloadMsg.conversationId, msgMap);

                m_repository->saveMessageAsync(payloadMsg, this, [this, payloadMsg](bool success) {
                    if (!success) {
                        qWarning() << "[MessageService] Failed to save outgoing message locally.";
                        emit messageUpdated(payloadMsg.conversationId, payloadMsg.id, "failed", "Local DB Error");
                        return;
                    }
                    emit transmitMessage(payloadMsg);
                });
            }
            continue;
        }

        if (msg.type == "media_decline") {
            QString reqId = msg.text.trimmed();
            qDebug() << "[MessageService] Received media_decline for request:" << reqId;
            m_repository->updateMessageStatusAsync(reqId, Domain::MessageStatus::Declined, "", this, [this, convId = msg.conversationId, reqId](bool) {
                emit messageUpdated(convId, reqId, "declined", "");
            });
            m_pendingMediaRequests.remove(reqId);
            continue;
        }

        Domain::Message currentMsg = msg;
        if (currentMsg.type == "media_request") {
            currentMsg.status = Domain::MessageStatus::Pending;
            m_receivedMediaRequests.insert(currentMsg.id, currentMsg);
        } else if (!currentMsg.errorText.isEmpty()) {
            // Incoming transmitted payload referencing previous request
            QString origReqId = currentMsg.errorText;
            emit messageRemoved(currentMsg.conversationId, origReqId);
            m_repository->deleteMessageAsync(origReqId);
            m_receivedMediaRequests.remove(origReqId);
            currentMsg.errorText = "";
        }

        msgsToSave.push_back(currentMsg);

        // When a message arrives from peer, clear their typing indicator
        emit peerTypingStatusChanged(currentMsg.conversationId, currentMsg.senderId, false);

        QVariantMap msgMap = domainToVariantMap(currentMsg);
        QMetaObject::invokeMethod(this, [this, msgMap, convId = currentMsg.conversationId]() {
            emit messageAdded(convId, msgMap);
        }, Qt::QueuedConnection);
    }
    
    if (!msgsToSave.empty()) {
        m_repository->saveMessagesAsync(msgsToSave, this, [this](bool success) {
            if (!success) {
                qWarning() << "[MessageService] Failed to save batch of incoming messages";
            }
        });
    }
}

void MessageService::handleMessageDeliveryStatus(const QString &messageId, bool success, const QString &errorText) {
    Domain::MessageStatus status = success ? Domain::MessageStatus::Sent : Domain::MessageStatus::Failed;
    QString statusStr = success ? "sent" : "failed";
    emit messageUpdated("", messageId, statusStr, errorText);

    m_repository->updateMessageStatusAsync(messageId, status, errorText, this, [messageId](bool dbSuccess) {
        if (!dbSuccess) {
            qWarning() << "[MessageService] Failed to update message delivery status in DB for:" << messageId;
        }
    });
}

QVariantMap MessageService::domainToVariantMap(const Domain::Message &msg) const {
    QVariantMap map;
    map["id"] = msg.id;
    map["messageId"] = msg.id;
    map["conversationId"] = msg.conversationId;
    map["senderId"] = msg.senderId;
    map["fromMe"] = (msg.senderId.toLower() == m_currentUserId.toLower());
    map["type"] = msg.type;
    map["text"] = msg.text;
    map["mediaUrl"] = msg.mediaUrl;
    map["fileName"] = msg.fileName;
    map["fileSize"] = msg.fileSize;
    map["duration"] = msg.duration;
    
    QJsonArray waveArray = QJsonDocument::fromJson(msg.waveform).array();
    QVariantList waveList;
    for (const QJsonValue &v : waveArray) {
        waveList.append(v.toInt());
    }
    map["waveform"] = waveList;
    
    switch (msg.status) {
        case Domain::MessageStatus::Sending: map["status"] = "sending"; break;
        case Domain::MessageStatus::Sent: map["status"] = "sent"; break;
        case Domain::MessageStatus::Failed: map["status"] = "failed"; break;
        case Domain::MessageStatus::Seen: map["status"] = "seen"; break;
        case Domain::MessageStatus::Pending: map["status"] = "pending"; break;
        case Domain::MessageStatus::Accepted: map["status"] = "accepted"; break;
        case Domain::MessageStatus::Declined: map["status"] = "declined"; break;
    }
    
    map["errorText"] = msg.errorText;
    map["timestamp"] = msg.timestamp;
    return map;
}

}
}
