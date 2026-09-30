#pragma once
#include <QObject>

class TestWireCodec : public QObject {
    Q_OBJECT
private slots:
    // Basic encoding
    void testInitialEnvelopeBasic();
    void testRatchetEnvelopeBasic();
    void testDeterministicEncoding();
    void testEmptyCiphertext();
    void testBinaryCiphertext();
    void testMaxAcceptedPayload();

    // Key fields
    void testInvalidIdentityKeyLength();
    void testInvalidEphemeralKeyLength();
    void testInvalidRatchetKeyLength();
    void testInvalidTagLength();

    // Version/type
    void testUnsupportedVersionRejected();
    void testUnknownEnvelopeTypeRejected();
    void testInvalidOpkFlagRejected();

    // Length handling
    void testTruncatedInputRejected();
    void testDeclaredCiphertextLengthLargerRejected();
    void testDeclaredCiphertextLengthOverflowRejected();
    void testOversizedEnvelopeRejected();
    void testValidEnvelopeWithTrailingGarbageRejected();

    // Counter handling
    void testNIsZero();
    void testNIsMax();
    void testPnIsZero();
    void testPnIsMax();

    // Canonicality
    void testSameObjectEncodedTwiceIdentical();
    void testAlternativeEncodingsRejected();

    // AAD
    void testCanonicalRatchetHeaderStable();
    void testChangingDhChangesAad();
    void testChangingPnChangesAad();
    void testChangingNChangesAad();
    void testCiphertextTagDoNotAlterAad();

    // X3DH metadata
    void testOpkAbsent();
    void testOpkPresent();
    void testSpkIdPreserved();
    void testOpkIdPreserved();
};
