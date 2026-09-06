#include "sqlmessagerepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QDir>
#include <QMetaObject>

namespace NeoNect {
namespace Storage {

class SqlMessageRepositoryWorker : public QObject {
    Q_OBJECT
public:
    explicit SqlMessageRepositoryWorker(const QString& dbPath, QObject* parent = nullptr) 
        : QObject(parent), m_dbPath(dbPath) {}

    ~SqlMessageRepositoryWorker() {
        if (m_db.isOpen()) {
            m_db.close();
        }
    }

public slots:
    void initialize() {
        m_db = QSqlDatabase::addDatabase("QSQLITE", "MessageRepoConnection");
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

        QSqlQuery query(m_db);
        query.prepare(
            "INSERT OR REPLACE INTO messages "
            "(id, server_id, conversation_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        query.addBindValue(msg.id);
        query.addBindValue(msg.serverId);
        query.addBindValue(msg.conversationId);
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
        query.addBindValue(msg.timestamp);

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
            query.bindValue(0, msg.id);
            query.bindValue(1, msg.serverId);
            query.bindValue(2, msg.conversationId);
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
            query.bindValue(13, msg.timestamp);

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

    void doGetMessages(const QString conversationId, int limit, qint64 beforeTimestamp, const QObject* context, IMessageRepository::FetchCallback callback) {
        std::vector<Domain::Message> results;
        if (!m_db.isOpen()) {
            invokeFetchCallback(context, callback, results);
            return;
        }

        QSqlQuery query(m_db);
        if (beforeTimestamp > 0) {
            query.prepare("SELECT id, server_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp "
                          "FROM messages WHERE conversation_id = ? AND timestamp < ? ORDER BY timestamp DESC LIMIT ?");
            query.addBindValue(conversationId);
            query.addBindValue(beforeTimestamp);
            query.addBindValue(limit);
        } else {
            query.prepare("SELECT id, server_id, sender_id, type, text, media_url, file_name, file_size, duration, waveform, status, error_text, timestamp "
                          "FROM messages WHERE conversation_id = ? ORDER BY timestamp DESC LIMIT ?");
            query.addBindValue(conversationId);
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
                msg.timestamp = query.value(12).toLongLong();
                
                results.push_back(msg);
            }
            std::reverse(results.begin(), results.end());
        } else {
            qWarning() << "[SqlMessageRepository] Fetch failed:" << query.lastError().text();
        }

        invokeFetchCallback(context, callback, results);
    }

private:
    QString m_dbPath;
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

void SqlMessageRepository::getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) {
    QMetaObject::invokeMethod(m_worker, [this, conversationId, limit, beforeTimestamp, context, callback]() {
        m_worker->doGetMessages(conversationId, limit, beforeTimestamp, context, callback);
    }, Qt::QueuedConnection);
}

}
}

#include "sqlmessagerepository.moc"
