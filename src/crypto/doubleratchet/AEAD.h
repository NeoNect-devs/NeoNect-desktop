#pragma once

#include "IAEAD.h"
#include "../ICryptoBackend.h"

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

class AEAD : public IAEAD {
public:
    explicit AEAD(ICryptoBackend* backend);
    ~AEAD() override = default;

    AeadEncryptResult encrypt(
        ByteView messageKey,
        ByteView plaintext,
        ByteView associatedData) override;

    std::optional<QByteArray> decrypt(
        ByteView messageKey,
        ByteView ciphertext,
        ByteView tag,
        ByteView associatedData) override;

private:
    ICryptoBackend* backend_;
};

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
