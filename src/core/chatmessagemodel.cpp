// src/core/chatmessagemodel.cpp
#include "chatmessagemodel.h"
#include <QUuid>
#include <QDateTime>
#include <QImageReader>
#include <QFile>
#include <QSet>
#include <algorithm>

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
        case MediaWidthRole: return item.mediaWidth;
        case MediaHeightRole: return item.mediaHeight;
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
        { TransferBytesRole, "transferBytes" },
        { MediaWidthRole, "mediaWidth" },
        { MediaHeightRole, "mediaHeight" }
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

    if (!m_activeConversationId.isEmpty()) {
        m_conversationCache[m_activeConversationId] = m_items;
    }
}

void ChatMessageModel::insertMessageItem(const QVariantMap &map) {
    MessageItem item = parseVariantMap(map);
    int newIndex = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newIndex, newIndex);

    bool isFirst = m_items.empty() || (m_items.back().fromMe != item.fromMe) || (m_items.back().senderName != item.senderName);

    if (!m_items.empty() && (m_items.back().fromMe == item.fromMe) && (m_items.back().senderName == item.senderName)) {
        m_items.back().isLastInBlock = false;
        QModelIndex prevIdx = index(static_cast<int>(m_items.size()) - 1);
        emit dataChanged(prevIdx, prevIdx, {LastInBlockRole});
    }

    item.isFirstInBlock = isFirst;
    item.isLastInBlock = true;

    m_items.push_back(std::move(item));
    endInsertRows();
    emit countChanged();

    if (!m_activeConversationId.isEmpty()) {
        m_conversationCache[m_activeConversationId] = m_items;
    }
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
    QString key = conversationId.trimmed().toLower();
    QString oldKey = m_activeConversationId.trimmed().toLower();
    if (oldKey == key && !key.isEmpty()) {
        return;
    }

    // Cache current active conversation items before switching
    if (!oldKey.isEmpty()) {
        m_conversationCache[oldKey] = m_items;
        m_canFetchMoreCache[oldKey] = m_canFetchMore;
    }

    m_activeConversationId = key;
    m_isLoadingMore = false;
    emit activeConversationIdChanged();

    if (!key.isEmpty() && m_conversationCache.contains(key)) {
        beginResetModel();
        m_items = m_conversationCache.value(key);
        m_canFetchMore = m_canFetchMoreCache.value(key, false);
        endResetModel();
        emit canFetchMoreChanged();
        emit isLoadingMoreChanged();
        emit countChanged();
        emit conversationReady(key);
    } else {
        clearActiveViewportStore();
    }
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
        m_canFetchMoreCache[conversationId] = false;
        emit canFetchMoreChanged();
        return;
    }

    m_canFetchMore = (messages.size() >= 30);
    m_canFetchMoreCache[conversationId] = m_canFetchMore;
    emit canFetchMoreChanged();

    std::vector<MessageItem> olderItems;
    olderItems.reserve(messages.size());
    for (const QVariant &msg : messages) {
        olderItems.push_back(parseVariantMap(msg.toMap()));
    }

    // Deduplicate against existing items
    QSet<QString> existingIds;
    for (const auto &it : m_items) {
        existingIds.insert(it.id);
    }
    olderItems.erase(std::remove_if(olderItems.begin(), olderItems.end(), [&](const MessageItem &item) {
        return existingIds.contains(item.id);
    }), olderItems.end());

    if (olderItems.empty()) {
        return;
    }

    int count = static_cast<int>(olderItems.size());
    beginInsertRows(QModelIndex(), 0, count - 1);
    m_items.insert(m_items.begin(), std::make_move_iterator(olderItems.begin()), std::make_move_iterator(olderItems.end()));
    recalculateBlocks();
    endInsertRows();
    emit countChanged();

    m_conversationCache[conversationId] = m_items;

    const size_t MAX_WINDOW_SIZE = 150;
    if (m_items.size() > MAX_WINDOW_SIZE) {
        int removeCount = static_cast<int>(m_items.size() - MAX_WINDOW_SIZE);
        int removeStart = static_cast<int>(m_items.size()) - removeCount;
        beginRemoveRows(QModelIndex(), removeStart, static_cast<int>(m_items.size()) - 1);
        m_items.erase(m_items.begin() + removeStart, m_items.end());
        recalculateBlocks();
        endRemoveRows();
        emit countChanged();
        m_conversationCache[conversationId] = m_items;
    }
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
    if (item.id.isEmpty()) {
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
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
    item.mediaWidth = map.value("mediaWidth", 0).toInt();
    item.mediaHeight = map.value("mediaHeight", 0).toInt();

    // Fast inspect local image dimensions if not already provided
    if (item.mediaWidth <= 0 && item.messageType == "image" && !item.mediaUrl.isEmpty()) {
        QString localPath = item.mediaUrl;
        if (localPath.startsWith("file:///")) localPath = localPath.mid(8);
        else if (localPath.startsWith("file://")) localPath = localPath.mid(7);
#ifdef _WIN32
        if (localPath.startsWith("/") && localPath.length() >= 3 && localPath.at(2) == ':') {
            localPath = localPath.mid(1);
        }
#endif
        if (QFile::exists(localPath)) {
            QImageReader reader(localPath);
            QSize sz = reader.size();
            if (sz.isValid() && sz.width() > 0 && sz.height() > 0) {
                item.mediaWidth = sz.width();
                item.mediaHeight = sz.height();
            }
        }
    }

    return item;
}

void ChatMessageModel::onConversationLoaded(const QString &conversationId, const QVariantList &messages) {
    std::vector<MessageItem> loadedItems;
    loadedItems.reserve(messages.size());
    for (const QVariant &msg : messages) {
        loadedItems.push_back(parseVariantMap(msg.toMap()));
    }
    std::stable_sort(loadedItems.begin(), loadedItems.end(), [](const MessageItem &a, const MessageItem &b) {
        if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
        return a.id < b.id;
    });

    // Update conversation cache
    m_conversationCache[conversationId] = loadedItems;
    bool canFetch = (messages.size() >= 40);
    m_canFetchMoreCache[conversationId] = canFetch;

    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) {
        return;
    }

    m_canFetchMore = canFetch;
    m_isLoadingMore = false;

    if (m_items.empty()) {
        beginResetModel();
        m_items = std::move(loadedItems);
        recalculateBlocks();
        endResetModel();
        emit canFetchMoreChanged();
        emit isLoadingMoreChanged();
        emit countChanged();
        emit conversationReady(conversationId);
    } else {
        mergeMessages(std::move(loadedItems));
        emit canFetchMoreChanged();
        emit isLoadingMoreChanged();
        emit conversationReady(conversationId);
    }
}

void ChatMessageModel::mergeMessages(std::vector<MessageItem> &&newItems) {
    if (newItems.empty()) return;

    QHash<QString, size_t> existingMap;
    for (size_t i = 0; i < m_items.size(); ++i) {
        existingMap.insert(m_items[i].id, i);
    }

    std::vector<MessageItem> toInsert;
    for (auto &item : newItems) {
        if (existingMap.contains(item.id)) {
            size_t idx = existingMap.value(item.id);
            bool statusChanged = (m_items[idx].status != item.status || m_items[idx].errorText != item.errorText);
            bool progressChanged = (m_items[idx].transferProgress != item.transferProgress || m_items[idx].transferBytes != item.transferBytes);
            bool textChanged = (m_items[idx].text != item.text);
            if (statusChanged || progressChanged || textChanged) {
                m_items[idx].status = item.status;
                m_items[idx].errorText = item.errorText;
                m_items[idx].transferProgress = item.transferProgress;
                m_items[idx].transferBytes = item.transferBytes;
                m_items[idx].text = item.text;
                QVector<int> roles;
                if (statusChanged) roles << StatusRole << ErrorTextRole;
                if (progressChanged) roles << TransferProgressRole << TransferBytesRole;
                if (textChanged) roles << TextRole;
                QModelIndex modelIdx = index(static_cast<int>(idx), 0);
                emit dataChanged(modelIdx, modelIdx, roles);
            }
        } else {
            toInsert.push_back(std::move(item));
        }
    }

    if (!toInsert.empty()) {
        for (auto &item : toInsert) {
            auto it = std::upper_bound(m_items.begin(), m_items.end(), item, [](const MessageItem &a, const MessageItem &b) {
                if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
                return a.id < b.id;
            });
            int insertIdx = static_cast<int>(std::distance(m_items.begin(), it));
            beginInsertRows(QModelIndex(), insertIdx, insertIdx);
            m_items.insert(it, std::move(item));
            endInsertRows();
        }
        recalculateBlocks();
        emit countChanged();
    }
}

void ChatMessageModel::onMessageAdded(const QString &conversationId, const QVariantMap &message) {
    MessageItem newItem = parseVariantMap(message);

    // Keep conversation cache up to date even if not currently focused
    if (!conversationId.isEmpty()) {
        auto &cached = m_conversationCache[conversationId];
        bool foundInCache = false;
        for (auto &cItem : cached) {
            if (cItem.id == newItem.id) {
                cItem = newItem;
                foundInCache = true;
                break;
            }
        }
        if (!foundInCache) {
            auto it = std::upper_bound(cached.begin(), cached.end(), newItem, [](const MessageItem &a, const MessageItem &b) {
                if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
                return a.id < b.id;
            });
            cached.insert(it, newItem);
        }
    }

    if (conversationId.compare(m_activeConversationId, Qt::CaseInsensitive) != 0) return;
    
    QString msgId = newItem.id;
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == msgId) {
            bool statusChanged = (m_items[i].status != newItem.status || m_items[i].errorText != newItem.errorText);
            bool progressChanged = (m_items[i].transferProgress != newItem.transferProgress || m_items[i].transferBytes != newItem.transferBytes);
            m_items[i] = newItem;
            recalculateBlocks();
            QVector<int> roles;
            if (statusChanged) roles << StatusRole << ErrorTextRole;
            if (progressChanged) roles << TransferProgressRole << TransferBytesRole;
            emit dataChanged(index(static_cast<int>(i), 0), index(static_cast<int>(i), 0), roles);
            return;
        }
    }
    
    auto it = std::upper_bound(m_items.begin(), m_items.end(), newItem, [](const MessageItem &a, const MessageItem &b) {
        if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
        return a.id < b.id;
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
    if (!conversationId.isEmpty() && m_conversationCache.contains(conversationId)) {
        auto &cached = m_conversationCache[conversationId];
        for (auto &item : cached) {
            if (messageId.isEmpty() || messageId == "all") {
                if (item.fromMe && item.status != status) {
                    item.status = status;
                    item.errorText = errorText;
                }
            } else if (item.id == messageId) {
                item.status = status;
                item.errorText = errorText;
                break;
            }
        }
    }

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
    if (!conversationId.isEmpty() && m_conversationCache.contains(conversationId)) {
        auto &cached = m_conversationCache[conversationId];
        cached.erase(std::remove_if(cached.begin(), cached.end(), [&](const MessageItem &it) {
            return it.id == messageId;
        }), cached.end());
    }

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

QString ChatMessageModel::generateUuid() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void ChatMessageModel::saveDraft(const QString &conversationId, const QString &text, const QVariantMap &attachment) {
    if (conversationId.isEmpty()) return;
    QString key = conversationId.toLower();
    m_draftTexts[key] = text;
    m_draftAttachments[key] = attachment;
}

QString ChatMessageModel::getDraftText(const QString &conversationId) const {
    if (conversationId.isEmpty()) return QString();
    return m_draftTexts.value(conversationId.toLower(), QString());
}

QVariantMap ChatMessageModel::getDraftAttachment(const QString &conversationId) const {
    if (conversationId.isEmpty()) return QVariantMap();
    return m_draftAttachments.value(conversationId.toLower(), QVariantMap());
}

void ChatMessageModel::saveScrollPosition(const QString &conversationId, qreal scrollPos, bool isAtBottom) {
    if (conversationId.isEmpty()) return;
    QString key = conversationId.trimmed().toLower();
    m_scrollPositions[key] = scrollPos;
    m_isAtBottomMap[key] = isAtBottom;
    if (!m_chatScrollPositions.contains(key)) {
        ScrollPosition pos;
        pos.messageId = QString();
        pos.pixelOffset = 0.0;
        pos.wasAtEnd = isAtBottom;
        m_chatScrollPositions[key] = pos;
    } else {
        m_chatScrollPositions[key].wasAtEnd = isAtBottom;
    }
}

qreal ChatMessageModel::getSavedScrollPosition(const QString &conversationId) const {
    if (conversationId.isEmpty()) return -1.0;
    return m_scrollPositions.value(conversationId.trimmed().toLower(), -1.0);
}

bool ChatMessageModel::isSavedAtBottom(const QString &conversationId) const {
    if (conversationId.isEmpty()) return true;
    return m_isAtBottomMap.value(conversationId.trimmed().toLower(), true);
}

bool ChatMessageModel::hasCachedConversation(const QString &conversationId) const {
    if (conversationId.isEmpty()) return false;
    QString key = conversationId.trimmed().toLower();
    if (key.compare(m_activeConversationId.trimmed().toLower()) == 0 && !m_items.empty()) return true;
    return m_conversationCache.contains(key) && !m_conversationCache.value(key).empty();
}

void ChatMessageModel::saveChatPosition(const QString &chatId, const QString &messageId, qreal offset, bool wasAtEnd) {
    if (chatId.isEmpty()) return;
    QString key = chatId.trimmed().toLower();
    ScrollPosition pos;
    pos.messageId = messageId;
    pos.pixelOffset = offset;
    pos.wasAtEnd = wasAtEnd;
    m_chatScrollPositions[key] = pos;
    m_isAtBottomMap[key] = wasAtEnd;
}

QVariantMap ChatMessageModel::getChatPosition(const QString &chatId) const {
    QVariantMap res;
    if (chatId.isEmpty()) {
        res["messageId"] = QString();
        res["lastVisibleMessageId"] = QString();
        res["pixelOffset"] = 0.0;
        res["offset"] = 0.0;
        res["wasAtEnd"] = true;
        return res;
    }
    QString key = chatId.trimmed().toLower();
    if (!m_chatScrollPositions.contains(key)) {
        res["messageId"] = QString();
        res["lastVisibleMessageId"] = QString();
        res["pixelOffset"] = 0.0;
        res["offset"] = 0.0;
        res["wasAtEnd"] = m_isAtBottomMap.value(key, true);
        return res;
    }
    const auto &pos = m_chatScrollPositions.value(key);
    res["messageId"] = pos.messageId;
    res["lastVisibleMessageId"] = pos.messageId;
    res["pixelOffset"] = pos.pixelOffset;
    res["offset"] = pos.pixelOffset;
    res["wasAtEnd"] = pos.wasAtEnd;
    return res;
}

bool ChatMessageModel::hasChatPosition(const QString &chatId) const {
    if (chatId.isEmpty()) return false;
    return m_chatScrollPositions.contains(chatId.trimmed().toLower());
}

void ChatMessageModel::saveScrollMemory(const QString &conversationId, const QString &lastVisibleMessageId, qreal pixelOffset, bool wasAtEnd) {
    saveChatPosition(conversationId, lastVisibleMessageId, pixelOffset, wasAtEnd);
}

QVariantMap ChatMessageModel::getScrollMemory(const QString &conversationId) const {
    return getChatPosition(conversationId);
}

bool ChatMessageModel::hasScrollMemory(const QString &conversationId) const {
    return hasChatPosition(conversationId);
}

int ChatMessageModel::indexOfMessageId(const QString &messageId) const {
    if (messageId.isEmpty()) return -1;
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == messageId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QString ChatMessageModel::getMessageIdAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return QString();
    }
    return m_items[index].id;
}