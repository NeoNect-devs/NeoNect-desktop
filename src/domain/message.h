/**
 * @file message.h
 * @brief Domain entity and status enumeration for chat messages, media packets, and signaling frames.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * This file defines the core Domain Entity and Value Objects for the chat sub-system in NeoNect.
 * Designed following Domain-Driven Design (DDD) principles, `Message` represents both persisted
 * conversation events (stored in SQLite via `IMessageRepository`) and in-flight communication packets
 * (transmitted across WebSocket / HTTP relays via `RelayService` and `MessageService`).
 *
 * @par Design Pattern:
 * - <b>Value Object / Domain Entity</b>: Encapsulates message attributes, identity, and transport status.
 * - <b>Optimistic Concurrency & State Machine</b>: Explicit state progression through `MessageStatus`.
 */

#pragma once
#include <QString>
#include <QByteArray>
#include <QVariantList>

namespace NeoNect {
namespace Domain {

/**
 * @enum MessageStatus
 * @brief Lifecycle and delivery states for a domain message.
 *
 * @details
 * Models the optimistic UI and two-phase delivery pipeline:
 * 1. An outgoing message is created locally as `Sending`.
 * 2. When the server or peer acknowledges delivery, the state transitions to `Sent`.
 * 3. If transmission fails or network times out, it moves to `Failed`.
 * 4. When the peer acknowledges reading the message, it updates to `Seen`.
 * 5. Two-phase media requests and call invites use `Pending`, `Accepted`, or `Declined`.
 */
enum class MessageStatus {
    Sending,   /**< Message created locally; currently awaiting network transmission or relay ACK. */
    Sent,      /**< Message successfully transmitted to relay server or delivered to peer. */
    Failed,    /**< Message transmission failed (network timeout, encryption error, or server rejection). */
    Seen,      /**< Read receipt acknowledged by the remote peer. */
    Pending,   /**< Two-phase media transfer or call invite awaiting recipient acceptance. */
    Accepted,  /**< Two-phase media transfer or invitation accepted by recipient. */
    Declined   /**< Two-phase media transfer or invitation explicitly rejected by recipient. */
};

/**
 * @struct Message
 * @brief Represents an individual message entity in the NeoNect communication engine.
 *
 * @details
 * The `Message` struct holds the complete state of a communication event, including
 * payload text, cryptographic identifiers, multimedia attachments, waveform telemetry
 * for voice recordings, and delivery lifecycle metadata.
 *
 * It is fully copyable, movable, and registered as a Qt Metatype (`Q_DECLARE_METATYPE`)
 * to allow seamless crossing of thread boundaries and queued signal/slot emissions.
 */
struct Message {
    /**
     * @brief Client-side locally generated unique identifier (RFC 4122 UUID v4).
     * Used by the UI and QML views to track message state optimistically before server ACK.
     */
    QString id;

    /**
     * @brief Server-side sequential relay identifier.
     * Assigned by the NeoNect relay service upon ingestion. 0 indicates an unacknowledged or local message.
     */
    qint64 serverId{0};

    /**
     * @brief Conversation scope identifier.
     * Typically formatted as `"dms:<username>"` for 1:1 direct messages or `"channel:<uuid>"` for group rooms.
     */
    QString conversationId;

    /**
     * @brief Unique username of the message originator / author.
     */
    QString senderId;

    /**
     * @brief Content discriminator / MIME-like type tag.
     * Supported values: `"text"`, `"image"`, `"voice"`, `"audio"`, `"video"`, `"file"`, `"sticker"`.
     */
    QString type{"text"};

    /**
     * @brief Decrypted plaintext content or text caption.
     */
    QString text;

    /**
     * @brief URI pointing to media attachment.
     * Can be a local file URI (`file:///...`) or an encrypted remote download endpoint.
     */
    QString mediaUrl;

    /**
     * @brief Original filename including extension for file attachments (e.g. `"invoice.pdf"`).
     */
    QString fileName;

    /**
     * @brief Size of the attached media or file in bytes.
     */
    qint64 fileSize{0};

    /**
     * @brief Playback duration in seconds for voice notes and audio/video media.
     */
    int duration{0};

    /**
     * @brief Pre-computed waveform amplitude data for voice message visualization.
     * Encoded as JSON or raw byte samples normalized between 0.0 and 1.0.
     */
    QByteArray waveform;

    /**
     * @brief Current delivery lifecycle state of the message.
     */
    MessageStatus status{MessageStatus::Sent};

    /**
     * @brief Human-readable diagnostic message explaining why delivery or decryption failed.
     * Empty if @ref status is not `MessageStatus::Failed`.
     */
    QString errorText;

    /**
     * @brief Message creation or transmission timestamp expressed in Unix epoch milliseconds.
     */
    qint64 timestamp{0};
};

} // namespace Domain
} // namespace NeoNect

Q_DECLARE_METATYPE(NeoNect::Domain::Message)
Q_DECLARE_METATYPE(std::vector<NeoNect::Domain::Message>)
