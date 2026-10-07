#pragma once
#include "Message.h"
#include "MessageStorage.h"
#include <memory>
#include <QByteArray>
#include <functional>
#include "../../crypto/session/SessionManager.h"
#include "../../transport/IIncomingEnvelopeHandler.h"
#include "IMessageQueue.h"

namespace NeoNect {
namespace Core {
namespace Messaging {

class MessageService : public std::enable_shared_from_this<MessageService> {
public:
    using MessageReceivedCallback = std::function<void(const Message&)>;
    using PreKeyClaimCallback = std::function<void(std::optional<Crypto::X3DH::BobPreKeyBundle>)>;
    using PreKeyClaimRequestCallback = std::function<void(const QString& targetUser, const QString& targetDevice, PreKeyClaimCallback resultCb)>;

    using DeviceResolverCallback = std::function<void(const QString& targetUser, std::function<void(std::optional<QString>)> resultCb)>;

    void setPreKeyClaimRequestCallback(PreKeyClaimRequestCallback cb) { m_preKeyClaimCb = cb; }
    void setDeviceResolverCallback(DeviceResolverCallback cb) { m_deviceResolverCb = cb; }

    MessageService(std::weak_ptr<IMessageStorage> storage,
                   std::shared_ptr<Crypto::Session::SessionManager> sessionManager,
                   std::weak_ptr<IMessageQueue> offlineQueue = {});

    // Temporary compatibility bridges for Phase 2 StorageContext
    void setStorage(std::weak_ptr<IMessageStorage> storage) { m_storage = std::move(storage); }
    void setOfflineQueue(std::weak_ptr<IMessageQueue> queue) { m_offlineQueue = std::move(queue); }

    // Sends a message
    bool sendMessage(Message& msg);

    // Receives a validated plaintext message from SessionManager (callback bound)
    void receiveMessage(const QString& sessionId, const QByteArray& plaintext);

    // Updates delivery state (e.g. from network acks)
    bool updateDeliveryState(const QString& messageId, MessageState newState);

    void setOnMessageReceived(MessageReceivedCallback cb) { m_onMessageReceived = cb; }

    // Exposed for testing
    static QByteArray serializePayload(const Message& msg);

private:
    std::weak_ptr<IMessageStorage> m_storage;
    std::shared_ptr<Crypto::Session::SessionManager> m_sessionManager;
    std::weak_ptr<IMessageQueue> m_offlineQueue;
    MessageReceivedCallback m_onMessageReceived;
    PreKeyClaimRequestCallback m_preKeyClaimCb;
    DeviceResolverCallback m_deviceResolverCb;
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
