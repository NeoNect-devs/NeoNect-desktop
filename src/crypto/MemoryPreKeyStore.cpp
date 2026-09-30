#include "MemoryPreKeyStore.h"
#include <cstring>

namespace NeoNect {
namespace Crypto {

std::optional<IdentityKeyPair> MemoryPreKeyStore::identityKey() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_identityKey.has_value()) return std::nullopt;
    IdentityKeyPair copy;
    copy.publicKey.data = m_identityKey->publicKey.data;
    copy.privateKey.data.resize(m_identityKey->privateKey.data.size());
    if (copy.privateKey.data.size() > 0) {
        std::memcpy(copy.privateKey.data.data(), m_identityKey->privateKey.data.data(), m_identityKey->privateKey.data.size());
    }
    return copy;
}

void MemoryPreKeyStore::storeIdentityKey(IdentityKeyPair key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_identityKey = std::move(key);
}

std::optional<SignedPreKey> MemoryPreKeyStore::signedPreKey() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_signedPreKey.has_value()) return std::nullopt;
    SignedPreKey copy;
    copy.id = m_signedPreKey->id;
    copy.publicKey.data = m_signedPreKey->publicKey.data;
    copy.signature.data = m_signedPreKey->signature.data;
    copy.timestamp = m_signedPreKey->timestamp;
    copy.privateKey.data.resize(m_signedPreKey->privateKey.data.size());
    if (copy.privateKey.data.size() > 0) {
        std::memcpy(copy.privateKey.data.data(), m_signedPreKey->privateKey.data.data(), m_signedPreKey->privateKey.data.size());
    }
    return copy;
}

void MemoryPreKeyStore::storeSignedPreKey(SignedPreKey key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_signedPreKey = std::move(key);
}

std::vector<OneTimePreKeyPublic> MemoryPreKeyStore::availableOneTimePreKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<OneTimePreKeyPublic> result;
    result.reserve(m_oneTimePreKeys.size());
    for (const auto& pair : m_oneTimePreKeys) {
        result.push_back({pair.first, pair.second.publicKey});
    }
    return result;
}

void MemoryPreKeyStore::storeOneTimePreKeys(std::vector<OneTimePreKey> keys) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& key : keys) {
        m_oneTimePreKeys.emplace(key.id, std::move(key));
    }
}

std::optional<OneTimePreKey> MemoryPreKeyStore::consumeOneTimePreKey(KeyId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_oneTimePreKeys.find(id);
    if (it != m_oneTimePreKeys.end()) {
        OneTimePreKey key = std::move(it->second);
        m_oneTimePreKeys.erase(it);
        return key;
    }
    return std::nullopt;
}

std::optional<OneTimePreKey> MemoryPreKeyStore::getOneTimePreKey(KeyId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_oneTimePreKeys.find(id);
    if (it != m_oneTimePreKeys.end()) {
        OneTimePreKey copy;
        copy.id = it->second.id;
        copy.publicKey.data = it->second.publicKey.data;
        copy.privateKey.data.resize(it->second.privateKey.data.size());
        if (copy.privateKey.data.size() > 0) {
            std::memcpy(copy.privateKey.data.data(), it->second.privateKey.data.data(), it->second.privateKey.data.size());
        }
        return copy;
    }
    return std::nullopt;
}

size_t MemoryPreKeyStore::availableOneTimePreKeyCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_oneTimePreKeys.size();
}

} // namespace Crypto
} // namespace NeoNect
