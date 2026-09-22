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
 *
 * @par Threading & Concurrency Constraints:
 * - Implementations must ensure that all SQLite queries, transactions, and I/O are executed off the UI thread.
 * - The `context` pointer must be checked for validity prior to executing callbacks to prevent use-after-free.
 * - Callback invocation must be dispatched back to the thread context associated with `context` (typically GUI thread).
 *
 * @par Data & Storage Constraints:
 * - <b>Message IDs</b>: Canonical RFC 4122 UUID (36 characters).
 * - <b>Timestamps</b>: Unix epoch in milliseconds (`qint64 > 0`).
 * - <b>Pagination Limit</b>: Bounded between 1 and 1,000 items per request.
 * - <b>Batch Size</b>: Transactional batching bounded to <= 5,000 messages per transaction.
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
 *
 * @par Operational Bounds:
 * - Maximum query batch size: 1,000 records.
 * - Message content max size: 65,536 bytes (64 KB).
 * - Conversation identifier max length: 256 characters.
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
     * @pre `msg.id()` must be a valid non-empty UUID string.
     * @pre `msg.timestamp()` must be positive (> 0).
     * @post Record is inserted into database or updated if matching `id` exists (UPSERT).
     */
    virtual void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously persists a collection of messages within a single atomic transaction.
     * @param msgs Vector of message entities to insert or update.
     * @param context Lifetime tracking object.
     * @param callback Result notification callback.
     * @pre `msgs.size() <= 5000` to prevent excessive transaction lock duration.
     * @post All messages are atomically committed; if any query fails, the entire transaction rolls back.
     */
    virtual void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously updates the delivery status and optional error message of an existing message.
     * @param id Message UUID.
     * @param status New delivery lifecycle status.
     * @param errorText Diagnostic error string if failed.
     * @param context Lifetime tracking object.
     * @param callback Result notification callback.
     * @pre `id` must be non-empty.
     * @post Message status and error text are updated on disk.
     */
    virtual void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) = 0;

    /**
     * @brief Asynchronously marks all messages from a specific sender in a conversation as seen.
     * @param conversationId Scope identifier (e.g. `"dms:<username>"`).
     * @param senderId Author username whose messages should be marked as seen.
     * @param context Optional lifetime tracking object.
     * @param callback Optional result callback.
     * @pre `conversationId` and `senderId` must be non-empty strings.
     * @post All matching records receive `MessageStatus::Read`.
     */
    virtual void markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context = nullptr, SaveCallback callback = nullptr) = 0;

    /**
     * @brief Asynchronously deletes a message by its local identifier.
     * @param id Message UUID.
     * @param context Optional lifetime tracking object.
     * @param callback Optional result callback.
     * @pre `id` must be non-empty.
     * @post Record matching `id` is deleted from database table.
     */
    virtual void deleteMessageAsync(const QString &id, const QObject* context = nullptr, SaveCallback callback = nullptr) = 0;

    /**
     * @brief Asynchronously retrieves a page of messages for a conversation ordered by timestamp descending.
     * @param conversationId Scope identifier.
     * @param limit Maximum number of records to return (clamped to [1..1000]).
     * @param beforeTimestamp Timestamp cursor for pagination (0 retrieves newest messages).
     * @param context Lifetime tracking object.
     * @param callback Callback receiving the retrieved messages.
     * @pre `limit >= 1 && limit <= 1000`.
     * @pre `beforeTimestamp >= 0`.
     * @post Returns up to `limit` messages with `timestamp < beforeTimestamp` (or newest if `beforeTimestamp == 0`).
     */
    virtual void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) = 0;

    /**
     * @brief Asynchronously searches for a specific message by its UUID.
     * @param id Message UUID.
     * @param context Lifetime tracking object.
     * @param callback Callback receiving the optional message if found.
     * @pre `id` must be non-empty.
     * @post Invokes `callback` with `std::nullopt` if not found or populated `Domain::Message` if found.
     */
    virtual void getMessageByIdAsync(const QString &id, const QObject* context, MessageCallback callback) = 0;

    /**
     * @brief Safely closes the active SQLite connection and opens another database file.
     * @param dbPath Absolute file path to the target SQLite database.
     * @pre `dbPath` must be an absolute filesystem path.
     * @post Old connection is closed, WAL checkpoints committed, and new connection opened and initialized.
     */
    virtual void switchDatabase(const QString& dbPath) = 0;
};

} // namespace Storage
} // namespace NeoNect
