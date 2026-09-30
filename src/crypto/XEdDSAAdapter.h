#pragma once

#include "IXEdDSA.h"

namespace NeoNect {
namespace Crypto {

class XEdDSAAdapter : public IXEdDSA {
public:
    Signature64 sign(
        const X25519PrivateKey& privateKey,
        ByteView message) override;

    bool verify(
        const X25519PublicKey& publicKey,
        ByteView message,
        const Signature64& signature) override;
};

} // namespace Crypto
} // namespace NeoNect
