// src/core/chatmessagemodel.cpp
#include "chatmessagemodel.h"
#include <QUuid>
#include <QDateTime>

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
        { MessageIdRole, "messageId" },
        { MessageTypeRole, "messageType" },
        { MediaUrlRole, "mediaUrl" },
        { FileNameRole, "fileName" },
        { FileSizeRole, "fileSize" },
        { DurationRole, "duration" },
        { WaveformRole, "waveform" },
        { StatusRole, "status" },
        { ErrorTextRole, "errorText" },
        { TimestampRole, "timestamp" }
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
                                    const QString &id, qint64 timestamp) {
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
    item.messageType = messageType.isEmpty() ? "text" : messageType;
    item.mediaUrl = mediaUrl;
    item.fileName = fileName;
    item.fileSize = fileSize;
    item.duration = duration;
    item.waveform = waveform;
    item.status = status.isEmpty() ? "sent" : status;
    item.errorText = "";
    item.timestamp = timestamp > 0 ? timestamp : QDateTime::currentSecsSinceEpoch();
    item.isFirstInBlock = isFirst;
    item.isLastInBlock = true;

    m_items.push_back(std::move(item));
    endInsertRows();
}

void ChatMessageModel::insertMessageItem(const QVariantMap &map) {
    QString text = map.value("text").toString();
    bool fromMe = map.value("fromMe", false).toBool();
    QString senderName = map.value("senderName", fromMe ? "Me" : "Anonymous").toString();
    QString senderAvatar = map.value("senderAvatar", "").toString();
    QString messageType = map.value("messageType", "text").toString();
    QString mediaUrl = map.value("mediaUrl", "").toString();
    QString fileName = map.value("fileName", "").toString();
    qint64 fileSize = map.value("fileSize", 0).toLongLong();
    int duration = map.value("duration", 0).toInt();
    QVariantList waveform = map.value("waveform").toList();
    QString status = map.value("status", "sent").toString();
    QString id = map.value("messageId", "").toString();
    qint64 timestamp = map.value("timestamp", 0).toLongLong();

    insertMessage(text, fromMe, senderName, senderAvatar, messageType, mediaUrl, fileName, fileSize, duration, waveform, status, id, timestamp);
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
}

void ChatMessageModel::clearActiveViewportStore() {
    beginResetModel();
    m_items.clear();
    endResetModel();
}