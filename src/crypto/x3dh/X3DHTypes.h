#pragma once

#include <optional>
#include <QByteArray>
#include "../../common/types.h"
#include "../CryptoTypes.h"
#include "../PreKeyTypes.h"

namespace NeoNect {
namespace Crypto {
namespace X3DH {

struct BobPreKeyBundle {
    X25519PublicKey identityKey;
    X25519PublicKey signedPreKey;
    KeyId signedPreKeyId;
    Signature64 signedPreKeySignature;
    std::optional<X25519PublicKey> oneTimePreKey;
    std::optional<KeyId> oneTimePreKeyId;
};

struct InitialMessageMetadata {
    X25519PublicKey aliceIdentityKey;
    X25519PublicKey aliceEphemeralKey;
    KeyId bobSignedPreKeyId;
    std::optional<KeyId> bobOneTimePreKeyId;
};

struct X3DHResult {
    SecureBuffer sharedSecret;
    QByteArray associatedData;
    std::optional<X25519PublicKey> localEphemeralPublicKey; // Alice side only
    KeyId signedPreKeyId;
    std::optional<KeyId> oneTimePreKeyId;
};

} // namespace X3DH
} // namespace Crypto
} // namespace NeoNect
