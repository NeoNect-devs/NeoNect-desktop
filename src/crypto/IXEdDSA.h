#pragma once

#include "../common/types.h"
#include "CryptoTypes.h"

namespace NeoNect {
namespace Crypto {

class IXEdDSA {
public:
    virtual ~IXEdDSA() = default;

    virtual Signature64 sign(
        const X25519PrivateKey& privateKey,
        ByteView message) = 0;

    virtual bool verify(
        const X25519PublicKey& publicKey,
        ByteView message,
        const Signature64& signature) = 0;
};

} // namespace Crypto
} // namespace NeoNect
