// tests/test_crypto.h
#pragma once
#include <QObject>

class TestCrypto : public QObject {
    Q_OBJECT

private slots:
    void testEncryptionDecryptionRoundtrip();
    void testKeyDerivation();
    void testTamperedCiphertextRejection();
    void testTamperedTagRejection();
    void testSecureBufferCleansing();
    void testRandomBytesGeneration();
    void testEnvelopeRoundTrip();
    void testEnvelopeTampering();
    void testEnvelopeNonceTampering();
    void testEnvelopeTruncation();
    void testEnvelopeInvalidVersion();
    void testEnvelopeEmptyPayload();
};
