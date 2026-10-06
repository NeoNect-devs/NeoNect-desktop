#pragma once
#include <QObject>

class TestSecureStorage : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testMasterKey_FirstInitCreatesKey();
    void testMasterKey_SecondLoadSameKey();
    void testMasterKey_WrongSecretStoreFails();
    void testMasterKey_NoPlaintextFallback();

    void testPlatformSecretStore_WriteRead();
    void testPlatformSecretStore_Overwrite();
    void testPlatformSecretStore_Delete();

    void testDatabase_EncryptedDbOpensWithKey();
    void testDatabase_WrongMasterKeyFails();
    void testDatabase_ExistingDbNotReplacedOnAuthFailure();
    void testDatabase_SchemaDeterministic();
    void testDatabase_SchemaVersionStored();

    void testIdentity_RoundTrip();
    void testIdentity_Exact32ByteValidation();
    void testIdentity_PrivateKeyNotLogged();

    void testSignedPreKey_RoundTrip();
    void testSignedPreKey_ExactLengths();
    void testSignedPreKey_ActiveKeySemantics();

    void testOneTimePreKeys_BatchInsertion();
    void testOneTimePreKeys_AvailableListing();
    void testOneTimePreKeys_AtomicConsumeSucceedsOnce();
    void testOneTimePreKeys_SecondConsumeFails();
    void testOneTimePreKeys_ConsumedStatePersists();

    void testSessions_RoundTrip();
    void testSessions_DHsPreserved();
    void testSessions_RKPreserved();
    void testSessions_CKsPreserved();
    void testSessions_NsPreserved();

    void testSkippedKeys_RoundTrip();
    void testSkippedKeys_CompositeKeyPreserved();
    void testSkippedKeys_DuplicateRejected();

    void testTransactions_RollbackSession();
    void testTransactions_RollbackOPK();
    void testTransactions_RollbackSkippedKey();

    void testCrashRecovery_WriteCloseReopenVerify();
    void testIsolation_DifferentProfilesDifferentKeys();
    void testIsolation_ProfileCannotOpenOtherProfileDb();
    void testLifecycle_CloseAndWipeDatabase();

    // Phase 3.2.1 tests
    void testMasterKey_LegacyMigration();
    void testMasterKey_ScopedCreation();
    void testMasterKey_CrossServerIsolation();
    void testMasterKey_RestartPersistence();
    void testMasterKey_MigrationIdempotency();
    void testMasterKey_MigrationReadBackFailureSafety();
};
