#include "MessageStorage.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <map>

namespace NeoNect {
namespace Core {
namespace Messaging {

namespace {
    struct DbConnection {
        QString name;
        explicit DbConnection(const QString& n) : name(n) {}
        ~DbConnection() {
            if (!QCoreApplication::instance()) {
                return;
            }
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

    thread_local ThreadConnections t_msgThreadConnections;
}

SqliteMessageStorage::SqliteMessageStorage(const QString& dbPath) : m_dbPath(dbPath) {
    if (m_dbPath == ":memory:") {
        m_dbPath = QString("file:memdb_%1?mode=memory&cache=shared")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    m_instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_aliveToken = std::make_shared<bool>(true);
}

SqliteMessageStorage::~SqliteMessageStorage() {
    QMutexLocker locker(&m_mutex);
    m_aliveToken.reset();
    t_msgThreadConnections.cleanup();
}

QSqlDatabase SqliteMessageStorage::getDatabase() {
    QMutexLocker locker(&m_mutex);
    t_msgThreadConnections.cleanup();
    auto it = t_msgThreadConnections.connections.find(m_instanceId);
    if (it == t_msgThreadConnections.connections.end()) {
        QString name = QUuid::createUuid().toString();
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
        if (m_dbPath.startsWith("file:memdb_")) {
            db.setConnectOptions("QSQLITE_OPEN_URI");
        }
        db.setDatabaseName(m_dbPath);
        if (db.open()) {
            bool migrationFailed = false;
            {
                QSqlQuery pragmaQuery(db);
                if (!m_dbPath.startsWith("file:memdb_")) {
                    if (!pragmaQuery.exec("PRAGMA journal_mode = WAL;")) {
                        qWarning() << "Failed to set WAL mode for MessageStorage:" << pragmaQuery.lastError().text();
                    }
                }
                if (!pragmaQuery.exec("PRAGMA synchronous = NORMAL;")) {
                    qWarning() << "Failed to set synchronous mode for MessageStorage:" << pragmaQuery.lastError().text();
                }
                if (!pragmaQuery.exec("PRAGMA busy_timeout = 5000;")) {
                    qWarning() << "Failed to set busy_timeout for MessageStorage:" << pragmaQuery.lastError().text();
                }
                QSqlQuery query(db);
                query.exec("CREATE TABLE IF NOT EXISTS messages ("
                           "id TEXT PRIMARY KEY, "
                           "server_id INTEGER, "
                           "conversation_id TEXT, "
                           "sender_id TEXT, "
                           "receiver_id TEXT, "
                           "plaintext TEXT, "
                           "state INTEGER, "
                           "created_at INTEGER, "
                           "updated_at INTEGER, "
                           "type TEXT DEFAULT 'text', "
                           "media_url TEXT DEFAULT '', "
                           "file_name TEXT DEFAULT '', "
                           "file_size INTEGER DEFAULT 0, "
                           "duration INTEGER DEFAULT 0, "
                           "waveform BLOB, "
                           "media_width INTEGER DEFAULT 0, "
                           "media_height INTEGER DEFAULT 0, "
                           "error_text TEXT DEFAULT '')");
                query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_server_id ON messages (server_id) WHERE server_id > 0");
                query.exec("CREATE INDEX IF NOT EXISTS idx_messages_conv_created ON messages (conversation_id, created_at ASC, id ASC)");
                
                // Schema migration check
                QSqlQuery pragmaCheck(db);
                if (pragmaCheck.exec("PRAGMA table_info(messages)")) {
                    QSet<QString> existingColumns;
                    while (pragmaCheck.next()) {
                        existingColumns.insert(pragmaCheck.value(1).toString());
                    }
                    
                    struct ColumnDef {
                        QString name;
                        QString def;
                    };
                    QList<ColumnDef> targetColumns = {
                        {"type", "type TEXT DEFAULT 'text'"},
                        {"media_url", "media_url TEXT DEFAULT ''"},
                        {"file_name", "file_name TEXT DEFAULT ''"},
                        {"file_size", "file_size INTEGER DEFAULT 0"},
                        {"duration", "duration INTEGER DEFAULT 0"},
                        {"waveform", "waveform BLOB"},
                        {"media_width", "media_width INTEGER DEFAULT 0"},
                        {"media_height", "media_height INTEGER DEFAULT 0"},
                        {"error_text", "error_text TEXT DEFAULT ''"}
                    };
                    
                    for (const auto& col : targetColumns) {
                        if (!existingColumns.contains(col.name)) {
                            QSqlQuery alterQuery(db);
                            QString alterSql = "ALTER TABLE messages ADD COLUMN " + col.def;
                            if (!alterQuery.exec(alterSql)) {
                                qWarning() << "Failed to migrate messages table:" << alterQuery.lastError().text();
                                migrationFailed = true;
                                break;
                            }
                        }
                    }
                } else {
                    qWarning() << "Failed to get table info for messages:" << pragmaCheck.lastError().text();
                    migrationFailed = true;
                }
            }
            
            if (migrationFailed) {
                db.close();
                db = QSqlDatabase();
                QSqlDatabase::removeDatabase(name);
                return QSqlDatabase();
            }
        } else {
            qWarning() << "Failed to open database:" << db.lastError().text();
            db = QSqlDatabase();
            QSqlDatabase::removeDatabase(name);
            return QSqlDatabase();
        }
        t_msgThreadConnections.connections[m_instanceId] =
            std::make_pair(std::weak_ptr<bool>(m_aliveToken), std::make_unique<DbConnection>(name));
        return db;
    }
    return QSqlDatabase::database(it->second.second->name);
}

bool SqliteMessageStorage::saveMessage(const Message& msg) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    if (!db.transaction()) return false;

    // Check for duplicate
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT 1 FROM messages WHERE id = :id");
    checkQuery.bindValue(":id", msg.messageId);
    if (checkQuery.exec() && checkQuery.next()) {
        // duplicate exists, do not overwrite, but return true
        db.rollback();
        return true;
    }

    if (msg.serverId > 0) {
        QSqlQuery delQuery(db);
        delQuery.prepare("DELETE FROM messages WHERE server_id = :srv AND id != :id");
        delQuery.bindValue(":srv", msg.serverId);
        delQuery.bindValue(":id", msg.messageId);
        if (!delQuery.exec()) {
            db.rollback();
            return false;
        }
    }

    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO messages (id, server_id, conversation_id, sender_id, receiver_id, plaintext, state, created_at, updated_at, type, media_url, file_name, file_size, duration, waveform, media_width, media_height, error_text) "
                  "VALUES (:id, :srv, :cid, :sid, :rid, :txt, :state, :created_at, :updated_at, :type, :media_url, :file_name, :file_size, :duration, :waveform, :media_width, :media_height, :error_text)");
    query.bindValue(":id", msg.messageId);
    query.bindValue(":srv", msg.serverId);
    query.bindValue(":cid", msg.conversationId);
    query.bindValue(":sid", msg.senderId);
    query.bindValue(":rid", msg.receiverId);
    query.bindValue(":txt", msg.plaintext);
    query.bindValue(":state", static_cast<int>(msg.state));
    query.bindValue(":created_at", msg.timestamp);
    query.bindValue(":updated_at", msg.timestamp);
    query.bindValue(":type", msg.type);
    query.bindValue(":media_url", msg.mediaUrl);
    query.bindValue(":file_name", msg.fileName);
    query.bindValue(":file_size", static_cast<qint64>(msg.fileSize));
    query.bindValue(":duration", msg.duration);
    query.bindValue(":waveform", msg.waveform);
    query.bindValue(":media_width", msg.mediaWidth);
    query.bindValue(":media_height", msg.mediaHeight);
    query.bindValue(":error_text", msg.errorText);
    
    if (query.exec()) {
        return db.commit();
    } else {
        db.rollback();
        return false;
    }
}

std::optional<Message> SqliteMessageStorage::getMessage(const QString& messageId) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return std::nullopt;

    QSqlQuery query(db);
    query.prepare("SELECT id, server_id, conversation_id, sender_id, receiver_id, plaintext, state, created_at, type, media_url, file_name, file_size, duration, waveform, media_width, media_height, error_text FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    if (query.exec() && query.next()) {
        Message msg;
        msg.messageId = query.value(0).toString();
        msg.serverId = query.value(1).toLongLong();
        msg.conversationId = query.value(2).toString();
        msg.senderId = query.value(3).toString();
        msg.receiverId = query.value(4).toString();
        msg.plaintext = query.value(5).toString();
        msg.state = static_cast<MessageState>(query.value(6).toInt());
        msg.timestamp = query.value(7).toLongLong();
        msg.type = query.value(8).toString();
        if (msg.type.isEmpty()) msg.type = "text";
        msg.mediaUrl = query.value(9).toString();
        msg.fileName = query.value(10).toString();
        msg.fileSize = query.value(11).toLongLong();
        msg.duration = query.value(12).toInt();
        msg.waveform = query.value(13).toByteArray();
        msg.mediaWidth = query.value(14).toInt();
        msg.mediaHeight = query.value(15).toInt();
        msg.errorText = query.value(16).toString();
        return msg;
    }
    return std::nullopt;
}

bool SqliteMessageStorage::saveMessages(const std::vector<Message>& msgs) {
    if (msgs.empty()) return true;
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    if (!db.transaction()) return false;

    for (const auto& msg : msgs) {
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT 1 FROM messages WHERE id = :id");
        checkQuery.bindValue(":id", msg.messageId);
        if (checkQuery.exec() && checkQuery.next()) {
            continue;
        }

        if (msg.serverId > 0) {
            QSqlQuery delQuery(db);
            delQuery.prepare("DELETE FROM messages WHERE server_id = :srv AND id != :id");
            delQuery.bindValue(":srv", msg.serverId);
            delQuery.bindValue(":id", msg.messageId);
            if (!delQuery.exec()) {
                db.rollback();
                return false;
            }
        }

        QSqlQuery query(db);
        query.prepare("INSERT OR REPLACE INTO messages (id, server_id, conversation_id, sender_id, receiver_id, plaintext, state, created_at, updated_at, type, media_url, file_name, file_size, duration, waveform, media_width, media_height, error_text) "
                      "VALUES (:id, :srv, :cid, :sid, :rid, :txt, :state, :created_at, :updated_at, :type, :media_url, :file_name, :file_size, :duration, :waveform, :media_width, :media_height, :error_text)");
        query.bindValue(":id", msg.messageId);
        query.bindValue(":srv", msg.serverId);
        query.bindValue(":cid", msg.conversationId);
        query.bindValue(":sid", msg.senderId);
        query.bindValue(":rid", msg.receiverId);
        query.bindValue(":txt", msg.plaintext);
        query.bindValue(":state", static_cast<int>(msg.state));
        query.bindValue(":created_at", msg.timestamp);
        query.bindValue(":updated_at", msg.timestamp);
        query.bindValue(":type", msg.type);
        query.bindValue(":media_url", msg.mediaUrl);
        query.bindValue(":file_name", msg.fileName);
        query.bindValue(":file_size", static_cast<qint64>(msg.fileSize));
        query.bindValue(":duration", msg.duration);
        query.bindValue(":waveform", msg.waveform);
        query.bindValue(":media_width", msg.mediaWidth);
        query.bindValue(":media_height", msg.mediaHeight);
        query.bindValue(":error_text", msg.errorText);

        if (!query.exec()) {
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

bool SqliteMessageStorage::updateMessageState(const QString& messageId, MessageState newState, const QString& errorText) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    // Single statement, SQLite handles atomicity for a single UPDATE.
    QSqlQuery query(db);
    if (!errorText.isNull()) {
        query.prepare("UPDATE messages SET state = :state, error_text = :error_text, updated_at = :updated_at WHERE id = :id");
        query.bindValue(":state", static_cast<int>(newState));
        query.bindValue(":error_text", errorText);
        query.bindValue(":updated_at", QDateTime::currentMSecsSinceEpoch());
        query.bindValue(":id", messageId);
    } else {
        query.prepare("UPDATE messages SET state = :state, updated_at = :updated_at WHERE id = :id");
        query.bindValue(":state", static_cast<int>(newState));
        query.bindValue(":updated_at", QDateTime::currentMSecsSinceEpoch());
        query.bindValue(":id", messageId);
    }
    return query.exec();
}

bool SqliteMessageStorage::updateMessageMediaUrl(const QString& messageId, const QString& mediaUrl) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE messages SET media_url = :media_url, updated_at = :updated_at WHERE id = :id");
    query.bindValue(":media_url", mediaUrl);
    query.bindValue(":updated_at", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::vector<Message> SqliteMessageStorage::getConversationMessages(const QString& conversationId) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return {};

    std::vector<Message> results;
    QSqlQuery query(db);
    // ordered by timestamp, then messageId tie breaker
    query.prepare("SELECT id, server_id, conversation_id, sender_id, receiver_id, plaintext, state, created_at, type, media_url, file_name, file_size, duration, waveform, media_width, media_height, error_text FROM messages "
                  "WHERE conversation_id = :cid ORDER BY created_at ASC, id ASC");
    query.bindValue(":cid", conversationId);
    if (query.exec()) {
        while (query.next()) {
            Message msg;
            msg.messageId = query.value(0).toString();
            msg.serverId = query.value(1).toLongLong();
            msg.conversationId = query.value(2).toString();
            msg.senderId = query.value(3).toString();
            msg.receiverId = query.value(4).toString();
            msg.plaintext = query.value(5).toString();
            msg.state = static_cast<MessageState>(query.value(6).toInt());
            msg.timestamp = query.value(7).toLongLong();
            msg.type = query.value(8).toString();
            if (msg.type.isEmpty()) msg.type = "text";
            msg.mediaUrl = query.value(9).toString();
            msg.fileName = query.value(10).toString();
            msg.fileSize = query.value(11).toLongLong();
            msg.duration = query.value(12).toInt();
            msg.waveform = query.value(13).toByteArray();
            msg.mediaWidth = query.value(14).toInt();
            msg.mediaHeight = query.value(15).toInt();
            msg.errorText = query.value(16).toString();
            results.push_back(msg);
        }
    }
    return results;
}

bool SqliteMessageStorage::deleteMessage(const QString& messageId) {
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    // Single statement, SQLite handles atomicity for a single DELETE.
    QSqlQuery query(db);
    query.prepare("DELETE FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    return query.exec();
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
