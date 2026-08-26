// src/crypto/icryptoservice.h
#pragma once
#include <QString>
#include <QByteArray>
#include "../common/types.h"

namespace Avila {
namespace Crypto {

class ICryptoService {
public:
    virtual ~ICryptoService() = default;

    virtual void setMasterKey(const QByteArray &key) = 0;
    virtual QByteArray getMasterKey() const = 0;
    virtual bool deriveKeyFromPassphrase(const QString &passphrase, const QByteArray &salt = QByteArray()) = 0;

    virtual EncryptedPayload encryptAesGcm(const QByteArray &plainData, const QByteArray &keyOverride = QByteArray()) = 0;
    virtual QByteArray decryptAesGcm(const QByteArray &cipherWithTag, const QByteArray &nonce, const QByteArray &keyOverride = QByteArray()) = 0;

    virtual QByteArray generateRandomBytes(std::size_t count) = 0;
};

} // namespace Crypto
} // namespace Avila
