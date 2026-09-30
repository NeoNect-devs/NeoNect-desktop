#pragma once

#include <QByteArray>
#include <utility>
#include <optional>
#include "CryptoTypes.h"

namespace NeoNect {
namespace Crypto {

/**
 * @class ICryptoBackend
 * @brief Abstract interface for low-level cryptographic primitives.
 * @details Provides OpenSSL EVP-backed primitives for randomness, X25519, SHA-256,
 * HMAC, HKDF, and AES-256-GCM. Designed to be a foundation for X3DH and Double Ratchet.
 */
class ICryptoBackend {
public:
    virtual ~ICryptoBackend() = default;

    /**
     * @brief Generates cryptographically secure random bytes.
     * @param size Number of bytes.
     * @return Byte array of specified size. Failure must result in an empty array or throw.
     */
    virtual QByteArray RandomBytes(std::size_t size) = 0;

    /**
     * @brief Generates a new X25519 key pair.
     * @return A pair containing the private and public keys.
     */
    virtual std::pair<X25519PrivateKey, X25519PublicKey> GenerateX25519KeyPair() = 0;

    /**
     * @brief Derives an X25519 shared secret.
     * @param privateKey The local private key.
     * @param peerPublicKey The remote public key.
     * @return 32-byte shared secret in a SecureBuffer. Empty on failure.
     */
    virtual SecureBuffer X25519(const X25519PrivateKey& privateKey, const X25519PublicKey& peerPublicKey) = 0;

    /**
     * @brief Computes SHA-256 digest.
     * @param input Data to hash.
     * @return 32-byte digest. Empty data on failure.
     */
    virtual Sha256Digest Sha256(const QByteArray& input) = 0;

    /**
     * @brief Computes HMAC-SHA256.
     * @param key HMAC key.
     * @param data Data to authenticate.
     * @return 32-byte HMAC.
     */
    virtual QByteArray HmacSha256(const QByteArray& key, const QByteArray& data) = 0;

    /**
     * @brief Derives key material using HKDF-SHA256.
     * @param ikm Input keying material.
     * @param salt Salt value.
     * @param info Context info.
     * @param outputLength Desired output length in bytes.
     * @return Derived key material, or empty array on failure.
     */
    virtual QByteArray HkdfSha256(const QByteArray& ikm, const QByteArray& salt, const QByteArray& info, std::size_t outputLength) = 0;

    /**
     * @brief Encrypts data using AES-256-GCM.
     * @param key 32-byte key.
     * @param nonce 12-byte nonce.
     * @param plaintext Data to encrypt.
     * @param aad Additional authenticated data.
     * @return AeadEncryptResult with ciphertext and 16-byte tag.
     */
    virtual AeadEncryptResult AeadEncrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& plaintext, const QByteArray& aad = QByteArray()) = 0;

    /**
     * @brief Decrypts data using AES-256-GCM.
     * @param key 32-byte key.
     * @param nonce 12-byte nonce.
     * @param ciphertext Data to decrypt.
     * @param tag 16-byte authentication tag.
     * @param aad Additional authenticated data.
     * @return Plaintext if successful, empty array if authentication fails.
     */
    virtual std::optional<QByteArray> AeadDecrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& ciphertext, const AeadTag& tag, const QByteArray& aad = QByteArray()) = 0;

    /**
     * @brief Securely zeroizes memory.
     * @param ptr Pointer to memory.
     * @param size Size in bytes.
     */
    virtual void SecureZero(void* ptr, std::size_t size) = 0;

    /**
     * @brief Constant-time byte comparison.
     * @param a First byte array.
     * @param b Second byte array.
     * @return True if equal, false otherwise.
     */
    virtual bool ConstantTimeCompare(const QByteArray& a, const QByteArray& b) = 0;
};

} // namespace Crypto
} // namespace NeoNect
