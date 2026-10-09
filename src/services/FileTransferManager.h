#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <memory>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QMap>
#include "../crypto/ICryptoBackend.h"
#include "../storage/e2ee/ISecureE2EEStore.h"
#include "../transport/IIncomingEnvelopeHandler.h"
#include "../storage/icapabilitiesrepository.h"
#include "../storage/isettingsrepository.h"
#include <QTimer>

namespace NeoNect {
namespace Services {

class FileTransferManager : public QObject, public Transport::IIncomingEnvelopeHandler {
    Q_OBJECT
public:
    using SendEnvelopeCb = std::function<bool(const QString& recipientUser, const QString& recipientDevice, const QString& msgId, const QByteArray& env)>;
    using SendChatMsgCb = std::function<void(const QJsonObject& payload, const QString& recipientUser, const QString& recipientDevice)>;

    explicit FileTransferManager(
        std::weak_ptr<Storage::ISecureE2EEStore> store,
        std::shared_ptr<Crypto::ICryptoBackend> crypto,
        std::shared_ptr<Storage::ICapabilitiesRepository> capabilities,
        std::shared_ptr<Storage::ISettingsRepository> settings,
        SendEnvelopeCb sendEnvelopeCb,
        SendChatMsgCb sendChatMsgCb,
        QObject* parent = nullptr);

    // API to start transfer
    void startTransfer(const QString& recipientUser, const QString& recipientDevice, const QString& filePath, const QString& overrideTransferId = "");
    void cancelTransfer(const QString& transferId, const QString& peerDevice, const QString& reason);
    void setStore(std::weak_ptr<Storage::ISecureE2EEStore> store) { m_store = store; }

    using IsImageTransferCb = std::function<bool(const QString& transferId)>;
    void setIsImageTransferCb(IsImageTransferCb cb) { m_isImageTransferCb = std::move(cb); }

    void setRetryParamsForTesting(int timerIntervalMs, int inFlightTimeoutMs) {
        m_inFlightTimeoutMs = inFlightTimeoutMs;
        m_retryTimer->setInterval(timerIntervalMs);
    }

    // Handle incoming data-plane chunk (IIncomingEnvelopeHandler)
    NeoNect::VoidResult handleEnvelope(const QByteArray &envelope, const Transport::TransportMetadata &metadata) override;

    // Handle incoming control-plane message (via SessionManager)
    void handleControlMessage(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj);

signals:
    void transferProgress(const QString& transferId, int percentage);
    void transferCompleted(const QString& transferId, const QString& localPath);
    void transferFailed(const QString& transferId, const QString& error);

private:
    void handleFileStart(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj);
    void handleFileAck(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj);
    void handleFileNack(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj);
    void handleFileCancel(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj);

    void processNextSend(const QString& transferId, const QString& peerDevice, const QString& recipientUser);
    void sendChunk(const Storage::E2EEFileTransfer& transfer, uint32_t chunkIndex, const QString& recipientUser);

    void reconstructFile(const Storage::E2EEFileTransfer& transfer);

    QByteArray deriveSenderSpoolKey(const QByteArray& rootKey, const QByteArray& transferId, const QString& senderDeviceId);
    QByteArray deriveRecipientDeviceKey(const QByteArray& rootKey, const QByteArray& transferId, const QString& recipientDeviceId);
    
    QByteArray generateNonce(uint32_t chunkIndex, uint8_t domain = 0);

private slots:
    void onRetryTimeout();

private:
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
    std::shared_ptr<Crypto::ICryptoBackend> m_crypto;
    std::shared_ptr<Storage::ICapabilitiesRepository> m_capabilities;
    std::shared_ptr<Storage::ISettingsRepository> m_settings;
    SendEnvelopeCb m_sendEnvelopeCb;
    SendChatMsgCb m_sendChatMsgCb;
    
    QString m_spoolDir;
    IsImageTransferCb m_isImageTransferCb;
    QMap<QString, QMap<uint32_t, qint64>> m_inFlight;
    QMap<QString, QMap<uint32_t, int>> m_retryCounts;
    QMap<QString, QString> m_activeUsers;
    QTimer* m_retryTimer;
    int m_inFlightTimeoutMs = 10000;
};

} // namespace Services
} // namespace NeoNect
