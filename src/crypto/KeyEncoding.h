#pragma once

#include <QByteArray>
#include "CryptoTypes.h"

namespace NeoNect {
namespace Crypto {

class KeyEncoding {
public:
    static QByteArray Encode(const X25519PublicKey& publicKey);
};

} // namespace Crypto
} // namespace NeoNect
