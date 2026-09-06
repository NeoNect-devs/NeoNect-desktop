#pragma once
#include "domain/message.h"
#include <vector>
#include <functional>
#include <QObject>

namespace NeoNect {
namespace Storage {

class IMessageRepository {
public:
    virtual ~IMessageRepository() = default;

    // Async operations. Callbacks are invoked on the thread that calls these methods (or main thread if specified)
    // For simplicity, we can pass a QObject* context and use QMetaObject::invokeMethod or similar.
    using SaveCallback = std::function<void(bool success)>;
    using FetchCallback = std::function<void(const std::vector<Domain::Message>& messages)>;

    virtual void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) = 0;
    virtual void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) = 0;
    virtual void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) = 0;
    virtual void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) = 0;
};

}
}
