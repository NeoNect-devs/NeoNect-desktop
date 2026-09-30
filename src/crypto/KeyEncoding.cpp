#include "KeyEncoding.h"

namespace NeoNect {
namespace Crypto {

QByteArray KeyEncoding::Encode(const X25519PublicKey& publicKey) {
    QByteArray encoded;
    encoded.reserve(1 + publicKey.data.size());
    encoded.append(static_cast<char>(0x05));
    encoded.append(publicKey.data);
    return encoded;
}

} // namespace Crypto
} // namespace NeoNect
