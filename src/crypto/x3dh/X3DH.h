#pragma once
#include "IX3DH.h"

namespace NeoNect {
namespace Crypto {
namespace X3DH {

class X3DHImpl : public IX3DH {
public:
    std::optional<X3DHResult> initiate(
        ICryptoBackend& backend,
        IXEdDSA& xeddsa,
        const IdentityKeyPair& aliceIdentity,
        const BobPreKeyBundle& bobBundle) override;

    std::optional<X3DHResult> respond(
        ICryptoBackend& backend,
        IPreKeyStore& store,
        const InitialMessageMetadata& msg) override;
};

} // namespace X3DH
} // namespace Crypto
} // namespace NeoNect
