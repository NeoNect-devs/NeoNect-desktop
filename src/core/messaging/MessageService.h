#pragma once
#include "Message.h"
#include "MessageStorage.h"
#include <memory>
#include <vector>
#include <QByteArray>
#include <QHash>
#include <QSet>
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
    using MultiDeviceResolverCallback = std::function<void(const QString& targetUser, std::function<void(QStringList)> resultCb)>;

    void setPreKeyClaimRequestCallback(PreKeyClaimRequestCallback cb) { m_preKeyClaimCb = cb; }
    void setDeviceResolverCallback(DeviceResolverCallback cb) { m_deviceResolverCb = cb; }
    void setMultiDeviceResolverCallback(MultiDeviceResolverCallback cb) { m_multiDeviceResolverCb = std::move(cb); }

    MessageService(std::weak_ptr<IMessageStorage> storage,
                   std::shared_ptr<Crypto::Session::SessionManager> sessionManager,
                   std::weak_ptr<IMessageQueue> offlineQueue = {});
    ~MessageService();

    void setStorage(std::weak_ptr<IMessageStorage> storage) { m_storage = std::move(storage); }
    void setOfflineQueue(std::weak_ptr<IMessageQueue> queue) { m_offlineQueue = std::move(queue); }

    bool sendMessage(Message& msg);

    bool persistIncomingMessage(const QString& sessionId, const QByteArray& plaintext);
    bool receiveMessage(const QString& sessionId, const QByteArray& plaintext);

    bool updateDeliveryState(const QString& messageId, MessageState newState);

    using DeliveryStateChangedCallback = std::function<void(const QString& messageId, MessageState newState, const QString& reason)>;
    void setOnMessageReceived(MessageReceivedCallback cb) { m_onMessageReceived = cb; }
    void setOnDeliveryStateChanged(DeliveryStateChangedCallback cb) { m_onDeliveryStateChanged = std::move(cb); }

    static QByteArray serializePayload(const Message& msg);

private:
    struct PendingOutboundMessage {
        QString msgId;
        QString receiver;
        QString targetDevice;
        QString sessionId;
        QByteArray rawPlaintext;
        bool isEphemeralControl;
    };

    static bool parseAndValidateIncoming(const QString& sessionId, const QByteArray& plaintext, Message& outMsg, bool& outIsEphemeralControl);
    void dispatchWithEstablishedSession(const PendingOutboundMessage& item);

    std::shared_ptr<MessageService*> m_lifeToken;
    std::weak_ptr<IMessageStorage> m_storage;
    std::shared_ptr<Crypto::Session::SessionManager> m_sessionManager;
    std::weak_ptr<IMessageQueue> m_offlineQueue;
    MessageReceivedCallback m_onMessageReceived;
    DeliveryStateChangedCallback m_onDeliveryStateChanged;
    PreKeyClaimRequestCallback m_preKeyClaimCb;
    DeviceResolverCallback m_deviceResolverCb;
    MultiDeviceResolverCallback m_multiDeviceResolverCb;
    QSet<QString> m_pendingSessionCreations;
    QHash<QString, std::vector<PendingOutboundMessage>> m_pendingSessionWaiters;
    QSet<QString> m_freshlyPersistedIds;
    QSet<QString> m_alreadyDeliveredIds;
};

}
}
}
