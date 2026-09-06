// tests/test_crypto.cpp
#include "test_crypto.h"
#include <QtTest>
#include "../src/crypto/cryptoservice.h"

void TestCrypto::testEncryptionDecryptionRoundtrip() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("test_secure_passphrase_123");

    QByteArray plain = "Hello, this is a secret E2EE message over NeoNect!";
    auto payload = crypto.encryptAesGcm(plain);

    QVERIFY(payload.success);
    QVERIFY(!payload.cipherWithTag.isEmpty());
    QCOMPARE(payload.nonce.size(), 12);
    QVERIFY(payload.cipherWithTag.size() > plain.size());

    QByteArray decrypted = crypto.decryptAesGcm(payload.cipherWithTag, payload.nonce);
    QCOMPARE(decrypted, plain);
}

void TestCrypto::testKeyDerivation() {
    NeoNect::Crypto::CryptoService crypto1;
    NeoNect::Crypto::CryptoService crypto2;

    QVERIFY(crypto1.deriveKeyFromPassphrase("my_secret_vault_pass"));
    QVERIFY(crypto2.deriveKeyFromPassphrase("my_secret_vault_pass"));

    QCOMPARE(crypto1.getMasterKey(), crypto2.getMasterKey());

    NeoNect::Crypto::CryptoService crypto3;
    crypto3.deriveKeyFromPassphrase("different_passphrase");
    QVERIFY(crypto1.getMasterKey() != crypto3.getMasterKey());
}

void TestCrypto::testTamperedCiphertextRejection() {
    NeoNect::Crypto::CryptoService crypto;
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
    NeoNect::Crypto::CryptoService crypto;
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

void TestCrypto::testEnvelopeRoundTrip() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    QByteArray plain = "Opaque backend transport test";
    auto payload = crypto.encryptAesGcm(plain);
    QVERIFY(payload.success);
    QVERIFY(!payload.envelope.isEmpty());

    // Simulate backend relay saving and returning just the envelope over Base64
    QString b64 = QString::fromLatin1(payload.envelope.toBase64());
    QByteArray received = QByteArray::fromBase64(b64.toLatin1());

    QByteArray decrypted = crypto.decryptAesGcmEnvelope(received);
    QCOMPARE(decrypted, plain);
}

void TestCrypto::testEnvelopeTampering() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    auto payload = crypto.encryptAesGcm("Test message");
    QByteArray env = payload.envelope;

    // Tamper ciphertext part (which is after version(1) + length(1) + nonce(12))
    env[15] = static_cast<char>(env[15] ^ 0xFF);
    QByteArray decrypted = crypto.decryptAesGcmEnvelope(env);
    QVERIFY(decrypted.isEmpty());
}

void TestCrypto::testEnvelopeNonceTampering() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    auto payload = crypto.encryptAesGcm("Test message");
    QByteArray env = payload.envelope;

    // Tamper nonce part (offset 2)
    env[2] = static_cast<char>(env[2] ^ 0xFF);
    QByteArray decrypted = crypto.decryptAesGcmEnvelope(env);
    QVERIFY(decrypted.isEmpty());
}

void TestCrypto::testEnvelopeTruncation() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    auto payload = crypto.encryptAesGcm("Test message");
    QByteArray env = payload.envelope;

    QByteArray truncated1 = env.left(1); // Only version
    QVERIFY(crypto.decryptAesGcmEnvelope(truncated1).isEmpty());

    QByteArray truncated2 = env.left(14); // Missing tag and ciphertext
    QVERIFY(crypto.decryptAesGcmEnvelope(truncated2).isEmpty());

    QByteArray truncated3 = env.left(env.size() - 5); // Cut off tag
    QVERIFY(crypto.decryptAesGcmEnvelope(truncated3).isEmpty());
}

void TestCrypto::testEnvelopeInvalidVersion() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    auto payload = crypto.encryptAesGcm("Test message");
    QByteArray env = payload.envelope;

    env[0] = static_cast<char>(0x02); // Change version from 1 to 2
    QVERIFY(crypto.decryptAesGcmEnvelope(env).isEmpty());
}

void TestCrypto::testEnvelopeEmptyPayload() {
    NeoNect::Crypto::CryptoService crypto;
    crypto.deriveKeyFromPassphrase("envelope_pass");
    QVERIFY(crypto.decryptAesGcmEnvelope(QByteArray()).isEmpty());
}
