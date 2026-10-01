#pragma once
#include <QString>
#include <QByteArray>
#include <vector>
#include <functional>
#include <optional>
#include <memory>

namespace NeoNect {
namespace Core {
namespace Messaging {

enum class QueueState {
    QUEUED = 0,
    SENDING = 1,
    ACK_PENDING = 2,
    DELIVERED = 3,
    FAILED = 4
};

struct QueueEntry {
    QString messageId;
    QByteArray envelopeBytes;
    QString recipientUsername;
    QString recipientDeviceId;
    QueueState state;
    int retryCount;
    qint64 createdAt;
    qint64 updatedAt;
};

class IMessageQueue {
public:
    virtual ~IMessageQueue() = default;
    virtual bool enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) = 0;
    virtual bool updateState(const QString& messageId, QueueState newState) = 0;
    virtual bool incrementRetry(const QString& messageId) = 0;
    virtual std::optional<QueueEntry> getEntry(const QString& messageId) = 0;
    virtual std::vector<QueueEntry> getPendingEntries() = 0; // returns entries in QUEUED, SENDING, ACK_PENDING
};

class MessageQueue : public IMessageQueue {
public:
    explicit MessageQueue(const QString& dbPath);
    ~MessageQueue() override;

    bool enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) override;
    bool updateState(const QString& messageId, QueueState newState) override;
    bool incrementRetry(const QString& messageId) override;
    std::optional<QueueEntry> getEntry(const QString& messageId) override;
    std::vector<QueueEntry> getPendingEntries() override;

private:
    QString m_dbPath;
    QString m_connectionName;
    void initDatabase();
    bool isValidTransition(QueueState from, QueueState to) const;
};

class OfflineQueueService {
public:
    using RelaySendCallback = std::function<bool(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes)>;

    OfflineQueueService(std::shared_ptr<IMessageQueue> queue, RelaySendCallback relaySendCb);
    
    // Simulates what SessionManager used to call directly
    void onEnvelopeReady(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes);

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
