/**
 * @file cryptomanager.h
 * @brief High-level facade coordinating cryptographic key generation, credentials, and passphrase derivation.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Exposes encryption capabilities, passphrase-based master key derivation, and local device
 * credentials provisioning to the QML runtime and GUI layer. Acts as a facade orchestrating
 * `ICryptoService` and `ISettingsRepository`.
 *
 * @par Design Patterns:
 * - <b>Facade Pattern</b>: Simplifies low-level OpenSSL cryptographic workflows for declarative QML consumption.
 * - <b>Dependency Injection</b>: Accepts abstract `ICryptoService` and `ISettingsRepository` instances.
 * - <b>Observer Pattern</b>: Emits signals when background encryption or decryption tasks complete.
 */

#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>
#include "../crypto/icryptoservice.h"
#include "../storage/isettingsrepository.h"

/**
 * @class CryptoManager
 * @brief QML-accessible facade for cryptographic operations and device identity keys.
 */
class CryptoManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the cryptographic facade manager.
     * @param cryptoService Shared pointer to cryptographic engine.
     * @param settingsRepo Shared pointer to persistent settings repository.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit CryptoManager(std::shared_ptr<NeoNect::Crypto::ICryptoService> cryptoService = nullptr,
                           std::shared_ptr<NeoNect::Storage::ISettingsRepository> settingsRepo = nullptr,
                           QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CryptoManager() override = default;

    /**
     * @brief Returns shared pointer to underlying crypto service strategy.
     */
    std::shared_ptr<NeoNect::Crypto::ICryptoService> service() const { return m_cryptoService; }

    /**
     * @brief Switches the active profile namespace and reloads associated keys.
     * @param profileName Target profile identifier.
     */
    Q_INVOKABLE void setProfile(const QString &profileName);

    /**
     * @brief Retrieves the persistent client device UUID.
     * @return Device UUID string.
     */
    Q_INVOKABLE QString getDeviceId();

    /**
     * @brief Retrieves the persistent client device public key.
     * @return Base64 public key string.
     */
    Q_INVOKABLE QString getDevicePublicKey();

    /**
     * @brief Derives master encryption key from user passphrase using PBKDF2-HMAC-SHA256.
     * @param passphrase Password secret string.
     */
    Q_INVOKABLE void initializeKeyFromPassphrase(const QString &passphrase);

    /**
     * @brief Asynchronously encrypts a plaintext message for a specific conversation channel.
     * @param channelId Target channel identifier.
     * @param plainText Plaintext message string.
     */
    Q_INVOKABLE void encryptMessageAsposing(const QString &channelId, const QString &plainText);

signals:
    /**
     * @brief Emitted when an asynchronous encryption task finishes.
     * @param channelId Target channel ID.
     * @param cipherBase64 Base64 ciphertext with authentication tag.
     * @param nonceBase64 Base64 initialization vector (IV).
     */
    void encryptionCompleted(const QString &channelId, const QString &cipherBase64, const QString &nonceBase64);

    /**
     * @brief Emitted when an asynchronous decryption task finishes.
     * @param messageId Unique message ID.
     * @param plainText Recovered plaintext string.
     */
    void decryptionCompleted(const QString &messageId, const QString &plainText);

private:
    /**
     * @brief Verifies that device ID and asymmetric keypair exist, generating new credentials if missing.
     */
    void ensureDeviceCredentials();

    /** @brief Underlying cryptographic service. */
    std::shared_ptr<NeoNect::Crypto::ICryptoService> m_cryptoService;
    /** @brief Persistent settings repository. */
    std::shared_ptr<NeoNect::Storage::ISettingsRepository> m_settingsRepo;
};