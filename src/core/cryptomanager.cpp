// src/core/cryptomanager.cpp
#include "cryptomanager.h"
#include "../crypto/cryptoservice.h"
#include "../storage/settingsrepository.h"
#include <QtConcurrent>
#include <QUuid>

CryptoManager* CryptoManager::instance() {
    static CryptoManager _instance;
    return &_instance;
}

CryptoManager::CryptoManager(std::shared_ptr<Avila::Crypto::ICryptoService> cryptoService,
                             std::shared_ptr<Avila::Storage::ISettingsRepository> settingsRepo,
                             QObject *parent)
    : QObject(parent),
      m_cryptoService(cryptoService ? cryptoService : std::make_shared<Avila::Crypto::CryptoService>()),
      m_settingsRepo(settingsRepo ? settingsRepo : std::make_shared<Avila::Storage::SettingsRepository>()) {
    ensureDeviceCredentials();
}

void CryptoManager::setProfile(const QString &profileName) {
    m_settingsRepo->setProfile(profileName);
    ensureDeviceCredentials();
}

void CryptoManager::ensureDeviceCredentials() {
    QString devId = m_settingsRepo->deviceId();
    if (devId.isEmpty()) {
        QString prof = m_settingsRepo->profile();
        devId = QString("avila-dev-%1%2").arg(prof.isEmpty() ? "" : prof + "-",
                                              QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_settingsRepo->setDeviceId(devId);
    }

    QString pubKey = m_settingsRepo->publicKey();
    if (pubKey.isEmpty()) {
        QByteArray randomKey = m_cryptoService->generateRandomBytes(32);
        if (!randomKey.isEmpty()) {
            pubKey = QString::fromLatin1(randomKey.toBase64());
            m_settingsRepo->setPublicKey(pubKey);
        }
    }
}

QString CryptoManager::getDeviceId() {
    return m_settingsRepo->deviceId();
}

QString CryptoManager::getDevicePublicKey() {
    return m_settingsRepo->publicKey();
}

void CryptoManager::initializeKeyFromPassphrase(const QString &passphrase) {
    auto cryptoSvc = m_cryptoService;
    QtConcurrent::run([cryptoSvc, passphrase]() {
        cryptoSvc->deriveKeyFromPassphrase(passphrase);
    });
}

void CryptoManager::encryptMessageAsposing(const QString &channelId, const QString &plainText) {
    auto cryptoSvc = m_cryptoService;
    QByteArray plainData = plainText.toUtf8();

    QtConcurrent::run([this, cryptoSvc, channelId, plainData]() {
        auto payload = cryptoSvc->encryptAesGcm(plainData);
        if (payload.success) {
            QString cipherB64 = QString::fromLatin1(payload.cipherWithTag.toBase64());
            QString nonceB64 = QString::fromLatin1(payload.nonce.toBase64());
            QMetaObject::invokeMethod(this, [this, channelId, cipherB64, nonceB64]() {
                emit encryptionCompleted(channelId, cipherB64, nonceB64);
            }, Qt::QueuedConnection);
        }
    });
}