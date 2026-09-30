#pragma once

#include <optional>
#include <QByteArray>
#include "../CryptoTypes.h"

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

class IAEAD {
public:
    virtual ~IAEAD() = default;

    virtual AeadEncryptResult encrypt(
        ByteView messageKey,
        ByteView plaintext,
        ByteView associatedData) = 0;

    virtual std::optional<QByteArray> decrypt(
        ByteView messageKey,
        ByteView ciphertext,
        ByteView tag,
        ByteView associatedData) = 0;
};

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
