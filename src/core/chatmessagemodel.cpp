// src/core/chatmessagemodel.cpp
#include "chatmessagemodel.h"

ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_items.reserve(64);
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
        { LastInBlockRole, "isLastInBlock" }
    };
}

void ChatMessageModel::insertOutgoingMessage(const QString &text) {
    if (text.trimmed().isEmpty()) return;
    insertMessage(text.trimmed(), true, "Me", "");
}

void ChatMessageModel::insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar) {
    if (text.trimmed().isEmpty()) return;

    int newIndex = static_cast<int>(m_items.size());
    beginInsertRows(QModelIndex(), newIndex, newIndex);

    bool isFirst = m_items.empty() || (m_items.back().fromMe != fromMe) || (m_items.back().senderName != senderName);

    if (!m_items.empty() && (m_items.back().fromMe == fromMe) && (m_items.back().senderName == senderName)) {
        m_items.back().isLastInBlock = false;
        QModelIndex prevIdx = index(static_cast<int>(m_items.size()) - 1);
        emit dataChanged(prevIdx, prevIdx, {LastInBlockRole});
    }

    MessageItem item;
    item.text = text.trimmed();
    item.fromMe = fromMe;
    item.senderName = senderName;
    item.senderAvatar = senderAvatar;
    item.isFirstInBlock = isFirst;
    item.isLastInBlock = true;

    m_items.push_back(std::move(item));
    endInsertRows();
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