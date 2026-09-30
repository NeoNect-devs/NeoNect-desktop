#pragma once
#include <QByteArray>
#include "../common/openssl_raii.h"

namespace NeoNect {
namespace Crypto {

struct X25519PublicKey {
    QByteArray data;
};

struct X25519PrivateKey {
    SecureBuffer data;
};

struct Sha256Digest {
    QByteArray data;
};

struct AeadKey {
    SecureBuffer data;
};

struct AeadNonce {
    QByteArray data;
};

struct AeadTag {
    QByteArray data;
};

struct AeadEncryptResult {
    bool success = false;
    QByteArray ciphertext;
    AeadTag tag;
};

} // namespace Crypto
} // namespace NeoNect
