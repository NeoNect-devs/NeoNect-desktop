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
 */
class SqlMessageRepository : public QObject, public IMessageRepository {
    Q_OBJECT
public:
    /**
     * @brief Constructs the repository and spawns the background worker thread.
     * @param dbPath Absolute file path to the SQLite database file.
     * @param parent Optional parent QObject for memory hierarchy.
     */
    explicit SqlMessageRepository(const QString& dbPath, QObject* parent = nullptr);

    /**
     * @brief Destructor. Gracefully stops the worker thread, waits for termination, and cleans up resources.
     */
    ~SqlMessageRepository() override;

    /**
     * @brief Dispatches asynchronous save of a single message to worker thread.
     * @param msg Domain message to save.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     */
    void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous batch save within a single transaction to worker thread.
     * @param msgs Vector of domain messages.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     */
    void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous status update to worker thread.
     * @param id Message UUID.
     * @param status New delivery status.
     * @param errorText Error description if failed.
     * @param context Lifetime guard object.
     * @param callback Completion callback.
     */
    void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) override;

    /**
     * @brief Dispatches asynchronous seen marker update to worker thread.
     * @param conversationId Scope identifier.
     * @param senderId Author whose messages were read.
     * @param context Optional lifetime guard object.
     * @param callback Optional completion callback.
     */
    void markMessagesSeenAsync(const QString &conversationId, const QString &senderId, const QObject* context = nullptr, SaveCallback callback = nullptr) override;

    /**
     * @brief Dispatches asynchronous message deletion to worker thread.
     * @param id Message UUID.
     * @param context Optional lifetime guard object.
     * @param callback Optional completion callback.
     */
    void deleteMessageAsync(const QString &id, const QObject* context = nullptr, SaveCallback callback = nullptr) override;

    /**
     * @brief Dispatches asynchronous page query to worker thread.
     * @param conversationId Scope identifier.
     * @param limit Page size.
     * @param beforeTimestamp Timestamp cursor.
     * @param context Lifetime guard object.
     * @param callback Callback receiving message results.
     */
    void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) override;

    /**
     * @brief Dispatches asynchronous single message lookup to worker thread.
     * @param id Message UUID.
     * @param context Lifetime guard object.
     * @param callback Callback receiving message result if found.
     */
    void getMessageByIdAsync(const QString &id, const QObject* context, MessageCallback callback) override;

    /**
     * @brief Requests worker thread to switch to a different SQLite database file.
     * @param dbPath Absolute file path to the new database.
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
