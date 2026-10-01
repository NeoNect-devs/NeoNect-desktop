#include "MessageStorage.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUuid>
#include <QDateTime>

namespace NeoNect {
namespace Core {
namespace Messaging {

SqliteMessageStorage::SqliteMessageStorage(const QString& dbPath) : m_dbPath(dbPath) {
    m_connectionName = QUuid::createUuid().toString();
    initDatabase();
}

SqliteMessageStorage::~SqliteMessageStorage() {
    QSqlDatabase::removeDatabase(m_connectionName);
}

void SqliteMessageStorage::initDatabase() {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) return;

    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS messages ("
               "id TEXT PRIMARY KEY, "
               "conversation_id TEXT, "
               "sender_id TEXT, "
               "receiver_id TEXT, "
               "state INTEGER, "
               "created_at INTEGER, "
               "updated_at INTEGER)");
}

bool SqliteMessageStorage::saveMessage(const Message& msg) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    // Check for duplicate
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT 1 FROM messages WHERE id = :id");
    checkQuery.bindValue(":id", msg.messageId);
    if (checkQuery.exec() && checkQuery.next()) {
        // duplicate exists, do not overwrite, but return true
        return true;
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO messages (id, conversation_id, sender_id, receiver_id, state, created_at, updated_at) "
                  "VALUES (:id, :cid, :sid, :rid, :state, :created_at, :updated_at)");
    query.bindValue(":id", msg.messageId);
    query.bindValue(":cid", msg.conversationId);
    query.bindValue(":sid", msg.senderId);
    query.bindValue(":rid", msg.receiverId);
    query.bindValue(":state", static_cast<int>(msg.state));
    query.bindValue(":created_at", msg.timestamp);
    query.bindValue(":updated_at", msg.timestamp);
    return query.exec();
}

std::optional<Message> SqliteMessageStorage::getMessage(const QString& messageId) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return std::nullopt;

    QSqlQuery query(db);
    query.prepare("SELECT id, conversation_id, sender_id, receiver_id, state, created_at FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    if (query.exec() && query.next()) {
        Message msg;
        msg.messageId = query.value(0).toString();
        msg.conversationId = query.value(1).toString();
        msg.senderId = query.value(2).toString();
        msg.receiverId = query.value(3).toString();
        msg.state = static_cast<MessageState>(query.value(4).toInt());
        msg.timestamp = query.value(5).toLongLong();
        return msg;
    }
    return std::nullopt;
}

bool SqliteMessageStorage::updateMessageState(const QString& messageId, MessageState newState) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE messages SET state = :state, updated_at = :updated_at WHERE id = :id");
    query.bindValue(":state", static_cast<int>(newState));
    query.bindValue(":updated_at", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", messageId);
    return query.exec();
}

std::vector<Message> SqliteMessageStorage::getConversationMessages(const QString& conversationId) {
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen() && !db.open()) return {};

    std::vector<Message> results;
    QSqlQuery query(db);
    // ordered by timestamp, then messageId tie breaker
    query.prepare("SELECT id, conversation_id, sender_id, receiver_id, state, created_at FROM messages "
                  "WHERE conversation_id = :cid ORDER BY created_at ASC, id ASC");
    query.bindValue(":cid", conversationId);
    if (query.exec()) {
        while (query.next()) {
            Message msg;
            msg.messageId = query.value(0).toString();
            msg.conversationId = query.value(1).toString();
            msg.senderId = query.value(2).toString();
            msg.receiverId = query.value(3).toString();
            msg.state = static_cast<MessageState>(query.value(4).toInt());
            msg.timestamp = query.value(5).toLongLong();
            results.push_back(msg);
        }
    }
    return results;
}

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
