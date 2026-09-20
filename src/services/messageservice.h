#pragma once
#include <QObject>
#include <QHash>
#include <memory>
#include "storage/imessagerepository.h"
#include "domain/message.h"

namespace NeoNect {
namespace Services {

class MessageService : public QObject {
    Q_OBJECT
public:
    explicit MessageService(std::shared_ptr<Storage::IMessageRepository> repository, QObject* parent = nullptr);
    ~MessageService() override = default;

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

    // Ephemeral Typing Status (Direct Messages)
    Q_INVOKABLE void sendTyping(const QString &conversationId, bool isTyping);

    // Provide the current user's ID so we can derive 'fromMe' logic if needed, or pass it to UI
    void setCurrentUserId(const QString &userId);

public slots:
    // Called by RelayService when a new message arrives from the network
    void handleIncomingMessage(const NeoNect::Domain::Message &msg);
    void handleIncomingMessages(const std::vector<NeoNect::Domain::Message> &msgs);
    // Called by RelayService when message delivery succeeds/fails
    void handleMessageDeliveryStatus(const QString &messageId, bool success, const QString &errorText);

signals:
    // UI (e.g. ChatMessageModel) listens to these to update itself
    void conversationLoaded(const QString &conversationId, const QVariantList &messages);
    void messageAdded(const QString &conversationId, const QVariantMap &message);
    void messageUpdated(const QString &conversationId, const QString &messageId, const QString &status, const QString &errorText);
    void messageRemoved(const QString &conversationId, const QString &messageId);
    void peerTypingStatusChanged(const QString &conversationId, const QString &senderId, bool isTyping);
    
    // Sent to RelayService to actually encrypt & transmit
    void transmitMessage(const NeoNect::Domain::Message &msg);

private:
    std::shared_ptr<Storage::IMessageRepository> m_repository;
    QString m_currentUserId;
    QHash<QString, Domain::Message> m_pendingMediaRequests;
    QHash<QString, Domain::Message> m_receivedMediaRequests;

    QVariantMap domainToVariantMap(const Domain::Message &msg) const;
};

}
}
