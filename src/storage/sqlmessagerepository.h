/**
 * @file sqlmessagerepository.h
 * @brief SQLite-backed implementation of IMessageRepository using an Active Object background worker thread.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Handles persistent message storage in local SQLite database files. To guarantee high frame rates
 * and responsive UI interactions, all database queries and schema initializations execute exclusively
 * on a dedicated background `QThread` via `SqlMessageRepositoryWorker`.
 *
 * @par Design Patterns:
 * - <b>Active Object Pattern</b>: Decouples method invocation on the main thread from execution on a dedicated I/O thread.
 * - <b>Proxy / Adapter Pattern</b>: `SqlMessageRepository` acts as the thread-safe proxy delegating to `SqlMessageRepositoryWorker`.
 * - <b>Repository Pattern</b>: Encapsulates message persistence, indices, schema updates, and pagination queries.
 *
 * @par Concurrency Constraints:
 * - All public repository methods can be safely called from the GUI main thread or any worker thread.
 * - Raw `QSqlDatabase` handle manipulation is strictly confined to `m_workerThread` to prevent Qt SQL multi-threading errors.
 * - Callbacks are safely marshalled back to the calling object's thread context via `QPointer<QObject>`.
 *
 * @par SQLite Pragmas & Invariants:
 * - `PRAGMA journal_mode = WAL;` (Write-Ahead Logging for concurrent read/write transactions).
 * - `PRAGMA synchronous = NORMAL;` (Ensures durability while minimizing disk sync overhead).
 * - `PRAGMA foreign_keys = ON;` (Referential integrity enforcement).
 */

#pragma once
#include "storage/imessagerepository.h"
#include <QObject>
#include <QThread>
#include <QString>

namespace NeoNect {
namespace Storage {

class SqlMessageRepositoryWorker;

/**
 * @class SqlMessageRepository
 * @brief Thread-safe SQLite repository proxy managing background persistence.
 *
 * @par Threading:
 * All calls are non-blocking and queued to `m_workerThread`. Callbacks are invoked in caller's thread.
 */
class SqlMessageRepository : public QObject, public IMessageRepository {
    Q_OBJECT
public:
    /**
     * @brief Constructs the repository and spawns the background worker thread.
     * @param dbPath Absolute file path to the SQLite database file.
     * @param parent Optional parent QObject for memory hierarchy.
     * @pre `dbPath` must be a valid directory path or absolute target database file location.
     * @post Background `QThread` is started and SQLite tables/indices are verified or created.
     */
    explicit SqlMessageRepository(const QString& dbPath, QObject* parent = nullptr);

    /**
     * @brief Destructor. Gracefully stops the worker thread, waits for termination, and cleans up resources.
     * @post Pending background tasks complete or cancel, worker thread exits, and SQLite connections close.
     */
    ~SqlMessageRepository() override;

    /**
     * @brief Dispatches asynchronous save of a single message to worker thread.
     * @param msg Domain message to save.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     * @pre `msg.id()` must be a non-empty UUID string.
     * @post Task is posted to worker event loop; callback fires with success boolean.
     */
    void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous batch save within a single transaction to worker thread.
     * @param msgs Vector of domain messages.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     * @pre `msgs.size() <= 5000`.
     * @post Entire batch is saved inside an atomic SQLite `BEGIN IMMEDIATE ... COMMIT` block.
     */
    void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous status update to worker thread.
     * @param id Message UUID.
     * @param status New delivery status.
     * @param errorText Error description if failed.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     * @pre `id` must be non-empty.
     * @post Database record is updated with new status code.
     */
    void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous seen marker update to worker thread.
     * @param conversationId Scope identifier.
     * @param senderId Author whose messages were read.
     * @param context Optional lifetime guard object.
     * @param callback Optional completion callback.
     * @pre `conversationId` and `senderId` must be non-empty strings.
     * @post Matching rows are marked with status `Read`.
     */
    void markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context = nullptr, SaveCallback callback = nullptr) override;

    /**
     * @brief Dispatches asynchronous message deletion to worker thread.
     * @param id Message UUID.
     * @param context Optional lifetime guard object.
     * @param callback Optional completion callback.
     * @pre `id` must be non-empty.
     * @post Matching record is purged from SQLite message table.
     */
    void deleteMessageAsync(const QString &id, const QObject* context = nullptr, SaveCallback callback = nullptr) override;

    /**
     * @brief Dispatches asynchronous page query to worker thread.
     * @param conversationId Scope identifier.
     * @param limit Page size (clamped to [1..1000]).
     * @param beforeTimestamp Timestamp cursor.
     * @param context Lifetime guard object.
     * @param callback Callback receiving message results.
     * @pre `limit >= 1 && limit <= 1000`.
     * @post Results are returned sorted chronologically ascending.
     */
    void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) override;

    /**
     * @brief Dispatches asynchronous single message lookup to worker thread.
     * @param id Message UUID.
     * @param context Lifetime guard object.
     * @param callback Callback receiving message result if found.
     * @pre `id` must be non-empty.
     * @post Callback receives `std::optional<Domain::Message>` matching `id`.
     */
    void getMessageByIdAsync(const QString &id, const QObject* context, MessageCallback callback) override;

    /**
     * @brief Requests worker thread to switch to a different SQLite database file.
     * @param dbPath Absolute file path to the new database.
     * @pre `dbPath` must be a valid filesystem target.
     * @post SQLite connection is closed and re-opened for `dbPath`.
     */
    void switchDatabase(const QString& dbPath) override;

private:
    /** @brief Dedicated worker thread for running database I/O. */
    QThread* m_workerThread;
    /** @brief Worker object living inside @ref m_workerThread executing raw SQLite queries. */
    SqlMessageRepositoryWorker* m_worker;
};

} // namespace Storage
} // namespace NeoNect
