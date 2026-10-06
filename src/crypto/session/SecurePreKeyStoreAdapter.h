#pragma once
#include <memory>
#include "../IPreKeyStore.h"
#include "../../storage/e2ee/ISecureE2EEStore.h"

namespace NeoNect {
namespace Crypto {
namespace Session {

class SecurePreKeyStoreAdapter : public IPreKeyStore {
public:
    explicit SecurePreKeyStoreAdapter(std::weak_ptr<Storage::ISecureE2EEStore> store) : m_store(store) {}

    // Temporary compatibility bridge for Phase 2
    void setStore(std::weak_ptr<Storage::ISecureE2EEStore> store) { m_store = store; }

    std::optional<IdentityKeyPair> identityKey() override;
    void storeIdentityKey(IdentityKeyPair key) override;

    std::optional<SignedPreKey> signedPreKey() override;
    void storeSignedPreKey(SignedPreKey key) override;

    std::vector<OneTimePreKeyPublic> availableOneTimePreKeys() override;
    void storeOneTimePreKeys(std::vector<OneTimePreKey> keys) override;

    std::optional<OneTimePreKey> consumeOneTimePreKey(KeyId id) override;
    std::optional<OneTimePreKey> getOneTimePreKey(KeyId id) override;
    size_t availableOneTimePreKeyCount() override;

private:
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
};

} // namespace Session
} // namespace Crypto
} // namespace NeoNect
