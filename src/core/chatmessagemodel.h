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
    bool isFirstUnread{false};
    qreal transferProgress{0.0};
    qint64 transferBytes{0};
};

class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
    Q_PROPERTY(bool isLoadingMore READ isLoadingMore NOTIFY isLoadingMoreChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum MessageRoles {
        TextRole = Qt::UserRole + 1,
        FromMeRole,
        SenderNameRole,
        SenderAvatarRole,
        FirstInBlockRole,
        LastInBlockRole,
        FirstUnreadRole,
        MessageIdRole,
        MessageTypeRole,
        MediaUrlRole,
        FileNameRole,
        FileSizeRole,
        DurationRole,
        WaveformRole,
        StatusRole,
        ErrorTextRole,
        TimestampRole,
        TransferProgressRole,
        TransferBytesRole
    };

    explicit ChatMessageModel(QObject *parent = nullptr);
    ~ChatMessageModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int count() const { return static_cast<int>(m_items.size()); }
    bool canFetchMore() const { return m_canFetchMore; }
    bool isLoadingMore() const { return m_isLoadingMore; }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void insertOutgoingMessage(const QString &text);
    Q_INVOKABLE void insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar,
                                  const QString &messageType = "text", const QString &mediaUrl = "",
                                  const QString &fileName = "", qint64 fileSize = 0, int duration = 0,
                                  const QVariantList &waveform = {}, const QString &status = "sent",
                                  const QString &id = "", qint64 timestamp = 0, const QString &errorText = "");
    Q_INVOKABLE void insertMessageItem(const QVariantMap &itemMap);
    Q_INVOKABLE void updateMessageStatus(const QString &messageId, const QString &status, const QString &errorText = "");
    Q_INVOKABLE void removeMessage(const QString &messageId);
    Q_INVOKABLE QVariantMap getMessageById(const QString &messageId) const;
    Q_INVOKABLE void clearActiveViewportStore();
    Q_INVOKABLE qint64 oldestTimestamp() const;
    Q_INVOKABLE void prependMessages(const QString &conversationId, const QVariantList &messages);
    Q_INVOKABLE void setFirstUnreadMessageId(const QString &messageId);
    Q_INVOKABLE void setFirstUnreadIndex(int index);
    Q_INVOKABLE void clearFirstUnread();
    Q_INVOKABLE void updateTransferProgress(const QString &messageId, qreal progress, qint64 bytes, qint64 totalBytes = 0);

    void addMessage(MessageItem &&item);

public slots:
    void onConversationLoaded(const QString &conversationId, const QVariantList &messages);
    void onMoreMessagesLoaded(const QString &conversationId, const QVariantList &messages);
    void onMessageAdded(const QString &conversationId, const QVariantMap &message);
    void onMessageUpdated(const QString &conversationId, const QString &messageId, const QString &status, const QString &errorText);
    void onMessageRemoved(const QString &conversationId, const QString &messageId);
    void onMediaTransferProgress(const QString &conversationId, const QString &messageId, qreal progress, qint64 bytesTransferred, qint64 totalBytes);
    void setActiveConversation(const QString &conversationId);

signals:
    void retryRequested(const QString &messageId, const QVariantMap &messageData);
    void canFetchMoreChanged();
    void isLoadingMoreChanged();
    void countChanged();

private:
    std::vector<MessageItem> m_items;
    QString m_activeConversationId;
    bool m_canFetchMore{false};
    bool m_isLoadingMore{false};
    void recalculateBlocks();
    MessageItem parseVariantMap(const QVariantMap &map) const;
};