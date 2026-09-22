/**
 * @file authservice.h
 * @brief Service layer coordinator managing user authentication, registration, and session lifecycle.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Encapsulates all identity and account management workflows against the NeoNect server API:
 * server availability verification, username collision checks, user registration, JWT login,
 * profile synchronization, and session clearance upon logout or 401 expiration.
 *
 * @par Design Patterns:
 * - <b>Service Layer Pattern</b>: Defines application boundary and coordinates domain workflow.
 * - <b>Dependency Injection</b>: Relies on `IHttpTransport` and `ISettingsRepository` interfaces.
 * - <b>Observer Pattern</b>: Emits Qt signals upon asynchronous completion of network operations.
 */

#pragma once
#include <QObject>
#include <memory>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"

namespace NeoNect {
namespace Services {

/**
 * @class AuthService
 * @brief Coordinates user credentials, authentication tokens, and server connectivity verification.
 */
class AuthService : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the authentication service.
     * @param transport Shared pointer to HTTP transport layer.
     * @param storage Shared pointer to persistent settings repository.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit AuthService(std::shared_ptr<Transport::IHttpTransport> transport,
                         std::shared_ptr<Storage::ISettingsRepository> storage,
                         QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~AuthService() override = default;

    /**
     * @brief Pings and verifies server reachability and API compatibility.
     * @param address Raw server address or domain (e.g. `"http://localhost:8080"`).
     */
    void verifyServer(const QString &address);

    /**
     * @brief Inquires whether a specific username is available for new registration.
     * @param username Desired username.
     */
    void checkUsernameAvailability(const QString &username);

    /**
     * @brief Registers a new user account on the server.
     * @param username Chosen username.
     * @param password Raw plaintext password to be salted and hashed by server.
     */
    void registerUser(const QString &username, const QString &password);

    /**
     * @brief Authenticates user credentials and stores the issued JWT Bearer token upon success.
     * @param username User identifier.
     * @param password User secret password.
     */
    void loginUser(const QString &username, const QString &password);

    /**
     * @brief Terminates the active session, clears persisted tokens, and purges credentials.
     */
    void logoutUser();

    /**
     * @brief Fetches account metadata, profile information, and server-side avatar URL.
     */
    void fetchUserProfile();

signals:
    /**
     * @brief Emitted when a server verification ping completes.
     * @param success True if the server responded with valid metadata.
     * @param message Status or error string.
     */
    void verificationResult(bool success, const QString &message);

    /**
     * @brief Emitted when a username availability query finishes.
     * @param username The queried username.
     * @param available True if the username is free to register.
     * @param error Error description if the check failed.
     */
    void availabilityResult(const QString &username, bool available, const QString &error);

    /**
     * @brief Emitted when account registration finishes.
     * @param success True if account was created.
     * @param message Feedback message.
     */
    void registrationResult(bool success, const QString &message);

    /**
     * @brief Emitted when login authentication finishes.
     * @param success True if authentication succeeded.
     * @param tokenOrError Bearer token string on success, error message on failure.
     */
    void loginResult(bool success, const QString &tokenOrError);

    /**
     * @brief Emitted when user profile details have been synchronized.
     * @param success True if successfully retrieved.
     * @param username Confirmed user identifier.
     */
    void userProfileFetched(bool success, const QString &username);

    /**
     * @brief Emitted when the server rejects a session token with HTTP 401 Unauthorized.
     */
    void authSessionExpired();

private:
    /** @brief Injected HTTP transport layer. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    /** @brief Injected persistent settings repository. */
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
};

} // namespace Services
} // namespace NeoNect
