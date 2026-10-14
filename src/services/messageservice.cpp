#include "../storage/StoragePathResolver.h"
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
#include <QImageReader>
#include <QFile>
#include <QDir>

namespace NeoNect {
namespace Services {

static QString resolveLocalMediaPath(const QString &rawUrl) {
    if (rawUrl.isEmpty()) return QString();
    QString cleanPath = rawUrl.trimmed();
    QUrl url(cleanPath);
    if (url.isLocalFile()) {
        cleanPath = url.toLocalFile();
    } else if (cleanPath.startsWith("file://")) {
        cleanPath = cleanPath.mid(7);
    }
#ifdef _WIN32
    if (cleanPath.startsWith("/") && cleanPath.length() >= 3 && cleanPath.at(2) == ':') {
        cleanPath = cleanPath.mid(1);
    }
#endif
    return cleanPath;
}

static qint64 determineFileSize(const QString &mediaUrl, qint64 providedSize) {
    if (providedSize > 0) return providedSize;
    const QString cleanPath = resolveLocalMediaPath(mediaUrl);
    if (cleanPath.isEmpty()) return 0;
    QFileInfo fi(cleanPath);
    return (fi.exists() && fi.isFile()) ? fi.size() : 0;
}

static QString resolveCompletedMediaUrl(const QString &type, const QString &id, const QString &path, const QString &serverUrl, const QString &userId) {
    if (type == "image") {
        if (!path.isEmpty() && QFile::exists(path) && !path.endsWith(".spool")) {
            if (!serverUrl.isEmpty() && !userId.isEmpty()) {
                QString imagesDir = NeoNect::Storage::StoragePathResolver::mediaImagesDirectory(serverUrl, userId);
                QDir().mkpath(imagesDir);
                QImageReader reader(path);
                QString ext = QString::fromUtf8(reader.format()).toLower();
                if (ext.isEmpty()) ext = QFileInfo(path).suffix().toLower();
                if (ext.isEmpty()) ext = "png";
                QString targetPath = imagesDir + "/" + id + "." + ext;
                if (path != targetPath) {
                    QFile::remove(targetPath);
                    if (QFile::rename(path, targetPath) || QFile::copy(path, targetPath)) {
                        if (QFile::exists(path) && path != targetPath) {
                            QFile::remove(path);
                        }
                    }
                }
            }
        }
        return id;
    }
    if (type == "voice" || type == "voice_note" || type == "audio") {
        QString destPath = path;
        if (!serverUrl.isEmpty() && !userId.isEmpty()) {
            QString voiceDir = NeoNect::Storage::StoragePathResolver::mediaVoiceDirectory(serverUrl, userId);
            QDir().mkpath(voiceDir);
            QString targetPath = voiceDir + "/" + id + ".wav";
            if (path != targetPath && QFile::exists(path)) {
                QFile::remove(targetPath);
                if (QFile::rename(path, targetPath) || QFile::copy(path, targetPath)) {
                    if (QFile::exists(path) && path != targetPath) {
                        QFile::remove(path);
                    }
                    destPath = targetPath;
                }
            } else if (QFile::exists(targetPath)) {
                destPath = targetPath;
            }
        }
        return QUrl::fromLocalFile(destPath).toString();
    }
    if (!path.isEmpty() && QFile::exists(path) && !path.endsWith(".spool") && !serverUrl.isEmpty() && !userId.isEmpty()) {
        QString attachDir = NeoNect::Storage::StoragePathResolver::mediaAttachmentsDirectory(serverUrl, userId);
        QDir().mkpath(attachDir);
        QString fn = QFileInfo(path).fileName();
        QString targetPath = attachDir + "/" + (fn.isEmpty() ? (id + ".final") : fn);
        if (path != targetPath) {
            QFile::remove(targetPath);
            if (QFile::rename(path, targetPath) || QFile::copy(path, targetPath)) {
                if (QFile::exists(path) && path != targetPath) {
                    QFile::remove(path);
                }
                return QUrl::fromLocalFile(targetPath).toString();
            }
        } else {
            return QUrl::fromLocalFile(targetPath).toString();
        }
    }
    return QUrl::fromLocalFile(path).toString();
}

MessageService::MessageService(std::weak_ptr<NeoNect::Core::Messaging::IMessageStorage> storage, QObject* parent)
    : QObject(parent), m_storage(std::move(storage))
{
    m_workerThread = new QThread(this);
    m_workerContext = new QObject();
    m_workerContext->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_workerContext, &QObject::deleteLater);
    m_workerThread->start();
}

MessageService::~MessageService() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
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

void MessageService::setFileTransferManager(std::shared_ptr<FileTransferManager> ftm) {
    m_fileTransferManager = ftm;
    if (!ftm) return;

    ftm->setIsImageTransferCb([this](const QString& transferId) -> bool {
        if (m_incomingMessages.contains(transferId)) {
            return m_incomingMessages.value(transferId).type == "image";
        }
        if (m_outgoingMessages.contains(transferId)) {
            return m_outgoingMessages.value(transferId).type == "image";
        }
        if (auto s = m_storage.lock()) {
            if (auto msgOpt = s->getMessage(transferId)) {
                return msgOpt->type == "image";
            }
        }
        return false;
    });

    connect(ftm.get(), &FileTransferManager::transferProgress, this, [this](const QString& tid, int pct) {
        if (m_completedTransfers.contains(tid) && pct < 100) {
            return;
        }
        QString cid;
        qint64 totalBytes = 0;
        if (m_outgoingMessages.contains(tid)) {
            const auto& m = m_outgoingMessages[tid];
            if (m.status == Domain::MessageStatus::Sent || m.status == Domain::MessageStatus::Seen || m.status == Domain::MessageStatus::Failed) {
                return;
            }
            cid = m.conversationId;
            totalBytes = m.fileSize;
        } else if (m_incomingMessages.contains(tid)) {
            const auto& m = m_incomingMessages[tid];
            if (m.status == Domain::MessageStatus::Sent || m.status == Domain::MessageStatus::Seen || m.status == Domain::MessageStatus::Failed) {
                return;
            }
            cid = m.conversationId;
            totalBytes = m.fileSize;
        } else if (auto s = m_storage.lock()) {
            if (auto cmOpt = s->getMessage(tid)) {
                cid = cmOpt->conversationId;
                totalBytes = cmOpt->fileSize;
            }
        }
        if (totalBytes <= 0 && m_fileTransferManager) {
            if (auto txOpt = m_fileTransferManager->getTransferInfo(tid)) {
                totalBytes = txOpt->file_size;
            }
        }
        if (totalBytes <= 0) {
            totalBytes = 100;
        }
        if (pct > 0 && m_activeTransfers.contains(tid)) {
            QTimer* timer = m_activeTransfers.take(tid);
            if (timer) {
                timer->stop();
                timer->deleteLater();
            }
        }
        qreal prog = std::clamp(pct / 100.0, 0.0, 1.0);
        qint64 bytes = static_cast<qint64>(prog * totalBytes);
        emit mediaTransferProgress(cid, tid, prog, bytes, totalBytes);
    });

    connect(ftm.get(), &FileTransferManager::transferCompleted, this, [this](const QString& tid, const QString& path) {
        if (m_activeTransfers.contains(tid)) {
            QTimer* timer = m_activeTransfers.take(tid);
            if (timer) {
                timer->stop();
                timer->deleteLater();
            }
        }
        if (m_outgoingMessages.contains(tid)) {
            m_outgoingMessages[tid].status = Domain::MessageStatus::Sent;
            m_outgoingMessages[tid].errorText = QStringLiteral("");
            QString cid = m_outgoingMessages[tid].conversationId;
            qint64 total = m_outgoingMessages[tid].fileSize > 0 ? m_outgoingMessages[tid].fileSize : 100;
            emit mediaTransferProgress(cid, tid, 1.0, total, total);
            this->updateMessageStatusAsync(tid, Domain::MessageStatus::Sent, "", this, nullptr);
            emit messageUpdated(cid, tid, "sent", "");
        } else {
            m_completedTransfers.insert(tid, path);
            QString cid;
            QString resolvedMediaUrl;
            if (auto s = m_storage.lock()) {
                if (auto cmOpt = s->getMessage(tid)) {
                    resolvedMediaUrl =
                        resolveCompletedMediaUrl(cmOpt->type, tid, path, m_serverUrl, m_currentUserId);
                    s->updateMessageMediaUrl(tid, resolvedMediaUrl);
                    s->updateMessageState(tid, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                    cid = cmOpt->conversationId;
                    Domain::Message dm;
                    dm.id = cmOpt->messageId;
                    dm.serverId = cmOpt->serverId;
                    dm.conversationId = cmOpt->conversationId;
                    dm.senderId = cmOpt->senderId;
                    dm.timestamp = cmOpt->timestamp;
                    dm.text = cmOpt->plaintext;
                    dm.type = cmOpt->type;
                    dm.mediaUrl = resolvedMediaUrl;
                    dm.fileName = cmOpt->fileName;
                    dm.fileSize = cmOpt->fileSize;
                    dm.duration = cmOpt->duration;
                    dm.waveform = cmOpt->waveform;
                    dm.mediaWidth = cmOpt->mediaWidth;
                    dm.mediaHeight = cmOpt->mediaHeight;
                    dm.errorText = QStringLiteral("");
                    dm.status = Domain::MessageStatus::Sent;
                    m_incomingMessages.insert(tid, dm);
                    qint64 total = dm.fileSize > 0 ? dm.fileSize : 100;
                    emit mediaTransferProgress(cid, tid, 1.0, total, total);
                }
            }
            if (resolvedMediaUrl.isEmpty() && m_incomingMessages.contains(tid)) {
                Domain::Message dm = m_incomingMessages.value(tid);
                resolvedMediaUrl = resolveCompletedMediaUrl(dm.type, tid, path, m_serverUrl, m_currentUserId);
                dm.mediaUrl = resolvedMediaUrl;
                dm.errorText = QStringLiteral("");
                dm.status = Domain::MessageStatus::Sent;
                m_incomingMessages.insert(tid, dm);
                cid = dm.conversationId;
                if (auto s = m_storage.lock()) {
                    s->updateMessageMediaUrl(tid, dm.mediaUrl);
                    s->updateMessageState(tid, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                }
                qint64 total = dm.fileSize > 0 ? dm.fileSize : 100;
                emit mediaTransferProgress(cid, tid, 1.0, total, total);
            }
            emit messageUpdated(cid, tid, "sent", "", resolvedMediaUrl);
        }
    });

    connect(ftm.get(), &FileTransferManager::transferFailed, this, [this](const QString& tid, const QString& err) {
        if (m_completedTransfers.contains(tid)) {
            return;
        }
        if (m_outgoingMessages.contains(tid) &&
            (m_outgoingMessages[tid].status == Domain::MessageStatus::Sent ||
             m_outgoingMessages[tid].status == Domain::MessageStatus::Seen)) {
            return;
        }
        if (m_incomingMessages.contains(tid) &&
            (m_incomingMessages[tid].status == Domain::MessageStatus::Sent ||
             m_incomingMessages[tid].status == Domain::MessageStatus::Seen)) {
            return;
        }
        if (m_activeTransfers.contains(tid)) {
            QTimer* timer = m_activeTransfers.take(tid);
            if (timer) {
                timer->stop();
                timer->deleteLater();
            }
        }
        if (m_outgoingMessages.contains(tid)) {
            m_outgoingMessages[tid].status = Domain::MessageStatus::Failed;
            m_outgoingMessages[tid].errorText = err;
            QString cid = m_outgoingMessages[tid].conversationId;
            this->updateMessageStatusAsync(tid, Domain::MessageStatus::Failed, err, this, nullptr);
            emit messageUpdated(cid, tid, "failed", err);
        } else {
            QString cid;
            if (m_incomingMessages.contains(tid)) {
                m_incomingMessages[tid].status = Domain::MessageStatus::Failed;
                m_incomingMessages[tid].errorText = err;
                cid = m_incomingMessages[tid].conversationId;
            }
            if (cid.isEmpty()) {
                if (auto s = m_storage.lock()) {
                    if (auto cmOpt = s->getMessage(tid)) {
                        cid = cmOpt->conversationId;
                    }
                }
            }
            this->updateMessageStatusAsync(tid, Domain::MessageStatus::Failed, err, this, nullptr);
            emit messageUpdated(cid, tid, "failed", err);
        }
    });
}

void MessageService::setCurrentUserId(const QString &userId) {
    if (m_currentUserId != userId) {
        m_outgoingMessages.clear();
        m_incomingMessages.clear();
        m_pendingMediaRequests.clear();
        m_receivedMediaRequests.clear();
    }
    m_currentUserId = userId;
}

void MessageService::startPeerTransfers(const QString &receiver, const QString &localPath, const QString &msgId) {
    if (!m_fileTransferManager) return;
    QString cleanReceiver = receiver;
    if (cleanReceiver.startsWith("dms:", Qt::CaseInsensitive)) {
        cleanReceiver = cleanReceiver.mid(4);
    }
    cleanReceiver = cleanReceiver.trimmed();
    if (m_multiDeviceResolver) {
        m_multiDeviceResolver(cleanReceiver, [this, cleanReceiver, localPath, msgId](QStringList deviceIds) {
            if (!m_fileTransferManager) return;
            if (deviceIds.isEmpty()) {
                if (m_deviceResolver) {
                    m_deviceResolver(cleanReceiver, [this, cleanReceiver, localPath, msgId](std::optional<QString> devOpt) {
                        if (m_fileTransferManager && devOpt.has_value() && !devOpt->isEmpty()) {
                            m_fileTransferManager->startTransfer(cleanReceiver, devOpt.value(), localPath, msgId);
                        }
                    });
                }
                return;
            }
            for (const QString &devId : deviceIds) {
                if (!devId.isEmpty()) {
                    m_fileTransferManager->startTransfer(cleanReceiver, devId, localPath, msgId);
                }
            }
        });
    } else if (m_deviceResolver) {
        m_deviceResolver(cleanReceiver, [this, cleanReceiver, localPath, msgId](std::optional<QString> devOpt) {
            if (m_fileTransferManager && devOpt.has_value() && !devOpt->isEmpty()) {
                m_fileTransferManager->startTransfer(cleanReceiver, devOpt.value(), localPath, msgId);
            }
        });
    } else {
        m_fileTransferManager->startTransfer(cleanReceiver, "*", localPath, msgId);
    }
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

    if (m_fileTransferManager) {
        m_fileTransferManager->cancelTransfer(messageId, "", "Transfer cancelled");
    }

    QString convId = conversationId;
    if (convId.isEmpty() && m_outgoingMessages.contains(messageId)) {
        convId = m_outgoingMessages.value(messageId).conversationId;
    }
    if (convId.isEmpty() && m_incomingMessages.contains(messageId)) {
        convId = m_incomingMessages.value(messageId).conversationId;
    }

    if (m_outgoingMessages.contains(messageId)) {
        m_outgoingMessages[messageId].status = Domain::MessageStatus::Failed;
        m_outgoingMessages[messageId].errorText = "Transfer cancelled";
    }
    if (m_incomingMessages.contains(messageId)) {
        m_incomingMessages[messageId].status = Domain::MessageStatus::Failed;
        m_incomingMessages[messageId].errorText = "Transfer cancelled";
    }

    this->updateMessageStatusAsync(messageId, Domain::MessageStatus::Failed, "Transfer cancelled", this, nullptr);
    emit messageUpdated(convId, messageId, "failed", "Transfer cancelled");
    emit mediaTransferProgress(convId, messageId, 0.0, 0, 0);
}

void MessageService::sendMessage(const QString &conversationId, const QString &text, const QString &type, 
                                 const QString &mediaUrl, const QString &fileName, qint64 fileSize, 
                                 int duration, const QVariantList &waveform, const QString &clientMessageId) 
{
    Domain::Message msg;
    msg.id = clientMessageId.trimmed().isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : clientMessageId.trimmed();
    msg.conversationId = conversationId;
    msg.senderId = m_currentUserId;
    msg.type = type;
    msg.text = text;
    msg.mediaUrl = mediaUrl;
    msg.fileName = fileName;
    msg.fileSize = determineFileSize(mediaUrl, fileSize);
    msg.duration = duration;

    if ((msg.type == "image" || type == "image") && !mediaUrl.isEmpty()) {
        const QString cleanPath = resolveLocalMediaPath(mediaUrl);
        if (!cleanPath.isEmpty() && QFile::exists(cleanPath)) {
            QImageReader reader(cleanPath);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                msg.mediaWidth = sz.width();
                msg.mediaHeight = sz.height();
            }
        }
    }
    
    QJsonArray waveArray;
    for (const QVariant &v : waveform) {
        double val = v.toDouble();
        if (val > 1.0) {
            val /= 100.0;
        }
        waveArray.append(val);
    }
    msg.waveform = QJsonDocument(waveArray).toJson(QJsonDocument::Compact);
    
    bool isSavedMessages = (conversationId == "dms:saved-messages" || conversationId == "saved-messages");
    if (type == "avatar_update") {
        msg.status = Domain::MessageStatus::Sent;
        msg.timestamp = QDateTime::currentMSecsSinceEpoch();
        if (!isSavedMessages) {
            emit transmitMessage(msg);
        }
        return;
    }
    msg.status = (isSavedMessages && type == "text") ? Domain::MessageStatus::Sent : Domain::MessageStatus::Sending;
    msg.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_outgoingMessages.insert(msg.id, msg);

    QVariantMap msgMap = domainToVariantMap(msg);
    emit messageAdded(msg.conversationId, msgMap);

    const QString localTransferPath = resolveLocalMediaPath(msg.mediaUrl);
    const bool hasRealFileTransfer =
        (!isSavedMessages && m_fileTransferManager != nullptr &&
         (msg.type == "image" || msg.type == "file" ||
          msg.type == "video" || msg.type == "audio" ||
          msg.type == "voice" || msg.type == "voice_note") &&
         !localTransferPath.isEmpty() && QFile::exists(localTransferPath));

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

        connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total, hasRealFileTransfer]() {
            if (!m_activeTransfers.contains(mId) || (m_outgoingMessages.contains(mId) && m_outgoingMessages[mId].status != Domain::MessageStatus::Sending)) {
                timer->stop();
                m_activeTransfers.remove(mId);
                timer->deleteLater();
                return;
            }
            (*step)++;
            const qreal maxSimulatedProg = hasRealFileTransfer ? 0.90 : 1.0;
            qreal prog = qMin(maxSimulatedProg, static_cast<qreal>(*step) / totalSteps);
            qint64 bytes = static_cast<qint64>(prog * total);

            emit mediaTransferProgress(cId, mId, prog, bytes, total);

            if (!hasRealFileTransfer && prog >= 1.0) {
                timer->stop();
                m_activeTransfers.remove(mId);
                timer->deleteLater();

                if (m_outgoingMessages.contains(mId)) {
                    m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                }
                this->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                emit messageUpdated(cId, mId, "sent", "");
            }
        });
        timer->start();
    }

    this->saveMessageAsync(msg, this, [this, msg, isSavedMessages, localTransferPath, hasRealFileTransfer](bool success) {
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
            if (hasRealFileTransfer && m_fileTransferManager) {
                startPeerTransfers(msg.conversationId, localTransferPath, msg.id);
            }
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
                                      const QString &mediaUrl, const QString &fileName, qint64 fileSize,
                                      const QString &clientMessageId)
{
    bool isSavedMessages = (conversationId == "dms:saved-messages" || conversationId == "saved-messages");
    if (isSavedMessages) {
        sendMessage(conversationId, text, mediaType, mediaUrl, fileName, determineFileSize(mediaUrl, fileSize), 0, {}, clientMessageId);
        return;
    }

    Domain::Message msg;
    msg.id = clientMessageId.trimmed().isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : clientMessageId.trimmed();
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

    if (mediaType == "image" && !mediaUrl.isEmpty()) {
        const QString cleanPath = resolveLocalMediaPath(mediaUrl);
        if (!cleanPath.isEmpty() && QFile::exists(cleanPath)) {
            QImageReader reader(cleanPath);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                msg.mediaWidth = sz.width();
                msg.mediaHeight = sz.height();
            }
        }
    }

    m_pendingMediaRequests.insert(msg.id, msg);
    m_outgoingMessages.insert(msg.id, msg);

    QVariantMap msgMap = domainToVariantMap(msg);
    emit messageAdded(msg.conversationId, msgMap);

    this->saveMessageAsync(msg, this, [this, msg](bool success) {
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

    this->updateMessageStatusAsync(requestId, Domain::MessageStatus::Accepted, "", this, [this, convId, requestId](bool) {
        emit messageUpdated(convId, requestId, "accepted", "");
    });

    // Remove the accepted card from local chat and delete from DB
    emit messageRemoved(convId, requestId);
    this->deleteMessageAsync(requestId);
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

    this->updateMessageStatusAsync(requestId, Domain::MessageStatus::Declined, "", this, [this, convId, requestId](bool) {
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
    this->getMessagesAsync(conversationId, 40, 0, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
        QVariantList msgList;
        for (const auto& msg : messages) {
            // Do not load old accepted media_requests from history
            if (msg.type == "media_request" && msg.status == Domain::MessageStatus::Accepted) {
                this->deleteMessageAsync(msg.id);
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
    this->getMessagesAsync(conversationId, fetchLimit, beforeTimestamp, this, [this, conversationId](const std::vector<Domain::Message>& messages) {
        QVariantList msgList;
        for (const auto& msg : messages) {
            if (msg.type == "media_request" && msg.status == Domain::MessageStatus::Accepted) {
                this->deleteMessageAsync(msg.id);
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

        if (msg.type == "avatar_update") {
            QString avatarBase64 = msg.text.trimmed();
            QString safeSender = msg.senderId.trimmed().toLower();
            if (!avatarBase64.isEmpty() && !safeSender.isEmpty() && safeSender != "anonymous") {
                QByteArray avBytes = QByteArray::fromBase64(avatarBase64.toLatin1());
                if (!avBytes.isEmpty() && avBytes.size() <= 1024 * 1024) {
                    emit peerAvatarDataReceived(safeSender, avBytes);
                    qDebug() << "[MessageService] Forwarding incoming peer avatar data for:" << safeSender;
                }
            }
            continue;
        }

        if (msg.type == "message_seen") {
            QString targetId = msg.text.trimmed();
            QString convId = msg.conversationId;
            qDebug() << "[MessageService] Received message_seen receipt from" << msg.senderId << "for:" << targetId << "conv:" << convId;
            if (!targetId.isEmpty() && targetId != "all") {
                this->updateMessageStatusAsync(targetId, Domain::MessageStatus::Seen, "", this, nullptr);
            }
            this->markMessagesSeenAsync(convId, m_currentUserId, this, [this, convId, targetId](bool) {
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
            this->updateMessageStatusAsync(reqId, Domain::MessageStatus::Accepted, "", this, [this, convId = msg.conversationId, reqId](bool) {
                emit messageUpdated(convId, reqId, "accepted", "");
            });

            emit messageRemoved(msg.conversationId, reqId);
            this->deleteMessageAsync(reqId);

            if (m_pendingMediaRequests.contains(reqId)) {
                Domain::Message orig = m_pendingMediaRequests.take(reqId);
                emit messageRemoved(orig.conversationId, reqId);
                QString realType = orig.errorText.isEmpty() ? "file" : orig.errorText;
                qDebug() << "[MessageService] Auto-sending media payload type:" << realType << "size:" << orig.fileSize;
                
                Domain::Message payloadMsg;
                payloadMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                payloadMsg.conversationId = orig.conversationId;
                payloadMsg.senderId = m_currentUserId;
                payloadMsg.type = realType;
                payloadMsg.text = orig.text;
                payloadMsg.mediaUrl = orig.mediaUrl;
                payloadMsg.fileName = orig.fileName;
                payloadMsg.fileSize = determineFileSize(payloadMsg.mediaUrl, orig.fileSize);
                payloadMsg.mediaWidth = orig.mediaWidth;
                payloadMsg.mediaHeight = orig.mediaHeight;
                payloadMsg.errorText = reqId; // Carry requestId to clear on receiver side
                payloadMsg.status = Domain::MessageStatus::Sending;
                payloadMsg.timestamp = QDateTime::currentMSecsSinceEpoch();

                const QString cleanPath = resolveLocalMediaPath(payloadMsg.mediaUrl);
                if (payloadMsg.type == "image" && payloadMsg.mediaWidth <= 0 && !cleanPath.isEmpty() && QFile::exists(cleanPath)) {
                    QImageReader reader(cleanPath);
                    QSize sz = reader.size();
                    if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                        payloadMsg.mediaWidth = sz.width();
                        payloadMsg.mediaHeight = sz.height();
                    }
                }

                m_outgoingMessages.insert(payloadMsg.id, payloadMsg);

                QVariantMap msgMap = domainToVariantMap(payloadMsg);
                emit messageAdded(payloadMsg.conversationId, msgMap);

                const bool hasRealFileTransfer =
                    (m_fileTransferManager != nullptr &&
                     (payloadMsg.type == "image" || payloadMsg.type == "file" ||
                      payloadMsg.type == "video" || payloadMsg.type == "audio") &&
                     !cleanPath.isEmpty() && QFile::exists(cleanPath));

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

                connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, reqId, cId, total, hasRealFileTransfer]() {
                    if (!m_activeTransfers.contains(mId) || (m_outgoingMessages.contains(mId) && m_outgoingMessages[mId].status != Domain::MessageStatus::Sending)) {
                        timer->stop();
                        m_activeTransfers.remove(mId);
                        timer->deleteLater();
                        return;
                    }
                    (*step)++;
                    const qreal maxSimulatedProg = hasRealFileTransfer ? 0.90 : 1.0;
                    qreal prog = qMin(maxSimulatedProg, static_cast<qreal>(*step) / totalSteps);
                    qint64 bytes = static_cast<qint64>(prog * total);

                    emit mediaTransferProgress(cId, mId, prog, bytes, total);
                    if (!reqId.isEmpty()) {
                        emit mediaTransferProgress(cId, reqId, prog, bytes, total);
                    }

                    if (!hasRealFileTransfer && prog >= 1.0) {
                        timer->stop();
                        m_activeTransfers.remove(mId);
                        timer->deleteLater();

                        if (m_outgoingMessages.contains(mId)) {
                            m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                        }
                        this->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                        emit messageUpdated(cId, mId, "sent", "");
                        if (!reqId.isEmpty()) {
                            emit messageUpdated(cId, reqId, "sent", "");
                        }
                    }
                });
                timer->start();

                this->saveMessageAsync(payloadMsg, this, [this, payloadMsg, reqId, cleanPath, hasRealFileTransfer](bool success) {
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

                    if (hasRealFileTransfer && m_fileTransferManager) {
                        startPeerTransfers(payloadMsg.conversationId, cleanPath, payloadMsg.id);
                    }
                });
            }
            continue;
        }

        if (msg.type == "media_decline") {
            QString reqId = msg.text.trimmed();
            qDebug() << "[MessageService] Received media_decline for request:" << reqId;
            this->updateMessageStatusAsync(reqId, Domain::MessageStatus::Declined, "", this, [this, convId = msg.conversationId, reqId](bool) {
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
            QString origReqId = currentMsg.errorText;
            emit messageRemoved(currentMsg.conversationId, origReqId);
            this->deleteMessageAsync(origReqId);
            m_receivedMediaRequests.remove(origReqId);
            currentMsg.errorText = "";
            if (auto s = m_storage.lock()) {
                s->updateMessageState(currentMsg.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
            }
        }

        bool isIncomingMedia = (currentMsg.type != "text" && currentMsg.type != "media_request" && currentMsg.type != "media_decline" && currentMsg.type != "media_accept" && currentMsg.type != "sticker" && !currentMsg.type.startsWith("typing_") && currentMsg.type != "seen");
        if (isIncomingMedia) {
            QString completedPath;
            bool isAlreadyCompleted = false;
            if (m_completedTransfers.contains(currentMsg.id)) {
                completedPath = m_completedTransfers.value(currentMsg.id);
                isAlreadyCompleted = true;
            } else if (m_fileTransferManager) {
                if (auto txOpt = m_fileTransferManager->getTransferInfo(currentMsg.id)) {
                    if (txOpt->status == "completed" && !txOpt->spool_path.isEmpty() && QFile::exists(txOpt->spool_path)) {
                        completedPath = txOpt->spool_path;
                        m_completedTransfers.insert(currentMsg.id, completedPath);
                        isAlreadyCompleted = true;
                    }
                }
            }

            if (isAlreadyCompleted) {
                currentMsg.mediaUrl =
                    resolveCompletedMediaUrl(currentMsg.type, currentMsg.id, completedPath, m_serverUrl, m_currentUserId);
                currentMsg.status = Domain::MessageStatus::Sent;
                if (auto s = m_storage.lock()) {
                    s->updateMessageMediaUrl(currentMsg.id, currentMsg.mediaUrl);
                    s->updateMessageState(currentMsg.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                }
            } else {
                currentMsg.status = Domain::MessageStatus::Downloading;
                currentMsg.mediaUrl = QString();
            }
            m_incomingMessages.insert(currentMsg.id, currentMsg);
        }

        msgsToSave.push_back(currentMsg);

        emit peerTypingStatusChanged(currentMsg.conversationId, currentMsg.senderId, false);

        QVariantMap msgMap = domainToVariantMap(currentMsg);
        emit messageAdded(currentMsg.conversationId, msgMap);

        if (isIncomingMedia && currentMsg.status == Domain::MessageStatus::Downloading) {
            qint64 total = currentMsg.fileSize > 0 ? currentMsg.fileSize : 2500000;
            emit mediaTransferProgress(currentMsg.conversationId, currentMsg.id, 0.05, static_cast<qint64>(total * 0.05), total);

            QTimer* timer = new QTimer(this);
            timer->setInterval(75);
            auto step = std::make_shared<int>(1);
            int totalSteps = 16;

            QString mId = currentMsg.id;
            QString cId = currentMsg.conversationId;
            const bool waitForTransferCompletion = (m_fileTransferManager != nullptr);

            m_activeTransfers.insert(mId, timer);

            connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total, waitForTransferCompletion]() {
                if (!m_activeTransfers.contains(mId) || m_completedTransfers.contains(mId) ||
                    (m_incomingMessages.contains(mId) && m_incomingMessages[mId].status != Domain::MessageStatus::Downloading)) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();
                    return;
                }
                (*step)++;
                const qreal maxSimulatedProg = waitForTransferCompletion ? 0.90 : 1.0;
                qreal prog = qMin(maxSimulatedProg, static_cast<qreal>(*step) / totalSteps);
                qint64 bytes = static_cast<qint64>(prog * total);

                emit mediaTransferProgress(cId, mId, prog, bytes, total);

                if (!waitForTransferCompletion && prog >= 1.0) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();

                    if (m_incomingMessages.contains(mId)) {
                        m_incomingMessages[mId].status = Domain::MessageStatus::Sent;
                    }
                    this->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
                    emit messageUpdated(cId, mId, "sent", "");
                }
            });
            timer->start();
        }
    }
    
    if (!msgsToSave.empty()) {
        this->saveMessagesAsync(msgsToSave, this, [this, msgsToSave](bool success) {
            if (!success) {
                qWarning() << "[MessageService] Failed to save batch of incoming messages";
            }
            if (auto s = m_storage.lock()) {
                for (const auto& savedMsg : msgsToSave) {
                    if (m_completedTransfers.contains(savedMsg.id)) {
                        const QString path = m_completedTransfers.value(savedMsg.id);
                        const bool isEncryptedImageSpool =
                            (savedMsg.type == "image" || path.isEmpty() || path.endsWith(".spool"));
                        const QString resolvedMediaUrl =
                            isEncryptedImageSpool ? savedMsg.id : QUrl::fromLocalFile(path).toString();
                        s->updateMessageMediaUrl(savedMsg.id, resolvedMediaUrl);
                        s->updateMessageState(savedMsg.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                    }
                }
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

        this->updateMessageStatusAsync(messageId, status, errorText, this, [messageId](bool dbSuccess) {
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

    bool isMediaRequest = m_pendingMediaRequests.contains(messageId);
    if (m_outgoingMessages.contains(messageId)) {
        m_outgoingMessages[messageId].status = status;
        if (m_outgoingMessages[messageId].type == "media_request") {
            isMediaRequest = true;
        }
        if (!errorText.isEmpty() || m_outgoingMessages[messageId].type != "media_request") {
            m_outgoingMessages[messageId].errorText = errorText;
        }
    }
    if (m_pendingMediaRequests.contains(messageId)) {
        m_pendingMediaRequests[messageId].status = status;
        if (!errorText.isEmpty()) {
            m_pendingMediaRequests[messageId].errorText = errorText;
        }
    }

    const QString dbErrorText = (errorText.isNull() && !isMediaRequest) ? QStringLiteral("") : errorText;
    this->updateMessageStatusAsync(messageId, status, dbErrorText, this, [messageId](bool dbSuccess) {
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
            this->updateMessageStatusAsync(msg.id, Domain::MessageStatus::Sent, "", this, nullptr);
            emit messageUpdated(msg.conversationId, msg.id, "sent", "");
            return;
        }

        const bool isFromMe = m_outgoingMessages.contains(msg.id) ||
                              m_pendingMediaRequests.contains(msg.id) ||
                              (m_currentUserId.isEmpty() && msg.senderId.isEmpty()) ||
                              (!m_currentUserId.isEmpty() && msg.senderId.compare(m_currentUserId, Qt::CaseInsensitive) == 0);

        if (!isFromMe) {
            if (m_activeTransfers.contains(msg.id)) {
                QTimer *oldTimer = m_activeTransfers.take(msg.id);
                if (oldTimer) {
                    oldTimer->stop();
                    oldTimer->deleteLater();
                }
            }

            QString completedPath;
            bool isAlreadyCompleted = false;
            if (m_completedTransfers.contains(msg.id)) {
                completedPath = m_completedTransfers.value(msg.id);
                isAlreadyCompleted = true;
            } else if (m_fileTransferManager) {
                if (auto txOpt = m_fileTransferManager->getTransferInfo(msg.id)) {
                    if (txOpt->status == "completed" && !txOpt->spool_path.isEmpty() && QFile::exists(txOpt->spool_path)) {
                        completedPath = txOpt->spool_path;
                        m_completedTransfers.insert(msg.id, completedPath);
                        isAlreadyCompleted = true;
                    }
                }
            }

            if (isAlreadyCompleted) {
                const bool isEncryptedImageSpool =
                    (msg.type == "image" || completedPath.isEmpty() || completedPath.endsWith(".spool"));
                msg.mediaUrl = isEncryptedImageSpool ? msg.id : QUrl::fromLocalFile(completedPath).toString();
                msg.status = Domain::MessageStatus::Sent;
                msg.errorText = QStringLiteral("");
                m_incomingMessages.insert(msg.id, msg);
                if (auto s = m_storage.lock()) {
                    s->updateMessageMediaUrl(msg.id, msg.mediaUrl);
                    s->updateMessageState(msg.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                }
                qint64 total = msg.fileSize > 0 ? msg.fileSize : 100;
                emit mediaTransferProgress(msg.conversationId, msg.id, 1.0, total, total);
                emit messageUpdated(msg.conversationId, msg.id, "sent", "", msg.mediaUrl);
                return;
            }

            if (m_fileTransferManager) {
                msg.status = Domain::MessageStatus::Downloading;
                msg.errorText = QStringLiteral("");
                m_incomingMessages.insert(msg.id, msg);
                if (auto s = m_storage.lock()) {
                    s->updateMessageState(msg.id, NeoNect::Core::Messaging::MessageState::CREATED, QStringLiteral(""));
                }
                qint64 total = msg.fileSize > 0 ? msg.fileSize : 2500000;
                emit messageUpdated(msg.conversationId, msg.id, "downloading", "");
                emit mediaTransferProgress(msg.conversationId, msg.id, 0.05, static_cast<qint64>(total * 0.05), total);

                QString senderUser = msg.senderId.trimmed();
                if (senderUser.isEmpty() && msg.conversationId.startsWith("dms:", Qt::CaseInsensitive)) {
                    senderUser = msg.conversationId.mid(4).trimmed();
                }
                m_fileTransferManager->resumeIncomingTransfer(msg.id, senderUser);
                return;
            }

            emit messageUpdated(msg.conversationId, msg.id, "failed", "Transfer unavailable");
            return;
        }

        msg.status = Domain::MessageStatus::Sending;
        if (msg.type != "media_request") {
            msg.errorText = "";
        }
        m_outgoingMessages.insert(msg.id, msg);

        if (auto s = m_storage.lock()) {
            s->updateMessageState(msg.id, NeoNect::Core::Messaging::MessageState::CREATED,
                                  msg.type == "media_request" ? QString() : QStringLiteral(""));
        }
        emit messageUpdated(msg.conversationId, msg.id, "sending", "");

        const QString cleanPath = resolveLocalMediaPath(msg.mediaUrl);
        const bool hasRealFileTransfer =
            (m_fileTransferManager != nullptr &&
             (msg.type == "image" || msg.type == "file" ||
              msg.type == "video" || msg.type == "audio") &&
             !cleanPath.isEmpty() && QFile::exists(cleanPath));

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

            connect(timer, &QTimer::timeout, this, [this, timer, step, totalSteps, mId, cId, total, hasRealFileTransfer]() {
                if (!m_activeTransfers.contains(mId) || (m_outgoingMessages.contains(mId) && m_outgoingMessages[mId].status != Domain::MessageStatus::Sending)) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();
                    return;
                }
                (*step)++;
                const qreal maxSimulatedProg = hasRealFileTransfer ? 0.90 : 1.0;
                qreal prog = qMin(maxSimulatedProg, static_cast<qreal>(*step) / totalSteps);
                qint64 bytes = static_cast<qint64>(prog * total);

                emit mediaTransferProgress(cId, mId, prog, bytes, total);

                if (!hasRealFileTransfer && prog >= 1.0) {
                    timer->stop();
                    m_activeTransfers.remove(mId);
                    timer->deleteLater();

                    if (m_outgoingMessages.contains(mId)) {
                        m_outgoingMessages[mId].status = Domain::MessageStatus::Sent;
                    }
                    this->updateMessageStatusAsync(mId, Domain::MessageStatus::Sent, "", this, nullptr);
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
            if (hasRealFileTransfer && m_fileTransferManager) {
                startPeerTransfers(msg.conversationId, cleanPath, msg.id);
            }
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

    if (m_incomingMessages.contains(messageId)) {
        executeRetry(m_incomingMessages.value(messageId));
        return;
    }

    this->getMessageByIdAsync(messageId, this, [executeRetry, messageId](const std::optional<Domain::Message>& optMsg) {
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
    const bool isFromMe = (msg.senderId.toLower() == m_currentUserId.toLower());
    map["fromMe"] = isFromMe;
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
        double val = v.toDouble();
        if (val > 1.0) {
            val /= 100.0;
        }
        waveList.append(val);
    }
    map["waveform"] = waveList;
    
    switch (msg.status) {
        case Domain::MessageStatus::Sending:
            map["status"] = (!isFromMe && !m_currentUserId.isEmpty() && !msg.senderId.isEmpty()) ? "downloading" : "sending";
            break;
        case Domain::MessageStatus::Downloading: map["status"] = "downloading"; break;
        case Domain::MessageStatus::Sent: map["status"] = "sent"; break;
        case Domain::MessageStatus::Failed: map["status"] = "failed"; break;
        case Domain::MessageStatus::Seen: map["status"] = "seen"; break;
        case Domain::MessageStatus::Pending: map["status"] = "pending"; break;
        case Domain::MessageStatus::Accepted: map["status"] = "accepted"; break;
        case Domain::MessageStatus::Declined: map["status"] = "declined"; break;
    }

    if ((msg.status == Domain::MessageStatus::Sending || msg.status == Domain::MessageStatus::Downloading) && msg.type != "text") {
        map["transferProgress"] = 0.05;
        map["transferBytes"] = static_cast<qint64>((msg.fileSize > 0 ? msg.fileSize : 2500000) * 0.05);
    } else {
        map["transferProgress"] = 1.0;
        map["transferBytes"] = msg.fileSize;
    }
    
    map["errorText"] = msg.errorText;
    map["timestamp"] = msg.timestamp;
    map["mediaWidth"] = msg.mediaWidth;
    map["mediaHeight"] = msg.mediaHeight;
    return map;
}
static NeoNect::Core::Messaging::MessageState mapStatus(NeoNect::Domain::MessageStatus status) {
    switch (status) {
        case NeoNect::Domain::MessageStatus::Sending: return NeoNect::Core::Messaging::MessageState::CREATED;
        case NeoNect::Domain::MessageStatus::Downloading: return NeoNect::Core::Messaging::MessageState::CREATED;
        case NeoNect::Domain::MessageStatus::Sent: return NeoNect::Core::Messaging::MessageState::SENT;
        case NeoNect::Domain::MessageStatus::Seen: return NeoNect::Core::Messaging::MessageState::READ;
        case NeoNect::Domain::MessageStatus::Failed: return NeoNect::Core::Messaging::MessageState::FAILED;
        default: return NeoNect::Core::Messaging::MessageState::CREATED;
    }
}
static NeoNect::Domain::MessageStatus mapState(NeoNect::Core::Messaging::MessageState state) {
    switch (state) {
        case NeoNect::Core::Messaging::MessageState::CREATED: return NeoNect::Domain::MessageStatus::Sending;
        case NeoNect::Core::Messaging::MessageState::ENCRYPTING: return NeoNect::Domain::MessageStatus::Sending;
        case NeoNect::Core::Messaging::MessageState::SENT: return NeoNect::Domain::MessageStatus::Sent;
        case NeoNect::Core::Messaging::MessageState::DELIVERED: return NeoNect::Domain::MessageStatus::Sent;
        case NeoNect::Core::Messaging::MessageState::READ: return NeoNect::Domain::MessageStatus::Seen;
        case NeoNect::Core::Messaging::MessageState::FAILED: return NeoNect::Domain::MessageStatus::Failed;
        default: return NeoNect::Domain::MessageStatus::Sent;
    }
}

void MessageService::reconcileMediaMessage(Domain::Message &dm, NeoNect::Core::Messaging::IMessageStorage *storage) {
    const bool isFromMe = m_currentUserId.isEmpty() ? true : (dm.senderId.compare(m_currentUserId, Qt::CaseInsensitive) == 0);
    if (!isFromMe && dm.status == Domain::MessageStatus::Sending) {
        dm.status = Domain::MessageStatus::Downloading;
    }

    const bool isMediaAttachment =
        (dm.type == "image" || dm.type == "file" || dm.type == "video" ||
         dm.type == "audio" || dm.type == "voice" || dm.type == "voice_note");
    if (!isMediaAttachment) {
        return;
    }

    if (!isFromMe && dm.type == "image") {
        if (dm.mediaUrl.contains('/') || dm.mediaUrl.contains('\\') || dm.mediaUrl.startsWith("file:")) {
            dm.mediaUrl = dm.id;
            if (storage) {
                storage->updateMessageMediaUrl(dm.id, dm.mediaUrl);
            }
        }
    }

    if (m_completedTransfers.contains(dm.id)) {
        const QString path = m_completedTransfers.value(dm.id);
        if (!isFromMe || dm.mediaUrl.isEmpty()) {
            dm.mediaUrl = resolveCompletedMediaUrl(dm.type, dm.id, path, m_serverUrl, m_currentUserId);
            if (storage) {
                storage->updateMessageMediaUrl(dm.id, dm.mediaUrl);
                storage->updateMessageState(dm.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
            }
        }
        if (dm.status != Domain::MessageStatus::Seen) {
            dm.status = Domain::MessageStatus::Sent;
        }
        dm.errorText = QStringLiteral("");
        return;
    }

    if (m_fileTransferManager && (dm.mediaUrl.isEmpty() || dm.mediaUrl == dm.id || dm.status == Domain::MessageStatus::Downloading || dm.status == Domain::MessageStatus::Sending)) {
        if (auto txOpt = m_fileTransferManager->getTransferInfo(dm.id)) {
            if (txOpt->status == "completed" && !txOpt->spool_path.isEmpty() && QFile::exists(txOpt->spool_path)) {
                m_completedTransfers.insert(dm.id, txOpt->spool_path);
                if (!isFromMe || dm.mediaUrl.isEmpty()) {
                    dm.mediaUrl = resolveCompletedMediaUrl(dm.type, dm.id, txOpt->spool_path, m_serverUrl, m_currentUserId);
                    if (storage) {
                        storage->updateMessageMediaUrl(dm.id, dm.mediaUrl);
                        storage->updateMessageState(dm.id, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                    }
                }
                if (dm.status != Domain::MessageStatus::Seen) {
                    dm.status = Domain::MessageStatus::Sent;
                }
                dm.errorText = QStringLiteral("");
                return;
            }
            if (txOpt->status == "failed" || txOpt->status == "cancelled") {
                dm.status = Domain::MessageStatus::Failed;
                if (dm.errorText.isEmpty()) {
                    dm.errorText = (txOpt->status == "cancelled") ? QStringLiteral("Transfer cancelled") : QStringLiteral("Transfer failed");
                }
                return;
            }
            if (txOpt->status == "active") {
                dm.status = isFromMe ? Domain::MessageStatus::Sending : Domain::MessageStatus::Downloading;
                return;
            }
        } else if (!isFromMe && dm.mediaUrl.isEmpty()) {
            if (m_activeTransfers.contains(dm.id)) {
                dm.status = Domain::MessageStatus::Downloading;
            } else if (dm.type == "image" || dm.type == "file" || dm.type == "video" || dm.type == "audio") {
                dm.status = Domain::MessageStatus::Failed;
                if (dm.errorText.isEmpty()) {
                    dm.errorText = QStringLiteral("Transfer incomplete");
                }
            }
        }
    }
}

void MessageService::saveMessageAsync(const Domain::Message &msg, const QObject* context, std::function<void(bool)> callback) {
    NeoNect::Core::Messaging::Message coreMsg;
    coreMsg.messageId = msg.id;
    coreMsg.serverId = msg.serverId;
    coreMsg.conversationId = msg.conversationId;
    coreMsg.senderId = msg.senderId;
    coreMsg.timestamp = msg.timestamp;
    coreMsg.plaintext = msg.text;
    coreMsg.type = msg.type;
    coreMsg.mediaUrl = msg.mediaUrl;
    coreMsg.fileName = msg.fileName;
    coreMsg.fileSize = msg.fileSize;
    coreMsg.duration = msg.duration;
    coreMsg.waveform = msg.waveform;
    coreMsg.mediaWidth = msg.mediaWidth;
    coreMsg.mediaHeight = msg.mediaHeight;
    coreMsg.errorText = msg.errorText;
    coreMsg.state = mapStatus(msg.status);
    
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;
    
    QMetaObject::invokeMethod(m_workerContext, [self, coreMsg, ctx, callback, storage, context]() {
        auto s = storage.lock();
        if (!s) return;
        bool success = s->saveMessage(coreMsg);
        if (callback && self) {
            QMetaObject::invokeMethod(self, [context, ctx, callback, success]() {
                if (context && !ctx) return;
                callback(success);
            });
        }
    });
}

void MessageService::saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, std::function<void(bool)> callback) {
    std::vector<NeoNect::Core::Messaging::Message> coreMsgs;
    for (const auto& msg : msgs) {
        NeoNect::Core::Messaging::Message coreMsg;
        coreMsg.messageId = msg.id;
        coreMsg.serverId = msg.serverId;
        coreMsg.conversationId = msg.conversationId;
        coreMsg.senderId = msg.senderId;
        coreMsg.timestamp = msg.timestamp;
        coreMsg.plaintext = msg.text;
        coreMsg.type = msg.type;
        coreMsg.mediaUrl = msg.mediaUrl;
        coreMsg.fileName = msg.fileName;
        coreMsg.fileSize = msg.fileSize;
        coreMsg.duration = msg.duration;
        coreMsg.waveform = msg.waveform;
        coreMsg.mediaWidth = msg.mediaWidth;
        coreMsg.mediaHeight = msg.mediaHeight;
        coreMsg.errorText = msg.errorText;
        coreMsg.state = mapStatus(msg.status);
        coreMsgs.push_back(coreMsg);
    }
    
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;
    
    QMetaObject::invokeMethod(m_workerContext, [self, coreMsgs, ctx, callback, storage, context]() mutable {
        auto s = storage.lock();
        if (!s) return;
        for (auto& cm : coreMsgs) {
            if (cm.state == NeoNect::Core::Messaging::MessageState::SENT) {
                if (auto existing = s->getMessage(cm.messageId)) {
                    if (existing->state == NeoNect::Core::Messaging::MessageState::DELIVERED ||
                        existing->state == NeoNect::Core::Messaging::MessageState::READ) {
                        cm.state = existing->state;
                    }
                }
            }
        }
        bool allSuccess = s->saveMessages(coreMsgs);
        for (const auto& cm : coreMsgs) {
            if (!cm.mediaUrl.isEmpty()) {
                if (auto existing = s->getMessage(cm.messageId)) {
                    if (existing->mediaUrl.isEmpty()) {
                        s->updateMessageMediaUrl(cm.messageId, cm.mediaUrl);
                        s->updateMessageState(cm.messageId, NeoNect::Core::Messaging::MessageState::DELIVERED, QStringLiteral(""));
                    }
                }
            }
        }
        if (callback && self) {
            QMetaObject::invokeMethod(self, [context, ctx, callback, allSuccess]() {
                if (context && !ctx) return;
                callback(allSuccess);
            });
        }
    });
}

void MessageService::updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, std::function<void(bool)> callback) {
    auto coreStatus = mapStatus(status);
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;

    if (auto s = storage.lock()) {
        if (s->getMessage(id).has_value()) {
            bool success = s->updateMessageState(id, coreStatus, errorText);
            if (callback && self) {
                QMetaObject::invokeMethod(self, [context, ctx, callback, success]() {
                    if (context && !ctx) return;
                    callback(success);
                });
            }
            return;
        }
    }
    
    QMetaObject::invokeMethod(m_workerContext, [self, id, coreStatus, errorText, ctx, callback, storage, context]() {
        auto s = storage.lock();
        if (!s) return;
        bool success = s->updateMessageState(id, coreStatus, errorText);
        if (callback && self) {
            QMetaObject::invokeMethod(self, [context, ctx, callback, success]() {
                if (context && !ctx) return;
                callback(success);
            });
        }
    });
}

void MessageService::markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context, std::function<void(bool)> callback) {
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;

    QMetaObject::invokeMethod(m_workerContext, [self, conversationId, senderId, ctx, callback, storage, context]() {
        auto s = storage.lock();
        if (s) {
            auto msgs = s->getConversationMessages(conversationId);
            for (const auto& cm : msgs) {
                if (senderId.isEmpty() || cm.senderId.compare(senderId, Qt::CaseInsensitive) == 0) {
                    if (cm.state != NeoNect::Core::Messaging::MessageState::READ) {
                        s->updateMessageState(cm.messageId, NeoNect::Core::Messaging::MessageState::READ);
                    }
                }
            }
        }
        if (callback && self) {
            QMetaObject::invokeMethod(self, [context, ctx, callback]() {
                if (context && !ctx) return;
                callback(true);
            });
        }
    });
}

void MessageService::deleteMessageAsync(const QString &id) {
    auto storage = m_storage;
    QString mediaUrlToClean;
    if (m_outgoingMessages.contains(id)) {
        if (m_outgoingMessages[id].type == "voice_note" || m_outgoingMessages[id].type == "media_request") {
            mediaUrlToClean = m_outgoingMessages[id].mediaUrl;
        }
    }
    
    QMetaObject::invokeMethod(m_workerContext, [storage, id, mediaUrlToClean, this]() {
        auto s = storage.lock();
        if (!s) return;
        s->deleteMessage(id);
        
        if (!mediaUrlToClean.isEmpty()) {
            QMetaObject::invokeMethod(this, [this, mediaUrlToClean]() {
                emit localMediaAbandoned(mediaUrlToClean);
            });
        }
    });
}

void MessageService::getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, std::function<void(const std::vector<Domain::Message>&)> callback) {
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;
    
    QMetaObject::invokeMethod(m_workerContext, [self, conversationId, limit, beforeTimestamp, ctx, callback, storage, context]() {
        auto s = storage.lock();
        if (!s) return;
        auto coreMsgs = s->getConversationMessages(conversationId);
        std::vector<Domain::Message> messages;
        for (const auto& cm : coreMsgs) {
            if (beforeTimestamp > 0 && cm.timestamp >= beforeTimestamp) {
                continue;
            }
            Domain::Message dm;
            dm.id = cm.messageId;
            dm.serverId = cm.serverId;
            dm.conversationId = cm.conversationId;
            dm.senderId = cm.senderId;
            dm.timestamp = cm.timestamp;
            dm.text = cm.plaintext;
            dm.type = cm.type;
            dm.mediaUrl = cm.mediaUrl;
            dm.fileName = cm.fileName;
            dm.fileSize = cm.fileSize;
            dm.duration = cm.duration;
            dm.waveform = cm.waveform;
            dm.mediaWidth = cm.mediaWidth;
            dm.mediaHeight = cm.mediaHeight;
            dm.errorText = cm.errorText;
            dm.status = mapState(cm.state);
            messages.push_back(dm);
        }
        if (limit > 0 && static_cast<int>(messages.size()) > limit) {
            messages.erase(messages.begin(), messages.end() - limit);
        }
        
        if (callback && self) {
            QMetaObject::invokeMethod(self, [self, context, ctx, callback, messages, storage]() mutable {
                if (!self || (context && !ctx)) return;
                auto sMain = storage.lock();
                for (auto& dm : messages) {
                    self->reconcileMediaMessage(dm, sMain.get());
                }
                callback(messages);
            });
        }
    });
}

void MessageService::getMessageByIdAsync(const QString &id, const QObject* context, std::function<void(const std::optional<Domain::Message>&)> callback) {
    QPointer<MessageService> self(this);
    QPointer<const QObject> ctx(context);
    auto storage = m_storage;
    
    QMetaObject::invokeMethod(m_workerContext, [self, id, ctx, callback, storage, context]() {
        auto s = storage.lock();
        if (!s) return;
        auto cmOpt = s->getMessage(id);
        std::optional<Domain::Message> optMsg;
        if (cmOpt) {
            Domain::Message dm;
            dm.id = cmOpt->messageId;
            dm.serverId = cmOpt->serverId;
            dm.conversationId = cmOpt->conversationId;
            dm.senderId = cmOpt->senderId;
            dm.timestamp = cmOpt->timestamp;
            dm.text = cmOpt->plaintext;
            dm.type = cmOpt->type;
            dm.mediaUrl = cmOpt->mediaUrl;
            dm.fileName = cmOpt->fileName;
            dm.fileSize = cmOpt->fileSize;
            dm.duration = cmOpt->duration;
            dm.waveform = cmOpt->waveform;
            dm.mediaWidth = cmOpt->mediaWidth;
            dm.mediaHeight = cmOpt->mediaHeight;
            dm.errorText = cmOpt->errorText;
            dm.status = mapState(cmOpt->state);
            optMsg = dm;
        }
        
        if (callback && self) {
            QMetaObject::invokeMethod(self, [self, context, ctx, callback, optMsg, storage]() mutable {
                if (!self || (context && !ctx)) return;
                if (optMsg.has_value()) {
                    auto sMain = storage.lock();
                    self->reconcileMediaMessage(optMsg.value(), sMain.get());
                }
                callback(optMsg);
            });
        }
    });
}

}
}
