#pragma once

#include <optional>
#include "X3DHTypes.h"
#include "../ICryptoBackend.h"
#include "../IXEdDSA.h"
#include "../IPreKeyStore.h"

namespace NeoNect {
namespace Crypto {
namespace X3DH {

class IX3DH {
public:
    virtual ~IX3DH() = default;

    virtual std::optional<X3DHResult> initiate(
        ICryptoBackend& backend,
        IXEdDSA& xeddsa,
        const IdentityKeyPair& aliceIdentity,
        const BobPreKeyBundle& bobBundle) = 0;

    virtual std::optional<X3DHResult> respond(
        ICryptoBackend& backend,
        IPreKeyStore& store,
        const InitialMessageMetadata& msg) = 0;
};

} // namespace X3DH
} // namespace Crypto
} // namespace NeoNect
