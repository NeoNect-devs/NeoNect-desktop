// src/core/chatmessagemodel.cpp
#include "chatmessagemodel.h"
#include <QUuid>
#include <QDateTime>

static QString detectMediaType(const QString &mediaUrl, const QString &fileName, const QString &fallbackType) {
    QString target = (fileName.isEmpty() ? mediaUrl : fileName).toLower();
    if (target.endsWith(".png") || target.endsWith(".jpg") || target.endsWith(".jpeg") ||
        target.endsWith(".webp") || target.endsWith(".gif") || target.endsWith(".bmp") ||
        target.endsWith(".svg") || target.endsWith(".ico") || target.endsWith(".tiff")) {
        return "image";
    }
    if (target.endsWith(".mp4") || target.endsWith(".webm") || target.endsWith(".mov") ||
        target.endsWith(".mkv") || target.endsWith(".avi") || target.endsWith(".m4v") ||
        target.endsWith(".flv") || target.endsWith(".wmv") || target.endsWith(".3gp")) {
        return "video";
    }
    if (target.endsWith(".mp3") || target.endsWith(".wav") || target.endsWith(".ogg") ||
        target.endsWith(".flac") || target.endsWith(".m4a") || target.endsWith(".aac") ||
        target.endsWith(".opus") || target.endsWith(".wma")) {
        return "audio";
    }
    return fallbackType.isEmpty() ? "file" : fallbackType;
}

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_items.reserve(128);
}

int ChatMessageModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }

    const auto &item = m_items[static_cast<std::size_t>(index.row())];
    switch (role) {
        case TextRole: return item.text;
        case FromMeRole: return item.fromMe;
        case SenderNameRole: return item.senderName;
        case SenderAvatarRole: return item.senderAvatar;
        case FirstInBlockRole: return item.isFirstInBlock;
        case LastInBlockRole: return item.isLastInBlock;
        case FirstUnreadRole: return item.isFirstUnread;
        case MessageIdRole: return item.id;
        case MessageTypeRole: return item.messageType;
        case MediaUrlRole: return item.mediaUrl;
        case FileNameRole: return item.fileName;
        case FileSizeRole: return item.fileSize;
        case DurationRole: return item.duration;
        case WaveformRole: return item.waveform;
        case StatusRole: return item.status;
        case ErrorTextRole: return item.errorText;
        case TimestampRole: return item.timestamp;
        case TransferProgressRole: return item.transferProgress;
        case TransferBytesRole: return item.transferBytes;
        default: return QVariant();
    }
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    return {
        { TextRole, "text" },
        { FromMeRole, "fromMe" },
        { SenderNameRole, "senderName" },
        { SenderAvatarRole, "senderAvatar" },
        { FirstInBlockRole, "isFirstInBlock" },
        { LastInBlockRole, "isLastInBlock" },
        { FirstUnreadRole, "isFirstUnread" },
        { MessageIdRole, "messageId" },
        { MessageTypeRole, "messageType" },
        { MediaUrlRole, "mediaUrl" },
        { FileNameRole, "fileName" },
        { FileSizeRole, "fileSize" },
        { DurationRole, "duration" },
        { WaveformRole, "waveform" },
        { StatusRole, "status" },
        { ErrorTextRole, "errorText" },
        { TimestampRole, "timestamp" },
        { TransferProgressRole, "transferProgress" },
        { TransferBytesRole, "transferBytes" }
    };
}

void ChatMessageModel::insertOutgoingMessage(const QString &text) {
    if (text.trimmed().isEmpty()) return;
    insertMessage(text.trimmed(), true, "Me", "", "text", "", "", 0, 0, {}, "sent");
}

void ChatMessageModel::insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar,
                                    const QString &messageType, const QString &mediaUrl,
                                    const QString &fileName, qint64 fileSize, int duration,
                                    const QVariantList &waveform, const QString &status,
                                    const QString &id, qint64 timestamp, const QString &errorText) {
    int newIndex = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newIndex, newIndex);

    bool isFirst = m_items.empty() || (m_items.back().fromMe != fromMe) || (m_items.back().senderName != senderName);

    if (!m_items.empty() && (m_items.back().fromMe == fromMe) && (m_items.back().senderName == senderName)) {
        m_items.back().isLastInBlock = false;
        QModelIndex prevIdx = index(static_cast<int>(m_items.size()) - 1);
        emit dataChanged(prevIdx, prevIdx, {LastInBlockRole});
    }

    MessageItem item;
    item.id = id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id;
    item.text = text;
    item.fromMe = fromMe;
    item.senderName = senderName;
    item.senderAvatar = senderAvatar;
    QString resolvedType = messageType;
    if ((resolvedType == "file" || resolvedType == "text" || resolvedType.isEmpty()) && (!mediaUrl.isEmpty() || !fileName.isEmpty())) {
        resolvedType = detectMediaType(mediaUrl, fileName, resolvedType);
    }
    item.messageType = resolvedType.isEmpty() ? "text" : resolvedType;
    item.mediaUrl = mediaUrl;
    item.fileName = fileName;
    item.fileSize = fileSize;
    item.duration = duration;
    item.waveform = waveform;
    item.status = status.isEmpty() ? "sent" : status;
    item.errorText = errorText;
    if (item.errorText.isEmpty() && item.messageType == "media_request") {
        item.errorText = detectMediaType(mediaUrl, fileName, "file");
    }
    qint64 finalTs = timestamp;
    if (finalTs > 0 && finalTs < 100000000000LL) {
        finalTs *= 1000LL;
    }
    item.timestamp = finalTs > 0 ? finalTs : QDateTime::currentMSecsSinceEpoch();
    item.isFirstInBlock = isFirst;
    item.isLastInBlock = true;

    m_items.push_back(std::move(item));
    endInsertRows();
    emit countChanged();
}

void ChatMessageModel::insertMessageItem(const QVariantMap &map) {
    QString text = map.value("text").toString();
    bool fromMe = map.value("fromMe", false).toBool();
    QString senderName = map.value("senderName", fromMe ? "Me" : "Anonymous").toString();
    QString senderAvatar = map.value("senderAvatar", "").toString();
    QString mediaUrl = map.value("mediaUrl", "").toString();
    QString fileName = map.value("fileName", "").toString();
    QString messageType = map.contains("messageType") ? map.value("messageType").toString() : map.value("type", "text").toString();
    if ((messageType == "file" || messageType == "text" || messageType.isEmpty()) && (!mediaUrl.isEmpty() || !fileName.isEmpty())) {
        messageType = detectMediaType(mediaUrl, fileName, messageType);
    }
    qint64 fileSize = map.value("fileSize", 0).toLongLong();
    int duration = map.value("duration", 0).toInt();
    QVariantList waveform = map.value("waveform").toList();
    QString status = map.value("status", "sent").toString();
    QString id = map.contains("messageId") && !map.value("messageId").toString().isEmpty() ? map.value("messageId").toString() : map.value("id").toString();
    qint64 timestamp = map.value("timestamp", 0).toLongLong();
    QString errorText = map.value("errorText", "").toString();
    if (errorText.isEmpty() && messageType == "media_request") {
        errorText = detectMediaType(mediaUrl, fileName, "file");
    }

    insertMessage(text, fromMe, senderName, senderAvatar, messageType, mediaUrl, fileName, fileSize, duration, waveform, status, id, timestamp, errorText);
}

void ChatMessageModel::updateMessageStatus(const QString &messageId, const QString &status, const QString &errorText) {
    if (messageId.isEmpty()) return;

    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            m_items[i].status = status;
            m_items[i].errorText = errorText;
            QModelIndex idx = index(static_cast<int>(i));
            emit dataChanged(idx, idx, {StatusRole, ErrorTextRole});
            break;
        }
    }
}

void ChatMessageModel::removeMessage(const QString &messageId) {
    if (messageId.isEmpty()) return;

    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            beginRemoveRows(QModelIndex(), static_cast<int>(i), static_cast<int>(i));
            m_items.erase(m_items.begin() + static_cast<long long>(i));
            endRemoveRows();
            recalculateBlocks();
            if (!m_items.empty()) {
                emit dataChanged(index(0, 0), index(static_cast<int>(m_items.size()) - 1, 0), {FirstInBlockRole, LastInBlockRole});
            }
            emit countChanged();
            break;
        }
    }
}

QVariantMap ChatMessageModel::getMessageById(const QString &messageId) const {
    QVariantMap res;
    for (const auto &item : m_items) {
        if (item.id == messageId) {
            res["messageId"] = item.id;
            res["text"] = item.text;
            res["fromMe"] = item.fromMe;
            res["senderName"] = item.senderName;
            res["senderAvatar"] = item.senderAvatar;
            res["messageType"] = item.messageType;
            res["mediaUrl"] = item.mediaUrl;
            res["fileName"] = item.fileName;
            res["fileSize"] = item.fileSize;
            res["duration"] = item.duration;
            res["waveform"] = item.waveform;
            res["status"] = item.status;
            res["errorText"] = item.errorText;
            res["timestamp"] = item.timestamp;
            break;
        }
    }
    return res;
}

void ChatMessageModel::addMessage(MessageItem &&item) {
    int newIndex = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_items.push_back(std::move(item));
    endInsertRows();
    emit countChanged();
}

void ChatMessageModel::clearActiveViewportStore() {
    beginResetModel();
    m_items.clear();
    m_canFetchMore = false;
    m_isLoadingMore = false;
    endResetModel();
    emit canFetchMoreChanged();
    emit isLoadingMoreChanged();
    emit countChanged();
}

void ChatMessageModel::setActiveConversation(const QString &conversationId) {
    m_activeConversationId = conversationId;
    clearActiveViewportStore();
}

qint64 ChatMessageModel::oldestTimestamp() const {
    if (m_items.empty()) return 0;
    return m_items.front().timestamp;
}

void ChatMessageModel::onMoreMessagesLoaded(const QString &conversationId, const QVariantList &messages) {
    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    prependMessages(conversationId, messages);
}

void ChatMessageModel::prependMessages(const QString &conversationId, const QVariantList &messages) {
    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    m_isLoadingMore = false;
    emit isLoadingMoreChanged();

    if (messages.isEmpty()) {
        m_canFetchMore = false;
        emit canFetchMoreChanged();
        return;
    }

    m_canFetchMore = (messages.size() >= 30);
    emit canFetchMoreChanged();

    int count = static_cast<int>(messages.size());
    beginInsertRows(QModelIndex(), 0, count - 1);
    std::vector<MessageItem> olderItems;
    olderItems.reserve(count);
    for (const QVariant &msg : messages) {
        olderItems.push_back(parseVariantMap(msg.toMap()));
    }
    m_items.insert(m_items.begin(), std::make_move_iterator(olderItems.begin()), std::make_move_iterator(olderItems.end()));
    recalculateBlocks();
    endInsertRows();
    emit countChanged();
}

void ChatMessageModel::setFirstUnreadMessageId(const QString &messageId) {
    if (messageId.isEmpty()) {
        clearFirstUnread();
        return;
    }
    // If a first unread marker is already set, keep it at the top of the unread batch
    for (const auto &item : m_items) {
        if (item.isFirstUnread) return;
    }

    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            m_items[i].isFirstUnread = true;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {FirstUnreadRole});
            break;
        }
    }
}

void ChatMessageModel::setFirstUnreadIndex(int unreadIndex) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        bool shouldBe = (static_cast<int>(i) == unreadIndex);
        if (m_items[i].isFirstUnread != shouldBe) {
            m_items[i].isFirstUnread = shouldBe;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {FirstUnreadRole});
        }
    }
}

void ChatMessageModel::clearFirstUnread() {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].isFirstUnread) {
            m_items[i].isFirstUnread = false;
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, {FirstUnreadRole});
        }
    }
}

void ChatMessageModel::updateTransferProgress(const QString &messageId, qreal progress, qint64 bytes, qint64 totalBytes) {
    if (messageId.isEmpty()) return;
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            m_items[i].transferProgress = progress;
            m_items[i].transferBytes = bytes;
            QVector<int> roles = {TransferProgressRole, TransferBytesRole};
            if (totalBytes > 0 && (m_items[i].fileSize <= 0 || m_items[i].fileSize != totalBytes)) {
                m_items[i].fileSize = totalBytes;
                roles.append(FileSizeRole);
            }
            QModelIndex idx = index(static_cast<int>(i), 0);
            emit dataChanged(idx, idx, roles);
            break;
        }
    }
}

void ChatMessageModel::onMediaTransferProgress(const QString &conversationId, const QString &messageId, qreal progress, qint64 bytesTransferred, qint64 totalBytes) {
    if (!conversationId.isEmpty() && conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    updateTransferProgress(messageId, progress, bytesTransferred, totalBytes);
}

MessageItem ChatMessageModel::parseVariantMap(const QVariantMap &map) const {
    MessageItem item;
    item.id = map.contains("id") && !map.value("id").toString().isEmpty() ? map.value("id").toString() : map.value("messageId").toString();
    item.text = map.value("text").toString();
    item.fromMe = map.value("fromMe").toBool();
    item.senderName = map.contains("senderId") && !map.value("senderId").toString().isEmpty() ? map.value("senderId").toString() : map.value("senderName").toString();
    item.senderAvatar = item.senderName.isEmpty() ? "" : item.senderName.left(1).toUpper();
    item.mediaUrl = map.value("mediaUrl").toString();
    item.fileName = map.value("fileName").toString();
    QString rawType = map.contains("type") ? map.value("type").toString() : map.value("messageType", "text").toString();
    if ((rawType == "file" || rawType == "text" || rawType.isEmpty()) && (!item.mediaUrl.isEmpty() || !item.fileName.isEmpty())) {
        rawType = detectMediaType(item.mediaUrl, item.fileName, rawType);
    }
    item.messageType = rawType;
    item.fileSize = map.value("fileSize").toLongLong();
    item.duration = map.value("duration").toInt();
    item.waveform = map.value("waveform").toList();
    item.status = map.value("status", "sent").toString();
    item.errorText = map.value("errorText").toString();
    if (item.errorText.isEmpty() && item.messageType == "media_request") {
        item.errorText = detectMediaType(item.mediaUrl, item.fileName, "file");
    }
    qint64 t = map.value("timestamp").toLongLong();
    if (t > 0 && t < 100000000000LL) {
        t *= 1000LL;
    }
    item.timestamp = t > 0 ? t : QDateTime::currentMSecsSinceEpoch();
    item.transferProgress = map.value("transferProgress", 0.0).toReal();
    item.transferBytes = map.value("transferBytes", 0).toLongLong();
    return item;
}

void ChatMessageModel::onConversationLoaded(const QString &conversationId, const QVariantList &messages) {
    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    
    m_canFetchMore = (messages.size() >= 40);
    m_isLoadingMore = false;
    
    beginResetModel();
    m_items.clear();
    for (const QVariant &msg : messages) {
        m_items.push_back(parseVariantMap(msg.toMap()));
    }
    std::stable_sort(m_items.begin(), m_items.end(), [](const MessageItem &a, const MessageItem &b) {
        return a.timestamp < b.timestamp;
    });
    recalculateBlocks();
    endResetModel();

    emit canFetchMoreChanged();
    emit isLoadingMoreChanged();
    emit countChanged();
}

void ChatMessageModel::onMessageAdded(const QString &conversationId, const QVariantMap &message) {
    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    
    QString msgId = message.value("id").toString();
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == msgId) {
            m_items[i] = parseVariantMap(message);
            recalculateBlocks();
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0));
            return;
        }
    }
    
    MessageItem newItem = parseVariantMap(message);
    auto it = std::upper_bound(m_items.begin(), m_items.end(), newItem, [](const MessageItem &a, const MessageItem &b) {
        return a.timestamp < b.timestamp;
    });
    int newIndex = static_cast<int>(std::distance(m_items.begin(), it));
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_items.insert(it, std::move(newItem));
    recalculateBlocks();
    endInsertRows();
    emit countChanged();
    
    if (m_items.size() > 1) {
        int startIdx = std::max(0, newIndex - 1);
        int endIdx = std::min(static_cast<int>(m_items.size()) - 1, newIndex + 1);
        emit dataChanged(index(startIdx, 0), index(endIdx, 0), {FirstInBlockRole, LastInBlockRole});
    }
}

void ChatMessageModel::onMessageUpdated(const QString &conversationId, const QString &messageId, const QString &status, const QString &errorText) {
    if (!conversationId.isEmpty() && conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    
    if (messageId.isEmpty() || messageId == "all") {
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].fromMe && m_items[i].status != status) {
                m_items[i].status = status;
                m_items[i].errorText = errorText;
                emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0), {StatusRole, ErrorTextRole});
            }
        }
        return;
    }

    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            m_items[i].status = status;
            m_items[i].errorText = errorText;
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0), {StatusRole, ErrorTextRole});
            break;
        }
    }
}

void ChatMessageModel::onMessageRemoved(const QString &conversationId, const QString &messageId) {
    if (!conversationId.isEmpty() && conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    removeMessage(messageId);
}

void ChatMessageModel::recalculateBlocks() {
    for (size_t i = 0; i < m_items.size(); ++i) {
        bool isFirst = true;
        bool isLast = true;
        
        if (i > 0) {
            const auto &prev = m_items[i - 1];
            if (prev.senderName == m_items[i].senderName && (m_items[i].timestamp - prev.timestamp < 300000)) {
                isFirst = false;
            }
        }
        
        if (i < m_items.size() - 1) {
            const auto &next = m_items[i + 1];
            if (next.senderName == m_items[i].senderName && (next.timestamp - m_items[i].timestamp < 300000)) {
                isLast = false;
            }
        }
        
        m_items[i].isFirstInBlock = isFirst;
        m_items[i].isLastInBlock = isLast;
    }
}