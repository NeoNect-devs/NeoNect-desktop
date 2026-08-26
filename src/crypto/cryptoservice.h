// src/crypto/cryptoservice.h
#pragma once
#include "icryptoservice.h"
#include "../common/openssl_raii.h"
#include <shared_mutex>

namespace Avila {
namespace Crypto {

class CryptoService : public ICryptoService {
public:
    CryptoService();
    ~CryptoService() override = default;

    void setMasterKey(const QByteArray &key) override;
    QByteArray getMasterKey() const override;
    bool deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt = QByteArray()) override;

    EncryptedPayload encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride = QByteArray()) override;
    QByteArray decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride = QByteArray()) override;

    QByteArray generateRandomBytes(std::size_t count) override;

private:
    mutable std::shared_mutex m_keyMutex;
    SecureBuffer m_masterKey;
};

} // namespace Crypto
} // namespace Avila
