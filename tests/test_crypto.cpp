// tests/test_crypto.cpp
#include "test_crypto.h"
#include <QtTest>
#include "../src/crypto/cryptoservice.h"

void TestCrypto::testEncryptionDecryptionRoundtrip() {
    Avila::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("test_secure_passphrase_123");

    QByteArray plain = "Hello, this is a secret E2EE message over Avila!";
    auto payload = crypto.encryptAesGcm(plain);

    QVERIFY(payload.success);
    QVERIFY(!payload.cipherWithTag.isEmpty());
    QCOMPARE(payload.nonce.size(), 12);
    QVERIFY(payload.cipherWithTag.size() > plain.size());

    QByteArray decrypted = crypto.decryptAesGcm(payload.cipherWithTag, payload.nonce);
    QCOMPARE(decrypted, plain);
}

void TestCrypto::testKeyDerivation() {
    Avila::Crypto::CryptoService crypto1;
    Avila::Crypto::CryptoService crypto2;

    QVERIFY(crypto1.deriveKeyFromPassphrase("my_secret_vault_pass"));
    QVERIFY(crypto2.deriveKeyFromPassphrase("my_secret_vault_pass"));

    QCOMPARE(crypto1.getMasterKey(), crypto2.getMasterKey());

    Avila::Crypto::CryptoService crypto3;
    crypto3.deriveKeyFromPassphrase("different_passphrase");
    QVERIFY(crypto1.getMasterKey() != crypto3.getMasterKey());
}

void TestCrypto::testTamperedCiphertextRejection() {
    Avila::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("test_passphrase");

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
    Avila::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("test_passphrase");

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
    Avila::Crypto::SecureBuffer buffer(32);
    std::memset(buffer.data(), 0xAA, 32);

    QCOMPARE(buffer.size(), 32);
    QCOMPARE(buffer.data()[0], static_cast<unsigned char>(0xAA));

    buffer.cleanse();
    QCOMPARE(buffer.data()[0], static_cast<unsigned char>(0x00));
}

void TestCrypto::testRandomBytesGeneration() {
    Avila::Crypto::CryptoService crypto;
    QByteArray bytes1 = crypto.generateRandomBytes(32);
    QByteArray bytes2 = crypto.generateRandomBytes(32);

    QCOMPARE(bytes1.size(), 32);
    QCOMPARE(bytes2.size(), 32);
    QVERIFY(bytes1 != bytes2);
}
