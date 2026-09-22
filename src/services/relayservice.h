/**
 * @file relayservice.h
 * @brief Gateway service managing encrypted packet relaying, WebSocket push, and HTTP polling.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Acts as the client's secure communication gateway. Decouples local message processing from
 * network transport by managing both persistent WebSocket connections and fallback HTTP polling.
 * Inbound ciphertext envelopes from the relay server are authenticated, decrypted via `ICryptoService`,
 * deduplicated, and dispatched to `MessageService` or `FriendService`.
 *
 * @par Design Patterns:
 * - <b>Gateway Pattern</b>: Serves as the single cryptographic and network entry/exit point for domain packets.
 * - <b>Hybrid Transport Pattern</b>: Real-time WebSocket streaming with automatic fallback to HTTP polling.
 * - <b>Idempotent Receiver Pattern</b>: Maintains a processed message cache to prevent replay/duplicate deliveries.
 * - <b>Observer Pattern</b>: Emits signals for decrypted messages, presence updates, and connection telemetry.
 */

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

/**
 * @class RelayService
 * @brief Ingress/egress gateway handling encrypted message transmission and streaming notifications.
 */
class RelayService : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the relay gateway service.
     * @param transport Shared pointer to HTTP transport.
     * @param storage Shared pointer to persistent settings.
     * @param cryptoService Shared pointer to cryptographic engine.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit RelayService(std::shared_ptr<Transport::IHttpTransport> transport,
                          std::shared_ptr<Storage::ISettingsRepository> storage,
                          std::shared_ptr<Crypto::ICryptoService> cryptoService,
                          QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes WebSocket connection and stops timers.
     */
    ~RelayService() override = default;

    /**
     * @brief Activates the fallback HTTP polling timer and opens the WebSocket stream.
     */
    void startPolling();

    /**
     * @brief Stops the polling timer and disconnects the WebSocket.
     */
    void stopPolling();

    /**
     * @brief Checks if polling is currently active.
     */
    bool isPolling() const;

    /**
     * @brief Checks if the client has an established transport connection (WebSocket or reachable HTTP).
     */
    bool isConnected() const;

    /**
     * @brief Dispatches an immediate query for queued relay messages via HTTP GET.
     */
    void pollPendingMessages();

    /**
     * @brief Sends an acknowledgment to the server confirming message receipt, allowing queue pruning.
     * @param messageId Sequential server message identifier.
     */
    void acknowledgeMessage(qint64 messageId);

public slots:
    /**
     * @brief Encrypts and transmits an outbound domain message through the relay network.
     * @param msg Domain message entity containing recipient, type, and payload.
     */
    void sendDomainMessage(const Domain::Message &msg);

signals:
    /** @brief Emitted when a single decrypted chat message arrives from the network. */
    void incomingDomainMessageReceived(const NeoNect::Domain::Message &msg);
    /** @brief Emitted when a batch of decrypted messages arrives. */
    void incomingDomainMessagesReceived(const std::vector<NeoNect::Domain::Message> &msgs);
    /** @brief Emitted when a signaling friend packet (request/status) arrives. */
    void incomingFriendPacket(const NeoNect::Domain::Message &msg);
    /** @brief Emitted when a peer's avatar URL is updated via relay telemetry. */
    void peerAvatarUpdated(const QString &username, const QString &avatarUrl);
    /** @brief Emitted upon completion of a message transmission. */
    void secureMessageTransmitted(const QString &targetUser, bool success);
    /** @brief Emitted detailing packet delivery status and errors. */
    void messageTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage);
    /** @brief Emitted when the server rejects requests with HTTP 401 Unauthorized. */
    void sessionUnauthorized(const QString &message);
    /** @brief Emitted when the relay service requests device key re-registration. */
    void deviceRegistrationRequested();

    /** @brief Emitted when the connection to the server is confirmed operational. */
    void serverConnected();
    /** @brief Emitted when connection to the server is lost. */
    void serverDisconnected();

private:
    /** @brief Internal handler for 401 errors, executing exponential backoff or logout signaling. */
    void handle401Error();
    /** @brief Decrypts and processes a base64-encoded encrypted envelope from the server. */
    void processIncomingRelayItem(qint64 msgId, const QString &base64Cipher);
    /** @brief Slot handling incoming text frames from WebSocketClient. */
    void onWebSocketMessageReceived(const QString &text);

    /** @brief HTTP transport layer. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    /** @brief Settings repository. */
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
    /** @brief Cryptographic engine. */
    std::shared_ptr<Crypto::ICryptoService> m_cryptoService;
    /** @brief Native WebSocket client. */
    std::unique_ptr<Transport::WebSocketClient> m_wsClient;
    /** @brief Polling timer for HTTP fallback. */
    QTimer *m_pollTimer;
    /** @brief Count of consecutive 401 responses. */
    int m_retry401Count{0};
    /** @brief Flag indicating active polling state. */
    bool m_isPollingActive{false};
    /** @brief Flag tracking WebSocket connectivity state. */
    bool m_isWsConnected{false};
    /** @brief Deduplication set storing processed server message IDs. */
    std::unordered_set<qint64> m_processedMessageIds;
};

} // namespace Services
} // namespace NeoNect
