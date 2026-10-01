#pragma once
#include <QString>
#include <QByteArray>
#include "../common/types.h"

namespace NeoNect {
namespace Transport {

struct TransportMetadata {
    qint64 messageId{0};
    QString senderDeviceId;
    QString recipientDeviceId;
};

class IIncomingEnvelopeHandler {
public:
    virtual ~IIncomingEnvelopeHandler() = default;

    virtual NeoNect::VoidResult handleEnvelope(
        const QByteArray &envelope,
        const TransportMetadata &metadata) = 0;
};

} // namespace Transport
} // namespace NeoNect
