// src/core/chatmessagemodel.h
#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

struct MessageItem {
    QString id;
    QString text;
    bool fromMe{false};
    QString senderName;
    QString senderAvatar;
    QString messageType{"text"}; // "text", "sticker", "image", "video", "audio", "voice", "file"
    QString mediaUrl;
    QString fileName;
    qint64 fileSize{0};
    int duration{0};
    QVariantList waveform;
    QString status{"sent"}; // "sending", "sent", "failed"
    QString errorText;
    qint64 timestamp{0};
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
        LastInBlockRole,
        MessageIdRole,
        MessageTypeRole,
        MediaUrlRole,
        FileNameRole,
        FileSizeRole,
        DurationRole,
        WaveformRole,
        StatusRole,
        ErrorTextRole,
        TimestampRole
    };

    explicit ChatMessageModel(QObject *parent = nullptr);
    ~ChatMessageModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void insertOutgoingMessage(const QString &text);
    Q_INVOKABLE void insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar,
                                  const QString &messageType = "text", const QString &mediaUrl = "",
                                  const QString &fileName = "", qint64 fileSize = 0, int duration = 0,
                                  const QVariantList &waveform = {}, const QString &status = "sent",
                                  const QString &id = "", qint64 timestamp = 0);
    Q_INVOKABLE void insertMessageItem(const QVariantMap &itemMap);
    Q_INVOKABLE void updateMessageStatus(const QString &messageId, const QString &status, const QString &errorText = "");
    Q_INVOKABLE void removeMessage(const QString &messageId);
    Q_INVOKABLE QVariantMap getMessageById(const QString &messageId) const;
    Q_INVOKABLE void clearActiveViewportStore();

    void addMessage(MessageItem &&item);

signals:
    void retryRequested(const QString &messageId, const QVariantMap &messageData);

private:
    std::vector<MessageItem> m_items;
};