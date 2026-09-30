#pragma once

#include <QByteArray>
#include <optional>
#include <cstdint>
#include "../CryptoTypes.h"

namespace NeoNect {
namespace Crypto {
namespace Wire {

enum class EnvelopeType : uint8_t {
    X3DH_INITIAL = 0x01,
    DOUBLE_RATCHET_MESSAGE = 0x02
};

struct InitialEnvelope {
    uint8_t version = 1;
    Crypto::X25519PublicKey senderIdentityKey;
    Crypto::X25519PublicKey senderEphemeralKey;
    uint32_t signedPreKeyId = 0;
    std::optional<uint32_t> oneTimePreKeyId;
    QByteArray ciphertext;
    Crypto::AeadTag tag;
};

struct RatchetHeader {
    Crypto::X25519PublicKey dh;
    uint32_t pn = 0;
    uint32_t n = 0;
};

struct RatchetEnvelope {
    uint8_t version = 1;
    RatchetHeader header;
    QByteArray ciphertext;
    Crypto::AeadTag tag;
};

} // namespace Wire
} // namespace Crypto
} // namespace NeoNect
