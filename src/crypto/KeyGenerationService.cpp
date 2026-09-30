#include "KeyGenerationService.h"
#include "KeyEncoding.h"
#include <chrono>

namespace NeoNect {
namespace Crypto {

KeyGenerationService::KeyGenerationService(std::shared_ptr<ICryptoBackend> cryptoBackend, std::shared_ptr<IXEdDSA> xeddsa)
    : m_cryptoBackend(std::move(cryptoBackend)), m_xeddsa(std::move(xeddsa)) {}

IdentityKeyPair KeyGenerationService::generateIdentityKeyPair() {
    auto pair = m_cryptoBackend->GenerateX25519KeyPair();
    return {std::move(pair.second), std::move(pair.first)};
}

SignedPreKey KeyGenerationService::generateSignedPreKey(const IdentityKeyPair& identityKey, KeyId id) {
    auto pair = m_cryptoBackend->GenerateX25519KeyPair();
    
    QByteArray encodedPub = KeyEncoding::Encode(pair.second);
    ByteView messageView{reinterpret_cast<const uint8_t*>(encodedPub.constData()), static_cast<size_t>(encodedPub.size())};
    
    Signature64 signature = m_xeddsa->sign(identityKey.privateKey, messageView);
    
    uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    return {id, std::move(pair.second), std::move(pair.first), std::move(signature), timestamp};
}

std::vector<OneTimePreKey> KeyGenerationService::generateOneTimePreKeys(size_t count, KeyId startId) {
    std::vector<OneTimePreKey> keys;
    keys.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        auto pair = m_cryptoBackend->GenerateX25519KeyPair();
        keys.push_back({static_cast<KeyId>(startId + i), std::move(pair.second), std::move(pair.first)});
    }
    
    return keys;
}

} // namespace Crypto
} // namespace NeoNect
