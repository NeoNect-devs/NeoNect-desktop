#include "OpenSSLBackend.h"
#include "../common/openssl_raii.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/crypto.h>
#include <optional>

namespace NeoNect {
namespace Crypto {

QByteArray OpenSSLBackend::RandomBytes(std::size_t size) {
    if (size == 0) return QByteArray();
    QByteArray bytes(static_cast<int>(size), 0);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), static_cast<int>(size)) != 1) {
        return QByteArray();
    }
    return bytes;
}

std::pair<X25519PrivateKey, X25519PublicKey> OpenSSLBackend::GenerateX25519KeyPair() {
    EvpPkeyCtxPtr pctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    if (!pctx) return std::make_pair(X25519PrivateKey(), X25519PublicKey());
    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) {
        return std::make_pair(X25519PrivateKey(), X25519PublicKey());
    }
    
    EVP_PKEY* rawPkey = nullptr;
    if (EVP_PKEY_keygen(pctx.get(), &rawPkey) <= 0) {
        return std::make_pair(X25519PrivateKey(), X25519PublicKey());
    }
    EvpPkeyPtr pkey(rawPkey);

    std::size_t privLen = 32;
    std::size_t pubLen = 32;
    X25519PrivateKey privKey;
    privKey.data.resize(32);
    X25519PublicKey pubKey;
    pubKey.data.resize(32);

    if (EVP_PKEY_get_raw_private_key(pkey.get(), privKey.data.data(), &privLen) != 1 || privLen != 32) {
        return std::make_pair(X25519PrivateKey(), X25519PublicKey());
    }
    if (EVP_PKEY_get_raw_public_key(pkey.get(), reinterpret_cast<unsigned char*>(pubKey.data.data()), &pubLen) != 1 || pubLen != 32) {
        return std::make_pair(X25519PrivateKey(), X25519PublicKey());
    }

    return {std::move(privKey), std::move(pubKey)};
}

SecureBuffer OpenSSLBackend::X25519(const X25519PrivateKey& privateKey, const X25519PublicKey& peerPublicKey) {
    if (privateKey.data.size() != 32 || peerPublicKey.data.size() != 32) return SecureBuffer();

    EvpPkeyPtr priv(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, privateKey.data.data(), 32));
    if (!priv) return SecureBuffer();
    
    EvpPkeyPtr pub(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, reinterpret_cast<const unsigned char*>(peerPublicKey.data.constData()), 32));
    if (!pub) {
        return SecureBuffer();
    }

    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(priv.get(), nullptr));
    if (!ctx) {
        return SecureBuffer();
    }

    if (EVP_PKEY_derive_init(ctx.get()) <= 0 || EVP_PKEY_derive_set_peer(ctx.get(), pub.get()) <= 0) {
        return SecureBuffer();
    }

    std::size_t secretLen = 0;
    if (EVP_PKEY_derive(ctx.get(), nullptr, &secretLen) <= 0 || secretLen != 32) {
        return SecureBuffer();
    }

    SecureBuffer secret(32);
    if (EVP_PKEY_derive(ctx.get(), secret.data(), &secretLen) <= 0) {
        return SecureBuffer();
    }

    return secret;
}

Sha256Digest OpenSSLBackend::Sha256(const QByteArray& input) {
    Sha256Digest digest;
    digest.data.resize(32);
    unsigned int len = 0;
    
    EvpMdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) return {};
    
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), input.constData(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(digest.data.data()), &len) != 1 ||
        len != 32) {
        return {};
    }
    
    return digest;
}

QByteArray OpenSSLBackend::HmacSha256(const QByteArray& key, const QByteArray& data) {
    EvpMacPtr mac(EVP_MAC_fetch(nullptr, "HMAC", nullptr));
    if (!mac) return QByteArray();
    
    EvpMacCtxPtr ctx(EVP_MAC_CTX_new(mac.get()));
    if (!ctx) {
        return QByteArray();
    }
    
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[1] = OSSL_PARAM_construct_end();
    
    if (EVP_MAC_init(ctx.get(), reinterpret_cast<const unsigned char*>(key.constData()), key.size(), params) != 1) {
        return QByteArray();
    }
    
    if (EVP_MAC_update(ctx.get(), reinterpret_cast<const unsigned char*>(data.constData()), data.size()) != 1) {
        return QByteArray();
    }
    
    QByteArray result(32, 0);
    std::size_t outLen = 0;
    if (EVP_MAC_final(ctx.get(), reinterpret_cast<unsigned char*>(result.data()), &outLen, 32) != 1 || outLen != 32) {
        return QByteArray();
    }
    
    return result;
}

QByteArray OpenSSLBackend::HkdfSha256(const QByteArray& ikm, const QByteArray& salt, const QByteArray& info, std::size_t outputLength) {
    if (outputLength == 0 || outputLength > 255 * 32) return QByteArray();

    EvpKdfPtr kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr));
    if (!kdf) return QByteArray();

    EvpKdfCtxPtr kctx(EVP_KDF_CTX_new(kdf.get()));
    if (!kctx) return QByteArray();

    OSSL_PARAM params[5];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", const_cast<char*>("SHA256"), 0);
    params[1] = OSSL_PARAM_construct_octet_string("key", const_cast<char*>(ikm.constData()), ikm.size());
    params[2] = OSSL_PARAM_construct_octet_string("info", const_cast<char*>(info.constData()), info.size());
    params[3] = OSSL_PARAM_construct_octet_string("salt", const_cast<char*>(salt.constData()), salt.size());
    params[4] = OSSL_PARAM_construct_end();

    QByteArray out(static_cast<int>(outputLength), 0);
    if (EVP_KDF_derive(kctx.get(), reinterpret_cast<unsigned char*>(out.data()), outputLength, params) <= 0) {
        return QByteArray();
    }

    return out;
}

AeadEncryptResult OpenSSLBackend::AeadEncrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& plaintext, const QByteArray& aad) {
    AeadEncryptResult result;
    if (key.data.size() != 32 || nonce.data.size() != 12) return result;

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return result;

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return result;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
        return result;
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data.data(), reinterpret_cast<const unsigned char*>(nonce.data.constData())) != 1) {
        return result;
    }

    int outLen = 0;
    if (!aad.isEmpty()) {
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &outLen, reinterpret_cast<const unsigned char*>(aad.constData()), aad.size()) != 1) {
            return result;
        }
    }

    QByteArray ciphertext;
    if (!plaintext.isEmpty()) {
        ciphertext.resize(plaintext.size());
        if (EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(ciphertext.data()), &outLen, reinterpret_cast<const unsigned char*>(plaintext.constData()), plaintext.size()) != 1) {
            return result;
        }
    }
    int totalLen = outLen;

    if (EVP_EncryptFinal_ex(ctx.get(), plaintext.isEmpty() ? nullptr : (reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen), &outLen) != 1) {
        return result;
    }
    totalLen += outLen;
    if (!plaintext.isEmpty()) ciphertext.resize(totalLen);

    result.tag.data.resize(16);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, result.tag.data.data()) != 1) {
        return result;
    }

    result.ciphertext = ciphertext;
    result.success = true;
    return result;
}

std::optional<QByteArray> OpenSSLBackend::AeadDecrypt(const AeadKey& key, const AeadNonce& nonce, const QByteArray& ciphertext, const AeadTag& tag, const QByteArray& aad) {
    if (key.data.size() != 32 || nonce.data.size() != 12 || tag.data.size() != 16) return std::nullopt;

    EvpCipherCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return std::nullopt;

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return std::nullopt;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) {
        return std::nullopt;
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data.data(), reinterpret_cast<const unsigned char*>(nonce.data.constData())) != 1) {
        return std::nullopt;
    }

    int outLen = 0;
    if (!aad.isEmpty()) {
        if (EVP_DecryptUpdate(ctx.get(), nullptr, &outLen, reinterpret_cast<const unsigned char*>(aad.constData()), aad.size()) != 1) {
            return std::nullopt;
        }
    }

    QByteArray plaintext;
    if (!ciphertext.isEmpty()) {
        plaintext.resize(ciphertext.size());
        if (EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char*>(plaintext.data()), &outLen, reinterpret_cast<const unsigned char*>(ciphertext.constData()), ciphertext.size()) != 1) {
            return std::nullopt;
        }
    }
    int totalLen = outLen;

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16, const_cast<char*>(tag.data.constData())) != 1) {
        return std::nullopt;
    }

    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.isEmpty() ? nullptr : (reinterpret_cast<unsigned char*>(plaintext.data()) + outLen), &outLen) <= 0) {
        return std::nullopt;
    }
    totalLen += outLen;
    if (!ciphertext.isEmpty()) plaintext.resize(totalLen);

    return plaintext;
}

void OpenSSLBackend::SecureZero(void* ptr, std::size_t size) {
    if (ptr && size > 0) {
        OPENSSL_cleanse(ptr, size);
    }
}

bool OpenSSLBackend::ConstantTimeCompare(const QByteArray& a, const QByteArray& b) {
    if (a.size() != b.size()) return false;
    if (a.isEmpty()) return true;
    return CRYPTO_memcmp(a.constData(), b.constData(), a.size()) == 0;
}

} // namespace Crypto
} // namespace NeoNect
