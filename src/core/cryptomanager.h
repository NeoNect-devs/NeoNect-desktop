// src/core/cryptomanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>

class CryptoManager : public QObject {
    Q_OBJECT
public:
    static CryptoManager* instance();
    explicit CryptoManager(QObject *parent = nullptr);

    Q_INVOKABLE QString getDeviceId();
    Q_INVOKABLE QString getDevicePublicKey();
    Q_INVOKABLE void initializeKeyFromPassphrase(const QString &passphrase);
    Q_INVOKABLE void encryptMessageAsposing(const QString &channelId, const QString &plainText);

signals:
    void encryptionCompleted(const QString &channelId, const QString &cipherBase64, const QString &nonceBase64);
    void decryptionCompleted(const QString &messageId, const QString &plainText);

private:
    QByteArray m_masterSymKey;
    QString m_deviceId;
    QString m_publicKey;
};