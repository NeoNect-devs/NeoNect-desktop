// src/core/chatmessagemodel.cpp
#include "chatmessagemodel.h"

ChatMessageModel::ChatMessageModel(QObject *parent) : QAbstractListModel(parent) {}

int ChatMessageModel::rowCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_items.size();
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const auto &item = m_items[index.row()];
    switch (role) {
        case TextRole: return item.text;
        case FromMeRole: return item.fromMe;
        case SenderNameRole: return item.senderName;
        case SenderAvatarRole: return item.senderAvatar;
        case FirstInBlockRole: return item.isFirstInBlock;
        case LastInBlockRole: return item.isLastInBlock;
    }
    return {};
}

QHash<int, QByteArray> ChatMessageModel::roleNames() const {
    return {
        { TextRole, "text" }, { FromMeRole, "fromMe" },
        { SenderNameRole, "senderName" }, { SenderAvatarRole, "senderAvatar" },
        { FirstInBlockRole, "isFirstInBlock" }, { LastInBlockRole, "isLastInBlock" }
    };
}

void ChatMessageModel::insertOutgoingMessage(const QString &text) {
    if (text.trimmed().isEmpty()) return;

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    
    bool isFirst = m_items.isEmpty() || !m_items.last().fromMe;
    if (!m_items.isEmpty() && m_items.last().fromMe) {
        m_items.last().isLastInBlock = false;
        QModelIndex idx = index(m_items.size() - 1);
        emit dataChanged(idx, idx, {LastInBlockRole});
    }

    m_items.append({text.trimmed(), true, "Me", "", isFirst, true});
    endInsertRows();
}

void ChatMessageModel::insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar) {
    if (text.trimmed().isEmpty()) return;

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    
    bool isFirst = m_items.isEmpty() || m_items.last().fromMe != fromMe || m_items.last().senderName != senderName;
    if (!m_items.isEmpty() && m_items.last().fromMe == fromMe && m_items.last().senderName == senderName) {
        m_items.last().isLastInBlock = false;
        QModelIndex idx = index(m_items.size() - 1);
        emit dataChanged(idx, idx, {LastInBlockRole});
    }

    m_items.append({text.trimmed(), fromMe, senderName, senderAvatar, isFirst, true});
    endInsertRows();
}

void ChatMessageModel::clearActiveViewportStore() {
    beginResetModel();
    m_items.clear();
    endResetModel();
}