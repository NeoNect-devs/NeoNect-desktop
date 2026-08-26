// src/crypto/cryptoservice.cpp
#include "cryptoservice.h"
#include "../common/constants.h"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <mutex>

namespace Avila {
namespace Crypto {

CryptoService::CryptoService() {
    m_masterKey.resize(Constants::AES_256_KEY_SIZE);
}

void CryptoService::setMasterKey(const QByteArray &key) {
    std::unique_lock<std::shared_mutex> lock(m_keyMutex);
    m_masterKey.resize(Constants::AES_256_KEY_SIZE);
    std::size_t copyLen = std::min(static_cast<std::size_t>(key.size()), Constants::AES_256_KEY_SIZE);
    if (copyLen > 0) {
        std::memcpy(m_masterKey.data(), key.constData(), copyLen);
    }
}

QByteArray CryptoService::getMasterKey() const {
    std::shared_lock<std::shared_mutex> lock(m_keyMutex);
    return QByteArray(reinterpret_cast<const char*>(m_masterKey.data()), static_cast<int>(m_masterKey.size()));
}

bool CryptoService::deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt) {
    QByteArray saltBytes = salt.isEmpty() ? QByteArray(Constants::STATIC_SALT_VAULT) : salt;
    QByteArray passBytes = passphrase.toUtf8();

    SecureBuffer derivedKey(Constants::AES_256_KEY_SIZE);

    int result = PKCS5_PBKDF2_HMAC(
        passBytes.constData(),
        static_cast<int>(passBytes.length()),
        reinterpret_cast<const unsigned char*>(saltBytes.constData()),
        static_cast<int>(saltBytes.length()),
        Constants::PBKDF2_ITERATIONS,
        EVP_sha256(),
        static_cast<int>(Constants::AES_256_KEY_SIZE),
        derivedKey.data()
    );

    if (result != 1) {
        return false;
    }

    {
        std::unique_lock<std::shared_mutex> lock(m_keyMutex);
        m_masterKey = std::move(derivedKey);
    }
    return true;
}

QByteArray CryptoService::generateRandomBytes(std::size_t count) {
    QByteArray bytes(static_cast<int>(count), 0);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(bytes.data()), static_cast<int>(count)) != 1) {
        return QByteArray();
    }
    return bytes;
}

EncryptedPayload CryptoService::encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride) {
    EncryptedPayload result;
    if (plainData.isEmpty()) {
        result.errorMessage = "Plaintext cannot be empty";
        return result;
    }

    // Determine encryption key
    SecureBuffer activeKey(Constants::AES_256_KEY_SIZE);
    if (!keyOverride.isEmpty()) {
        std::size_t copyLen = std::min(static_cast<std::size_t>(keyOverride.size()), Constants::AES_256_KEY_SIZE);
        std::memcpy(activeKey.data(), keyOverride.constData(), copyLen);
    } else {
        std::shared_lock<std::shared_mutex> lock(m_keyMutex);
        activeKey = m_masterKey;
    }

    // Generate random 12-byte IV/nonce
    QByteArray nonce = generateRandomBytes(Constants::AES_GCM_IV_SIZE);
    if (nonce.size() != static_cast<int>(Constants::AES_GCM_IV_SIZE)) {
        result.errorMessage = "Failed to generate cryptographically secure nonce";
        return result;
    }

    EvpCipherCtxPtr ctx = makeEvpCipherCtx();
    if (!ctx) {
        result.errorMessage = "Failed to allocate OpenSSL cipher context";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        result.errorMessage = "Failed to initialize AES-256-GCM cipher";
        return result;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(Constants::AES_GCM_IV_SIZE), nullptr) != 1) {
        result.errorMessage = "Failed to set GCM IV length";
        return result;
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, activeKey.data(), reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
        result.errorMessage = "Failed to set key and IV in cipher context";
        return result;
    }

    QByteArray ciphertext;
    ciphertext.resize(plainData.size());
    int outLen = 0;

    if (EVP_EncryptUpdate(ctx.get(),
                          reinterpret_cast<unsigned char*>(ciphertext.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char*>(plainData.constData()),
                          plainData.size()) != 1) {
        result.errorMessage = "Encryption update failed";
        return result;
    }

    int ciphertextLen = outLen;

    if (EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(ciphertext.data()) + outLen, &outLen) != 1) {
        result.errorMessage = "Encryption finalization failed";
        return result;
    }
    ciphertextLen += outLen;
    ciphertext.resize(ciphertextLen);

    unsigned char tag[Constants::AES_GCM_TAG_SIZE];
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(Constants::AES_GCM_TAG_SIZE), tag) != 1) {
        result.errorMessage = "Failed to extract GCM authentication tag";
        return result;
    }

    // Append 16-byte authentication tag to ciphertext
    ciphertext.append(reinterpret_cast<const char*>(tag), static_cast<int>(Constants::AES_GCM_TAG_SIZE));

    result.cipherWithTag = ciphertext;
    result.nonce = nonce;
    result.success = true;
    return result;
}

QByteArray CryptoService::decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride) {
    if (cipherWithTag.size() < static_cast<int>(Constants::AES_GCM_TAG_SIZE) || nonce.size() != static_cast<int>(Constants::AES_GCM_IV_SIZE)) {
        return QByteArray();
    }

    SecureBuffer activeKey(Constants::AES_256_KEY_SIZE);
    if (!keyOverride.isEmpty()) {
        std::size_t copyLen = std::min(static_cast<std::size_t>(keyOverride.size()), Constants::AES_256_KEY_SIZE);
        std::memcpy(activeKey.data(), keyOverride.constData(), copyLen);
    } else {
        std::shared_lock<std::shared_mutex> lock(m_keyMutex);
        activeKey = m_masterKey;
    }

    int actualCipherLen = cipherWithTag.size() - static_cast<int>(Constants::AES_GCM_TAG_SIZE);
    const unsigned char *tagPtr = reinterpret_cast<const unsigned char*>(cipherWithTag.constData()) + actualCipherLen;

    EvpCipherCtxPtr ctx = makeEvpCipherCtx();
    if (!ctx) {
        return QByteArray();
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        return QByteArray();
    }

    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(Constants::AES_GCM_IV_SIZE), nullptr) != 1) {
        return QByteArray();
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, activeKey.data(), reinterpret_cast<const unsigned char*>(nonce.constData())) != 1) {
        return QByteArray();
    }

    QByteArray plaintext;
    plaintext.resize(actualCipherLen);
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx.get(),
                          reinterpret_cast<unsigned char*>(plaintext.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char*>(cipherWithTag.constData()),
                          actualCipherLen) != 1) {
        return QByteArray();
    }

    int plainLen = outLen;

    // Set expected tag value for verification
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(Constants::AES_GCM_TAG_SIZE), const_cast<unsigned char*>(tagPtr)) != 1) {
        return QByteArray();
    }

    // EVP_DecryptFinal_ex verifies the tag
    if (EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(plaintext.data()) + outLen, &outLen) <= 0) {
        // Tag verification failed - message altered or wrong key
        return QByteArray();
    }

    plainLen += outLen;
    plaintext.resize(plainLen);
    return plaintext;
}

} // namespace Crypto
} // namespace Avila
