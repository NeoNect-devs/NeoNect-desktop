/**
 * @file deviceservice.h
 * @brief Service layer coordinator managing client device identity, cryptographic public keys, and multi-device key distribution.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Manages device registration on the backend server, queries recipient device public keys for
 * multi-device end-to-end encryption (E2EE), and performs device revocation. Implements an automatic
 * retry mechanism with backoff for device registration to handle network latency or early connection attempts.
 *
 * @par Design Patterns:
 * - <b>Service Layer Pattern</b>: Orchestrates device key exchange and registration operations.
 * - <b>Retry Pattern</b>: Recursively attempts device registration with bounded backoff.
 * - <b>Observer Pattern</b>: Emits signals for key discovery and device lifecycle status.
 */

#pragma once
#include <QObject>
#include <memory>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"

namespace NeoNect {
namespace Services {

/**
 * @class DeviceService
 * @brief Coordinates device identity keys and cryptographic recipient bundles.
 */
class DeviceService : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the device management service.
     * @param transport Shared pointer to HTTP transport abstraction.
     * @param storage Shared pointer to settings repository.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit DeviceService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~DeviceService() override = default;

    /**
     * @brief Registers the current device's public key on the server.
     * @param deviceId Unique client device UUID.
     * @param publicKey Base64-encoded public key.
     */
    void registerDevice(const QString &deviceId, const QString &publicKey);

    /**
     * @brief Fetches the public key associated with a specific device identifier.
     * @param deviceId Target device UUID.
     */
    void fetchDevicePublicKey(const QString &deviceId);

    /**
     * @brief Revokes a registered device, invalidating its cryptographic identity.
     * @param deviceId Device UUID to revoke.
     */
    void revokeDevice(const QString &deviceId);

    /**
     * @brief Queries public keys for all active devices registered under a recipient username.
     * @param username Recipient username.
     */
    void fetchRecipientKeys(const QString &username);

signals:
    /**
     * @brief Emitted when device registration succeeds or fails.
     * @param success True if registered.
     * @param message Informational or error description.
     */
    void deviceRegistrationResult(bool success, const QString &message);

    /**
     * @brief Emitted when a queried device public key is returned.
     * @param deviceId Queried device UUID.
     * @param publicKey Base64-encoded public key string.
     */
    void deviceKeyFetched(const QString &deviceId, const QString &publicKey);

    /**
     * @brief Emitted when device revocation succeeds or fails.
     * @param success True if revoked.
     * @param message Informational or error description.
     */
    void deviceRevocationResult(bool success, const QString &message);

    /**
     * @brief Emitted when recipient device public key bundles are received.
     * @param username Recipient username.
     * @param devices List of device maps containing `deviceId` and `publicKey`.
     */
    void recipientKeysFetched(const QString &username, const QVariantList &devices);

private:
    /**
     * @brief Internal recursive helper for retrying device registration.
     * @param deviceId Device UUID.
     * @param publicKey Base64 public key.
     * @param attempt Current retry attempt index.
     */
    void registerDeviceInternal(const QString &deviceId, const QString &publicKey, int attempt);

    /** @brief HTTP transport layer. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    /** @brief Settings repository. */
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
};

} // namespace Services
} // namespace NeoNect
