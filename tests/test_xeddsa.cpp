#include "test_xeddsa.h"
#include <QtTest>
#include "../src/crypto/XEdDSAAdapter.h"
#include "../third_party/libxeddsa/include/xeddsa.h"
#include <openssl/rand.h>
#include <openssl/evp.h>

using namespace NeoNect::Crypto;

static std::vector<uint8_t> fromHex(const std::string& hex) {
    std::vector<uint8_t> result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = (uint8_t) strtol(byteString.c_str(), NULL, 16);
        result.push_back(byte);
    }
    return result;
}

void TestXEdDSA::testReferenceVector() {
    XEdDSAAdapter adapter;
    
    // Official test vector sourced from Syndace/libxeddsa, a reference XEdDSA implementation.
    // The Signal specification does not provide test vectors, so we rely on the
    // cross-implementation vector from libxeddsa's test suite.
    auto priv_vec = fromHex("0100000000000000000000000000000000000000000000000000000000000000");
    auto pub_vec = fromHex("2fe57da347cd62431528daac5fbb290730fff684afc4cfc2ed90995f58cb3b74");
    auto sig_vec = fromHex("29c9f8cf9d12f493dbad3d4b90232e7359995f600b91877f4040efa1fb24e7c4f680d3aa10ac811f65c8f5b46fa7124c3b1b39147be72e66c21624c35da3f204");
    std::string msg = "test message";
    
    X25519PrivateKey priv;
    priv.data.resize(32);
    std::copy(priv_vec.begin(), priv_vec.end(), priv.data.data());
    
    X25519PublicKey pub;
    pub.data.resize(32);
    std::copy(pub_vec.begin(), pub_vec.end(), pub.data.begin());
    
    Signature64 sig;
    sig.data.resize(64);
    std::copy(sig_vec.begin(), sig_vec.end(), sig.data.begin());
    
    // Verify the reference signature
    QVERIFY(adapter.verify(pub, {(const uint8_t*)msg.data(), msg.size()}, sig));
}

void TestXEdDSA::testSignVerifyCycle() {
    XEdDSAAdapter adapter;
    
    X25519PrivateKey priv;
    priv.data.resize(32);
    RAND_bytes(priv.data.data(), priv.data.size());
    
    X25519PublicKey pub;
    pub.data.resize(32);
    size_t publen = pub.data.size();
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, priv.data.data(), priv.data.size());
    EVP_PKEY_get_raw_public_key(pkey, (unsigned char*)pub.data.data(), &publen);
    EVP_PKEY_free(pkey);
    
    std::string msg = "Hello, NeoNect XEdDSA!";
    ByteView msgView{(const uint8_t*)msg.data(), msg.size()};
    
    Signature64 sig = adapter.sign(priv, msgView);
    QVERIFY(adapter.verify(pub, msgView, sig));
    
    // Wrong message -> reject
    std::string wrong_msg = "Goodbye, NeoNect XEdDSA!";
    QVERIFY(!adapter.verify(pub, {(const uint8_t*)wrong_msg.data(), wrong_msg.size()}, sig));
    
    // Wrong public key -> reject
    X25519PublicKey wrong_pub;
    wrong_pub.data.resize(32);
    RAND_bytes((unsigned char*)wrong_pub.data.data(), wrong_pub.data.size());
    QVERIFY(!adapter.verify(wrong_pub, msgView, sig));
    
    // Modified R -> reject
    Signature64 bad_sig = sig;
    bad_sig.data[0] = bad_sig.data[0] ^ 1;
    QVERIFY(!adapter.verify(pub, msgView, bad_sig));
    
    // Modified s -> reject
    Signature64 bad_sig2 = sig;
    bad_sig2.data[32] = bad_sig2.data[32] ^ 1;
    QVERIFY(!adapter.verify(pub, msgView, bad_sig2));
    
    // Same key + same message -> signatures differ (fresh Z used)
    Signature64 sig2 = adapter.sign(priv, msgView);
    bool diff = false;
    for (size_t i = 0; i < sig.data.size(); ++i) {
        if (sig.data[i] != sig2.data[i]) diff = true;
    }
    QVERIFY(diff);
    
    // --- Alternate Sign Bit Strictness Test ---
    // Generate a signature that is valid ONLY for the alternate sign bit (sign bit = 1).
    // This proves the verifier strictly checks sign bit 0 and does NOT accept sign bit 1.
    uint8_t alt_priv[32];
    priv_force_sign(alt_priv, priv.data.data(), true); // force sign bit 1
    
    Signature64 alt_sig;
    alt_sig.data.resize(64);
    uint8_t nonce[64];
    RAND_bytes(nonce, sizeof(nonce));
    
    // Create the XEdDSA signature targeting the alternate sign bit
    ed25519_priv_sign((uint8_t*)alt_sig.data.data(), alt_priv, msgView.data, msgView.size, nonce);
    
    // The strict verifier MUST reject it because it forces sign bit 0.
    QVERIFY(!adapter.verify(pub, msgView, alt_sig));
}
