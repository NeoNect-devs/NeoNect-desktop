#pragma once

#include "WireTypes.h"
#include "../../common/constants.h"
#include <QByteArray>
#include <optional>

namespace NeoNect {
namespace Crypto {
namespace Wire {

class WireCodec {
public:
    static constexpr uint8_t CURRENT_VERSION = 1;

    // Encoders
    static QByteArray encodeInitialEnvelope(const InitialEnvelope& env);
    static QByteArray encodeRatchetEnvelope(const RatchetEnvelope& env);
    
    // Decoders
    static std::optional<InitialEnvelope> decodeInitialEnvelope(const QByteArray& data);
    static std::optional<RatchetEnvelope> decodeRatchetEnvelope(const QByteArray& data);

    // Canonical Ratchet Header
    static QByteArray encodeRatchetHeader(const RatchetHeader& header);
    static std::optional<RatchetHeader> decodeRatchetHeader(const QByteArray& data);

    // Associated Data
    static QByteArray getRatchetMessageAAD(const RatchetHeader& header);
};

} // namespace Wire
} // namespace Crypto
} // namespace NeoNect
