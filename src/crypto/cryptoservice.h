/**
 * @file cryptoservice.h
 * @brief OpenSSL-based concrete implementation of ICryptoService.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Implements AES-256-GCM authenticated encryption/decryption, PBKDF2 key derivation,
 * and CSPRNG using OpenSSL EVP APIs. Employs `std::shared_mutex` for thread-safe
 * concurrent key access and `SecureBuffer` for automated memory zeroization (scrubbing).
 *
 * @par Design Patterns:
 * - <b>Concrete Strategy</b>: Implements the `ICryptoService` contract using OpenSSL.
 * - <b>Monitor Object / Reader-Writer Lock</b>: Uses `std::shared_mutex` to allow concurrent readers while serializing key updates.
 * - <b>RAII Secure Erasure</b>: Uses `SecureBuffer` (`OPENSSL_cleanse`) to prevent key leakage in memory dumps.
 */

#pragma once
#include "icryptoservice.h"
#include "../common/openssl_raii.h"
#include <shared_mutex>

namespace NeoNect {
namespace Crypto {

/**
 * @class CryptoService
 * @brief Thread-safe concrete cryptographic engine backed by OpenSSL 3.x EVP.
 *
 * @details
 * Thread safety: Reads from master key (`getMasterKey`, `encryptAesGcm`, `decryptAesGcm`)
 * acquire a shared lock (`std::shared_lock`), while key mutations (`setMasterKey`,
 * `deriveKeyFromPassphrase`) acquire an exclusive lock (`std::unique_lock`).
 */
class CryptoService : public ICryptoService {
public:
    /**
     * @brief Constructs the cryptographic service and initializes OpenSSL contexts.
     */
    CryptoService();

    /**
     * @brief Default virtual destructor. Zeroizes internal buffers via SecureBuffer RAII.
     */
    ~CryptoService() override = default;

    /**
     * @brief Stores the master encryption key securely using exclusive lock.
     * @param key 32-byte master key.
     *
     * @pre `key.size() == 32`
     * @post Acquires exclusive lock (`std::unique_lock<std::shared_mutex>`). Replaces @ref m_masterKey and zeroizes previous key.
     * @par Thread-Safety Constraints:
     * - Blocks all concurrent readers and writers until the key update finishes.
     */
    void setMasterKey(const QByteArray &key) override;

    /**
     * @brief Retrieves a copy of the current master key under shared lock.
     * @return 32-byte key copy.
     *
     * @post Acquires non-blocking shared lock (`std::shared_lock<std::shared_mutex>`).
     * @par Concurrency Guarantee:
     * - Safe for simultaneous invocation by multiple concurrent reader threads.
     */
    QByteArray getMasterKey() const override;

    /**
     * @brief Derives a 256-bit AES master key from passphrase and salt using PBKDF2-HMAC-SHA256.
     * @param passphrase Password or secret passphrase string.
     * @param salt Optional cryptographic salt (defaults to application salt if empty).
     * @return True if derivation succeeded, false otherwise.
     *
     * @pre `!passphrase.isEmpty()`
     * @post On success, acquires exclusive lock and securely assigns derived 32-byte key.
     * @par Performance Constraint:
     * - PBKDF2 executes 100,000 iterations of HMAC-SHA256; may take 10-30ms of CPU time.
     */
    bool deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt = QByteArray()) override;

    /**
     * @brief Encrypts data using AES-256-GCM with a newly generated random 12-byte IV.
     * @param plainData Plaintext buffer to encrypt.
     * @param keyOverride Optional custom key to use instead of master key.
     * @return EncryptedPayload with ciphertext, IV, and 16-byte authentication tag.
     *
     * @pre If keyOverride is empty, master key must be set.
     * @post Employs RAII wrapper `EvpCipherCtxPtr` guaranteeing context cleanup on exception or return.
     * @par Concurrency & Performance Constraints:
     * - Acquires shared lock during cipher execution if reading master key.
     * - Pure function: no mutable state modified.
     */
    EncryptedPayload encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride = QByteArray()) override;

    /**
     * @brief Decrypts ciphertext and verifies GCM authentication tag.
     * @param cipherWithTag Encrypted payload data.
     * @param nonce 12-byte initialization vector.
     * @param keyOverride Optional custom key to use instead of master key.
     * @return Decrypted plaintext, or empty byte array if tag verification fails.
     *
     * @pre `nonce.size() == 12`
     * @par Failure Mode:
     * - Returns empty byte array if MAC verification fails.
     */
    QByteArray decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride = QByteArray()) override;

    /**
     * @brief Unpacks and decrypts a binary envelope containing nonce, tag, and ciphertext.
     * @param envelope Serialized byte stream (IV + Tag + Ciphertext).
     * @param keyOverride Optional custom key to use instead of master key.
     * @return Decrypted plaintext, or empty byte array on corruption/authentication failure.
     *
     * @pre `envelope.size() >= 28`
     */
    QByteArray decryptAesGcmEnvelope(const QByteArray &envelope, const QByteArray &keyOverride = QByteArray()) override;

    /**
     * @brief Generates cryptographically strong random bytes via `RAND_bytes`.
     * @param count Desired byte count.
     * @return Byte array containing high-entropy random bytes.
     *
     * @pre `count > 0`
     */
    QByteArray generateRandomBytes(std::size_t count) override;

private:
    /**
     * @brief Mutex synchronizing concurrent reads and writes to @ref m_masterKey.
     */
    mutable std::shared_mutex m_keyMutex;

    /**
     * @brief RAII buffer storing the active 256-bit symmetric master key in zeroizable memory.
     */
    SecureBuffer m_masterKey;
};

} // namespace Crypto
} // namespace NeoNect
