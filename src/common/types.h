/**
 * @file types.h
 * @brief Fundamental system data structures, enumerations, and value objects for NeoNect.
 * @details Defines cross-layer common types used throughout the transport, service,
 * storage, and presentation layers.
 * 
 * @par Design Pattern:
 * Value Object / Monad (ServiceResult)
 * @author NeoNect Development Team
 * @version 1.0.0
 */

#pragma once
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <optional>
#include <functional>
#include <variant>

/**
 * @namespace NeoNect
 * @brief Primary root namespace for the NeoNect Secure Communication Platform.
 * @details Encompasses all core client subsystems, business services, domain models, storage engines,
 * cryptographic primitives, and transport protocols.
 */
namespace NeoNect {

/**
 * @brief High-level network connection state machine values.
 * @details Represents the transport connectivity state between client and server.
 */
enum class ConnectionStatus {
    Disconnected, ///< Client is offline or disconnected from network services.
    Connecting,   ///< Connection handshake or WebSocket establishment is in progress.
    Connected,    ///< Real-time duplex relay connection is active and operational.
    Error         ///< Connection encountered an unrecoverable failure (e.g., DNS, auth rejection).
};

/**
 * @brief Real-time user peer presence states.
 * @details Reflects whether a remote friend is reachable over the relay network.
 */
enum class PresenceStatus {
    Offline, ///< User has disconnected or their presence heartbeat timed out.
    Online   ///< User is connected and actively exchanging heartbeats.
};

/**
 * @brief Represents an authenticated user profile entity.
 * @details Stores the current session's identity credentials retrieved from backend.
 */
struct UserProfile {
    QString username;     ///< Canonical username (lowercase, alphanumeric).
    bool isValid{false};  ///< True if profile has been validated against backend authority.
};

/**
 * @brief Cryptographic device identity and enrollment credentials.
 * @details Represents an individual device registered under an account's device inventory.
 */
struct DeviceCredentials {
    QString deviceId;         ///< Unique device identifier (e.g., "neonect-dev-user-uuid").
    QString publicKeyBase64;  ///< Base64-encoded public key used for end-to-end cryptographic handshakes.
};

/**
 * @brief Wire-level relay payload data container.
 * @details Encapsulates raw message delivery parameters across the transport layer.
 */
struct RelayMessagePacket {
    qint64 id{0};          ///< Unique numerical relay identifier assigned by mailbox.
    QString sender;        ///< Originating user's canonical username.
    QString target;        ///< Destination user's canonical username.
    QString content;       ///< Plaintext or serialized JSON message body.
    qint64 timestamp{0};   ///< Unix epoch timestamp in milliseconds when packet was dispatched.
    bool isFromMe{false};  ///< Indicates if the packet originated from the active local device.
};

/**
 * @brief Friend list entry representing a peer relationship.
 * @details Combines relationship identity, presence telemetry, and last-seen activity timestamps.
 */
struct FriendEntry {
    QString username;                             ///< Peer's canonical username.
    PresenceStatus status{PresenceStatus::Offline}; ///< Current known presence status.
    QDateTime lastSeen;                           ///< Timestamp of last verified activity or presence beacon.
};

/**
 * @brief Encapsulates the results of an AES-256-GCM encryption or decryption operation.
 * @details Contains the ciphertext, authentication tag, initialization vector (IV),
 * and the self-contained serialized binary envelope.
 */
struct EncryptedPayload {
    QByteArray cipherWithTag; ///< Ciphertext bytes concatenated with the 16-byte GCM authentication tag.
    QByteArray nonce;         ///< 12-byte (96-bit) Initialization Vector / Nonce generated via CSPRNG.
    QByteArray envelope;      ///< Self-contained binary envelope: `[version(1B)][iv_len(1B)][iv][cipher+tag]`.
    bool success{false};      ///< True if the cryptographic transformation succeeded without error.
    QString errorMessage;     ///< Detailed error description if `success` is false.
};

/**
 * @brief Monadic result container for service-layer operations.
 * @tparam T Type of payload data returned upon success.
 * @details Eliminates exception-based control flow by encapsulating success status,
 * user-friendly error messages, and optional return data.
 * 
 * @pattern Result Monad Pattern
 */
template <typename T>
struct ServiceResult {
    bool success{false};               ///< True if operation executed successfully.
    QString message;                   ///< Informational or error description string.
    std::optional<T> data{std::nullopt}; ///< Operation return payload, populated only on success.

    /**
     * @brief Constructs a successful ServiceResult.
     * @param data Resulting payload object.
     * @param msg Optional informational message.
     * @return Configured ServiceResult instance.
     */
    static ServiceResult<T> ok(T data, const QString &msg = QString()) {
        return {true, msg, std::make_optional(std::move(data))};
    }

    /**
     * @brief Constructs a failed ServiceResult.
     * @param msg Descriptive error explanation.
     * @return Configured ServiceResult instance with nullopt data.
     */
    static ServiceResult<T> fail(const QString &msg) {
        return {false, msg, std::nullopt};
    }
};

/**
 * @brief Convenience alias for service operations returning no payload data.
 */
using VoidResult = ServiceResult<std::monostate>;

} // namespace NeoNect
