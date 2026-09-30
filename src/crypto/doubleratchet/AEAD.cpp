#include "AEAD.h"

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

AEAD::AEAD(ICryptoBackend* backend) : backend_(backend) {}

AeadEncryptResult AEAD::encrypt(
    ByteView messageKey,
    ByteView plaintext,
    ByteView associatedData)
{
    AeadEncryptResult result;
    result.success = false;

    if (messageKey.size != 32 || messageKey.data == nullptr) {
        return result;
    }

    QByteArray ikm(reinterpret_cast<const char*>(messageKey.data), messageKey.size);
    QByteArray salt(32, 0); // 32 zero bytes
    QByteArray info("NeoNectAEADv1");

    QByteArray okm = backend_->HkdfSha256(ikm, salt, info, 44);
    if (okm.size() != 44) {
        return result;
    }

    AeadKey key;
    key.data.resize(32);
    std::copy(okm.constData(), okm.constData() + 32, key.data.data());

    AeadNonce nonce;
    nonce.data.resize(12);
    std::copy(okm.constData() + 32, okm.constData() + 44, nonce.data.data());

    QByteArray pt;
    if (plaintext.data && plaintext.size > 0) {
        pt = QByteArray(reinterpret_cast<const char*>(plaintext.data), plaintext.size);
    }
    
    QByteArray aad;
    if (associatedData.data && associatedData.size > 0) {
        aad = QByteArray(reinterpret_cast<const char*>(associatedData.data), associatedData.size);
    }

    result = backend_->AeadEncrypt(key, nonce, pt, aad);

    backend_->SecureZero(okm.data(), okm.size());

    return result;
}

std::optional<QByteArray> AEAD::decrypt(
    ByteView messageKey,
    ByteView ciphertext,
    ByteView tag,
    ByteView associatedData)
{
    if (messageKey.size != 32 || messageKey.data == nullptr) {
        return std::nullopt;
    }
    
    if (tag.size != 16 || tag.data == nullptr) {
        return std::nullopt;
    }

    QByteArray ikm(reinterpret_cast<const char*>(messageKey.data), messageKey.size);
    QByteArray salt(32, 0); // 32 zero bytes
    QByteArray info("NeoNectAEADv1");

    QByteArray okm = backend_->HkdfSha256(ikm, salt, info, 44);
    if (okm.size() != 44) {
        return std::nullopt;
    }

    AeadKey key;
    key.data.resize(32);
    std::copy(okm.constData(), okm.constData() + 32, key.data.data());

    AeadNonce nonce;
    nonce.data.resize(12);
    std::copy(okm.constData() + 32, okm.constData() + 44, nonce.data.data());

    QByteArray ct;
    if (ciphertext.data && ciphertext.size > 0) {
        ct = QByteArray(reinterpret_cast<const char*>(ciphertext.data), ciphertext.size);
    }

    AeadTag aeadTag;
    aeadTag.data = QByteArray(reinterpret_cast<const char*>(tag.data), tag.size);

    QByteArray aad;
    if (associatedData.data && associatedData.size > 0) {
        aad = QByteArray(reinterpret_cast<const char*>(associatedData.data), associatedData.size);
    }

    auto result = backend_->AeadDecrypt(key, nonce, ct, aeadTag, aad);

    backend_->SecureZero(okm.data(), okm.size());

    return result;
}

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
