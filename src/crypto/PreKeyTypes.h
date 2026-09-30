#pragma once

#include <cstdint>
#include "CryptoTypes.h"

namespace NeoNect {
namespace Crypto {

using KeyId = uint32_t;

struct IdentityKeyPair {
    X25519PublicKey publicKey;
    X25519PrivateKey privateKey;
};

struct SignedPreKey {
    KeyId id;
    X25519PublicKey publicKey;
    X25519PrivateKey privateKey;
    Signature64 signature;
    uint64_t timestamp;
};

struct OneTimePreKey {
    KeyId id;
    X25519PublicKey publicKey;
    X25519PrivateKey privateKey;
    bool isConsumed{false};
};

struct OneTimePreKeyPublic {
    KeyId id;
    X25519PublicKey publicKey;
};

} // namespace Crypto
} // namespace NeoNect
