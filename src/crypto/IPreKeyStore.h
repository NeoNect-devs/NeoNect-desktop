#pragma once
#include <optional>
#include <vector>
#include "PreKeyTypes.h"

namespace NeoNect {
namespace Crypto {

class IPreKeyStore {
public:
    virtual ~IPreKeyStore() = default;

    virtual std::optional<IdentityKeyPair> identityKey() = 0;
    virtual void storeIdentityKey(IdentityKeyPair key) = 0;

    virtual std::optional<SignedPreKey> signedPreKey() = 0;
    virtual void storeSignedPreKey(SignedPreKey key) = 0;

    virtual std::vector<OneTimePreKeyPublic> availableOneTimePreKeys() = 0;
    virtual void storeOneTimePreKeys(std::vector<OneTimePreKey> keys) = 0;

    virtual std::optional<OneTimePreKey> consumeOneTimePreKey(KeyId id) = 0;
    virtual std::optional<OneTimePreKey> getOneTimePreKey(KeyId id) = 0;
    virtual size_t availableOneTimePreKeyCount() = 0;
};

} // namespace Crypto
} // namespace NeoNect
