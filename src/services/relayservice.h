// src/services/relayservice.h
#pragma once
#include <QObject>
#include <QTimer>
#include <memory>
#include <unordered_set>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"
#include "../crypto/icryptoservice.h"

namespace Avila {
namespace Services {

class RelayService : public QObject {
    Q_OBJECT
public:
    explicit RelayService(std::shared_ptr<Transport::IHttpTransport> transport,
                          std::shared_ptr<Storage::ISettingsRepository> storage,
                          std::shared_ptr<Crypto::ICryptoService> cryptoService,
                          QObject *parent = nullptr);
    ~RelayService() override = default;

    void startPolling();
    void stopPolling();
    bool isPolling() const;

    void sendRelayMessage(const QString &toUsername, const QString &plainText);
    void pollPendingMessages();
    void acknowledgeMessage(qint64 messageId);

signals:
    void incomingRelayMessageReceived(const QString &fromUsername, const QString &target, const QString &text, qint64 timestamp);
    void secureMessageTransmitted(const QString &targetUser, bool success);
    void sessionUnauthorized(const QString &message);
    void deviceRegistrationRequested();

private:
    void handle401Error();

    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
    std::shared_ptr<Crypto::ICryptoService> m_cryptoService;
    QTimer *m_pollTimer;
    int m_retry401Count{0};
    bool m_isPollingActive{false};
    std::unordered_set<qint64> m_processedMessageIds;
};

} // namespace Services
} // namespace Avila
