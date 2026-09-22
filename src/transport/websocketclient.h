/**
 * @file websocketclient.h
 * @brief Native RFC 6455 WebSocket protocol implementation with SSL/TLS and auto-reconnection.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Implements a lightweight, RFC 6455 compliant WebSocket client over `QSslSocket`.
 * It provides custom HTTP upgrade handshaking, Sec-WebSocket-Key generation and validation,
 * 4-byte client masking, frame fragmentation handling, ping/pong keepalives, and automatic
 * exponential backoff reconnection.
 *
 * @par Design Patterns:
 * - <b>State Machine Pattern</b>: Tracks connection progression through `WebSocketState`.
 * - <b>Observer Pattern</b>: Emits signals on frame arrival, state changes, and socket errors.
 * - <b>Protocol Engine / Adapter</b>: Adapts low-level TCP/SSL byte streams into discrete WebSocket frames.
 *
 * @par RFC 6455 Protocol Constraints & Invariants:
 * - <b>Client Masking</b>: All frames sent from client to server MUST have the MASK bit set (1)
 *   and provide a 4-byte pseudo-random masking key generated via `QRandomGenerator`.
 * - <b>Inbound Validation</b>: Server-to-client frames MUST NOT be masked.
 * - <b>Control Frames</b>: Ping (0x9), Pong (0xA), and Close (0x8) frames MUST NOT exceed 125 bytes payload length.
 * - <b>Reconnection Backoff Bounds</b>: Backoff delay starts at 1,000 ms, doubles each retry ($2^n \times 1000$),
 *   and is capped at a maximum ceiling of 30,000 ms (30 seconds).
 * - <b>State Guard</b>: Outgoing frames can only be sent when state is `WebSocketState::Connected`.
 */

#pragma once

#include <QObject>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>
#include <QByteArray>
#include <QString>
#include <QRandomGenerator>

namespace NeoNect {
namespace Transport {

/**
 * @enum WebSocketState
 * @brief Represents the lifecycle states of the RFC 6455 WebSocket client.
 *
 * @par State Machine Invariant:
 * State transitions must follow the valid directed DAG:
 * `Disconnected -> Connecting -> Handshaking -> Connected -> Closing -> Disconnected`.
 */
enum class WebSocketState {
    Disconnected, /**< Socket is closed and inactive. */
    Connecting,   /**< TCP/SSL connection establishment in progress. */
    Handshaking,  /**< HTTP 101 Switching Protocols upgrade request sent, awaiting server response. */
    Connected,    /**< Handshake verified; bi-directional WebSocket frames can be exchanged. */
    Closing       /**< Close control frame sent or received, closing socket. */
};

/**
 * @class WebSocketClient
 * @brief Real-time bidirectional streaming client for relay push notifications and signaling.
 *
 * @par Operational Bounds:
 * - Maximum frame payload size: 65,536 bytes (64 KB).
 * - Maximum control frame size: 125 bytes.
 * - Maximum reconnection backoff: 30,000 ms.
 */
class WebSocketClient : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the WebSocket client.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit WebSocketClient(QObject *parent = nullptr);

    /**
     * @brief Destructor. Closes active socket and stops reconnection timers.
     * @post Underlying socket is disconnected, SSL session closed, and timers terminated.
     */
    ~WebSocketClient() override;

    /**
     * @brief Initiates connection to the remote WebSocket relay endpoint.
     * @param serverUrl Server base address (e.g. `"http://localhost:8080"` or `"wss://relay.neonect.chat"`).
     * @param deviceId Client device UUID sent in headers/query.
     * @param token Authentication bearer token.
     * @pre `serverUrl` must be non-empty and parseable as a valid QUrl.
     * @pre `deviceId` must be non-empty.
     * @post State transitions to `WebSocketState::Connecting`.
     */
    void open(const QString &serverUrl, const QString &deviceId, const QString &token);

    /**
     * @brief Closes the WebSocket connection gracefully using close frame.
     * @post State transitions to `WebSocketState::Closing` and subsequently `Disconnected`.
     * @post Automatic reconnection is suppressed.
     */
    void close();

    /**
     * @brief Checks if the client is currently connected and handshaked.
     * @return True if state is `WebSocketState::Connected`.
     */
    bool isConnected() const { return m_state == WebSocketState::Connected; }

    /**
     * @brief Retrieves the current state machine state.
     * @return Current `WebSocketState`.
     */
    WebSocketState state() const { return m_state; }

    /**
     * @brief Retrieves the registered device ID.
     * @return Device UUID string.
     */
    QString deviceId() const { return m_deviceId; }

    /**
     * @brief Retrieves the session token used for authentication.
     * @return Authentication token.
     */
    QString token() const { return m_token; }

    /**
     * @brief Sends a text frame (opcode 0x1) masked according to RFC 6455.
     * @param text UTF-8 string payload.
     * @pre Client state must be `WebSocketState::Connected`.
     * @pre Byte size of `text.toUtf8()` must be <= 65,536 bytes (64 KB).
     * @post 4-byte mask is generated and XOR-applied; frame is queued to socket output buffer.
     */
    void sendTextMessage(const QString &text);

    /**
     * @brief Sends a ping control frame (opcode 0x9) for keepalive heartbeat.
     * @param data Optional heartbeat payload.
     * @pre `data.size() <= 125` bytes (RFC 6455 control frame payload limit).
     * @pre State must be `WebSocketState::Connected`.
     * @post Ping frame is transmitted with mask bit set.
     */
    void sendPing(const QByteArray &data = QByteArray());

signals:
    /** @brief Emitted upon successful completion of the HTTP 101 WebSocket upgrade handshake. */
    void connected();

    /** @brief Emitted when the socket disconnects or closes. */
    void disconnected();

    /**
     * @brief Emitted when a complete unmasked text frame is received from the server.
     * @param message Text payload string.
     */
    void textMessageReceived(const QString &message);

    /**
     * @brief Emitted on network socket errors or protocol validation failures.
     * @param error Descriptive error message.
     */
    void errorOccurred(const QString &error);

    /**
     * @brief Emitted whenever the internal state machine transitions to a new state.
     * @param newState Target state.
     */
    void stateChanged(WebSocketState newState);

private slots:
    /** @brief Socket connected slot: triggers handshake transmission. */
    void onSocketConnected();
    /** @brief Socket disconnected slot: initiates exponential backoff reconnect if requested. */
    void onSocketDisconnected();
    /** @brief Inbound data ready slot: processes handshake or binary frames. */
    void onSocketReadyRead();
    /** @brief Socket error slot. */
    void onSocketError(QAbstractSocket::SocketError socketError);
    /** @brief Reconnection timer tick slot. */
    void onReconnectTimer();

private:
    /** @brief Transitions connection state and emits @ref stateChanged. */
    void setState(WebSocketState newState);
    /** @brief Sends HTTP GET upgrade request with Sec-WebSocket-Key and headers. */
    void performHandshake();
    /** @brief Parses incoming HTTP 101 upgrade handshake response. */
    void processIncomingData();
    /** @brief Parses raw RFC 6455 frames from incoming buffer. */
    void processFrames();
    /** @brief Sends pong frame (opcode 0xA) in response to server ping. */
    void sendPong(const QByteArray &data);
    /** @brief Encodes, masks, and writes an RFC 6455 frame to the underlying socket. */
    void sendFrame(quint8 opcode, const QByteArray &data);

    /** @brief Underlying SSL/TCP socket. */
    QSslSocket *m_socket = nullptr;
    /** @brief Timer for managing exponential backoff reconnect attempts. */
    QTimer *m_reconnectTimer = nullptr;

    QString m_serverUrl;
    QString m_deviceId;
    QString m_token;
    /** @brief Base64 expected accept key computed from client nonce. */
    QString m_expectedAcceptKey;

    WebSocketState m_state{WebSocketState::Disconnected};
    /** @brief Indicates whether the user desires to maintain an active connection. */
    bool m_shouldBeConnected{false};
    /** @brief Number of consecutive failed reconnection attempts. */
    int m_reconnectAttempts{0};

    /** @brief Buffer accumulating raw incoming TCP bytes. */
    QByteArray m_readBuffer;
    /** @brief Flag indicating whether handshake negotiation succeeded. */
    bool m_handshakeComplete{false};
};

} // namespace Transport
} // namespace NeoNect
