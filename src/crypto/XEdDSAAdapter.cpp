#include "XEdDSAAdapter.h"
#include "../../third_party/libxeddsa/include/xeddsa.h"
#include <openssl/rand.h>

namespace NeoNect {
namespace Crypto {

Signature64 XEdDSAAdapter::sign(
    const X25519PrivateKey& privateKey,
    ByteView message) 
{
    Signature64 sig;
    sig.data.resize(64);
    uint8_t nonce[64];
    uint8_t priv[32];
    
    // Generate a fresh 64-byte cryptographically secure random value
    if (RAND_bytes(nonce, sizeof(nonce)) != 1) {
        throw std::runtime_error("Failed to generate random nonce for XEdDSA signing");
    }

    xeddsa_init();
    
    // Adjust the private key such that the sign of the derived Ed25519 public key is zero
    priv_force_sign(priv, privateKey.data.data(), false);
    
    ed25519_priv_sign((uint8_t*)sig.data.data(), priv, message.data, message.size, nonce);
    
    // Wipe the nonce and intermediate private key
    OPENSSL_cleanse(nonce, sizeof(nonce));
    OPENSSL_cleanse(priv, sizeof(priv));
    
    return sig;
}

bool XEdDSAAdapter::verify(
    const X25519PublicKey& publicKey,
    ByteView message,
    const Signature64& signature) 
{
    uint8_t ed_pub[32];
    xeddsa_init();
    
    // Strict XEd25519 verification: reject u >= p
    // p = 2^255 - 19
    // Little endian: ED FF FF FF FF FF ... FF 7F
    const uint8_t* u = (const uint8_t*)publicKey.data.data();
    uint8_t masked_top = u[31] & 0x7F;
    if (masked_top == 0x7F) {
        bool all_ff = true;
        for (int i = 30; i >= 1; i--) {
            if (u[i] != 0xFF) {
                all_ff = false;
                break;
            }
        }
        if (all_ff && u[0] >= 0xED) {
            return false;
        }
    }

    // Strict XEd25519 verification: sign bit MUST be 0
    curve25519_pub_to_ed25519_pub(ed_pub, u, false);
    return ed25519_verify((const uint8_t*)signature.data.data(), ed_pub, message.data, message.size) == 0;
}

} // namespace Crypto
} // namespace NeoNect
