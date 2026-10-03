#pragma once
#include <QString>
#include <QByteArray>
#include <vector>
#include <optional>

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
    virtual std::vector<QueueEntry> getPendingEntries(int limit = 50, qint64 afterCreatedAt = -1, const QString& afterMessageId = QString()) = 0; // returns entries in QUEUED, SENDING, ACK_PENDING
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
