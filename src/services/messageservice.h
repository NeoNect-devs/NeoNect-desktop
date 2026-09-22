#pragma once
#include <QObject>
#include <QHash>
#include <memory>
#include "storage/imessagerepository.h"
#include "domain/message.h"

class QTimer;

namespace NeoNect {
namespace Services {

class MessageService : public QObject {
    Q_OBJECT
public:
    explicit MessageService(std::shared_ptr<Storage::IMessageRepository> repository, QObject* parent = nullptr);
    ~MessageService() override;

    // Send a message from the local user
    Q_INVOKABLE void sendMessage(const QString &conversationId, const QString &text, const QString &type = "text", 
                                 const QString &mediaUrl = "", const QString &fileName = "", qint64 fileSize = 0, 
                                 int duration = 0, const QVariantList &waveform = {});
    
    // Two-phase media request & approval protocol
    Q_INVOKABLE void sendMediaRequest(const QString &conversationId, const QString &text, const QString &mediaType,
                                      const QString &mediaUrl, const QString &fileName, qint64 fileSize);
    Q_INVOKABLE void acceptMediaRequest(const QString &conversationId, const QString &requestId);
    Q_INVOKABLE void declineMediaRequest(const QString &conversationId, const QString &requestId);
    
    // Load a conversation (UI calls this to fetch history)
    Q_INVOKABLE void loadConversation(const QString &conversationId);
    Q_INVOKABLE void loadMoreMessages(const QString &conversationId, qint64 beforeTimestamp, int limit = 30);

    // Ephemeral Typing Status (Direct Messages)
    Q_INVOKABLE void sendTyping(const QString &conversationId, bool isTyping);

    // Seen Receipts (Direct Messages)
    Q_INVOKABLE void sendSeenReceipt(const QString &conversationId, const QString &messageId = "all");

    // Real-time Peer Presence (Direct Messages)
    Q_INVOKABLE void sendPresenceStatus(const QString &targetUser, const QString &status);

    // Retry sending a previously failed message or media
    Q_INVOKABLE void retryMessage(const QString &messageId);

    // Cancel in-flight media transfer
    Q_INVOKABLE void cancelMediaTransfer(const QString &conversationId, const QString &messageId);

    // Provide the current user's ID so we can derive 'fromMe' logic if needed, or pass it to UI
    void setCurrentUserId(const QString &userId);

    // Invisible / Stealth mode support
    bool isInvisible() const { return m_isInvisible; }
    void setIsInvisible(bool invisible) { m_isInvisible = invisible; }

public slots:
    // Called by RelayService when a new message arrives from the network
    void handleIncomingMessage(const NeoNect::Domain::Message &msg);
    void handleIncomingMessages(const std::vector<NeoNect::Domain::Message> &msgs);
    // Called by RelayService when message delivery succeeds/fails
    void handleMessageDeliveryStatus(const QString &messageId, bool success, const QString &errorText);

signals:
    // UI (e.g. ChatMessageModel) listens to these to update itself
    void conversationLoaded(const QString &conversationId, const QVariantList &messages);
    void moreMessagesLoaded(const QString &conversationId, const QVariantList &messages);
    void messageAdded(const QString &conversationId, const QVariantMap &message);
    void messageUpdated(const QString &conversationId, const QString &messageId, const QString &status, const QString &errorText);
    void messageRemoved(const QString &conversationId, const QString &messageId);
    void mediaTransferProgress(const QString &conversationId, const QString &messageId, qreal progress, qint64 bytesTransferred, qint64 totalBytes);
    void peerTypingStatusChanged(const QString &conversationId, const QString &senderId, bool isTyping);
    
    // Sent to RelayService to actually encrypt & transmit
    void transmitMessage(const NeoNect::Domain::Message &msg);

private:
    std::shared_ptr<Storage::IMessageRepository> m_repository;
    QString m_currentUserId;
    QHash<QString, Domain::Message> m_outgoingMessages;
    QHash<QString, Domain::Message> m_pendingMediaRequests;
    QHash<QString, Domain::Message> m_receivedMediaRequests;
    QHash<QString, QTimer*> m_activeTransfers;
    bool m_isInvisible{false};

    QVariantMap domainToVariantMap(const Domain::Message &msg) const;
};

}
}
