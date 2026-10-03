#include "OfflineQueue.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QUuid>
#include <QVariant>
#include <QDateTime>
#include <QThread>
#include <QtAssert>

namespace NeoNect {
namespace Core {
namespace Messaging {

MessageQueue::MessageQueue(const QString& dbPath) : m_dbPath(dbPath) {
    m_connectionName = QUuid::createUuid().toString();
    m_owningThread = QThread::currentThread();
    initDatabase();
}

MessageQueue::~MessageQueue() {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
    QSqlDatabase::removeDatabase(m_connectionName);
}

void MessageQueue::initDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (db.open()) {
        QSqlQuery pragmaQuery(db);
        if (!pragmaQuery.exec("PRAGMA journal_mode = WAL;")) {
            qWarning() << "Failed to set WAL mode for OfflineQueue:" << pragmaQuery.lastError().text();
        }
        if (!pragmaQuery.exec("PRAGMA synchronous = NORMAL;")) {
            qWarning() << "Failed to set synchronous mode for OfflineQueue:" << pragmaQuery.lastError().text();
        }
        if (!pragmaQuery.exec("PRAGMA busy_timeout = 5000;")) {
            qWarning() << "Failed to set busy_timeout for OfflineQueue:" << pragmaQuery.lastError().text();
        }
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
}

bool MessageQueue::isValidTransition(QueueState from, QueueState to) const {
    switch (from) {
        case QueueState::QUEUED:
            return to == QueueState::SENDING || to == QueueState::FAILED;
        case QueueState::SENDING:
            return to == QueueState::ACK_PENDING || to == QueueState::QUEUED || to == QueueState::FAILED || to == QueueState::DELIVERED;
        case QueueState::ACK_PENDING:
            return to == QueueState::DELIVERED || to == QueueState::QUEUED || to == QueueState::FAILED;
        case QueueState::DELIVERED:
            return false; // terminal
        case QueueState::FAILED:
            return to == QueueState::QUEUED; // allow manual/forced retry
    }
    return false;
}

bool MessageQueue::enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    // Check if it already exists
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT 1 FROM offline_queue WHERE message_id = :id");
    checkQuery.bindValue(":id", messageId);
    if (checkQuery.exec() && checkQuery.next()) {
        return false; // already enqueued, do not duplicate
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO offline_queue (message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at) "
                  "VALUES (:id, :env, :usr, :dev, :state, 0, :created, :updated)");
    query.bindValue(":id", messageId);
    query.bindValue(":env", envelopeBytes);
    query.bindValue(":usr", recipientUsername);
    query.bindValue(":dev", recipientDeviceId);
    query.bindValue(":state", static_cast<int>(QueueState::QUEUED));
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.bindValue(":created", now);
    query.bindValue(":updated", now);
    
    return query.exec();
}

bool MessageQueue::updateState(const QString& messageId, QueueState newState) {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    // Verify valid transition
    auto optEntry = getEntry(messageId);
    if (!optEntry) return false;
    
    if (!isValidTransition(optEntry->state, newState)) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE offline_queue SET state = :state, updated_at = :updated WHERE message_id = :id");
    query.bindValue(":state", static_cast<int>(newState));
    query.bindValue(":updated", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

bool MessageQueue::incrementRetry(const QString& messageId) {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE offline_queue SET retry_count = retry_count + 1, updated_at = :updated WHERE message_id = :id");
    query.bindValue(":updated", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::optional<QueueEntry> MessageQueue::getEntry(const QString& messageId) {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
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

std::vector<QueueEntry> MessageQueue::getPendingEntries(int limit, qint64 afterCreatedAt, const QString& afterMessageId) {
    Q_ASSERT(QThread::currentThread() == m_owningThread);
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return {};

    std::vector<QueueEntry> results;
    QSqlQuery query(db);
    
    QString sql = "SELECT message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at "
                  "FROM offline_queue WHERE state IN (0, 1, 2) ";
                  
    if (afterCreatedAt != -1 && !afterMessageId.isEmpty()) {
        sql += "AND (created_at > :after_created_at OR (created_at = :after_created_at AND message_id > :after_message_id)) ";
    }
    
    sql += "ORDER BY created_at ASC, message_id ASC";
    if (limit > 0) {
        sql += " LIMIT :limit";
    }
    
    query.prepare(sql);
    
    if (afterCreatedAt != -1 && !afterMessageId.isEmpty()) {
        query.bindValue(":after_created_at", afterCreatedAt);
        query.bindValue(":after_message_id", afterMessageId);
    }
    if (limit > 0) {
        query.bindValue(":limit", limit);
    }
    
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

    if (optEntry->state == QueueState::SENDING || optEntry->state == QueueState::ACK_PENDING) {
        m_queue->incrementRetry(messageId);
        
        // If max retries reached, fail it, else queue it again for retry
        if (optEntry->retryCount + 1 >= MAX_RETRIES) {
            m_queue->updateState(messageId, QueueState::FAILED);
        } else {
            m_queue->updateState(messageId, QueueState::QUEUED);
            // Optionally could call processEntry immediately, but usually rely on retryPending
        }
    }
}

void OfflineQueueService::handleTimeout(const QString& messageId) {
    auto optEntry = m_queue->getEntry(messageId);
    if (!optEntry) return;

    if (optEntry->state == QueueState::ACK_PENDING) {
        m_queue->incrementRetry(messageId);
        if (optEntry->retryCount + 1 >= MAX_RETRIES) {
            m_queue->updateState(messageId, QueueState::FAILED);
        } else {
            m_queue->updateState(messageId, QueueState::QUEUED);
        }
    }
}

void OfflineQueueService::resume() {
    qint64 lastCreatedAt = -1;
    QString lastMessageId = "";
    const int BATCH_SIZE = 50;

    while (true) {
        auto pending = m_queue->getPendingEntries(BATCH_SIZE, lastCreatedAt, lastMessageId);
        if (pending.empty()) {
            break;
        }

        for (auto& entry : pending) {
            // Reset state to QUEUED on restart so we resend instead of getting stuck in SENDING/ACK_PENDING
            m_queue->updateState(entry.messageId, QueueState::QUEUED);
            entry.state = QueueState::QUEUED;
            processEntry(entry);
            
            lastCreatedAt = entry.createdAt;
            lastMessageId = entry.messageId;
        }
    }
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
