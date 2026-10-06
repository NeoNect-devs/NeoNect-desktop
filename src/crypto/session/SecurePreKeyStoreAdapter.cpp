#include "SecurePreKeyStoreAdapter.h"

namespace NeoNect {
namespace Crypto {
namespace Session {

std::optional<IdentityKeyPair> SecurePreKeyStoreAdapter::identityKey() {
    auto store = m_store.lock();
    if (!store) return std::nullopt;
    auto res = store->getIdentity();
    if (!res.success) return std::nullopt;
    IdentityKeyPair kp;
    kp.privateKey.data.resize(32);
    kp.publicKey.data.resize(32);
    std::copy(res.data.value().private_key.begin(), res.data.value().private_key.end(), kp.privateKey.data.data());
    std::copy(res.data.value().public_key.begin(), res.data.value().public_key.end(), kp.publicKey.data.data());
    return kp;
}

void SecurePreKeyStoreAdapter::storeIdentityKey(IdentityKeyPair key) {
    auto store = m_store.lock();
    if (!store) return;
    Storage::E2EEIdentity id;
    id.public_key = QByteArray(reinterpret_cast<const char*>(key.publicKey.data.data()), key.publicKey.data.size());
    id.private_key = QByteArray(reinterpret_cast<const char*>(key.privateKey.data.data()), key.privateKey.data.size());
    store->saveIdentity(id);
}

std::optional<SignedPreKey> SecurePreKeyStoreAdapter::signedPreKey() {
    auto store = m_store.lock();
    if (!store) return std::nullopt;
    auto res = store->getAllSignedPreKeys();
    if (!res.success || res.data.value().empty()) return std::nullopt;
    auto spk = res.data.value().back(); // return the latest
    SignedPreKey key;
    key.id = spk.key_id;
    key.privateKey.data.resize(32);
    key.publicKey.data.resize(32);
    key.signature.data.resize(64);
    std::copy(spk.private_key.begin(), spk.private_key.end(), key.privateKey.data.data());
    std::copy(spk.public_key.begin(), spk.public_key.end(), key.publicKey.data.data());
    std::copy(spk.signature.begin(), spk.signature.end(), key.signature.data.data());
    return key;
}

void SecurePreKeyStoreAdapter::storeSignedPreKey(SignedPreKey key) {
    auto store = m_store.lock();
    if (!store) return;
    Storage::E2EESignedPreKey spk;
    spk.key_id = key.id;
    spk.public_key = QByteArray(reinterpret_cast<const char*>(key.publicKey.data.data()), key.publicKey.data.size());
    spk.private_key = QByteArray(reinterpret_cast<const char*>(key.privateKey.data.data()), key.privateKey.data.size());
    spk.signature = QByteArray(reinterpret_cast<const char*>(key.signature.data.data()), key.signature.data.size());
    store->saveSignedPreKey(spk);
}

std::vector<OneTimePreKeyPublic> SecurePreKeyStoreAdapter::availableOneTimePreKeys() {
    auto store = m_store.lock();
    if (!store) return {};
    auto res = store->getAvailableOneTimePreKeys();
    std::vector<OneTimePreKeyPublic> keys;
    if (res.success) {
        for (const auto& opk : res.data.value()) {
            OneTimePreKeyPublic pk;
            pk.id = opk.key_id;
            pk.publicKey.data.resize(32);
            std::copy(opk.public_key.begin(), opk.public_key.end(), pk.publicKey.data.data());
            keys.push_back(pk);
        }
    }
    return keys;
}

void SecurePreKeyStoreAdapter::storeOneTimePreKeys(std::vector<OneTimePreKey> keys) {
    auto store = m_store.lock();
    if (!store) return;
    std::vector<Storage::E2EEOneTimePreKey> opks;
    for (const auto& k : keys) {
        Storage::E2EEOneTimePreKey opk;
        opk.key_id = k.id;
        opk.public_key = QByteArray(reinterpret_cast<const char*>(k.publicKey.data.data()), k.publicKey.data.size());
        opk.private_key = QByteArray(reinterpret_cast<const char*>(k.privateKey.data.data()), k.privateKey.data.size());
        opk.state = Storage::OPKState::AVAILABLE;
        opks.push_back(opk);
    }
    store->saveOneTimePreKeys(opks);
}

std::optional<OneTimePreKey> SecurePreKeyStoreAdapter::consumeOneTimePreKey(KeyId id) {
    auto store = m_store.lock();
    if (!store) return std::nullopt;
    auto getRes = store->getOneTimePreKey(id);
    if (!getRes.success || getRes.data.value().state != Storage::OPKState::AVAILABLE) return std::nullopt;
    
    auto consumeRes = store->consumeOneTimePreKeyAtomically(id);
    if (!consumeRes.success) return std::nullopt;

    OneTimePreKey pk;
    pk.id = getRes.data.value().key_id;
    pk.privateKey.data.resize(32);
    pk.publicKey.data.resize(32);
    std::copy(getRes.data.value().private_key.begin(), getRes.data.value().private_key.end(), pk.privateKey.data.data());
    std::copy(getRes.data.value().public_key.begin(), getRes.data.value().public_key.end(), pk.publicKey.data.data());
    return pk;
}

std::optional<OneTimePreKey> SecurePreKeyStoreAdapter::getOneTimePreKey(KeyId id) {
    auto store = m_store.lock();
    if (!store) return std::nullopt;
    auto getRes = store->getOneTimePreKey(id);
    if (!getRes.success) return std::nullopt;

    OneTimePreKey pk;
    pk.id = getRes.data.value().key_id;
    pk.privateKey.data.resize(32);
    pk.publicKey.data.resize(32);
    std::copy(getRes.data.value().private_key.begin(), getRes.data.value().private_key.end(), pk.privateKey.data.data());
    std::copy(getRes.data.value().public_key.begin(), getRes.data.value().public_key.end(), pk.publicKey.data.data());
    return pk;
}

size_t SecurePreKeyStoreAdapter::availableOneTimePreKeyCount() {
    auto store = m_store.lock();
    if (!store) return 0;
    auto res = store->getAvailableOneTimePreKeys();
    if (!res.success) return 0;
    return res.data.value().size();
}

} // namespace Session
} // namespace Crypto
} // namespace NeoNect
