#pragma once
#include <QObject>
#include <QTimer>
#include <memory>
#include <unordered_set>
#include "../transport/ihttptransport.h"
#include "../transport/websocketclient.h"
#include "../storage/isettingsrepository.h"
#include "../crypto/icryptoservice.h"
#include "../domain/message.h"

namespace NeoNect {
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
    bool isConnected() const;

    void pollPendingMessages();
    void acknowledgeMessage(qint64 messageId);

public slots:
    void sendDomainMessage(const Domain::Message &msg);

signals:
    void incomingDomainMessageReceived(const NeoNect::Domain::Message &msg);
    void incomingDomainMessagesReceived(const std::vector<NeoNect::Domain::Message> &msgs);
    void incomingFriendPacket(const NeoNect::Domain::Message &msg);
    void peerAvatarUpdated(const QString &username, const QString &avatarUrl);
    void secureMessageTransmitted(const QString &targetUser, bool success);
    void messageTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage);
    void sessionUnauthorized(const QString &message);
    void deviceRegistrationRequested();

    void serverConnected();
    void serverDisconnected();

private:
    void handle401Error();
    void processIncomingRelayItem(qint64 msgId, const QString &base64Cipher);
    void onWebSocketMessageReceived(const QString &text);

    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
    std::shared_ptr<Crypto::ICryptoService> m_cryptoService;
    std::unique_ptr<Transport::WebSocketClient> m_wsClient;
    QTimer *m_pollTimer;
    int m_retry401Count{0};
    bool m_isPollingActive{false};
    bool m_isWsConnected{false};
    std::unordered_set<qint64> m_processedMessageIds;
};

} // namespace Services
} // namespace NeoNect
