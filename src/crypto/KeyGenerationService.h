#pragma once
#include <memory>
#include <vector>
#include "PreKeyTypes.h"
#include "ICryptoBackend.h"
#include "IXEdDSA.h"

namespace NeoNect {
namespace Crypto {

class KeyGenerationService {
public:
    KeyGenerationService(std::shared_ptr<ICryptoBackend> cryptoBackend, std::shared_ptr<IXEdDSA> xeddsa);

    IdentityKeyPair generateIdentityKeyPair();
    
    SignedPreKey generateSignedPreKey(const IdentityKeyPair& identityKey, KeyId id);
    
    std::vector<OneTimePreKey> generateOneTimePreKeys(size_t count, KeyId startId);
    
private:
    std::shared_ptr<ICryptoBackend> m_cryptoBackend;
    std::shared_ptr<IXEdDSA> m_xeddsa;
};

} // namespace Crypto
} // namespace NeoNect
