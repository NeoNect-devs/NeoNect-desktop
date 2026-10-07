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
    void testLegacySchemaMigration();
    void testMigrationIdempotency();
    void testFreshSchemaColumns();
    void testPartialSchemaMigration();
    void testMigrationFailureHandling();

    // Phase 3.5.2 tests
    void testMediaMetadataRestartPersistence();
    void testImageMetadataRestartPersistence();
    void testLegacyRowCompatibility();
    void testCrudIntegrity();
};
