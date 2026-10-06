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
#include <map>
#include <memory>


namespace NeoNect {
namespace Core {
namespace Messaging {


namespace {
    struct DbConnection {
        QString name;
        DbConnection(const QString& n, const QString& path) : name(n) {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(path);
            db.open();

            QSqlQuery pragmaQuery(db);
            pragmaQuery.exec("PRAGMA journal_mode = WAL;");
            pragmaQuery.exec("PRAGMA synchronous = NORMAL;");
            pragmaQuery.exec("PRAGMA busy_timeout = 5000;");
        }
        ~DbConnection() {
            if (QSqlDatabase::contains(name)) {
                {
                    QSqlDatabase db = QSqlDatabase::database(name, false);
                    if (db.isOpen()) {
                        db.close();
                    }
                }
                QSqlDatabase::removeDatabase(name);
            }
        }
    };

    struct ThreadConnections {
        std::map<QString, std::pair<std::weak_ptr<bool>, std::unique_ptr<DbConnection>>> connections;

        ~ThreadConnections() {
            connections.clear();
        }

        void cleanup() {
            for (auto it = connections.begin(); it != connections.end(); ) {
                if (it->second.first.expired()) {
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
        }
    };

    thread_local ThreadConnections t_threadConnections;
}

MessageQueue::MessageQueue(const QString& dbPath) : m_dbPath(dbPath) {
    m_instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_aliveToken = std::make_shared<bool>(true);
    initDatabase();
}

MessageQueue::~MessageQueue() {
    // m_aliveToken goes out of scope here.
    // The thread_local ThreadConnections will lazily clean up connections
    // for this instanceId in getDatabase() or on thread exit.
}

QSqlDatabase MessageQueue::getDatabase() {
    t_threadConnections.cleanup();

    auto it = t_threadConnections.connections.find(m_instanceId);
    if (it == t_threadConnections.connections.end()) {
        QString connName = QString("mq_%1_%2").arg(m_instanceId).arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        auto dbConn = std::make_unique<DbConnection>(connName, m_dbPath);
        t_threadConnections.connections[m_instanceId] = std::make_pair(std::weak_ptr<bool>(m_aliveToken), std::move(dbConn));
        return QSqlDatabase::database(connName);
    }
    return QSqlDatabase::database(it->second.second->name);
}

QString MessageQueue::_testConnectionName() {
    return getDatabase().connectionName();
}

bool MessageQueue::_testHasConnection(const QString& connectionName) {
    return QSqlDatabase::contains(connectionName);
}

void MessageQueue::initDatabase() {
    QSqlDatabase db = getDatabase();
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS offline_queue ("
               "message_id TEXT PRIMARY KEY, "
               "envelope_bytes BLOB, "
               "recipient_username TEXT, "
               "recipient_device_id TEXT, "
               "state INTEGER, "
               "retry_count INTEGER, "
               "created_at INTEGER, "
               "updated_at INTEGER"
               ")");
}

bool MessageQueue::isValidTransition(QueueState from, QueueState to) const {
    if (from == QueueState::FAILED || from == QueueState::DELIVERED) return false;
    return true;
}

bool MessageQueue::enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("INSERT INTO offline_queue (message_id, envelope_bytes, recipient_username, recipient_device_id, state, retry_count, created_at, updated_at) "
                  "VALUES (:id, :env, :ru, :rd, :state, 0, :created, :updated)");
    query.bindValue(":id", messageId);
    query.bindValue(":env", envelopeBytes);
    query.bindValue(":ru", recipientUsername);
    query.bindValue(":rd", recipientDeviceId);
    query.bindValue(":state", static_cast<int>(QueueState::QUEUED));
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    query.bindValue(":created", now);
    query.bindValue(":updated", now);

    return query.exec();
}

bool MessageQueue::updateState(const QString& messageId, QueueState newState) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

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
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE offline_queue SET retry_count = retry_count + 1, updated_at = :updated WHERE message_id = :id");
    query.bindValue(":updated", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::optional<QueueEntry> MessageQueue::getEntry(const QString& messageId) {
    QSqlDatabase db = getDatabase();
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
    QSqlDatabase db = getDatabase();
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

OfflineQueueService::OfflineQueueService(std::weak_ptr<IMessageQueue> queue, RelaySendCallback relaySendCb)
    : m_queue(std::move(queue)), m_relaySendCb(std::move(relaySendCb)) {
}

bool OfflineQueueService::onEnvelopeReady(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes) {
    auto q = m_queue.lock();
    if (!q) return false;
    if (!q->enqueue(messageId, envelopeBytes, recipientUsername, recipientDeviceId)) {
        return false; // failed to queue or duplicate
    }

    auto optEntry = q->getEntry(messageId);
    if (optEntry) {
        processEntry(*optEntry);
    }
    return true;
}

void OfflineQueueService::processEntry(QueueEntry& entry) {
    auto q = m_queue.lock();
    if (!q) return;
    if (entry.state == QueueState::QUEUED || entry.state == QueueState::SENDING || entry.state == QueueState::ACK_PENDING) {
        if (entry.retryCount >= MAX_RETRIES) {
            q->updateState(entry.messageId, QueueState::FAILED);
            return;
        }

        q->updateState(entry.messageId, QueueState::SENDING);

        bool success = false;
        if (m_relaySendCb) {
            success = m_relaySendCb(entry.recipientUsername, entry.recipientDeviceId, entry.messageId, entry.envelopeBytes);
        }

        if (success) {
            q->updateState(entry.messageId, QueueState::ACK_PENDING);
        } else {
            // Transport failed immediately
            handleTransportFailure(entry.messageId);
        }
    }
}

void OfflineQueueService::handleAck(const QString& messageId) {
    auto q = m_queue.lock();
    if (!q) return;
    auto optEntry = q->getEntry(messageId);
    if (!optEntry) return;

    if (optEntry->state == QueueState::DELIVERED) return; // duplicate ACK idempotent

    q->updateState(messageId, QueueState::DELIVERED);
}

void OfflineQueueService::handleTransportFailure(const QString& messageId) {
    auto q = m_queue.lock();
    if (!q) return;
    auto optEntry = q->getEntry(messageId);
    if (!optEntry) return;

    if (optEntry->state == QueueState::SENDING || optEntry->state == QueueState::ACK_PENDING) {
        q->incrementRetry(messageId);

        // If max retries reached, fail it, else queue it again for retry
        if (optEntry->retryCount + 1 >= MAX_RETRIES) {
            q->updateState(messageId, QueueState::FAILED);
        } else {
            q->updateState(messageId, QueueState::QUEUED);
            // Optionally could call processEntry immediately, but usually rely on retryPending
        }
    }
}

void OfflineQueueService::handleTimeout(const QString& messageId) {
    auto q = m_queue.lock();
    if (!q) return;
    auto optEntry = q->getEntry(messageId);
    if (!optEntry) return;

    if (optEntry->state == QueueState::ACK_PENDING) {
        q->incrementRetry(messageId);
        if (optEntry->retryCount + 1 >= MAX_RETRIES) {
            q->updateState(messageId, QueueState::FAILED);
        } else {
            q->updateState(messageId, QueueState::QUEUED);
        }
    }
}

void OfflineQueueService::resume() {
    auto q = m_queue.lock();
    if (!q) return;
    qint64 lastCreatedAt = -1;
    QString lastMessageId = "";
    const int BATCH_SIZE = 50;

    while (true) {
        auto pending = q->getPendingEntries(BATCH_SIZE, lastCreatedAt, lastMessageId);
        if (pending.empty()) {
            break;
        }

        for (auto& entry : pending) {
            // Reset state to QUEUED on restart so we resend instead of getting stuck in SENDING/ACK_PENDING
            q->updateState(entry.messageId, QueueState::QUEUED);
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
