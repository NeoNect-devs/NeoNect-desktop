#include "OfflineQueue.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>
#include <QDateTime>

namespace NeoNect {
namespace Core {
namespace Messaging {

MessageQueue::MessageQueue(const QString& dbPath) : m_dbPath(dbPath) {
    m_connectionName = "queue_" + QUuid::createUuid().toString();
    initDatabase();
}

MessageQueue::~MessageQueue() {
    QSqlDatabase::removeDatabase(m_connectionName);
}

void MessageQueue::initDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) return;

    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS offline_queue ("
               "message_id TEXT PRIMARY KEY, "
               "envelope_bytes BLOB, "
               "recipient_username TEXT, "
               "recipient_device_id TEXT, "
               "state INTEGER, "
               "retry_count INTEGER, "
               "created_at INTEGER, "
               "updated_at INTEGER)");
}

bool MessageQueue::isValidTransition(QueueState from, QueueState to) const {
    if (from == to) return true;
    switch(from) {
        case QueueState::QUEUED:
            return to == QueueState::SENDING || to == QueueState::FAILED;
        case QueueState::SENDING:
            return to == QueueState::ACK_PENDING || to == QueueState::QUEUED || to == QueueState::FAILED || to == QueueState::DELIVERED;
        case QueueState::ACK_PENDING:
            return to == QueueState::DELIVERED || to == QueueState::QUEUED || to == QueueState::FAILED;
        case QueueState::DELIVERED:
            return false;
        case QueueState::FAILED:
            return to == QueueState::QUEUED;
        default:
            return false;
    }
}

bool MessageQueue::enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    // Reject duplicate enqueue
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT 1 FROM offline_queue WHERE message_id = :id");
    checkQuery.bindValue(":id", messageId);
    if (checkQuery.exec() && checkQuery.next()) {
        return false;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query(db);
    query.prepare("INSERT INTO offline_queue (message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at) "
                  "VALUES (:id, :bytes, :user, :device, :state, 0, :created, :updated)");
    query.bindValue(":id", messageId);
    query.bindValue(":bytes", envelopeBytes);
    query.bindValue(":user", recipientUsername);
    query.bindValue(":device", recipientDeviceId);
    query.bindValue(":state", static_cast<int>(QueueState::QUEUED));
    query.bindValue(":created", now);
    query.bindValue(":updated", now);
    return query.exec();
}

bool MessageQueue::updateState(const QString& messageId, QueueState newState) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    auto optEntry = getEntry(messageId);
    if (!optEntry) return false;

    if (!isValidTransition(optEntry->state, newState)) return false;
    if (optEntry->state == newState) return true;

    QSqlQuery query(db);
    query.prepare("UPDATE offline_queue SET state = :state, updated_at = :updated WHERE message_id = :id");
    query.bindValue(":state", static_cast<int>(newState));
    query.bindValue(":updated", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

bool MessageQueue::incrementRetry(const QString& messageId) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;
    
    QSqlQuery query(db);
    query.prepare("UPDATE offline_queue SET retry_count = retry_count + 1, updated_at = :updated WHERE message_id = :id");
    query.bindValue(":updated", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::optional<QueueEntry> MessageQueue::getEntry(const QString& messageId) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return std::nullopt;

    QSqlQuery query(db);
    query.prepare("SELECT message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at "
                  "FROM offline_queue WHERE message_id = :id");
    query.bindValue(":id", messageId);
    if (query.exec() && query.next()) {
        QueueEntry entry;
        entry.messageId = query.value(0).toString();
        entry.envelopeBytes = query.value(1).toByteArray();
        entry.recipientUsername = query.value(2).toString();
        entry.recipientDeviceId = query.value(3).toString();
        entry.state = static_cast<QueueState>(query.value(4).toInt());
        entry.retryCount = query.value(5).toInt();
        entry.createdAt = query.value(6).toLongLong();
        entry.updatedAt = query.value(7).toLongLong();
        return entry;
    }
    return std::nullopt;
}

std::vector<QueueEntry> MessageQueue::getPendingEntries() {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return {};

    std::vector<QueueEntry> results;
    QSqlQuery query(db);
    // Load anything not DELIVERED and not FAILED
    query.prepare("SELECT message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at "
                  "FROM offline_queue WHERE state IN (0, 1, 2) ORDER BY created_at ASC");
    if (query.exec()) {
        while (query.next()) {
            QueueEntry entry;
            entry.messageId = query.value(0).toString();
            entry.envelopeBytes = query.value(1).toByteArray();
            entry.recipientUsername = query.value(2).toString();
            entry.recipientDeviceId = query.value(3).toString();
            entry.state = static_cast<QueueState>(query.value(4).toInt());
            entry.retryCount = query.value(5).toInt();
            entry.createdAt = query.value(6).toLongLong();
            entry.updatedAt = query.value(7).toLongLong();
            results.push_back(entry);
        }
    }
    return results;
}

OfflineQueueService::OfflineQueueService(std::shared_ptr<IMessageQueue> queue, RelaySendCallback relaySendCb)
    : m_queue(std::move(queue)), m_relaySendCb(std::move(relaySendCb)) {
}

void OfflineQueueService::onEnvelopeReady(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes) {
    if (!m_queue->enqueue(messageId, envelopeBytes, recipientUsername, recipientDeviceId)) {
        return; // failed to queue or duplicate
    }
    
    auto optEntry = m_queue->getEntry(messageId);
    if (optEntry) {
        processEntry(*optEntry);
    }
}

void OfflineQueueService::processEntry(QueueEntry& entry) {
    if (entry.state == QueueState::QUEUED || entry.state == QueueState::SENDING || entry.state == QueueState::ACK_PENDING) {
        if (entry.retryCount >= MAX_RETRIES) {
            m_queue->updateState(entry.messageId, QueueState::FAILED);
            return;
        }

        m_queue->updateState(entry.messageId, QueueState::SENDING);
        
        bool success = false;
        if (m_relaySendCb) {
            success = m_relaySendCb(entry.recipientUsername, entry.recipientDeviceId, entry.messageId, entry.envelopeBytes);
        }

        if (success) {
            m_queue->updateState(entry.messageId, QueueState::ACK_PENDING);
        } else {
            // Transport failed immediately
            handleTransportFailure(entry.messageId);
        }
    }
}

void OfflineQueueService::handleAck(const QString& messageId) {
    auto optEntry = m_queue->getEntry(messageId);
    if (!optEntry) return;

    if (optEntry->state == QueueState::DELIVERED) return; // duplicate ACK idempotent

    m_queue->updateState(messageId, QueueState::DELIVERED);
}

void OfflineQueueService::handleTransportFailure(const QString& messageId) {
    auto optEntry = m_queue->getEntry(messageId);
    if (!optEntry) return;

    m_queue->incrementRetry(messageId);
    optEntry = m_queue->getEntry(messageId); // reload to get new retry count

    if (optEntry->retryCount >= MAX_RETRIES) {
        m_queue->updateState(messageId, QueueState::FAILED);
    } else {
        m_queue->updateState(messageId, QueueState::QUEUED);
    }
}

void OfflineQueueService::handleTimeout(const QString& messageId) {
    auto optEntry = m_queue->getEntry(messageId);
    if (!optEntry) return;

    // Only retry if it's still waiting for ACK
    if (optEntry->state == QueueState::ACK_PENDING) {
        m_queue->incrementRetry(messageId);
        optEntry = m_queue->getEntry(messageId); // reload

        if (optEntry->retryCount >= MAX_RETRIES) {
            m_queue->updateState(messageId, QueueState::FAILED);
        } else {
            m_queue->updateState(messageId, QueueState::QUEUED);
            // Optionally could call processEntry immediately, but usually rely on retryPending
        }
    }
}

void OfflineQueueService::resume() {
    auto pending = m_queue->getPendingEntries();
    for (auto& entry : pending) {
        // Reset state to QUEUED on restart so we resend instead of getting stuck in SENDING/ACK_PENDING
        m_queue->updateState(entry.messageId, QueueState::QUEUED);
        entry.state = QueueState::QUEUED;
        processEntry(entry);
    }
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
