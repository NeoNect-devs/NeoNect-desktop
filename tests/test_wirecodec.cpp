#include "test_wirecodec.h"
#include <QtTest>
#include "../src/crypto/wire/WireCodec.h"

using namespace NeoNect::Crypto;
using namespace NeoNect::Crypto::Wire;

static X25519PublicKey makeKey(uint8_t firstByte) {
    X25519PublicKey pk;
    pk.data.fill(firstByte, 32);
    return pk;
}

static AeadTag makeTag() {
    AeadTag tag;
    tag.data.fill(0xAA, 16);
    return tag;
}

void TestWireCodec::testInitialEnvelopeBasic() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(1);
    env.senderEphemeralKey = makeKey(2);
    env.signedPreKeyId = 42;
    env.oneTimePreKeyId = 100;
    env.ciphertext = QByteArray("hello");
    env.tag = makeTag();

    QByteArray enc = WireCodec::encodeInitialEnvelope(env);
    QVERIFY(!enc.isEmpty());

    auto dec = WireCodec::decodeInitialEnvelope(enc);
    QVERIFY(dec.has_value());
    QCOMPARE(dec->version, 1);
    QCOMPARE(dec->senderIdentityKey.data, env.senderIdentityKey.data);
    QCOMPARE(dec->senderEphemeralKey.data, env.senderEphemeralKey.data);
    QCOMPARE(dec->signedPreKeyId, 42);
    QVERIFY(dec->oneTimePreKeyId.has_value());
    QCOMPARE(dec->oneTimePreKeyId.value(), 100);
    QCOMPARE(dec->ciphertext, env.ciphertext);
    QCOMPARE(dec->tag.data, env.tag.data);
}

void TestWireCodec::testRatchetEnvelopeBasic() {
    RatchetEnvelope env;
    env.header.dh = makeKey(3);
    env.header.pn = 123;
    env.header.n = 456;
    env.ciphertext = QByteArray("ratchet");
    env.tag = makeTag();

    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    QVERIFY(!enc.isEmpty());

    auto dec = WireCodec::decodeRatchetEnvelope(enc);
    QVERIFY(dec.has_value());
    QCOMPARE(dec->version, 1);
    QCOMPARE(dec->header.dh.data, env.header.dh.data);
    QCOMPARE(dec->header.pn, 123);
    QCOMPARE(dec->header.n, 456);
    QCOMPARE(dec->ciphertext, env.ciphertext);
    QCOMPARE(dec->tag.data, env.tag.data);
}

void TestWireCodec::testDeterministicEncoding() {
    RatchetEnvelope env;
    env.header.dh = makeKey(4);
    env.header.pn = 1;
    env.header.n = 2;
    env.ciphertext = QByteArray("det");
    env.tag = makeTag();

    QByteArray enc1 = WireCodec::encodeRatchetEnvelope(env);
    QByteArray enc2 = WireCodec::encodeRatchetEnvelope(env);
    QCOMPARE(enc1, enc2);
}

void TestWireCodec::testEmptyCiphertext() {
    RatchetEnvelope env;
    env.header.dh = makeKey(5);
    env.ciphertext = QByteArray();
    env.tag = makeTag();

    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    auto dec = WireCodec::decodeRatchetEnvelope(enc);
    QVERIFY(dec.has_value());
    QVERIFY(dec->ciphertext.isEmpty());
}

void TestWireCodec::testBinaryCiphertext() {
    RatchetEnvelope env;
    env.header.dh = makeKey(6);
    env.ciphertext = QByteArray::fromHex("00ff00ff00ff");
    env.tag = makeTag();

    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    auto dec = WireCodec::decodeRatchetEnvelope(enc);
    QVERIFY(dec.has_value());
    QCOMPARE(dec->ciphertext, QByteArray::fromHex("00ff00ff00ff"));
}

void TestWireCodec::testMaxAcceptedPayload() {
    RatchetEnvelope env;
    env.header.dh = makeKey(7);
    env.tag = makeTag();
    env.ciphertext.fill('A', WireCodec::MAX_ENVELOPE_SIZE + 10);
    
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    QVERIFY(enc.isEmpty());
}

void TestWireCodec::testInvalidIdentityKeyLength() {
    InitialEnvelope ienv;
    ienv.senderIdentityKey.data.fill(1, 31); // Not 32
    ienv.senderEphemeralKey = makeKey(2);
    ienv.tag = makeTag();
    QByteArray ienc = WireCodec::encodeInitialEnvelope(ienv);
    QVERIFY(ienc.isEmpty());
}

void TestWireCodec::testInvalidEphemeralKeyLength() {
    InitialEnvelope ienv;
    ienv.senderIdentityKey = makeKey(1);
    ienv.senderEphemeralKey.data.fill(1, 33);
    ienv.tag = makeTag();
    QByteArray ienc = WireCodec::encodeInitialEnvelope(ienv);
    QVERIFY(ienc.isEmpty());
}

void TestWireCodec::testInvalidRatchetKeyLength() {
    RatchetEnvelope env;
    env.header.dh.data.fill(1, 31);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    QVERIFY(enc.isEmpty());
}

void TestWireCodec::testInvalidTagLength() {
    RatchetEnvelope env;
    env.header.dh = makeKey(1);
    env.tag.data.fill(0xAA, 15);
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    QVERIFY(enc.isEmpty());
    
    InitialEnvelope ienv;
    ienv.senderIdentityKey = makeKey(1);
    ienv.senderEphemeralKey = makeKey(2);
    ienv.tag.data.fill(0xAA, 17);
    QByteArray ienc = WireCodec::encodeInitialEnvelope(ienv);
    QVERIFY(ienc.isEmpty());
}

void TestWireCodec::testUnsupportedVersionRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(8);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    enc[0] = 2;
    QVERIFY(!WireCodec::decodeRatchetEnvelope(enc).has_value());
}

void TestWireCodec::testUnknownEnvelopeTypeRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(8);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    enc[1] = 0x03; // Not X3DH or Ratchet
    QVERIFY(!WireCodec::decodeRatchetEnvelope(enc).has_value());
    QVERIFY(!WireCodec::decodeInitialEnvelope(enc).has_value());
}

void TestWireCodec::testInvalidOpkFlagRejected() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(1);
    env.senderEphemeralKey = makeKey(2);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeInitialEnvelope(env);
    
    // Modify OPK flag (at index 1+1+33+33+4 = 72)
    enc[72] = 2; 
    QVERIFY(!WireCodec::decodeInitialEnvelope(enc).has_value());
}

void TestWireCodec::testTruncatedInputRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(9);
    env.tag = makeTag();
    env.ciphertext = QByteArray("length_test");
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    
    QByteArray trunc = enc.left(enc.size() - 5);
    QVERIFY(!WireCodec::decodeRatchetEnvelope(trunc).has_value());
}

void TestWireCodec::testDeclaredCiphertextLengthLargerRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(9);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    
    // length is at 1+1+33+4+4 = 43
    // increase length by 1
    enc[46] = (enc[46] + 1); 
    QVERIFY(!WireCodec::decodeRatchetEnvelope(enc).has_value());
}

void TestWireCodec::testDeclaredCiphertextLengthOverflowRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(9);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    
    // Make length very large (overflow)
    enc[43] = (char)0xFF;
    enc[44] = (char)0xFF;
    enc[45] = (char)0xFF;
    enc[46] = (char)0xFF;
    QVERIFY(!WireCodec::decodeRatchetEnvelope(enc).has_value());
}

void TestWireCodec::testOversizedEnvelopeRejected() {
    QByteArray huge(WireCodec::MAX_ENVELOPE_SIZE + 1, 'A');
    QVERIFY(!WireCodec::decodeRatchetEnvelope(huge).has_value());
    QVERIFY(!WireCodec::decodeInitialEnvelope(huge).has_value());
}

void TestWireCodec::testValidEnvelopeWithTrailingGarbageRejected() {
    RatchetEnvelope env;
    env.header.dh = makeKey(9);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    
    QByteArray garbage = enc + QByteArray("extra");
    QVERIFY(!WireCodec::decodeRatchetEnvelope(garbage).has_value());
}

void TestWireCodec::testNIsZero() {
    RatchetEnvelope env;
    env.header.dh = makeKey(10);
    env.header.n = 0;
    env.tag = makeTag();
    auto dec = WireCodec::decodeRatchetEnvelope(WireCodec::encodeRatchetEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->header.n, 0);
}

void TestWireCodec::testNIsMax() {
    RatchetEnvelope env;
    env.header.dh = makeKey(10);
    env.header.n = 0xFFFFFFFF;
    env.tag = makeTag();
    auto dec = WireCodec::decodeRatchetEnvelope(WireCodec::encodeRatchetEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->header.n, 0xFFFFFFFF);
}

void TestWireCodec::testPnIsZero() {
    RatchetEnvelope env;
    env.header.dh = makeKey(10);
    env.header.pn = 0;
    env.tag = makeTag();
    auto dec = WireCodec::decodeRatchetEnvelope(WireCodec::encodeRatchetEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->header.pn, 0);
}

void TestWireCodec::testPnIsMax() {
    RatchetEnvelope env;
    env.header.dh = makeKey(10);
    env.header.pn = 0xFFFFFFFF;
    env.tag = makeTag();
    auto dec = WireCodec::decodeRatchetEnvelope(WireCodec::encodeRatchetEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->header.pn, 0xFFFFFFFF);
}

void TestWireCodec::testSameObjectEncodedTwiceIdentical() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(1);
    env.senderEphemeralKey = makeKey(2);
    env.tag = makeTag();
    QByteArray enc1 = WireCodec::encodeInitialEnvelope(env);
    QByteArray enc2 = WireCodec::encodeInitialEnvelope(env);
    QCOMPARE(enc1, enc2);
}

void TestWireCodec::testAlternativeEncodingsRejected() {
    // There are no optional fields in RatchetEnvelope.
    // For X3DH, OPK-present = 0 or 1. Any other is alternative/malformed.
    // We already tested OPK flag rejection.
    // Also, another X25519 encoding format is not permitted since we expect 0x05 prefix.
    RatchetEnvelope env;
    env.header.dh = makeKey(10);
    env.tag = makeTag();
    QByteArray enc = WireCodec::encodeRatchetEnvelope(env);
    enc[2] = 0x04; // Change prefix from 0x05 to 0x04
    QVERIFY(!WireCodec::decodeRatchetEnvelope(enc).has_value());
}

void TestWireCodec::testCanonicalRatchetHeaderStable() {
    RatchetHeader header1;
    header1.dh = makeKey(11);
    header1.pn = 1;
    header1.n = 2;
    RatchetHeader header2 = header1;
    QCOMPARE(WireCodec::getRatchetMessageAAD(header1), WireCodec::getRatchetMessageAAD(header2));
}

void TestWireCodec::testChangingDhChangesAad() {
    RatchetHeader header1;
    header1.dh = makeKey(11);
    RatchetHeader header2;
    header2.dh = makeKey(12);
    QVERIFY(WireCodec::getRatchetMessageAAD(header1) != WireCodec::getRatchetMessageAAD(header2));
}

void TestWireCodec::testChangingPnChangesAad() {
    RatchetHeader header1;
    header1.dh = makeKey(11);
    header1.pn = 1;
    RatchetHeader header2 = header1;
    header2.pn = 2;
    QVERIFY(WireCodec::getRatchetMessageAAD(header1) != WireCodec::getRatchetMessageAAD(header2));
}

void TestWireCodec::testChangingNChangesAad() {
    RatchetHeader header1;
    header1.dh = makeKey(11);
    header1.n = 1;
    RatchetHeader header2 = header1;
    header2.n = 2;
    QVERIFY(WireCodec::getRatchetMessageAAD(header1) != WireCodec::getRatchetMessageAAD(header2));
}

void TestWireCodec::testCiphertextTagDoNotAlterAad() {
    RatchetEnvelope env1;
    env1.header.dh = makeKey(11);
    env1.ciphertext = QByteArray("1");
    env1.tag = makeTag();
    
    RatchetEnvelope env2 = env1;
    env2.ciphertext = QByteArray("2");
    env2.tag.data[0] = 0xBB;
    
    QCOMPARE(WireCodec::getRatchetMessageAAD(env1.header), WireCodec::getRatchetMessageAAD(env2.header));
}

void TestWireCodec::testOpkAbsent() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(12);
    env.senderEphemeralKey = makeKey(13);
    env.tag = makeTag();
    env.signedPreKeyId = 999;
    env.oneTimePreKeyId = std::nullopt;
    
    auto dec = WireCodec::decodeInitialEnvelope(WireCodec::encodeInitialEnvelope(env));
    QVERIFY(dec.has_value());
    QVERIFY(!dec->oneTimePreKeyId.has_value());
}

void TestWireCodec::testOpkPresent() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(12);
    env.senderEphemeralKey = makeKey(13);
    env.tag = makeTag();
    env.signedPreKeyId = 999;
    env.oneTimePreKeyId = 888;
    
    auto dec = WireCodec::decodeInitialEnvelope(WireCodec::encodeInitialEnvelope(env));
    QVERIFY(dec.has_value());
    QVERIFY(dec->oneTimePreKeyId.has_value());
    QCOMPARE(dec->oneTimePreKeyId.value(), 888);
}

void TestWireCodec::testSpkIdPreserved() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(12);
    env.senderEphemeralKey = makeKey(13);
    env.tag = makeTag();
    env.signedPreKeyId = 0x12345678;
    
    auto dec = WireCodec::decodeInitialEnvelope(WireCodec::encodeInitialEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->signedPreKeyId, 0x12345678);
}

void TestWireCodec::testOpkIdPreserved() {
    InitialEnvelope env;
    env.senderIdentityKey = makeKey(12);
    env.senderEphemeralKey = makeKey(13);
    env.tag = makeTag();
    env.oneTimePreKeyId = 0x87654321;
    
    auto dec = WireCodec::decodeInitialEnvelope(WireCodec::encodeInitialEnvelope(env));
    QVERIFY(dec.has_value());
    QCOMPARE(dec->oneTimePreKeyId.value(), 0x87654321);
}
