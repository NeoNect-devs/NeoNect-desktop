/**
 * @file chatmessagemodel.h
 * @brief High-performance QAbstractListModel implementation driving QML chat message lists.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Binds conversation message history to QML ListView elements. Incorporates smart bubble grouping
 * logic (`recalculateBlocks`), unread demarcation ribbons, two-phase transfer progress tracking,
 * and seamless pagination prepending without viewport jitter.
 *
 * @par Design Patterns:
 * - <b>Model-View Pattern (Qt MVVM)</b>: Inherits `QAbstractListModel` to expose C++ data structures to declarative QML components.
 * - <b>Block Grouping Pattern</b>: Automatically computes `firstInBlock` and `lastInBlock` flags for Telegram-style message clustering.
 * - <b>Observer Pattern</b>: Subscribes to `MessageService` events to trigger granular view updates.
 */

#pragma once
#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

/**
 * @struct MessageItem
 * @brief Internal presentation representation of a single chat bubble item.
 */
struct MessageItem {
    QString id;                 /**< Local message UUID. */
    QString text;               /**< Plaintext body or caption. */
    bool fromMe{false};         /**< True if authored by the local user. */
    QString senderName;         /**< Display name or username of sender. */
    QString senderAvatar;       /**< Local path or URL to sender avatar image. */
    QString messageType{"text"};/**< Content category (`"text"`, `"sticker"`, `"image"`, `"voice"`, `"file"`). */
    QString mediaUrl;           /**< Local or remote media file URL. */
    QString fileName;           /**< Attachment file name. */
    qint64 fileSize{0};         /**< Attachment size in bytes. */
    int duration{0};            /**< Playback length in seconds. */
    QVariantList waveform;      /**< Normalized audio waveform amplitudes. */
    QString status{"sent"};     /**< Delivery status string (`"sending"`, `"sent"`, `"failed"`, `"seen"`). */
    QString errorText;          /**< Diagnostic error message if failed. */
    qint64 timestamp{0};        /**< Unix epoch milliseconds. */
    bool isFirstInBlock{true};  /**< True if first message in a grouped author cluster. */
    bool isLastInBlock{true};   /**< True if last message in a grouped author cluster. */
    bool isFirstUnread{false};  /**< True if this bubble anchors the "Unread Messages" divider line. */
    qreal transferProgress{0.0};/**< Transfer progress percentage [0.0 - 1.0]. */
    qint64 transferBytes{0};    /**< Transferred bytes counter. */
    int mediaWidth{0};          /**< Natural width of attached media if known. */
    int mediaHeight{0};         /**< Natural height of attached media if known. */
};

/**
 * @struct ScrollPosition
 * @brief Anchors viewport scroll state to a concrete message ID rather than fragile raw pixels.
 */
struct ScrollPosition {
    QString messageId;            /**< Concrete message UUID visible at the top of the viewport. */
    qreal pixelOffset{0.0};       /**< Sub-pixel offset of anchor item relative to viewport top. */
    bool wasAtEnd{true};          /**< True if user was pinned to the bottom of the chat. */
};
using ScrollPositionMemory = ScrollPosition;

/**
 * @class ChatMessageModel
 * @brief List model providing reactive message list data to QML chat views.
 */
class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT

    /** @brief True if earlier historical messages exist in local storage to load. */
    Q_PROPERTY(bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
    /** @brief True while an asynchronous history pagination fetch is in progress. */
    Q_PROPERTY(bool isLoadingMore READ isLoadingMore NOTIFY isLoadingMoreChanged)
    /** @brief Current count of loaded messages in the active viewport. */
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /** @brief Active conversation ID. */
    Q_PROPERTY(QString activeConversationId READ activeConversationId NOTIFY activeConversationIdChanged)

public:
    /**
     * @enum MessageRoles
     * @brief Custom model role definitions mapped to QML delegate properties.
     */
    enum MessageRoles {
        TextRole = Qt::UserRole + 1, /**< Message text string. */
        FromMeRole,                  /**< Boolean indicating local ownership. */
        SenderNameRole,              /**< Sender display name. */
        SenderAvatarRole,            /**< Sender avatar image source. */
        FirstInBlockRole,            /**< Boolean indicating start of cluster. */
        LastInBlockRole,             /**< Boolean indicating end of cluster. */
        FirstUnreadRole,             /**< Boolean anchoring unread divider. */
        MessageIdRole,               /**< Local UUID string. */
        MessageTypeRole,             /**< Message type string. */
        MediaUrlRole,                /**< File or media URI. */
        FileNameRole,                /**< Attachment filename. */
        FileSizeRole,                /**< Attachment size in bytes. */
        DurationRole,                /**< Duration in seconds. */
        WaveformRole,                /**< Normalized waveform amplitude list. */
        StatusRole,                  /**< Message delivery status. */
        ErrorTextRole,               /**< Failure explanation string. */
        TimestampRole,               /**< Epoch timestamp. */
        TransferProgressRole,        /**< Real progress [0.0 - 1.0]. */
        TransferBytesRole,           /**< Integer transferred bytes. */
        MediaWidthRole,              /**< Natural media width. */
        MediaHeightRole              /**< Natural media height. */
    };

    /**
     * @brief Constructs the message list model.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit ChatMessageModel(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ChatMessageModel() override = default;

    /**
     * @brief Returns total number of rows currently loaded in model.
     * @param parent Unused for flat list models.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /** @brief Convenience accessor returning total message count. */
    int count() const { return static_cast<int>(m_items.size()); }
    /** @brief Checks if older history can be requested. */
    bool canFetchMore() const { return m_canFetchMore; }
    /** @brief Checks if older history is actively loading. */
    bool isLoadingMore() const { return m_isLoadingMore; }
    /** @brief Returns currently active conversation ID. */
    QString activeConversationId() const { return m_activeConversationId; }

    /**
     * @brief Queries model data for a given role and index.
     * @param index Model index representing message position.
     * @param role Target MessageRoles enumeration value.
     * @return Variant containing data mapped to role.
     */
    QVariant data(const QModelIndex &index, int role) const override;

    /**
     * @brief Exposes hash table of role identifiers to QML property strings.
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief Optimistically inserts a text message authored by the local user.
     * @param text Message body string.
     */
    Q_INVOKABLE void insertOutgoingMessage(const QString &text);

    /**
     * @brief Inserts an arbitrary message into the view model.
     */
    Q_INVOKABLE void insertMessage(const QString &text, bool fromMe, const QString &senderName, const QString &senderAvatar,
                                  const QString &messageType = "text", const QString &mediaUrl = "",
                                  const QString &fileName = "", qint64 fileSize = 0, int duration = 0,
                                  const QVariantList &waveform = {}, const QString &status = "sent",
                                  const QString &id = "", qint64 timestamp = 0, const QString &errorText = "");

    /**
     * @brief Inserts a structured message item map into the view model.
     * @param itemMap Property map describing message.
     */
    Q_INVOKABLE void insertMessageItem(const QVariantMap &itemMap);

    /**
     * @brief Updates status and error information for a message.
     * @param messageId Target UUID.
     * @param status New status string.
     * @param errorText Optional error description.
     */
    Q_INVOKABLE void updateMessageStatus(const QString &messageId, const QString &status, const QString &errorText = "");

    /**
     * @brief Removes a message by ID from the active list model.
     * @param messageId Target UUID.
     */
    Q_INVOKABLE void removeMessage(const QString &messageId);

    /**
     * @brief Searches model for a message by UUID and returns its properties.
     * @param messageId Target UUID.
     * @return Property map or empty map if not found.
     */
    Q_INVOKABLE QVariantMap getMessageById(const QString &messageId) const;

    /**
     * @brief Purges all messages from the model, clearing the active viewport.
     */
    Q_INVOKABLE void clearActiveViewportStore();

    /**
     * @brief Returns timestamp of the oldest message loaded in view.
     */
    Q_INVOKABLE qint64 oldestTimestamp() const;

    /**
     * @brief Prepends an older page of messages at the top of the list without jumping view.
     * @param conversationId Scope channel ID.
     * @param messages List of older message maps.
     */
    Q_INVOKABLE void prependMessages(const QString &conversationId, const QVariantList &messages);

    /**
     * @brief Anchors the "New / Unread Messages" divider above a specific message ID.
     * @param messageId Target UUID.
     */
    Q_INVOKABLE void setFirstUnreadMessageId(const QString &messageId);

    /**
     * @brief Anchors the unread divider at an explicit index.
     * @param index Row index.
     */
    Q_INVOKABLE void setFirstUnreadIndex(int index);

    /**
     * @brief Clears the unread messages divider flag across all items.
     */
    Q_INVOKABLE void clearFirstUnread();

    /**
     * @brief Updates transfer telemetry for an in-flight upload or download.
     * @param messageId Target UUID.
     * @param progress Progress percentage [0.0 - 1.0].
     * @param bytes Number of bytes transferred so far.
     * @param totalBytes Total file size.
     */
    Q_INVOKABLE void updateTransferProgress(const QString &messageId, qreal progress, qint64 bytes, qint64 totalBytes = 0);

    /**
     * @brief Generates an RFC 4122 UUID v4 string for optimistic message creation.
     */
    Q_INVOKABLE QString generateUuid() const;

    /**
     * @brief Saves the current user input draft text and attachment for a conversation.
     * @param conversationId Scope channel ID.
     * @param text Unfinished draft body text.
     * @param attachment Optional staged file attachment map.
     */
    Q_INVOKABLE void saveDraft(const QString &conversationId, const QString &text, const QVariantMap &attachment = {});

    /**
     * @brief Retrieves stored draft text for a conversation.
     */
    Q_INVOKABLE QString getDraftText(const QString &conversationId) const;

    /**
     * @brief Retrieves stored draft attachment map for a conversation.
     */
    Q_INVOKABLE QVariantMap getDraftAttachment(const QString &conversationId) const;

    /**
     * @brief Persists the active scroll offset and bottom-lock status for a conversation.
     * @param conversationId Scope channel ID.
     * @param scrollPos ListView contentY offset.
     * @param isAtBottom True if user was pinned to the bottom.
     */
    Q_INVOKABLE void saveScrollPosition(const QString &conversationId, qreal scrollPos, bool isAtBottom);

    /**
     * @brief Retrieves cached scroll position for a conversation (-1.0 if unset).
     */
    Q_INVOKABLE qreal getSavedScrollPosition(const QString &conversationId) const;

    /**
     * @brief Checks if conversation was left pinned to the bottom.
     */
    Q_INVOKABLE bool isSavedAtBottom(const QString &conversationId) const;

    /**
     * @brief Checks if a conversation is already cached in memory.
     */
    Q_INVOKABLE bool hasCachedConversation(const QString &conversationId) const;

    /**
     * @brief Persists anchor message ID, pixel offset, and bottom-pinned state for a conversation.
     * @param chatId Scope channel ID.
     * @param messageId UUID of top-most visible message in viewport.
     * @param offset Sub-pixel offset of message relative to top of viewport.
     * @param wasAtEnd True if view was pinned to the bottom of the feed.
     */
    Q_INVOKABLE void saveChatPosition(const QString &chatId, const QString &messageId, qreal offset, bool wasAtEnd);

    /**
     * @brief Retrieves stored scroll position memory for a conversation.
     * @param chatId Scope channel ID.
     * @return Map containing `messageId`, `lastVisibleMessageId`, `offset`, `pixelOffset`, and `wasAtEnd`.
     */
    Q_INVOKABLE QVariantMap getChatPosition(const QString &chatId) const;

    /**
     * @brief Checks if scroll position exists for a conversation.
     */
    Q_INVOKABLE bool hasChatPosition(const QString &chatId) const;

    /**
     * @brief Persists anchor message ID, pixel offset, and bottom-pinned state for a conversation (Backward-compatible).
     * @param conversationId Scope channel ID.
     * @param lastVisibleMessageId UUID of top-most visible message in viewport.
     * @param pixelOffset Sub-pixel offset of message relative to top of viewport.
     * @param wasAtEnd True if view was pinned to the bottom of the feed.
     */
    Q_INVOKABLE void saveScrollMemory(const QString &conversationId, const QString &lastVisibleMessageId, qreal pixelOffset, bool wasAtEnd);

    /**
     * @brief Retrieves stored scroll position memory for a conversation (Backward-compatible).
     * @param conversationId Scope channel ID.
     * @return Map containing `lastVisibleMessageId`, `pixelOffset`, and `wasAtEnd`.
     */
    Q_INVOKABLE QVariantMap getScrollMemory(const QString &conversationId) const;

    /**
     * @brief Checks if scroll memory exists for a conversation (Backward-compatible).
     */
    Q_INVOKABLE bool hasScrollMemory(const QString &conversationId) const;

    /**
     * @brief Returns row index of a message by UUID in the currently active conversation, or -1 if not found.
     * @param messageId Target message UUID.
     */
    Q_INVOKABLE int indexOfMessageId(const QString &messageId) const;

    /**
     * @brief Returns message UUID at row index in the active conversation, or empty string if out of bounds.
     * @param index Row index.
     */
    Q_INVOKABLE QString getMessageIdAt(int index) const;

    /**
     * @brief Internal fast-path appender for rvalue MessageItem instances.
     * @param item Rvalue reference to MessageItem.
     */
    void addMessage(MessageItem &&item);

public slots:
    /** @brief Slot handling conversation history loaded signal from MessageService. */
    void onConversationLoaded(const QString &conversationId, const QVariantList &messages);
    /** @brief Slot handling pagination history loaded signal. */
    void onMoreMessagesLoaded(const QString &conversationId, const QVariantList &messages);
    /** @brief Slot handling newly arrived message signal. */
    void onMessageAdded(const QString &conversationId, const QVariantMap &message);
    /** @brief Slot handling message delivery status updates. */
    void onMessageUpdated(const QString &conversationId, const QString &messageId, const QString &status, const QString &errorText);
    /** @brief Slot handling message deletion signal. */
    void onMessageRemoved(const QString &conversationId, const QString &messageId);
    /** @brief Slot handling transfer progress telemetry. */
    void onMediaTransferProgress(const QString &conversationId, const QString &messageId, qreal progress, qint64 bytesTransferred, qint64 totalBytes);
    /** @brief Configures the active conversation scope filter. */
    void setActiveConversation(const QString &conversationId);

signals:
    /** @brief Emitted when user clicks retry on a failed message bubble. */
    void retryRequested(const QString &messageId, const QVariantMap &messageData);
    /** @brief Emitted when canFetchMore property changes. */
    void canFetchMoreChanged();
    /** @brief Emitted when isLoadingMore property changes. */
    void isLoadingMoreChanged();
    /** @brief Emitted when item count changes. */
    void countChanged();
    /** @brief Emitted when a conversation's initial batch or cached history is fully loaded into the model. */
    void conversationReady(const QString &conversationId);
    /** @brief Emitted when active conversation scope changes. */
    void activeConversationIdChanged();

private:
    /** @brief Re-computes block cluster flags (`isFirstInBlock`, `isLastInBlock`) across items. */
    void recalculateBlocks();
    /** @brief Converts variant map into typed internal MessageItem struct. */
    MessageItem parseVariantMap(const QVariantMap &map) const;
    /** @brief Idempotently merges loaded messages without triggering full model resets. */
    void mergeMessages(std::vector<MessageItem> &&newItems);

    std::vector<MessageItem> m_items;
    QString m_activeConversationId;
    bool m_canFetchMore{false};
    bool m_isLoadingMore{false};

    // In-memory conversation state persistence
    QHash<QString, std::vector<MessageItem>> m_conversationCache;
    QHash<QString, bool> m_canFetchMoreCache;
    QHash<QString, qreal> m_scrollPositions;
    QHash<QString, bool> m_isAtBottomMap;
    QHash<QString, ScrollPosition> m_chatScrollPositions;
    QHash<QString, QString> m_draftTexts;
    QHash<QString, QVariantMap> m_draftAttachments;
};