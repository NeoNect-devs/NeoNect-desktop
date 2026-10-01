#pragma once

#include <QObject>

class TestSessionManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void testCreateSessionAndSend();
    void testReceiveInitialMessage();
    void testBidirectionalMessaging();
    void testRestartSimulation();
    void testWrongSessionCannotDecrypt();
    void testModifiedCiphertextRejected();
    void testModifiedEnvelopeRejected();
    void testStorageFailureDoesNotCorrupt();
    void testTransportFailureNoStateLoss();
    void testDuplicateDeliveryHandled();
};
