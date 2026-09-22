#include "messageservice.h"
#include <QUuid>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>
#include <QSet>

namespace NeoNect {
namespace Services {

static qint64 determineFileSize(const QString &mediaUrl, qint64 providedSize) {
    if (providedSize > 0) return providedSize;
    if (mediaUrl.isEmpty()) return 0;
    QString cleanPath = mediaUrl;
    QUrl url(mediaUrl);
    if (url.isLocalFile()) {
        cleanPath = url.toLocalFile();
    } else if (cleanPath.startsWith("file:///")) {
        cleanPath = cleanPath.mid(8);
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);
    }
#ifdef _WIN32
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath.at(2) == ':') {
        cleanPath = cleanPath.mid(1);
    }
#endif
    QFileInfo fi(cleanPath);
    return (fi.exists() && fi.isFile()) ? fi.size() : 0;
}

MessageService::MessageService(std::shared_ptr<Storage::IMessageRepository> repository, QObject* parent)
    : QObject(parent), m_repository(std::move(repository))
{
}

MessageService::~MessageService() {
    QSet<QTimer*> uniqueTimers;
    for (auto timer : m_activeTransfers) {
        if (timer) uniqueTimers.insert(timer);
    }
    m_activeTransfers.clear();
    for (auto timer : uniqueTimers) {
        timer->stop();
        delete timer;
    }
}

void MessageService::setCurrentUserId(const QString &userId) {
    if (m_currentUserId != userId) {
        m_outgoingMessages.clear();
        m_pendingMediaRequests.clear();
        m_receivedMediaRequests.clear();
    }
    m_currentUserId = userId;
}

void MessageService::cancelMediaTransfer(const QString &conversationId, const QString &messageId) {
    if (messageId.isEmpty()) return;

    if (m_activeTransfers.contains(messageId)) {
        QTimer* timer = m_activeTransfers.take(messageId);
        if (timer) {
            timer->stop();
            timer->deleteLater();
        }
    }

    QString convId = conversationId;
    if (convId.isEmpty() && m_outgoingMessages.contains(messageId)) {
        convId = m_outgoingMessages.value(messageId).conversationId;
    }

    if (m_outgoingMessages.contains(messageId)) {
        m_outgoingMessages[messageId].status = Domain::MessageStatus::Failed;
        m_outgoingMessages[messageId].errorText = "Transfer cancelled";
    }

    m_repository->updateMessageStatusAsync(messageId, Domain::MessageStatus::Failed, "Transfer cancelled", this, nullptr);
    emit messageUpdated(convId, messageId, "failed", "Transfer cancelled");
    emit mediaTransferProgress(convId, messageId, 0.0, 0, 0);
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
    msg.fileSize = determineFileSize(mediaUrl, fileSize);
    msg.duration = duration;
    
    QJsonArray waveArray;
    for (const QVariant &v : waveform) {
        waveArray.append(v.toInt());
    }
    msg.waveform = QJsonDocument(waveArray).toJson(QJsonDocument::Compact);
    
    bool isSavedMessages = (conversationId == "dms:saved-messages" || conversationId == "saved-messages");
    msg.status = (isSavedMessages && type == "text") ? Domain::MessageStatus::Sent : Domain::MessageStatus::Sending;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_outgoingMessages.insert(msg.id, msg);

    QVariantMap msgMap = domainToVariantMap(msg);
    emit messageAdded(msg.conversationId, msgMap);

    if (msg.type != "text") {
        qint64 total = msg.fileSize > 0 ? msg.fileSize : 2500000;
        emit mediaTransferProgress(msg.conversationId, msg.id, 0.05, static_cast<qint64>(total * 0.05), total);

        QTimer* timer = new QTimer(this);
        timer->setInterval(60);
        auto step = std::make_shared<int>(1);
        int totalSteps = 25;

        QString mId = msg.id;
        QString cId = msg.conversationId;

        m_activeTransfers.insert(mId, timer);

        connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total]() {
            (*step)++;
            qreal prog = qMin(1.0, static_cast<qreal>(*step) / totalSteps);
            qint64 bytes = static_cast<qint64>(prog * total);

            emit mediaTransferProgress(cId, mId, prog, bytes, total);

            if (prog >= 1.0) {
                timer->stop();
                m_activeTransfers.remove(mId);
                timer->deleteLater();

                if (m_outgoingMessages.contains(mId)) {
                    m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                }
                m_repository->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                emit messageUpdated(cId, mId, "sent", "");
            }
        });
        timer->start();
    }

    m_repository->saveMessageAsync(msg, this, [this, msg, isSavedMessages](bool success) {
        if (!success) {
            qWarning() << "[MessageService] Failed to save outgoing message locally.";
            if (m_activeTransfers.contains(msg.id)) {
                QTimer *t = m_activeTransfers.take(msg.id);
                if (t) {
                    t->stop();
                    t->deleteLater();
                }
            }
            emit messageUpdated(msg.conversationId, msg.id, "failed", "Local DB Error");
            return;
        }

        if (!isSavedMessages) {
            emit transmitMessage(msg);
        } else if (msg.type == "text") {
            emit messageUpdated(msg.conversationId, msg.id, "sent", "");
        }
    });
}

void MessageService::sendTyping(const QString &conversationId, bool isTyping) {
    if (m_isInvisible) return;
    if (conversationId.isEmpty() || !conversationId.startsWith("dms:") || conversationId == "dms:saved-messages" || conversationId == "saved-messages") return;
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
    if (m_isInvisible) return;
    if (conversationId.isEmpty() || !conversationId.startsWith("dms:") || conversationId == "dms:saved-messages" || conversationId == "saved-messages") return;
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

void MessageService::sendPresenceStatus(const QString &targetUser, const QString &status) {
    if (m_isInvisible && status != "offline") return;
    QString cleanTarget = targetUser.trimmed().toLower();
    if (cleanTarget.isEmpty() || cleanTarget == "saved-messages" || cleanTarget == "friends") return;
    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = cleanTarget.startsWith("dms:") ? cleanTarget : ("dms:" + cleanTarget);
    msg.senderId = m_currentUserId;
    msg.type = "presence_status";
    msg.text = status;
    msg.status = Domain::MessageStatus::Sent;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();
    emit transmitMessage(msg);
}

void MessageService::sendMediaRequest(const QString &conversationId, const QString &text, const QString &mediaType,
                                      const QString &mediaUrl, const QString &fileName, qint64 fileSize)
{
    bool isSavedMessages = (conversationId == "dms:saved-messages" || conversationId == "saved-messages");
    if (isSavedMessages) {
        sendMessage(conversationId, text, mediaType, mediaUrl, fileName, determineFileSize(mediaUrl, fileSize), 0, {});
        return;
    }

    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = "media_request";
    msg.text = text;
    msg.mediaUrl = mediaUrl;
    msg.fileName = fileName;
    msg.fileSize = determineFileSize(mediaUrl, fileSize);
    msg.errorText = mediaType;
    msg.status = Domain::MessageStatus::Pending;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_pendingMediaRequests.insert(msg.id, msg);
    m_outgoingMessages.insert(msg.id, msg);

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
    m_repository->getMessagesAsync(conversationId, 40, 0, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
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

void MessageService::loadMoreMessages(const QString &conversationId, qint64 beforeTimestamp, int limit) {
    if (conversationId.isEmpty() || beforeTimestamp <= 0) return;
    int fetchLimit = limit > 0 ? limit : 30;
    m_repository->getMessagesAsync(conversationId, fetchLimit, beforeTimestamp, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
        QVariantList msgList;
        for (const auto& msg : messages) {
            if (msg.type == "media_request" && msg.status == Domain::MessageStatus::Accepted) {
                m_repository->deleteMessageAsync(msg.id);
                continue;
            }
            msgList.append(domainToVariantMap(msg));
        }
        emit moreMessagesLoaded(conversationId, msgList);
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
        if (msg.type == "presence_status") {
            continue;
        }

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
                payloadMsg.fileSize = determineFileSize(payloadMsg.mediaUrl, orig.fileSize);
                payloadMsg.errorText = reqId; // Carry requestId to clear on receiver side
                payloadMsg.status = Domain::MessageStatus::Sending;
                payloadMsg.timestamp = QDateTime::currentMSecsSinceEpoch();

                m_outgoingMessages.insert(payloadMsg.id, payloadMsg);

                QVariantMap msgMap = domainToVariantMap(payloadMsg);
                emit messageAdded(payloadMsg.conversationId, msgMap);

                qint64 total = payloadMsg.fileSize > 0 ? payloadMsg.fileSize : 2500000;
                emit mediaTransferProgress(payloadMsg.conversationId, payloadMsg.id, 0.05, static_cast<qint64>(total * 0.05), total);
                if (!reqId.isEmpty()) {
                    emit mediaTransferProgress(payloadMsg.conversationId, reqId, 0.05, static_cast<qint64>(total * 0.05), total);
                }

                QTimer* timer = new QTimer(this);
                timer->setInterval(60);
                auto step = std::make_shared<int>(1);
                int totalSteps = 25;

                QString mId = payloadMsg.id;
                QString cId = payloadMsg.conversationId;

                m_activeTransfers.insert(mId, timer);

                connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, reqId, cId, total]() {
                    (*step)++;
                    qreal prog = qMin(1.0, static_cast<qreal>(*step) / totalSteps);
                    qint64 bytes = static_cast<qint64>(prog * total);

                    emit mediaTransferProgress(cId, mId, prog, bytes, total);
                    if (!reqId.isEmpty()) {
                        emit mediaTransferProgress(cId, reqId, prog, bytes, total);
                    }

                    if (prog >= 1.0) {
                        timer->stop();
                        m_activeTransfers.remove(mId);
                        timer->deleteLater();

                        if (m_outgoingMessages.contains(mId)) {
                            m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                        }
                        m_repository->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                        emit messageUpdated(cId, mId, "sent", "");
                        if (!reqId.isEmpty()) {
                            emit messageUpdated(cId, reqId, "sent", "");
                        }
                    }
                });
                timer->start();

                m_repository->saveMessageAsync(payloadMsg, this, [this, payloadMsg, reqId](bool success) {
                    if (!success) {
                        qWarning() << "[MessageService] Failed to save outgoing message locally.";
                        if (m_activeTransfers.contains(payloadMsg.id)) {
                            QTimer *t = m_activeTransfers.take(payloadMsg.id);
                            if (t) {
                                t->stop();
                                t->deleteLater();
                            }
                        }
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

        bool isIncomingMedia = (currentMsg.type != "text" && currentMsg.type != "media_request" && currentMsg.type != "media_decline" && currentMsg.type != "media_accept" && currentMsg.type != "sticker" && !currentMsg.type.startsWith("typing_") && currentMsg.type != "seen");
        if (isIncomingMedia) {
            currentMsg.status = Domain::MessageStatus::Sending;
        }

        msgsToSave.push_back(currentMsg);

        // When a message arrives from peer, clear their typing indicator
        emit peerTypingStatusChanged(currentMsg.conversationId, currentMsg.senderId, false);

        QVariantMap msgMap = domainToVariantMap(currentMsg);
        QMetaObject::invokeMethod(this, [this, msgMap, convId = currentMsg.conversationId]() {
            emit messageAdded(convId, msgMap);
        }, Qt::QueuedConnection);

        if (isIncomingMedia) {
            qint64 total = currentMsg.fileSize > 0 ? currentMsg.fileSize : 2500000;
            emit mediaTransferProgress(currentMsg.conversationId, currentMsg.id, 0.05, static_cast<qint64>(total * 0.05), total);

            QTimer* timer = new QTimer(this);
            timer->setInterval(75);
            auto step = std::make_shared<int>(1);
            int totalSteps = 16;

            QString mId = currentMsg.id;
            QString cId = currentMsg.conversationId;

            m_activeTransfers.insert(mId, timer);

            connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total]() {
                (*step)++;
                qreal prog = qMin(1.0, static_cast<qreal>(*step) / totalSteps);
                qint64 bytes = static_cast<qint64>(prog * total);

                emit mediaTransferProgress(cId, mId, prog, bytes, total);

                if (prog >= 1.0) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();

                    m_repository->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                    emit messageUpdated(cId, mId, "sent", "");
                }
            });
            timer->start();
        }
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
    if (!success) {
        if (m_activeTransfers.contains(messageId)) {
            QTimer *timer = m_activeTransfers.take(messageId);
            if (timer) {
                timer->stop();
                timer->deleteLater();
            }
        }
        Domain::MessageStatus status = Domain::MessageStatus::Failed;
        emit messageUpdated("", messageId, "failed", errorText);

        if (m_outgoingMessages.contains(messageId)) {
            m_outgoingMessages[messageId].status = status;
            m_outgoingMessages[messageId].errorText = errorText;
        }
        if (m_pendingMediaRequests.contains(messageId)) {
            m_pendingMediaRequests[messageId].status = status;
            m_pendingMediaRequests[messageId].errorText = errorText;
        }

        m_repository->updateMessageStatusAsync(messageId, status, errorText, this, [messageId](bool dbSuccess) {
            if (!dbSuccess) {
                qWarning() << "[MessageService] Failed to update message delivery status in DB for:" << messageId;
            }
        });
        return;
    }

    if (m_activeTransfers.contains(messageId)) {
        qDebug() << "[MessageService] Message delivery confirmed, but media transfer is actively in progress for:" << messageId;
        return;
    }

    Domain::MessageStatus status = Domain::MessageStatus::Sent;
    emit messageUpdated("", messageId, "sent", errorText);

    if (m_outgoingMessages.contains(messageId)) {
        m_outgoingMessages[messageId].status = status;
        m_outgoingMessages[messageId].errorText = errorText;
    }
    if (m_pendingMediaRequests.contains(messageId)) {
        m_pendingMediaRequests[messageId].status = status;
        m_pendingMediaRequests[messageId].errorText = errorText;
    }

    m_repository->updateMessageStatusAsync(messageId, status, errorText, this, [messageId](bool dbSuccess) {
        if (!dbSuccess) {
            qWarning() << "[MessageService] Failed to update message delivery status in DB for:" << messageId;
        }
    });
}

void MessageService::retryMessage(const QString &messageId) {
    if (messageId.isEmpty()) return;

    auto executeRetry = [this](Domain::Message msg) {
        if (msg.conversationId == "dms:saved-messages" || msg.conversationId == "saved-messages") {
            msg.status = Domain::MessageStatus::Sent;
            msg.errorText = "";
            m_outgoingMessages.insert(msg.id, msg);
            m_repository->updateMessageStatusAsync(msg.id, Domain::MessageStatus::Sent, "", this, nullptr);
            emit messageUpdated(msg.conversationId, msg.id, "sent", "");
            return;
        }

        msg.status = Domain::MessageStatus::Sending;
        msg.errorText = "";
        m_outgoingMessages.insert(msg.id, msg);

        emit messageUpdated(msg.conversationId, msg.id, "sending", "");
        m_repository->updateMessageStatusAsync(msg.id, Domain::MessageStatus::Sending, "", this, nullptr);

        if (msg.type != "text" && msg.type != "media_request") {
            qint64 total = msg.fileSize > 0 ? msg.fileSize : 2500000;
            emit mediaTransferProgress(msg.conversationId, msg.id, 0.05, static_cast<qint64>(total * 0.05), total);

            QTimer* timer = new QTimer(this);
            timer->setInterval(60);
            auto step = std::make_shared<int>(1);
            int totalSteps = 25;

            QString mId = msg.id;
            QString cId = msg.conversationId;

            m_activeTransfers.insert(mId, timer);

            connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total]() {
                (*step)++;
                qreal prog = qMin(1.0, static_cast<qreal>(*step) / totalSteps);
                qint64 bytes = static_cast<qint64>(prog * total);

                emit mediaTransferProgress(cId, mId, prog, bytes, total);

                if (prog >= 1.0) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();

                    if (m_outgoingMessages.contains(mId)) {
                        m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                    }
                    m_repository->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                    emit messageUpdated(cId, mId, "sent", "");
                }
            });
            timer->start();
        }

        if (msg.type == "media_request") {
            m_pendingMediaRequests.insert(msg.id, msg);
            emit transmitMessage(msg);
        } else {
            emit transmitMessage(msg);
        }
    };

    if (m_outgoingMessages.contains(messageId)) {
        executeRetry(m_outgoingMessages.value(messageId));
        return;
    }

    if (m_pendingMediaRequests.contains(messageId)) {
        executeRetry(m_pendingMediaRequests.value(messageId));
        return;
    }

    m_repository->getMessageByIdAsync(messageId, this, [executeRetry, messageId](const std::optional<Domain::Message>& optMsg) {
        if (optMsg.has_value()) {
            executeRetry(optMsg.value());
        } else {
            qWarning() << "[MessageService] retryMessage: Message not found in DB or cache for id:" << messageId;
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
    map["messageType"] = msg.type;
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

    if (msg.status == Domain::MessageStatus::Sending && msg.type != "text") {
        map["transferProgress"] = 0.05;
        map["transferBytes"] = static_cast<qint64>((msg.fileSize > 0 ? msg.fileSize : 2500000) * 0.05);
    } else {
        map["transferProgress"] = 1.0;
        map["transferBytes"] = msg.fileSize;
    }
    
    map["errorText"] = msg.errorText;
    map["timestamp"] = msg.timestamp;
    return map;
}

}
}
