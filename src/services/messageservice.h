#pragma once
#include <QObject>
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
    
    // Load a conversation (UI calls this to fetch history)
    Q_INVOKABLE void loadConversation(const QString &conversationId);

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
    
    // Sent to RelayService to actually encrypt & transmit
    void transmitMessage(const NeoNect::Domain::Message &msg);

private:
    std::shared_ptr<Storage::IMessageRepository> m_repository;
    QString m_currentUserId;

    QVariantMap domainToVariantMap(const Domain::Message &msg) const;
};

}
}
