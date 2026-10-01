#pragma once
#include <QString>
#include <vector>
#include <optional>
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
};

class SqliteMessageStorage : public IMessageStorage {
public:
    explicit SqliteMessageStorage(const QString& dbPath);
    ~SqliteMessageStorage() override;

    bool saveMessage(const Message& msg) override;
    std::optional<Message> getMessage(const QString& messageId) override;
    bool updateMessageState(const QString& messageId, MessageState newState) override;
    std::vector<Message> getConversationMessages(const QString& conversationId) override;

private:
    QString m_dbPath;
    QString m_connectionName;
    void initDatabase();
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
