#pragma once
#include <QString>
#include <QByteArray>
#include <QVariantList>

namespace NeoNect {
namespace Domain {

enum class MessageStatus {
    Sending,
    Sent,
    Failed,
    Seen,
    Pending,
    Accepted,
    Declined
};

struct Message {
    QString id;                 // Local UUID (used by UI to identify message uniquely)
    qint64 serverId{0};         // Relay server ID (unique on the server side)
    QString conversationId;     // E.g. "dms:username"
    QString senderId;           // The sender's username
    QString type{"text"};       // "text", "image", "voice", etc.
    QString text;               // Payload text
    QString mediaUrl;
    QString fileName;
    qint64 fileSize{0};
    int duration{0};
    QByteArray waveform;        // JSON array or raw bytes representing waveform
    MessageStatus status{MessageStatus::Sent};
    QString errorText;
    qint64 timestamp{0};        // Unix epoch ms
};

} // namespace Domain
} // namespace NeoNect

Q_DECLARE_METATYPE(NeoNect::Domain::Message)
Q_DECLARE_METATYPE(std::vector<NeoNect::Domain::Message>)
