/**
 * @file application.h
 * @brief Application Controller, Composition Root, and Bootstrap Engine for NeoNect Desktop.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Serves as the central bootstrap class, Composition Root, and lifecycle controller for the NeoNect
 * client desktop application. Manages CLI argument parsing, multi-tier dependency injection,
 * logging configuration, QML type registrations, global user activity interception, and the
 * Qt event loop execution.
 *
 * @par Design Patterns:
 * - <b>Application Controller Pattern</b>: Centralizes application lifecycle, initialization, execution, and shutdown.
 * - <b>Composition Root Pattern</b>: The single location in the codebase where all service dependencies and infrastructure repositories are instantiated and wired together.
 * - <b>Event Filter / Decorator Pattern</b>: Installs a global GUI event filter (`m_activityFilter`) to intercept user input and report presence activity.
 * - <b>RAII Lifetime Management</b>: Uses `std::unique_ptr` and `std::shared_ptr` to ensure deterministic destruction of threads and network sockets.
 *
 * @par Lifecycle & Concurrency Invariants:
 * - <b>Main Thread Exclusivity</b>: Must be constructed, initialized, and executed on the primary process thread (GUI thread).
 *   Qt GUI objects and `QQmlApplicationEngine` cannot be accessed or migrated to worker threads.
 * - <b>Single Instance Guarantee</b>: Only one `Application` instance may exist per process. Copy and move operations are deleted.
 * - <b>Deterministic Teardown Sequence</b>: Upon application exit, QML visual trees are destroyed first,
 *   followed by high-level business services, then background transport and storage worker threads, and finally
 *   cryptographic resources, ensuring zero resource leakage or race conditions during exit.
 * - <b>Fail-Safe Bootstrap</b>: If `Main.qml` fails to compile or instantiate, `run()` immediately terminates with exit code -1
 *   without starting the event loop.
 */

#pragma once
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>
#include <memory>

class CryptoManager;
class NetworkManager;
class AudioManager;
#include "services/messageservice.h"
#include "storage/imessagerepository.h"

namespace NeoNect {

namespace Storage { class SettingsRepository; }
namespace Crypto { class ICryptoService; }
namespace Transport { class IHttpTransport; }
namespace Core { class NotificationManager; class VersionInfo; }
namespace Services { class RelayService; }

/**
 * @class Application
 * @brief Application bootstrap and lifecycle controller acting as the Composition Root.
 *
 * @par Architectural Constraints:
 * - Composition wiring must follow strict DAG: Infrastructure -> Repositories -> Services -> UI Facades.
 * - No service layer component may depend directly on QML or UI types.
 */
class Application {
public:
    /**
     * @brief Constructs the application bootstrap controller.
     * @param argc Reference to command-line argument count.
     * @param argv Pointer to command-line argument strings.
     * @pre `argc > 0 && argv != nullptr`.
     * @pre Must be called from the process entry point (`main()`) on the primary thread.
     * @post Underlying `QGuiApplication` instance is created and DPI scaling policies configured.
     */
    explicit Application(int &argc, char **argv);

    /**
     * @brief Destructor. Ensures graceful teardown of worker threads, network sockets, and database connections.
     * @post Subsystems are unwound in reverse order of initialization.
     */
    ~Application();

    // Disable copy and move semantics to enforce single application instance
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;

    /**
     * @brief Executes application bootstrap, initializes services, loads QML, and runs the Qt event loop.
     * @return Process exit code returned by `QGuiApplication::exec()` (0 on success, -1 on startup failure).
     * @pre Subsystems must not have been previously initialized in the same process.
     * @post Blocks until the user closes the application or `QCoreApplication::exit()` is triggered.
     */
    int run();

private:
    /**
     * @brief Configures standard logging categories, formatters, and optional log-to-file handlers.
     */
    void setupLogging();

    /**
     * @brief Parses CLI options (e.g. `--profile`, `--mock`, `--server`, `--dev`).
     * @post Configures `m_profile` and sets active operational mode flags.
     */
    void parseCommandLine();

    /**
     * @brief Composition Root wiring: instantiates repositories, crypto, transport, services, and managers.
     * @post All shared instances (`m_cryptoService`, `m_transport`, `m_storage`, `m_messageRepo`, `m_relayService`)
     *       are valid and interconnected.
     */
    void initializeServices();

    /**
     * @brief Registers custom C++ models and facades with the QML type system.
     * @post QML singletons and types (`ChatMessageModel`, `ThemeData`, `NetworkManager`, `AudioManager`)
     *       are registered in the `"NeoNect"` QML namespace.
     */
    void registerQmlTypes();

    /**
     * @brief Loads `Main.qml` into the QQmlApplicationEngine and connects root window signals.
     * @return True if QML root objects were successfully instantiated; false on syntax or component error.
     */
    bool loadMainUi();

    /**
     * @brief Resolves the user-specific SQLite database file path in AppData/Local.
     * @param username Active profile username.
     * @return Absolute file path to user SQLite database.
     * @pre `username` must not contain path traversal characters.
     * @post Target directory is verified to exist on disk.
     */
    QString getUserDatabasePath(const QString &username) const;

    /** @brief Underlying Qt GUI application instance. */
    std::unique_ptr<QGuiApplication> m_app;
    /** @brief Declarative QML runtime engine. */
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    
    // Core Infrastructure
    /** @brief Persistent client settings repository. */
    std::shared_ptr<Storage::SettingsRepository> m_storage;
    /** @brief Asynchronous SQLite message history repository. */
    std::shared_ptr<Storage::IMessageRepository> m_messageRepo;
    /** @brief Concrete OpenSSL cryptographic service. */
    std::shared_ptr<Crypto::ICryptoService> m_cryptoService;
    /** @brief Production HTTP network transport. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;

    // Facades & Managers
    /** @brief Cryptographic facade exposed to QML. */
    std::unique_ptr<::CryptoManager> m_cryptoManager;
    /** @brief Master networking facade exposed to QML. */
    std::unique_ptr<::NetworkManager> m_networkManager;
    /** @brief Audio capture and playback manager exposed to QML. */
    std::unique_ptr<::AudioManager> m_audioManager;
    /** @brief Toast notification manager exposed to QML. */
    std::unique_ptr<Core::NotificationManager> m_notificationManager;
    /** @brief Build metadata and changelog provider exposed to QML. */
    std::unique_ptr<Core::VersionInfo> m_versionInfo;
    /** @brief Messaging business service coordinator. */
    std::unique_ptr<Services::MessageService> m_messageService;
    /** @brief Encrypted packet relay and WebSocket gateway. */
    std::shared_ptr<Services::RelayService> m_relayService;
    /** @brief Global event filter monitoring user activity for AFK/idle detection. */
    std::unique_ptr<QObject> m_activityFilter;
    
    /** @brief Active profile identifier passed from command line. */
    QString m_profile;
    /** @brief Flag enabling mock network/crypto drivers. */
    bool m_isMockMode{false};
};

} // namespace NeoNect
