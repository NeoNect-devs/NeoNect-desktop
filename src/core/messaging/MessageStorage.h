#pragma once
#include <QString>
#include <vector>
#include <optional>
#include <QMutex>
#include <QHash>
#include <QThread>
#include <QSqlDatabase>
#include "Message.h"

namespace NeoNect {
namespace Core {
namespace Messaging {

class IMessageStorage {
public:
    virtual ~IMessageStorage() = default;
    virtual bool saveMessage(const Message& msg) = 0;
    virtual std::optional<Message> getMessage(const QString& messageId) = 0;
    virtual bool updateMessageState(const QString& messageId, MessageState newState) = 0;
    virtual std::vector<Message> getConversationMessages(const QString& conversationId) = 0;
    virtual bool deleteMessage(const QString& messageId) = 0;
};

class SqliteMessageStorage : public IMessageStorage {
public:
    explicit SqliteMessageStorage(const QString& dbPath);
    ~SqliteMessageStorage() override;

    bool saveMessage(const Message& msg) override;
    std::optional<Message> getMessage(const QString& messageId) override;
    bool updateMessageState(const QString& messageId, MessageState newState) override;
    std::vector<Message> getConversationMessages(const QString& conversationId) override;
    bool deleteMessage(const QString& messageId) override;

private:
    QString m_dbPath;
    QMutex m_mutex;
    QHash<Qt::HANDLE, QString> m_connectionNames;

    QSqlDatabase getDatabase();
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
