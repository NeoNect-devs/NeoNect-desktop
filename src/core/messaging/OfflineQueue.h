#pragma once
#include <QString>
#include <QThread>
#include <QByteArray>
#include <vector>
#include <functional>
#include <optional>
#include <memory>
#include "IMessageQueue.h"

namespace NeoNect {
namespace Core {
namespace Messaging {

class MessageQueue : public IMessageQueue {
public:
    explicit MessageQueue(const QString& dbPath);
    ~MessageQueue() override;

    bool enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) override;
    bool updateState(const QString& messageId, QueueState newState) override;
    bool incrementRetry(const QString& messageId) override;
    std::optional<QueueEntry> getEntry(const QString& messageId) override;
    std::vector<QueueEntry> getPendingEntries(int limit = 50, qint64 afterCreatedAt = -1, const QString& afterMessageId = QString()) override;

private:
    QString m_dbPath;
    QString m_connectionName;
    QThread* m_owningThread;
    void initDatabase();
    bool isValidTransition(QueueState from, QueueState to) const;
};

class OfflineQueueService {
public:
    using RelaySendCallback = std::function<bool(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes)>;

    OfflineQueueService(std::shared_ptr<IMessageQueue> queue, RelaySendCallback relaySendCb);
    
    // Simulates what SessionManager used to call directly
    bool onEnvelopeReady(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes);

    // Processes ACK from network
    void handleAck(const QString& messageId);
    
    // Processes transport failure (e.g. from RelayService)
    void handleTransportFailure(const QString& messageId);

    // Processes lost ACK (timeout)
    void handleTimeout(const QString& messageId);

    // Resumes processing of pending messages, e.g. on restart
    void resume();

    static constexpr int MAX_RETRIES = 5;

private:
    std::shared_ptr<IMessageQueue> m_queue;
    RelaySendCallback m_relaySendCb;
    
    void processEntry(QueueEntry& entry);
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
