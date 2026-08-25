// src/core/chatmessagemodel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct MessageItem {
    QString text;
    bool fromMe;
    QString senderName;
    QString senderAvatar;
    bool isFirstInBlock;
    bool isLastInBlock;
};

class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum MessageRoles { TextRole = Qt::UserRole + 1, FromMeRole, SenderNameRole, SenderAvatarRole, FirstInBlockRole, LastInBlockRole };

    explicit ChatMessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void insertOutgoingMessage(const QString &text);
    Q_INVOKABLE void insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar);
    Q_INVOKABLE void clearActiveViewportStore();

private:
    QVector<MessageItem> m_items;
};