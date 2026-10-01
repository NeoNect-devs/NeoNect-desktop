#pragma once
#include <QObject>

class TestOfflineQueue : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void testEnqueueMessage();
    void testDuplicateQueuePrevention();
    void testPersistAfterRestart();
    void testSendSuccess();
    void testAckTransition();
    void testDuplicateAck();
    void testTransportFailureRetry();
    void testRetryLimit();
    void testCrashRecovery();
    void testAtomicFailure();
};
