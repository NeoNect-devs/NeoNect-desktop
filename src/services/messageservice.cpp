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

void MessageService::loadConversation(const QString &conversationId) {
    m_repository->getMessagesAsync(conversationId, 50, 0, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
        QVariantList msgList;
        for (const auto& msg : messages) {
            msgList.append(domainToVariantMap(msg));
        }
        emit conversationLoaded(conversationId, msgList);
    });
}

void MessageService::handleIncomingMessage(const Domain::Message &msg) {
    m_repository->saveMessageAsync(msg, this, [this, msg](bool success) {
        if (success) {
            emit messageAdded(msg.conversationId, domainToVariantMap(msg));
        }
    });
}


void MessageService::handleIncomingMessages(const std::vector<Domain::Message> &msgs) {
    if (msgs.empty()) return;
    
    QVariantList vMsgs;
    for (const auto& msg : msgs) {
        QVariantMap msgMap;
        msgMap["id"] = msg.id;
        msgMap["serverId"] = msg.serverId;
        msgMap["conversationId"] = msg.conversationId;
        msgMap["senderId"] = msg.senderId;
        msgMap["type"] = msg.type;
        msgMap["text"] = msg.text;
        msgMap["mediaUrl"] = msg.mediaUrl;
        msgMap["fileName"] = msg.fileName;
        msgMap["fileSize"] = msg.fileSize;
        msgMap["duration"] = msg.duration;
        msgMap["waveform"] = msg.waveform;
        msgMap["status"] = static_cast<int>(msg.status);
        msgMap["errorText"] = msg.errorText;
        msgMap["timestamp"] = msg.timestamp;
        
        QMetaObject::invokeMethod(this, [this, msgMap, convId = msg.conversationId]() {
            emit messageAdded(convId, msgMap);
        }, Qt::QueuedConnection);
    }
    
    m_repository->saveMessagesAsync(msgs, this, [this](bool success) {
        if (!success) {
            qWarning() << "[MessageService] Failed to save batch of incoming messages";
        }
    });
}

void MessageService::handleMessageDeliveryStatus(const QString &messageId, bool success, const QString &errorText) {
    Domain::MessageStatus status = success ? Domain::MessageStatus::Sent : Domain::MessageStatus::Failed;
    m_repository->updateMessageStatusAsync(messageId, status, errorText, this, [this, messageId, success, errorText](bool dbSuccess) {
        if (dbSuccess) {
            QString statusStr = success ? "sent" : "failed";
            // We don't have conversationId easily here unless we query the DB, 
            // but the UI model can update by messageId globally.
            emit messageUpdated("", messageId, statusStr, errorText);
        }
    });
}

QVariantMap MessageService::domainToVariantMap(const Domain::Message &msg) const {
    QVariantMap map;
    map["id"] = msg.id;
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
    }
    
    map["errorText"] = msg.errorText;
    map["timestamp"] = msg.timestamp;
    return map;
}

}
}
