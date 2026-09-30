#pragma once

#include "ICryptoBackend.h"
#include <optional>

namespace NeoNect {
namespace Crypto {

class OpenSSLBackend : public ICryptoBackend {
public:
    OpenSSLBackend() = default;
    ~OpenSSLBackend() override = default;

    QByteArray RandomBytes(std::size_t size) override;
    std::pair<X25519PrivateKey, X25519PublicKey> GenerateX25519KeyPair() override;
    SecureBuffer X25519(const X25519PrivateKey& privateKey, const X25519PublicKey& peerPublicKey) override;
    Sha256Digest Sha256(const QByteArray& input) override;
    QByteArray HmacSha256(const QByteArray& key, const QByteArray& data) override;
    QByteArray HkdfSha256(const QByteArray& ikm, const QByteArray& salt, const QByteArray& info, std::size_t outputLength) override;
    AeadEncryptResult AeadEncrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& plaintext, const QByteArray& aad = QByteArray()) override;
    std::optional<QByteArray> AeadDecrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& ciphertext, const AeadTag& tag, const QByteArray& aad = QByteArray()) override;
    void SecureZero(void* ptr, std::size_t size) override;
    bool ConstantTimeCompare(const QByteArray& a, const QByteArray& b) override;
};

} // namespace Crypto
} // namespace NeoNect
