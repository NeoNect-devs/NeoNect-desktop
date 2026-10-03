#include "MessageStorage.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>
#include <QDateTime>
#include <QThread>

namespace NeoNect {
namespace Core {
namespace Messaging {

SqliteMessageStorage::SqliteMessageStorage(const QString& dbPath) : m_dbPath(dbPath) {
    getDatabase();
}

SqliteMessageStorage::~SqliteMessageStorage() {
}

QSqlDatabase SqliteMessageStorage::getDatabase() {
    thread_local QHash<QString, QString> connectionNames;
    if (!connectionNames.contains(m_dbPath)) {
        connectionNames[m_dbPath] = QString("SqliteMessageStorage_%1").arg(QUuid::createUuid().toString());
    }
    QString name = connectionNames[m_dbPath];
    if (QSqlDatabase::contains(name)) {
        return QSqlDatabase::database(name);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(m_dbPath);
    if (db.open()) {
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS messages ("
                   "id TEXT PRIMARY KEY, "
                   "server_id INTEGER DEFAULT 0, "
                   "conversation_id TEXT, "
                   "sender_id TEXT, "
                   "receiver_id TEXT, "
                   "state INTEGER, "
                   "created_at INTEGER, "
                   "updated_at INTEGER, "
                   "plaintext TEXT)");
        query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_server_id ON messages(server_id) WHERE server_id > 0");
    }
    return db;
}

bool SqliteMessageStorage::saveMessage(const Message& msg) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    // We do INSERT OR REPLACE to handle duplicate server_id or id updates seamlessly
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO messages (id, server_id, conversation_id, sender_id, receiver_id, state, created_at, updated_at, plaintext) "
                  "VALUES (:id, :server_id, :cid, :sid, :rid, :state, :created_at, :updated_at, :plaintext)");
    query.bindValue(":id", msg.messageId);
    query.bindValue(":server_id", msg.serverId);
    query.bindValue(":cid", msg.conversationId);
    query.bindValue(":sid", msg.senderId);
    query.bindValue(":rid", msg.receiverId);
    query.bindValue(":state", static_cast<int>(msg.state));
    query.bindValue(":created_at", msg.timestamp);
    query.bindValue(":updated_at", msg.timestamp);
    query.bindValue(":plaintext", msg.plaintext);
    return query.exec();
}

std::optional<Message> SqliteMessageStorage::getMessage(const QString& messageId) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return std::nullopt;

    QSqlQuery query(db);
    query.prepare("SELECT id, server_id, conversation_id, sender_id, receiver_id, state, created_at, plaintext FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    if (query.exec() && query.next()) {
        Message msg;
        msg.messageId = query.value(0).toString();
        msg.serverId = query.value(1).toLongLong();
        msg.conversationId = query.value(2).toString();
        msg.senderId = query.value(3).toString();
        msg.receiverId = query.value(4).toString();
        msg.state = static_cast<MessageState>(query.value(5).toInt());
        msg.timestamp = query.value(6).toLongLong();
        msg.plaintext = query.value(7).toString();
        return msg;
    }
    return std::nullopt;
}

bool SqliteMessageStorage::updateMessageState(const QString& messageId, MessageState newState) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE messages SET state = :state, updated_at = :updated_at WHERE id = :id");
    query.bindValue(":state", static_cast<int>(newState));
    query.bindValue(":updated_at", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::vector<Message> SqliteMessageStorage::getConversationMessages(const QString& conversationId) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return {};

    std::vector<Message> results;
    QSqlQuery query(db);
    // ordered by timestamp, then messageId tie breaker
    query.prepare("SELECT id, server_id, conversation_id, sender_id, receiver_id, state, created_at, plaintext FROM messages "
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
            msg.state = static_cast<MessageState>(query.value(5).toInt());
            msg.timestamp = query.value(6).toLongLong();
            msg.plaintext = query.value(7).toString();
            results.push_back(msg);
        }
    }
    return results;
}

bool SqliteMessageStorage::deleteMessage(const QString& messageId) {
    QSqlDatabase db = getDatabase();
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("DELETE FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    return query.exec();
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
