#include "test_backend.h"
#include <QtTest>
#include "../src/crypto/OpenSSLBackend.h"

using namespace NeoNect::Crypto;

void TestBackend::testRandomBytes() {
    OpenSSLBackend backend;
    QByteArray r1 = backend.RandomBytes(32);
    QByteArray r2 = backend.RandomBytes(32);
    QCOMPARE(r1.size(), 32);
    QCOMPARE(r2.size(), 32);
    QVERIFY(r1 != r2);
}

void TestBackend::testX25519() {
    OpenSSLBackend backend;
    auto kpAlice = backend.GenerateX25519KeyPair();
    auto kpBob = backend.GenerateX25519KeyPair();
    
    QCOMPARE(kpAlice.first.data.size(), 32);
    QCOMPARE(kpAlice.second.data.size(), 32);
    
    SecureBuffer secretAlice = backend.X25519(kpAlice.first, kpBob.second);
    SecureBuffer secretBob = backend.X25519(kpBob.first, kpAlice.second);
    
    QCOMPARE(secretAlice.size(), 32);
    QCOMPARE(secretBob.size(), 32);
    QVERIFY(backend.ConstantTimeCompare(
        QByteArray(reinterpret_cast<const char*>(secretAlice.data()), 32),
        QByteArray(reinterpret_cast<const char*>(secretBob.data()), 32)
    ));
}

void TestBackend::testSha256() {
    OpenSSLBackend backend;
    Sha256Digest digest = backend.Sha256("test");
    QCOMPARE(digest.data.size(), 32);
    QCOMPARE(digest.data.toHex(), QByteArray("9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"));
}

void TestBackend::testHmacSha256() {
    OpenSSLBackend backend;
    QByteArray mac = backend.HmacSha256("key", "The quick brown fox jumps over the lazy dog");
    QCOMPARE(mac.size(), 32);
    QCOMPARE(mac.toHex(), QByteArray("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"));
}

void TestBackend::testHkdfSha256() {
    OpenSSLBackend backend;
    QByteArray ikm = QByteArray::fromHex("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b");
    QByteArray salt = QByteArray::fromHex("000102030405060708090a0b0c");
    QByteArray info = QByteArray::fromHex("f0f1f2f3f4f5f6f7f8f9");
    QByteArray out = backend.HkdfSha256(ikm, salt, info, 42);
    QCOMPARE(out.size(), 42);
    QCOMPARE(out.left(10).toHex(), QByteArray("3cb25f25faacd57a9043"));
}

void TestBackend::testAeadGcm() {
    OpenSSLBackend backend;
    AeadKey key; key.data.resize(32);
    backend.SecureZero(key.data.data(), 32);
    AeadNonce nonce; nonce.data.resize(12);
    nonce.data.fill(0);
    
    QByteArray plaintext = "hello";
    QByteArray aad = "aad";
    
    AeadEncryptResult enc = backend.AeadEncrypt(key, nonce, plaintext, aad);
    QVERIFY(enc.success);
    QCOMPARE(enc.tag.data.size(), 16);
    
    auto dec = backend.AeadDecrypt(key, nonce, enc.ciphertext, enc.tag, aad);
    QVERIFY(dec.has_value());
    QCOMPARE(dec.value(), plaintext);
    
    AeadTag badTag = enc.tag;
    badTag.data[0] = static_cast<char>(badTag.data[0] ^ 1);
    QVERIFY(!backend.AeadDecrypt(key, nonce, enc.ciphertext, badTag, aad).has_value());
    
    QVERIFY(!backend.AeadDecrypt(key, nonce, enc.ciphertext, enc.tag, "bad").has_value());

    QByteArray badCipher = enc.ciphertext;
    if (!badCipher.isEmpty()) {
        badCipher[0] = static_cast<char>(badCipher[0] ^ 1);
        QVERIFY(!backend.AeadDecrypt(key, nonce, badCipher, enc.tag, aad).has_value());
    }

    AeadKey wrongKey; wrongKey.data.resize(32);
    memcpy(wrongKey.data.data(), key.data.data(), 32);
    wrongKey.data.data()[0] = static_cast<char>(wrongKey.data.data()[0] ^ 1);
    QVERIFY(!backend.AeadDecrypt(wrongKey, nonce, enc.ciphertext, enc.tag, aad).has_value());

    AeadNonce wrongNonce = nonce;
    wrongNonce.data[0] = static_cast<char>(wrongNonce.data[0] ^ 1);
    QVERIFY(!backend.AeadDecrypt(key, wrongNonce, enc.ciphertext, enc.tag, aad).has_value());

    AeadEncryptResult encEmpty = backend.AeadEncrypt(key, nonce, QByteArray(), aad);
    QVERIFY(encEmpty.success);
    auto decEmpty = backend.AeadDecrypt(key, nonce, encEmpty.ciphertext, encEmpty.tag, aad);
    QVERIFY(decEmpty.has_value());
    QVERIFY(decEmpty.value().isEmpty());
}

void TestBackend::testSecurityConstraints() {
    OpenSSLBackend backend;
    
    X25519PrivateKey badPriv; badPriv.data.resize(31);
    X25519PublicKey pub; pub.data.resize(32);
    QVERIFY(backend.X25519(badPriv, pub).size() == 0);
    
    AeadKey badKey; badKey.data.resize(31);
    AeadNonce nonce; nonce.data.resize(12);
    QVERIFY(!backend.AeadEncrypt(badKey, nonce, "test").success);
    
    AeadKey goodKey; goodKey.data.resize(32);
    AeadNonce badNonce; badNonce.data.resize(11);
    QVERIFY(!backend.AeadEncrypt(goodKey, badNonce, "test").success);
    
    AeadTag badTag; badTag.data.resize(15);
    QVERIFY(!backend.AeadDecrypt(goodKey, nonce, "test", badTag).has_value());
    
    QVERIFY(backend.HkdfSha256("ikm", "salt", "info", 0).isEmpty());
    QVERIFY(backend.HkdfSha256("ikm", "salt", "info", 256 * 32 + 1).isEmpty());
}
