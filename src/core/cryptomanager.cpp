// src/core/cryptomanager.cpp
#include "cryptomanager.h"
#include <QtConcurrent>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <QUuid>
#include <QSettings>

CryptoManager* CryptoManager::instance() {
    static CryptoManager _instance;
    return &_instance;
}

CryptoManager::CryptoManager(QObject *parent) : QObject(parent) {
    m_masterSymKey.resize(32); // 256-bit Key Allocation

    QSettings settings("Avila", "DesktopClient");
    m_deviceId = settings.value("device_id").toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = "avila-dev-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue("device_id", m_deviceId);
    }

    m_publicKey = settings.value("public_key").toString();
    if (m_publicKey.isEmpty()) {
        QByteArray randomKey(32, 0);
        RAND_bytes(reinterpret_cast<unsigned char*>(randomKey.data()), 32);
        m_publicKey = randomKey.toBase64();
        settings.setValue("public_key", m_publicKey);
    }
}

QString CryptoManager::getDeviceId() {
    return m_deviceId;
}

QString CryptoManager::getDevicePublicKey() {
    return m_publicKey;
}


void CryptoManager::initializeKeyFromPassphrase(const QString &passphrase) {
    QtConcurrent::run([this, passphrase]() {
        QByteArray salt = "AVILA_STATIC_NETWORK_SALT_VAULT";
        // Native OpenSSL hardware key derivation processing block
        PKCS5_PBKDF2_HMAC(passphrase.toUtf8().constData(), passphrase.length(),
                          reinterpret_cast<const unsigned char*>(salt.constData()), salt.length(),
                          10000, EVP_sha256(), 32, 
                          reinterpret_cast<unsigned char*>(m_masterSymKey.data()));
    });
}

void CryptoManager::encryptMessageAsposing(const QString &channelId, const QString &plainText) {
    QByteArray plainData = plainText.toUtf8();
    QByteArray localKey = m_masterSymKey;

    QtConcurrent::run([this, channelId, plainData, localKey]() {
        unsigned char nonce[12];
        RAND_bytes(nonce, sizeof(nonce));

        QByteArray ciphertext;
        ciphertext.resize(plainData.size());
        int len = 0, ciphertext_len = 0;

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, sizeof(nonce), nullptr);
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, 
                           reinterpret_cast<const unsigned char*>(localKey.constData()), nonce);

        EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                          reinterpret_cast<const unsigned char*>(plainData.constData()), plainData.size());
        ciphertext_len = len;

        unsigned char tag[16];
        EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len);
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, sizeof(tag), tag);
        EVP_CIPHER_CTX_free(ctx);

        // Package ciphertext + authentication tag sequentially
        ciphertext.append(reinterpret_cast<const char*>(tag), sizeof(tag));

        emit encryptionCompleted(channelId, ciphertext.toBase64(), QByteArray(reinterpret_cast<const char*>(nonce), sizeof(nonce)).toBase64());
    });
}