/**
 * @file imessagerepository.h
 * @brief Abstract asynchronous repository interface for persisting and querying chat messages.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Defines the persistence contract for domain messages. All database operations are asynchronous
 * to prevent blocking the GUI thread during intensive disk I/O, database migrations, or bulk batch operations.
 * Callbacks accept a `QObject* context` guard to guarantee that deleted UI controllers are not invoked.
 *
 * @par Design Patterns:
 * - <b>Repository Pattern</b>: Encapsulates database schema, SQL queries, and object mapping.
 * - <b>Asynchronous Completion Token</b>: Uses `std::function` callbacks bound to `QObject` contexts.
 * - <b>Unit of Work / Batching</b>: Supports transactional multi-message saves via `saveMessagesAsync`.
 */

#pragma once
#include "domain/message.h"
#include <vector>
#include <functional>
#include <optional>
#include <QObject>

namespace NeoNect {
namespace Storage {

/**
 * @class IMessageRepository
 * @brief Abstract interface for non-blocking local message history storage.
 */
class IMessageRepository {
public:
    /**
     * @brief Virtual destructor for clean polymorphic disposal.
     */
    virtual ~IMessageRepository() = default;

    /** @brief Completion callback invoked when a write operation completes. */
    using SaveCallback = std::function<void(bool success)>;

    /** @brief Completion callback invoked when a batch of messages is queried. */
    using FetchCallback = std::function<void(const std::vector<Domain::Message>& messages)>;

    /** @brief Completion callback invoked when a single message is queried by ID. */
    using MessageCallback = std::function<void(const std::optional<Domain::Message>&)>;

    /**
     * @brief Asynchronously persists a single message into local storage.
     * @param msg The message entity to persist.
     * @param context Lifetime tracking object. If destroyed before completion, callback is skipped.
     * @param callback Result notification callback.
     */
    virtual void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously persists a collection of messages within a single atomic transaction.
     * @param msgs Vector of message entities to insert or update.
     * @param context Lifetime tracking object.
     * @param callback Result notification callback.
     */
    virtual void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously updates the delivery status and optional error message of an existing message.
     * @param id Message UUID.
     * @param status New delivery lifecycle status.
     * @param errorText Diagnostic error string if failed.
     * @param context Lifetime tracking object.
     * @param callback Result notification callback.
     */
    virtual void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously marks all messages from a specific sender in a conversation as seen.
     * @param conversationId Scope identifier (e.g. `"dms:<username>"`).
     * @param senderId Author username whose messages should be marked as seen.
     * @param context Optional lifetime tracking object.
     * @param callback Optional result callback.
     */
    virtual void markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context = nullptr, SaveCallback callback = nullptr) = 0;

    /**
     * @brief Asynchronously deletes a message by its local identifier.
     * @param id Message UUID.
     * @param context Optional lifetime tracking object.
     * @param callback Optional result callback.
     */
    virtual void deleteMessageAsync(const QString &id, const QObject* context = nullptr, SaveCallback callback = nullptr) = 0;

    /**
     * @brief Asynchronously retrieves a page of messages for a conversation ordered by timestamp descending.
     * @param conversationId Scope identifier.
     * @param limit Maximum number of records to return.
     * @param beforeTimestamp Timestamp cursor for pagination (0 retrieves latest).
     * @param context Lifetime tracking object.
     * @param callback Callback receiving the retrieved messages.
     */
    virtual void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) = 0;

    /**
     * @brief Asynchronously searches for a specific message by its UUID.
     * @param id Message UUID.
     * @param context Lifetime tracking object.
     * @param callback Callback receiving the optional message if found.
     */
    virtual void getMessageByIdAsync(const QString &id, const QObject* context, MessageCallback callback) = 0;

    /**
     * @brief Safely closes the active SQLite connection and opens another database file.
     * @param dbPath Absolute file path to the target SQLite database.
     */
    virtual void switchDatabase(const QString& dbPath) = 0;
};

} // namespace Storage
} // namespace NeoNect
