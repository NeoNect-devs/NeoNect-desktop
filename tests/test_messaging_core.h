#pragma once
#include <QObject>

class TestMessagingCore : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void testCreateMessage();
    void testValidStateTransitions();
    void testInvalidStateTransitionRejection();
    void testSuccessfulSendPipeline();
    void testTransportFailureHandling();
    void testDuplicateMessageRejection();
    void testReceiveEncryptedMessage();
    void testConversationOrdering();
    void testRestartPersistence();
    void testDatabaseFailureAtomicity();
};
