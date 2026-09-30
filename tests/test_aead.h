#pragma once

#include <QObject>
#include <QtTest>

class TestAEAD : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Basic encryption/decryption
    void testBasicEncryptDecrypt();
    void testEmptyPlaintext();
    void testLargePlaintext();
    void testEmptyAAD();
    void testNonEmptyBinaryAAD();
    void testUtf8Plaintext();

    // Authentication
    void testModifiedCiphertext();
    void testModifiedTag();
    void testModifiedAAD();
    void testWrongMessageKey();
    void testTruncatedCiphertext();
    void testMalformedTagLength();

    // Deterministic construction
    void testDeterministicOutput();
    void testChangingMessageKey();
    void testChangingAAD();
    void testChangingPlaintext();

    // Fixed vector
    void testFixedVector();

    // OpenSSL interoperability
    void testOpenSSLInteroperability();

private:
    class Private;
    Private* d;
};
