#include "sqlmessagerepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QUuid>

namespace NeoNect {
namespace Storage {

class SqlMessageRepositoryWorker : public QObject {
    Q_OBJECT
public:
    explicit SqlMessageRepositoryWorker(const QString& dbPath, QObject* parent = nullptr) 
        : QObject(parent), m_dbPath(dbPath),
          m_connectionName(QString("MsgRepo_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {}

    ~SqlMessageRepositoryWorker() {
        if (m_db.isOpen()) {
            m_db.close();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    void doSwitchDatabase(const QString &newDbPath) {
        if (m_dbPath == newDbPath && m_db.isOpen()) {
            return;
        }
        if (m_db.isOpen()) {
            m_db.close();
        }
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName = QString("MsgRepo_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_dbPath = newDbPath;
        initialize();
    }

public slots:
    void initialize() {
        m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
        m_db.setDatabaseName(m_dbPath);
        if (!m_db.open()) {
            qWarning() << "[SqlMessageRepository] Failed to open database:" << m_db.lastError().text();
            return;
        }

        QSqlQuery query(m_db);
        bool success = query.exec(
            "CREATE TABLE IF NOT EXISTS messages ("
            "id TEXT PRIMARY KEY, "
            "server_id INTEGER DEFAULT 0, "
            "conversation_id TEXT NOT NULL, "
            "sender_id TEXT NOT NULL, "
            "type TEXT NOT NULL, "
            "text TEXT, "
            "media_url TEXT, "
            "file_name TEXT, "
            "file_size INTEGER, "
            "duration INTEGER, "
            "waveform BLOB, "
            "status INTEGER, "
            "error_text TEXT, "
            "timestamp INTEGER NOT NULL"
            ")"
        );
        if (!success) {
            qWarning() << "[SqlMessageRepository] Failed to create table:" << query.lastError().text();
        }

        query.exec("CREATE INDEX IF NOT EXISTS idx_conversation_time ON messages(conversation_id, timestamp)");
        query.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_server_id ON messages(server_id) WHERE server_id > 0");
    }

    void doSaveMessage(const Domain::Message msg, const QObject* context, IMessageRepository::SaveCallback callback) {
        if (!m_db.isOpen()) {
            invokeCallback(context, callback, false);
            return;
        }

        qint64 ts = msg.timestamp;
        if (ts > 0 && ts < 100000000000LL) {
            ts *= 1000LL;
        }
        if (ts <= 0) {
            ts = QDateTime::currentMSecsSinceEpoch();
        }

        QSqlQuery query(m_db);
        query.prepare(
            "INSERT OR REPLACE INTO messages "
            "(id, server_id, conversation_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(msg.id);
        query.addBindValue(msg.serverId);
        query.addBindValue(msg.conversationId.trimmed().toLower());
        query.addBindValue(msg.senderId);
        query.addBindValue(msg.type);
        query.addBindValue(msg.text);
        query.addBindValue(msg.mediaUrl);
        query.addBindValue(msg.fileName);
        query.addBindValue(msg.fileSize);
        query.addBindValue(msg.duration);
        query.addBindValue(msg.waveform);
        query.addBindValue(static_cast<int>(msg.status));
        query.addBindValue(msg.errorText);
        query.addBindValue(ts);

        bool success = query.exec();
        if (!success) {
            qWarning() << "[SqlMessageRepository] Save failed:" << query.lastError().text();
        }

        invokeCallback(context, callback, success);
    }

        void doSaveMessages(const std::vector<Domain::Message> msgs, const QObject* context, IMessageRepository::SaveCallback callback) {
        if (!m_db.isOpen() || msgs.empty()) {
            invokeCallback(context, callback, false);
            return;
        }

        m_db.transaction();
        QSqlQuery query(m_db);
        query.prepare(
            "INSERT OR REPLACE INTO messages "
            "(id, server_id, conversation_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        
        bool success = true;
        for (const auto& msg : msgs) {
            qint64 ts = msg.timestamp;
            if (ts > 0 && ts < 100000000000LL) {
                ts *= 1000LL;
            }
            if (ts <= 0) {
                ts = QDateTime::currentMSecsSinceEpoch();
            }

            query.bindValue(0, msg.id);
            query.bindValue(1, msg.serverId);
            query.bindValue(2, msg.conversationId.trimmed().toLower());
            query.bindValue(3, msg.senderId);
            query.bindValue(4, msg.type);
            query.bindValue(5, msg.text);
            query.bindValue(6, msg.mediaUrl);
            query.bindValue(7, msg.fileName);
            query.bindValue(8, msg.fileSize);
            query.bindValue(9, msg.duration);
            query.bindValue(10, msg.waveform);
            query.bindValue(11, static_cast<int>(msg.status));
            query.bindValue(12, msg.errorText);
            query.bindValue(13, ts);

            if (!query.exec()) {
                qWarning() << "[SqlMessageRepository] Batch save failed for msg" << msg.id << ":" << query.lastError().text();
                success = false;
                break;
            }
        }

        if (success) {
            m_db.commit();
        } else {
            m_db.rollback();
        }

        invokeCallback(context, callback, success);
    }

    void doUpdateStatus(const QString id, Domain::MessageStatus status, const QString errorText, const QObject* context, IMessageRepository::SaveCallback callback) {
        if (!m_db.isOpen()) {
            invokeCallback(context, callback, false);
            return;
        }

        QSqlQuery query(m_db);
        query.prepare("UPDATE messages SET status = ?, error_text = ? WHERE id = ?");
        query.addBindValue(static_cast<int>(status));
        query.addBindValue(errorText);
        query.addBindValue(id);

        bool success = query.exec();
        invokeCallback(context, callback, success);
    }

    void doMarkMessagesSeen(const QString conversationId, const QString senderId, const QObject* context, IMessageRepository::SaveCallback callback) {
        if (!m_db.isOpen()) {
            invokeCallback(context, callback, false);
            return;
        }

        QSqlQuery query(m_db);
        if (!senderId.isEmpty()) {
            query.prepare("UPDATE messages SET status = ? WHERE LOWER(conversation_id) = LOWER(?) AND sender_id = ? AND status != ?");
            query.addBindValue(static_cast<int>(Domain::MessageStatus::Seen));
            query.addBindValue(conversationId.trimmed().toLower());
            query.addBindValue(senderId);
            query.addBindValue(static_cast<int>(Domain::MessageStatus::Seen));
        } else {
            query.prepare("UPDATE messages SET status = ? WHERE LOWER(conversation_id) = LOWER(?) AND status != ?");
            query.addBindValue(static_cast<int>(Domain::MessageStatus::Seen));
            query.addBindValue(conversationId.trimmed().toLower());
            query.addBindValue(static_cast<int>(Domain::MessageStatus::Seen));
        }

        bool success = query.exec();
        invokeCallback(context, callback, success);
    }

    void doDeleteMessage(const QString id, const QObject* context, IMessageRepository::SaveCallback callback) {
        if (!m_db.isOpen()) {
            invokeCallback(context, callback, false);
            return;
        }

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM messages WHERE id = ?");
        query.addBindValue(id);

        bool success = query.exec();
        invokeCallback(context, callback, success);
    }

    void doGetMessages(const QString conversationId, int limit, qint64 beforeTimestamp, const QObject* context, IMessageRepository::FetchCallback callback) {
        std::vector<Domain::Message> results;
        if (!m_db.isOpen()) {
            invokeFetchCallback(context, callback, results);
            return;
        }

        QSqlQuery query(m_db);
        if (beforeTimestamp > 0) {
            query.prepare("SELECT id, server_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp "
                          "FROM messages WHERE LOWER(conversation_id) = LOWER(?) AND timestamp < ? ORDER BY timestamp DESC LIMIT ?");
            query.addBindValue(conversationId.trimmed().toLower());
            query.addBindValue(beforeTimestamp);
            query.addBindValue(limit);
        } else {
            query.prepare("SELECT id, server_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp "
                          "FROM messages WHERE LOWER(conversation_id) = LOWER(?) ORDER BY timestamp DESC LIMIT ?");
            query.addBindValue(conversationId.trimmed().toLower());
            query.addBindValue(limit);
        }

        if (query.exec()) {
            while (query.next()) {
                Domain::Message msg;
                msg.conversationId = conversationId;
                msg.id = query.value(0).toString();
                msg.serverId = query.value(1).toLongLong();
                msg.senderId = query.value(2).toString();
                msg.type = query.value(3).toString();
                msg.text = query.value(4).toString();
                msg.mediaUrl = query.value(5).toString();
                msg.fileName = query.value(6).toString();
                msg.fileSize = query.value(7).toLongLong();
                msg.duration = query.value(8).toInt();
                msg.waveform = query.value(9).toByteArray();
                msg.status = static_cast<Domain::MessageStatus>(query.value(10).toInt());
                msg.errorText = query.value(11).toString();
                qint64 rawTs = query.value(12).toLongLong();
                if (rawTs > 0 && rawTs < 100000000000LL) {
                    rawTs *= 1000LL;
                }
                msg.timestamp = rawTs > 0 ? rawTs : QDateTime::currentMSecsSinceEpoch();
                
                results.push_back(msg);
            }
            std::reverse(results.begin(), results.end());
        } else {
            qWarning() << "[SqlMessageRepository] Fetch failed:" << query.lastError().text();
        }

        invokeFetchCallback(context, callback, results);
    }

    void doGetMessageById(const QString id, const QObject* context, IMessageRepository::MessageCallback callback) {
        std::optional<Domain::Message> result;
        if (!m_db.isOpen()) {
            invokeMessageCallback(context, callback, result);
            return;
        }

        QSqlQuery query(m_db);
        query.prepare("SELECT id, server_id, conversation_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp "
                      "FROM messages WHERE id = ?");
        query.addBindValue(id);

        if (query.exec() && query.next()) {
            Domain::Message msg;
            msg.id = query.value(0).toString();
            msg.serverId = query.value(1).toLongLong();
            msg.conversationId = query.value(2).toString();
            msg.senderId = query.value(3).toString();
            msg.type = query.value(4).toString();
            msg.text = query.value(5).toString();
            msg.mediaUrl = query.value(6).toString();
            msg.fileName = query.value(7).toString();
            msg.fileSize = query.value(8).toLongLong();
            msg.duration = query.value(9).toInt();
            msg.waveform = query.value(10).toByteArray();
            msg.status = static_cast<Domain::MessageStatus>(query.value(11).toInt());
            msg.errorText = query.value(12).toString();
            qint64 rawTs = query.value(13).toLongLong();
            if (rawTs > 0 && rawTs < 100000000000LL) {
                rawTs *= 1000LL;
            }
            msg.timestamp = rawTs > 0 ? rawTs : QDateTime::currentMSecsSinceEpoch();
            result = msg;
        }

        invokeMessageCallback(context, callback, result);
    }

private:
    QString m_dbPath;
    QString m_connectionName;
    QSqlDatabase m_db;

    void invokeCallback(const QObject* context, IMessageRepository::SaveCallback callback, bool success) {
        if (!context || !callback) return;
        QMetaObject::invokeMethod(const_cast<QObject*>(context), [callback, success]() {
            callback(success);
        }, Qt::QueuedConnection);
    }

    void invokeFetchCallback(const QObject* context, IMessageRepository::FetchCallback callback, const std::vector<Domain::Message>& results) {
        if (!context || !callback) return;
        QMetaObject::invokeMethod(const_cast<QObject*>(context), [callback, results]() {
            callback(results);
        }, Qt::QueuedConnection);
    }

    void invokeMessageCallback(const QObject* context, IMessageRepository::MessageCallback callback, const std::optional<Domain::Message>& result) {
        if (!context || !callback) return;
        QMetaObject::invokeMethod(const_cast<QObject*>(context), [callback, result]() {
            callback(result);
        }, Qt::QueuedConnection);
    }
};

SqlMessageRepository::SqlMessageRepository(const QString& dbPath, QObject* parent)
    : QObject(parent), m_workerThread(new QThread(this)), m_worker(new SqlMessageRepositoryWorker(dbPath))
{
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, m_worker, &SqlMessageRepositoryWorker::initialize);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread->start();
}

SqlMessageRepository::~SqlMessageRepository() {
    m_workerThread->quit();
    m_workerThread->wait();
}

void SqlMessageRepository::saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, msg, context, callback]() {
        m_worker->doSaveMessage(msg, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, msgs, context, callback]() {
        m_worker->doSaveMessages(msgs, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, id, status, errorText, context, callback]() {
        m_worker->doUpdateStatus(id, status, errorText, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context, SaveCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, conversationId, senderId, context, callback]() {
        m_worker->doMarkMessagesSeen(conversationId, senderId, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::deleteMessageAsync(const QString &id, const QObject* context, SaveCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, id, context, callback]() {
        m_worker->doDeleteMessage(id, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, conversationId, limit, beforeTimestamp, context, callback]() {
        m_worker->doGetMessages(conversationId, limit, beforeTimestamp, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::getMessageByIdAsync(const QString &id, const QObject* context, MessageCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, id, context, callback]() {
        m_worker->doGetMessageById(id, context, callback);
    }, Qt::QueuedConnection);
}

void SqlMessageRepository::switchDatabase(const QString &dbPath) {
    QMetaObject::invokeMethod(m_worker, [this, dbPath]() {
        m_worker->doSwitchDatabase(dbPath);
    }, Qt::QueuedConnection);
}

}
}

#include "sqlmessagerepository.moc"
