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
 *
 * @par Security & Cryptographic Invariants:
 * - <b>Cipher Specification</b>: AES-256 in Galois/Counter Mode (GCM), standard NIST SP 800-38D.
 * - <b>Symmetric Key Size</b>: Strictly 256 bits (32 bytes).
 * - <b>Initialization Vector (IV / Nonce) Size</b>: Strictly 96 bits (12 bytes), generated via CSPRNG.
 * - <b>Authentication Tag Size</b>: Strictly 128 bits (16 bytes), constant-time MAC verification.
 * - <b>Nonce Uniqueness Invariant</b>: Under no circumstances may an IV be reused with the same key.
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
     *
     * @pre `key.size() == 32` (Must be exactly 256 bits).
     * @post The key is copied into secure, zeroizable internal memory.
     * @par Cryptographic Constraints:
     * - Key material MUST possess at least 256 bits of cryptographic entropy.
     * - Callers are responsible for securely wiping (`OPENSSL_cleanse`) temporary source buffers after injection.
     */
    virtual void setMasterKey(const QByteArray &key) = 0;

    /**
     * @brief Retrieves the active master encryption key.
     * @return 32-byte binary key.
     *
     * @post Returns a valid 32-byte buffer if master key was set, or empty byte array if uninitialized.
     * @par Security Constraints:
     * - Plaintext key exposure in user-space RAM should be minimized.
     * - Callers must store the returned key in zeroizable structures (`SecureBuffer`) to avoid residual leaks in crash dumps or swap.
     */
    virtual QByteArray getMasterKey() const = 0;

    /**
     * @brief Derives a 256-bit master key from a user passphrase using PBKDF2-HMAC-SHA256.
     * @param passphrase User-provided passphrase.
     * @param salt Optional 16-byte cryptographic salt. If empty, the default application salt is utilized.
     * @return True if key derivation succeeded, false otherwise.
     *
     * @pre `!passphrase.isEmpty()` with recommended minimum entropy of 60 bits (e.g. >= 10 characters).
     * @post Upon success, @ref m_masterKey is populated with a 32-byte key derived via PBKDF2.
     * @par Computational & Security Constraints:
     * - Derivation iteration count is strictly pinned to `Constants::PBKDF2_ITERATIONS` (100,000 rounds) to resist GPU/ASIC brute-force dictionary attacks.
     * - Salt must be at least 16 bytes (128 bits) to eliminate rainbow-table precomputation attacks.
     */
    virtual bool deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt = QByteArray()) = 0;

    /**
     * @brief Encrypts plaintext data using AES-256-GCM authenticated encryption.
     * @param plainData Binary plaintext buffer to encrypt.
     * @param keyOverride Optional override key. If empty, uses the internal master key.
     * @return EncryptedPayload struct containing ciphertext, 12-byte IV/nonce, and 16-byte auth tag.
     *
     * @pre Either `keyOverride.size() == 32` OR the service has a pre-configured master key of size 32.
     * @pre `plainData.size() <= 104857600` (Max payload constraint of 100 MB per encryption unit).
     * @post Generated IV is guaranteed 100% unique per call (fresh 12-byte CSPRNG).
     * @post Authentication tag is strictly 16 bytes validating payload integrity and authenticity.
     * @par Cryptographic Invariants & Constraints:
     * - <b>Zero Nonce-Reuse Guarantee</b>: Every invocation samples fresh 96-bit randomness from `generateRandomBytes(12)`. Nonce reuse under AES-GCM allows complete algebraic recovery of the GHASH authentication key!
     * - <b>Confidentiality & Authenticity</b>: Provides IND-CCA2 security guarantee against active ciphertext tampering.
     */
    virtual EncryptedPayload encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Decrypts ciphertext and verifies integrity using AES-256-GCM.
     * @param cipherWithTag Ciphertext concatenated with the 16-byte authentication tag (or separate).
     * @param nonce 12-byte initialization vector (IV).
     * @param keyOverride Optional override key. If empty, uses the internal master key.
     * @return Decrypted plaintext buffer, or empty byte array if tag verification fails.
     *
     * @pre `nonce.size() == 12` (Strict GCM standard requirement).
     * @pre Either `keyOverride.size() == 32` OR internal master key is 32 bytes.
     * @pre `cipherWithTag.size() >= 16` (Must contain at least the 16-byte authentication tag).
     * @post Constant-time MAC comparison prevents timing side-channel attacks.
     * @par Failure Constraints:
     * - If even 1 bit of the ciphertext, tag, or nonce was altered in transit, decryption returns empty `QByteArray()`.
     * - Partial or unauthenticated plaintext is NEVER exposed to callers (strictly all-or-nothing plaintext release).
     */
    virtual QByteArray decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Decrypts a unified binary envelope (containing nonce + tag + ciphertext).
     * @param envelope Serialized byte stream containing the IV, tag, and ciphertext.
     * @param keyOverride Optional override key.
     * @return Decrypted plaintext, or empty byte array on MAC failure.
     *
     * @pre `envelope.size() >= 28` (12-byte IV + 16-byte Tag + 0-byte minimum ciphertext).
     * @post Returns verified plaintext on MAC success, or empty array on corruption.
     * @par Binary Layout Constraint:
     * - `[Bytes 0..11]`: 12-byte Initialization Vector (IV).
     * - `[Bytes 12..27]`: 16-byte Authentication Tag.
     * - `[Bytes 28..N]`: AES-GCM Ciphertext.
     */
    virtual QByteArray decryptAesGcmEnvelope(const QByteArray &envelope, const QByteArray &keyOverride = QByteArray()) = 0;

    /**
     * @brief Generates cryptographically secure random bytes using OpenSSL CSPRNG (`RAND_bytes`).
     * @param count Number of random bytes to generate.
     * @return Byte array containing high-entropy random bytes.
     *
     * @pre `count > 0` and `count <= 1048576` (1 MB max batch allocation).
     * @post Returns exactly `count` bytes of cryptographically certified pseudorandom data.
     * @par Security Constraints:
     * - Backed directly by OpenSSL's internal CSPRNG seeded by hardware entropy (`RDRAND` / OS kernel `/dev/urandom`).
     * - Throws or fails safely if kernel entropy pool exhaustion occurs.
     */
    virtual QByteArray generateRandomBytes(std::size_t count) = 0;
};

} // namespace Crypto
} // namespace NeoNect
