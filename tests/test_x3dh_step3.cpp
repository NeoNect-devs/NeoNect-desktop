#include "test_x3dh_step3.h"
#include <QtTest/QtTest>
#include <memory>
#include <set>
#include "crypto/OpenSSLBackend.h"
#include "crypto/XEdDSAAdapter.h"
#include "crypto/KeyGenerationService.h"
#include "crypto/MemoryPreKeyStore.h"
#include "crypto/KeyEncoding.h"

using namespace NeoNect::Crypto;

void TestX3DHStep3::testIdentityKey() {
    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    KeyGenerationService service(backend, xeddsa);

    auto identityKey = service.generateIdentityKeyPair();
    QCOMPARE(identityKey.privateKey.data.size(), 32);
    QCOMPARE(identityKey.publicKey.data.size(), 32);

    X25519PublicKey expectedPub = identityKey.publicKey;

    MemoryPreKeyStore store;
    store.storeIdentityKey(std::move(identityKey));

    auto retrieved = store.identityKey();
    QVERIFY(retrieved.has_value());
    QCOMPARE(retrieved->publicKey.data, expectedPub.data);
    QCOMPARE(retrieved->privateKey.data.size(), 32);
}

void TestX3DHStep3::testSignedPreKey() {
    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    KeyGenerationService service(backend, xeddsa);

    auto identityKey = service.generateIdentityKeyPair();
    auto spk = service.generateSignedPreKey(identityKey, 1);

    QCOMPARE(spk.id, 1U);
    QCOMPARE(spk.signature.data.size(), 64);
    
    QByteArray encodedPub = KeyEncoding::Encode(spk.publicKey);
    ByteView messageView{reinterpret_cast<const uint8_t*>(encodedPub.constData()), static_cast<size_t>(encodedPub.size())};
    
    QVERIFY(xeddsa->verify(identityKey.publicKey, messageView, spk.signature));
    
    // modify pub key
    auto modifiedPub = spk.publicKey;
    if (modifiedPub.data.size() > 0) modifiedPub.data[0] = modifiedPub.data[0] ^ 0x01;
    QByteArray modifiedEncodedPub = KeyEncoding::Encode(modifiedPub);
    ByteView modifiedMessageView{reinterpret_cast<const uint8_t*>(modifiedEncodedPub.constData()), static_cast<size_t>(modifiedEncodedPub.size())};
    QVERIFY(!xeddsa->verify(identityKey.publicKey, modifiedMessageView, spk.signature));
    
    // modify identity key
    auto anotherIdentity = service.generateIdentityKeyPair();
    QVERIFY(!xeddsa->verify(anotherIdentity.publicKey, messageView, spk.signature));
}

void TestX3DHStep3::testOneTimePreKeys() {
    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    KeyGenerationService service(backend, xeddsa);

    auto opks = service.generateOneTimePreKeys(5, 100);
    QCOMPARE(opks.size(), 5);

    std::set<KeyId> ids;
    for (const auto& opk : opks) {
        ids.insert(opk.id);
    }
    QCOMPARE(ids.size(), 5);

    MemoryPreKeyStore store;
    store.storeOneTimePreKeys(std::move(opks));

    QCOMPARE(store.availableOneTimePreKeyCount(), 5);
    auto available = store.availableOneTimePreKeys();
    QCOMPARE(available.size(), 5);

    auto consumed = store.consumeOneTimePreKey(101);
    QVERIFY(consumed.has_value());
    QCOMPARE(consumed->id, 101U);
    QCOMPARE(store.availableOneTimePreKeyCount(), 4);

    auto consumedTwice = store.consumeOneTimePreKey(101);
    QVERIFY(!consumedTwice.has_value());

    auto unknown = store.consumeOneTimePreKey(999);
    QVERIFY(!unknown.has_value());
}
