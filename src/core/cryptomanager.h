// src/core/cryptomanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>
#include "../crypto/icryptoservice.h"
#include "../storage/isettingsrepository.h"

class CryptoManager : public QObject {
    Q_OBJECT
public:
    static CryptoManager* instance();
    explicit CryptoManager(std::shared_ptr<Avila::Crypto::ICryptoService> cryptoService = nullptr,
                           std::shared_ptr<Avila::Storage::ISettingsRepository> settingsRepo = nullptr,
                           QObject *parent = nullptr);
    ~CryptoManager() override = default;

    std::shared_ptr<Avila::Crypto::ICryptoService> service() const { return m_cryptoService; }

    Q_INVOKABLE void setProfile(const QString &profileName);
    Q_INVOKABLE QString getDeviceId();
    Q_INVOKABLE QString getDevicePublicKey();
    Q_INVOKABLE void initializeKeyFromPassphrase(const QString &passphrase);
    Q_INVOKABLE void encryptMessageAsposing(const QString &channelId, const QString &plainText);

signals:
    void encryptionCompleted(const QString &channelId, const QString &cipherBase64, const QString &nonceBase64);
    void decryptionCompleted(const QString &messageId, const QString &plainText);

private:
    void ensureDeviceCredentials();

    std::shared_ptr<Avila::Crypto::ICryptoService> m_cryptoService;
    std::shared_ptr<Avila::Storage::ISettingsRepository> m_settingsRepo;
};