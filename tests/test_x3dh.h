#pragma once
#include <QObject>

class TestX3DH : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    
    // Deterministic test
    void testDeterministicX3DH();
    
    // Specific requirements
    void testAliceAndBobWithOPK();
    void testAliceAndBobWithoutOPK();
    void testInvalidSPKSignature();
    void testModifiedSPKSignatureAborts();
    void testModifiedIdentityKeyCausesMismatch();
    void testModifiedOPKCausesMismatch();
    void testAliceAndBobProduceIdenticalAD();
    void testSKIsExactly32Bytes();
    void testADEncoding();
    void testOPKNotConsumed();
    void testInvalidSignatureDoesNotConsumeOPK();
    void testFailedDHDoesNotConsumeOPK();
    void testAllZeroX25519Rejected();
    void testDHOrdering();
    void testDH4IncludedOnlyWhenOPKExists();
};
