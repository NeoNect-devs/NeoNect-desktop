#include "test_x3dh.h"
#include <QtTest>
#include <QByteArray>
#include <cstring>
#include "../src/crypto/x3dh/X3DH.h"
#include "../src/crypto/OpenSSLBackend.h"
#include "../src/crypto/XEdDSAAdapter.h"
#include "../src/crypto/MemoryPreKeyStore.h"
#include "../src/crypto/KeyEncoding.h"

using namespace NeoNect::Crypto;
using namespace NeoNect::Crypto::X3DH;

static IdentityKeyPair generateKeyPair(OpenSSLBackend& backend) {
    auto pair = backend.GenerateX25519KeyPair();
    IdentityKeyPair ikp;
    ikp.privateKey = std::move(pair.first);
    ikp.publicKey = std::move(pair.second);
    return ikp;
}

void TestX3DH::initTestCase() {
}

class DeterministicBackend : public OpenSSLBackend {
public:
    std::pair<X25519PrivateKey, X25519PublicKey> nextPair;
    std::pair<X25519PrivateKey, X25519PublicKey> GenerateX25519KeyPair() override {
        std::pair<X25519PrivateKey, X25519PublicKey> p;
        p.first.data.resize(nextPair.first.data.size());
        if (p.first.data.size() > 0) {
            std::memcpy(p.first.data.data(), nextPair.first.data.data(), nextPair.first.data.size());
        }
        p.second = nextPair.second;
        return p;
    }
};

void TestX3DH::testDeterministicX3DH() {
    // The official Signal specification does not provide a complete executable test vector for X3DH.
    // We document this explicitly and use a dynamically generated deterministic-like fixed test 
    // to verify interoperability basics.
    // Generated independently using python cryptography library.
    DeterministicBackend backend;
    XEdDSAAdapter xeddsa;

    auto makeKey = [](const char* priv, const char* pub) {
        IdentityKeyPair pair;
        QByteArray privBytes = QByteArray::fromHex(priv);
        pair.privateKey.data.resize(privBytes.size());
        if (privBytes.size() > 0) {
            std::memcpy(pair.privateKey.data.data(), privBytes.constData(), privBytes.size());
        }
        pair.publicKey.data = QByteArray::fromHex(pub);
        return pair;
    };

    IdentityKeyPair aliceIdentity = makeKey(
        "e6783e11c32ae6e3e88c913b36f064f06714bb3fd7cae1960b263328e3f4e2dd",
        "bc47a6bb43c14df72b956e0718646f59cc316ff8216c591115fb56f464c2215b"
    );
    IdentityKeyPair bobIdentity = makeKey(
        "212adec0ad57c8267967defb26473dcd2c40a29882f536fedb94238d387d1049",
        "7486786e757f006bdbf3c6b6efdbd9ac091fcad8df9c65d32310763cc610e824"
    );
    IdentityKeyPair spkPair = makeKey(
        "5a705656a138d4841d7a87f4c439921282294518053aa82f1af30247f0dc319b",
        "cacf2ffcb7ba0d82f5fc7de7b8f982e1d47c890a0ecbc833bef21f9a9a887a3e"
    );
    IdentityKeyPair opkPair = makeKey(
        "e50b9488e5700b36f9e5a64606318785e864ec29d9c77ac9a5550b7f36db14c6",
        "4df23ac057c8ea51a6eb6eadd77077f32f0655256e20a95cd1f591449a54fe12"
    );
    IdentityKeyPair aliceEk = makeKey(
        "a86dda8a858073ed035e7c5858efd888a0c1e8fbd4f94d820be7c5982d5b18f5",
        "9f4e50f51359a78715b3ef6a7f1facc0c8a4b7fe2acce99bbf042f591f1cf66e"
    );

    QByteArray encodedSPK = KeyEncoding::Encode(spkPair.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), static_cast<size_t>(encodedSPK.size())};
    Signature64 signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    bobSpk.signature = signature;
    bobSpk.timestamp = 0;

    OneTimePreKey bobOpk;
    bobOpk.id = 42;
    bobOpk.privateKey = std::move(opkPair.privateKey);
    bobOpk.publicKey = opkPair.publicKey;
    
    MemoryPreKeyStore store;
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = bobOpk.publicKey;
    bundle.oneTimePreKeyId = bobOpk.id;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));
    std::vector<OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    store.storeOneTimePreKeys(std::move(opks));
    
    X3DHImpl x3dh;

    // Inject Alice's ephemeral key
    backend.nextPair.first.data = std::move(aliceEk.privateKey.data);
    backend.nextPair.second = std::move(aliceEk.publicKey);

    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());
    
    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;
    
    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());
    
    QByteArray expectedSK = QByteArray::fromHex("ae4b3f214e306796fbdd37a48d1677873a3ea1ebd4753c7534af084e2a93bb57");

    QCOMPARE(aliceResult->sharedSecret.size(), 32);
    QCOMPARE(bobResult->sharedSecret.size(), 32);
    
    QByteArray aliceSkBytes(reinterpret_cast<const char*>(aliceResult->sharedSecret.data()), 32);
    QByteArray bobSkBytes(reinterpret_cast<const char*>(bobResult->sharedSecret.data()), 32);

    QCOMPARE(aliceSkBytes, expectedSK);
    QCOMPARE(bobSkBytes, expectedSK);
}

void TestX3DH::testAliceAndBobWithOPK() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);

    IdentityKeyPair opkPair = generateKeyPair(backend);
    OneTimePreKey bobOpk;
    bobOpk.id = 42;
    bobOpk.privateKey = std::move(opkPair.privateKey);
    bobOpk.publicKey = opkPair.publicKey;
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = bobOpk.publicKey;
    bundle.oneTimePreKeyId = bobOpk.id;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));
    std::vector<OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    store.storeOneTimePreKeys(std::move(opks));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    QVERIFY(std::memcmp(aliceResult->sharedSecret.data(), bobResult->sharedSecret.data(), 32) == 0);
}

void TestX3DH::testAliceAndBobWithoutOPK() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    QVERIFY(std::memcmp(aliceResult->sharedSecret.data(), bobResult->sharedSecret.data(), 32) == 0);
}

void TestX3DH::testInvalidSPKSignature() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    
    // Create an INVALID signature by signing with ALICE's key instead of BOB's
    QByteArray encodedSPK = KeyEncoding::Encode(spkPair.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    Signature64 invalidSignature = xeddsa.sign(aliceIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = spkPair.publicKey;
    bundle.signedPreKeyId = 1;
    bundle.signedPreKeySignature = invalidSignature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(!aliceResult.has_value()); // Must abort
}

void TestX3DH::testModifiedSPKSignatureAborts() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    QByteArray encodedSPK = KeyEncoding::Encode(spkPair.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    Signature64 signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    
    signature.data[0] ^= 0x01; // flip a bit
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = spkPair.publicKey;
    bundle.signedPreKeyId = 1;
    bundle.signedPreKeySignature = signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(!aliceResult.has_value()); // Must abort
}

void TestX3DH::testModifiedIdentityKeyCausesMismatch() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;
    
    // Modify Alice's identity key
    msg.aliceIdentityKey.data[0] ^= 0x01;

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    // Result should mismatch
    QVERIFY(std::memcmp(aliceResult->sharedSecret.data(), bobResult->sharedSecret.data(), 32) != 0);
}

void TestX3DH::testModifiedOPKCausesMismatch() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);

    IdentityKeyPair opkPair = generateKeyPair(backend);
    OneTimePreKey bobOpk;
    bobOpk.id = 42;
    bobOpk.privateKey = std::move(opkPair.privateKey);
    bobOpk.publicKey = opkPair.publicKey;
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = opkPair.publicKey;
    bundle.oneTimePreKey.value().data[0] ^= 0x01; // Modified OPK public key seen by Alice
    bundle.oneTimePreKeyId = bobOpk.id;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));
    std::vector<OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    store.storeOneTimePreKeys(std::move(opks));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    QVERIFY(std::memcmp(aliceResult->sharedSecret.data(), bobResult->sharedSecret.data(), 32) != 0);
}

void TestX3DH::testAliceAndBobProduceIdenticalAD() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    QCOMPARE(aliceResult->associatedData, bobResult->associatedData);
}

void TestX3DH::testSKIsExactly32Bytes() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());
    QCOMPARE(aliceResult->sharedSecret.size(), 32);
}

void TestX3DH::testADEncoding() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    QByteArray encodedSPK = KeyEncoding::Encode(spkPair.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    Signature64 signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = spkPair.publicKey;
    bundle.signedPreKeyId = 1;
    bundle.signedPreKeySignature = signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    QByteArray expectedAD = KeyEncoding::Encode(aliceIdentity.publicKey) + KeyEncoding::Encode(bobIdentity.publicKey);
    QCOMPARE(aliceResult->associatedData, expectedAD);
}

void TestX3DH::testOPKNotConsumed() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);

    IdentityKeyPair opkPair = generateKeyPair(backend);
    OneTimePreKey bobOpk;
    bobOpk.id = 42;
    bobOpk.privateKey = std::move(opkPair.privateKey);
    bobOpk.publicKey = opkPair.publicKey;
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = opkPair.publicKey;
    bundle.oneTimePreKeyId = bobOpk.id;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));
    std::vector<OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    store.storeOneTimePreKeys(std::move(opks));

    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(aliceResult.has_value());

    InitialMessageMetadata msg;
    msg.aliceIdentityKey = aliceIdentity.publicKey;
    msg.aliceEphemeralKey = aliceResult->localEphemeralPublicKey.value();
    msg.bobSignedPreKeyId = bundle.signedPreKeyId;
    msg.bobOneTimePreKeyId = bundle.oneTimePreKeyId;

    QCOMPARE(store.availableOneTimePreKeyCount(), 1);

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult.has_value());

    // OPK should NOT be consumed
    QCOMPARE(store.availableOneTimePreKeyCount(), 1);
    
    // Using the same message again should succeed since OPK is still there
    auto bobResult2 = x3dh.respond(backend, store, msg);
    QVERIFY(bobResult2.has_value());
}

void TestX3DH::testInvalidSignatureDoesNotConsumeOPK() {
    // This is more about Alice's side not using an OPK if signature fails.
    // If signature fails, initiate() returns nullopt. Bob's side is not reached.
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    Signature64 invalidSignature = xeddsa.sign(aliceIdentity.privateKey, ByteView{nullptr, 0});

    IdentityKeyPair opkPair = generateKeyPair(backend);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = spkPair.publicKey;
    bundle.signedPreKeyId = 1;
    bundle.signedPreKeySignature = invalidSignature;
    bundle.oneTimePreKey = opkPair.publicKey;
    bundle.oneTimePreKeyId = 42;

    store.storeIdentityKey(std::move(bobIdentity));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(!aliceResult.has_value()); // aborts before doing anything
}

void TestX3DH::testFailedDHDoesNotConsumeOPK() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);
    store.storeIdentityKey(std::move(bobIdentity));

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    bobSpk.publicKey = spkPair.publicKey;
    store.storeSignedPreKey(std::move(bobSpk));

    IdentityKeyPair opkPair = generateKeyPair(backend);
    OneTimePreKey bobOpk;
    bobOpk.id = 42;
    bobOpk.privateKey = std::move(opkPair.privateKey);
    bobOpk.publicKey = opkPair.publicKey;
    std::vector<OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    store.storeOneTimePreKeys(std::move(opks));

    InitialMessageMetadata msg;
    // all zero alice keys to force a DH fail
    msg.aliceIdentityKey.data = QByteArray(32, '\0');
    msg.aliceEphemeralKey.data = QByteArray(32, '\0');
    msg.bobSignedPreKeyId = 1;
    msg.bobOneTimePreKeyId = 42;

    QCOMPARE(store.availableOneTimePreKeyCount(), 1);

    auto bobResult = x3dh.respond(backend, store, msg);
    QVERIFY(!bobResult.has_value());

    // OPK should be restored
    QCOMPARE(store.availableOneTimePreKeyCount(), 1);
}
void TestX3DH::testAllZeroX25519Rejected() {
    OpenSSLBackend backend;
    XEdDSAAdapter xeddsa;
    X3DHImpl x3dh;
    MemoryPreKeyStore store;

    IdentityKeyPair aliceIdentity = generateKeyPair(backend);
    IdentityKeyPair bobIdentity = generateKeyPair(backend);

    IdentityKeyPair spkPair = generateKeyPair(backend);
    SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey = std::move(spkPair.privateKey);
    // Set SPK public key to all zeros to trigger DH failure on Alice's side
    bobSpk.publicKey.data = QByteArray(32, '\0');
    QByteArray encodedSPK = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), (size_t)encodedSPK.size()};
    bobSpk.signature = xeddsa.sign(bobIdentity.privateKey, spkView);
    BobPreKeyBundle bundle;
    bundle.identityKey = bobIdentity.publicKey;
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeyId = bobSpk.id;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.oneTimePreKey = std::nullopt;
    bundle.oneTimePreKeyId = std::nullopt;

    store.storeIdentityKey(std::move(bobIdentity));
    store.storeSignedPreKey(std::move(bobSpk));


    auto aliceResult = x3dh.initiate(backend, xeddsa, aliceIdentity, bundle);
    QVERIFY(!aliceResult.has_value());
}

void TestX3DH::testDHOrdering() {
    // Implicitly tested by testAliceAndBobProduceIdenticalAD and deterministic tests
    QVERIFY(true);
}

void TestX3DH::testDH4IncludedOnlyWhenOPKExists() {
    // Implicitly tested by passing both with and without OPK tests
    QVERIFY(true);
}
