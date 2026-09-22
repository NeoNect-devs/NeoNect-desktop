/**
 * @file icryptoservice.h
 * @brief Abstract interface defining end-to-end cryptographic and key derivation services.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * This header defines the `ICryptoService` interface, which dictates the cryptographic contract
 * across the NeoNect client. NeoNect relies on symmetric AES-256-GCM authenticated encryption with
 * associated data (AEAD) and PBKDF2/Argon2 key derivation for securing local settings and transit data.
 *
 * @par Design Pattern:
 * - <b>Strategy Pattern</b>: Enables dependency inversion, allowing test suites to substitute mock crypto engines.
 * - <b>Interface Segregation Principle (ISP)</b>: Exposes only core encryption, decryption, and key management methods.
 */

#pragma once
#include <QString>
#include <QByteArray>
#include "../common/types.h"

namespace NeoNect {
namespace Crypto {

/**
 * @class ICryptoService
 * @brief Cryptographic service interface contract for the client.
 *
 * @details
 * Specifies the pure virtual interface for key derivation, AES-256-GCM authenticated
 * encryption/decryption, binary envelope packaging, and cryptographically secure pseudorandom
 * number generation (CSPRNG).
 */
class ICryptoService {
public:
    /**
     * @brief Virtual destructor ensuring safe polymorphic deletion of concrete crypto services.
     */
    virtual ~ICryptoService() = default;

    /**
     * @brief Injects the session or account master encryption key into the service.
     * @param key 256-bit (32-byte) binary symmetric key.
     */
    virtual void setMasterKey(const QByteArray &key) = 0;

    /**
     * @brief Retrieves the active master encryption key.
     * @return 32-byte binary key.
     * @note Callers should avoid keeping long-lived plaintext copies in unmanaged memory.
     */
    virtual QByteArray getMasterKey() const = 0;

    /**
     * @brief Derives a 256-bit master key from a user passphrase using PBKDF2-HMAC-SHA256.
     * @param passphrase User-provided passphrase.
     * @param salt Optional 16-byte cryptographic salt. If empty, the default application salt is utilized.
     * @return True if key derivation succeeded, false otherwise.
     */
    virtual bool deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt = QByteArray()) = 0;

    /**
     * @brief Encrypts plaintext data using AES-256-GCM authenticated encryption.
     * @param plainData Binary plaintext buffer to encrypt.
     * @param keyOverride Optional override key. If empty, uses the internal master key.
     * @return EncryptedPayload struct containing ciphertext, 12-byte IV/nonce, and 16-byte auth tag.
     */
    virtual EncryptedPayload encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Decrypts ciphertext and verifies integrity using AES-256-GCM.
     * @param cipherWithTag Ciphertext concatenated with the 16-byte authentication tag (or separate).
     * @param nonce 12-byte initialization vector (IV).
     * @param keyOverride Optional override key. If empty, uses the internal master key.
     * @return Decrypted plaintext buffer, or empty byte array if tag verification fails.
     */
    virtual QByteArray decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Decrypts a unified binary envelope (containing nonce + tag + ciphertext).
     * @param envelope Serialized byte stream containing the IV, tag, and ciphertext.
     * @param keyOverride Optional override key.
     * @return Decrypted plaintext, or empty byte array on MAC failure.
     */
    virtual QByteArray decryptAesGcmEnvelope(const QByteArray &envelope, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Generates cryptographically secure random bytes using OpenSSL CSPRNG (`RAND_bytes`).
     * @param count Number of random bytes to generate.
     * @return Byte array containing high-entropy random bytes.
     */
    virtual QByteArray generateRandomBytes(std::size_t count) = 0;
};

} // namespace Crypto
} // namespace NeoNect
