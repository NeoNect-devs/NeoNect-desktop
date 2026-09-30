#pragma once
#include "IPreKeyStore.h"
#include <mutex>
#include <unordered_map>

namespace NeoNect {
namespace Crypto {

class MemoryPreKeyStore : public IPreKeyStore {
public:
    std::optional<IdentityKeyPair> identityKey() override;
    void storeIdentityKey(IdentityKeyPair key) override;

    std::optional<SignedPreKey> signedPreKey() override;
    void storeSignedPreKey(SignedPreKey key) override;

    std::vector<OneTimePreKeyPublic> availableOneTimePreKeys() override;
    void storeOneTimePreKeys(std::vector<OneTimePreKey> keys) override;

    std::optional<OneTimePreKey> consumeOneTimePreKey(KeyId id) override;
    size_t availableOneTimePreKeyCount() override;

private:
    std::mutex m_mutex;
    std::optional<IdentityKeyPair> m_identityKey;
    std::optional<SignedPreKey> m_signedPreKey;
    std::unordered_map<KeyId, OneTimePreKey> m_oneTimePreKeys;
};

} // namespace Crypto
} // namespace NeoNect
