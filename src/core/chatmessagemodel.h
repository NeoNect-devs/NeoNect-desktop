// src/core/chatmessagemodel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <vector>

struct MessageItem {
    QString text;
    bool fromMe{false};
    QString senderName;
    QString senderAvatar;
    bool isFirstInBlock{true};
    bool isLastInBlock{true};
};

class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum MessageRoles {
        TextRole = Qt::UserRole + 1,
        FromMeRole,
        SenderNameRole,
        SenderAvatarRole,
        FirstInBlockRole,
        LastInBlockRole
    };

    explicit ChatMessageModel(QObject *parent = nullptr);
    ~ChatMessageModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void insertOutgoingMessage(const QString &text);
    Q_INVOKABLE void insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar);
    Q_INVOKABLE void clearActiveViewportStore();

    void addMessage(MessageItem &&item);

private:
    std::vector<MessageItem> m_items;
};