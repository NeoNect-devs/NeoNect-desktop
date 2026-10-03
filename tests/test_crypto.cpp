// tests/test_crypto.cpp
#include "test_crypto.h"
#include <QtTest>
#include "../src/crypto/cryptoservice.h"

void TestCrypto::testEncryptionDecryptionRoundtrip() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.setMasterKey(QByteArray(32, 1));

    QByteArray plain = "Hello, this is a secret E2EE message over NeoNect!";
    auto payload = crypto.encryptAesGcm(plain);

    QVERIFY(payload.success);
    QVERIFY(!payload.cipherWithTag.isEmpty());
    QCOMPARE(payload.nonce.size(), 12);
    QVERIFY(payload.cipherWithTag.size() > plain.size());

    QByteArray decrypted = crypto.decryptAesGcm(payload.cipherWithTag, payload.nonce);
    QCOMPARE(decrypted, plain);
}


void TestCrypto::testTamperedCiphertextRejection() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.setMasterKey(QByteArray(32, 1));

    QByteArray plain = "Top secret message content";
    auto payload = crypto.encryptAesGcm(plain);
    QVERIFY(payload.success);

    // Tamper with ciphertext (flip first byte)
    QByteArray tampered = payload.cipherWithTag;
    tampered[0] = static_cast<char>(tampered[0] ^ 0xFF);

    QByteArray decrypted = crypto.decryptAesGcm(tampered, payload.nonce);
    QVERIFY(decrypted.isEmpty());
}

void TestCrypto::testTamperedTagRejection() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.setMasterKey(QByteArray(32, 1));

    QByteArray plain = "Confidential authentication data";
    auto payload = crypto.encryptAesGcm(plain);
    QVERIFY(payload.success);

    // Tamper with authentication tag (last byte of payload)
    QByteArray tampered = payload.cipherWithTag;
    tampered[tampered.size() - 1] = static_cast<char>(tampered[tampered.size() - 1] ^ 0x01);

    QByteArray decrypted = crypto.decryptAesGcm(tampered, payload.nonce);
    QVERIFY(decrypted.isEmpty());
}

void TestCrypto::testSecureBufferCleansing() {
    NeoNect::Crypto::SecureBuffer buffer(32);
    std::memset(buffer.data(), 0xAA, 32);

    QCOMPARE(buffer.size(), 32);
    QCOMPARE(buffer.data()[0], static_cast<unsigned char>(0xAA));

    buffer.cleanse();
    QCOMPARE(buffer.data()[0], static_cast<unsigned char>(0x00));
}

void TestCrypto::testRandomBytesGeneration() {
    NeoNect::Crypto::CryptoService crypto;
    QByteArray bytes1 = crypto.generateRandomBytes(32);
    QByteArray bytes2 = crypto.generateRandomBytes(32);

    QCOMPARE(bytes1.size(), 32);
    QCOMPARE(bytes2.size(), 32);
    QVERIFY(bytes1 != bytes2);
}






