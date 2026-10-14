#pragma once
#include <QString>
#include <vector>
#include <optional>
#include <QMutex>
#include <QThread>
#include <QSqlDatabase>
#include <memory>
#include "Message.h"

namespace NeoNect {
namespace Core {
namespace Messaging {

class IMessageStorage {
public:
    virtual ~IMessageStorage() = default;
    virtual bool saveMessage(const Message& msg) = 0;
    virtual bool saveMessages(const std::vector<Message>& msgs) {
        bool allOk = true;
        for (const auto& m : msgs) {
            if (!saveMessage(m)) allOk = false;
        }
        return allOk;
    }
    virtual std::optional<Message> getMessage(const QString& messageId) = 0;
    virtual bool updateMessageState(const QString& messageId, MessageState newState, const QString& errorText = QString()) = 0;
    virtual bool updateMessageMediaUrl(const QString& messageId, const QString& mediaUrl) {
        Q_UNUSED(messageId);
        Q_UNUSED(mediaUrl);
        return false;
    }
    virtual std::vector<Message> getConversationMessages(const QString& conversationId) = 0;
    virtual bool deleteMessage(const QString& messageId) = 0;
};

class SqliteMessageStorage : public IMessageStorage {
public:
    explicit SqliteMessageStorage(const QString& dbPath);
    ~SqliteMessageStorage() override;

    bool saveMessage(const Message& msg) override;
    bool saveMessages(const std::vector<Message>& msgs) override;
    std::optional<Message> getMessage(const QString& messageId) override;
    bool updateMessageState(const QString& messageId, MessageState newState, const QString& errorText = QString()) override;
    bool updateMessageMediaUrl(const QString& messageId, const QString& mediaUrl) override;
    std::vector<Message> getConversationMessages(const QString& conversationId) override;
    bool deleteMessage(const QString& messageId) override;

private:
    QString m_dbPath;
    QString m_instanceId;
    std::shared_ptr<bool> m_aliveToken;
    QRecursiveMutex m_mutex;

    QSqlDatabase getDatabase();
};

} // namespace Messaging
} // namespace Core
} // namespace NeoNect
