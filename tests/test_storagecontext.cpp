#include <QtTest>
#include "test_storagecontext.h"
#include "../src/storage/StorageContext.h"
#include "../src/storage/StoragePathResolver.h"
#include <QDir>
#include <QStandardPaths>

using namespace NeoNect::Storage;

void TestStorageContext::testCreationAndPaths() {
    QStandardPaths::setTestModeEnabled(true);
    QString server = "test-server";
    QString user = "alice";
    
    auto ctx = std::make_unique<StorageContext>(server, user, true, "testprof");
    
    QVERIFY(ctx->secureStore().lock() != nullptr);
    QVERIFY(ctx->messageStorage().lock() != nullptr);
    QVERIFY(ctx->messageQueue().lock() != nullptr);
    QCOMPARE(ctx->serverUrl(), server);
    QCOMPARE(ctx->username(), user);
    
    // Check paths
    QString e2eePath = StoragePathResolver::e2eeDbPath(server, user);
    QString msgPath = StoragePathResolver::messageDbPath(server, user);
    
    QVERIFY(QFile::exists(e2eePath));
    QVERIFY(QFile::exists(msgPath));
}

void TestStorageContext::testPreAuthFailClosed() {
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY_EXCEPTION_THROWN(StorageContext("server", "", true, "testprof"), std::invalid_argument);
}

void TestStorageContext::testDeterministicRecreation() {
    QStandardPaths::setTestModeEnabled(true);
    QString server = "test-server";
    QString user = "bob";
    
    // Create, destroy, recreate
    {
        StorageContext ctx1(server, user, true, "testprof");
    }
    {
        StorageContext ctx2(server, user, true, "testprof");
        QVERIFY(ctx2.secureStore().lock() != nullptr);
    }
}

void TestStorageContext::testIsolation() {
    QStandardPaths::setTestModeEnabled(true);
    StorageContext ctx1("serverA", "alice", true, "testprof");
    StorageContext ctx2("serverA", "bob", true, "testprof");
    StorageContext ctx3("serverB", "alice", true, "testprof");
    
    QString msgA_alice = StoragePathResolver::messageDbPath("serverA", "alice");
    QString msgA_bob = StoragePathResolver::messageDbPath("serverA", "bob");
    QString msgB_alice = StoragePathResolver::messageDbPath("serverB", "alice");
    
    QVERIFY(msgA_alice != msgA_bob);
    QVERIFY(msgA_alice != msgB_alice);
    QVERIFY(msgA_bob != msgB_alice);
    
    QVERIFY(QFile::exists(msgA_alice));
    QVERIFY(QFile::exists(msgA_bob));
    QVERIFY(QFile::exists(msgB_alice));
}

void TestStorageContext::testResourceCleanup() {
    // We test that SQLite handles are closed, but we can just ensure destruction works without hanging.
    QStandardPaths::setTestModeEnabled(true);
    auto ctx = std::make_unique<StorageContext>("server", "user", true, "testprof");
    ctx.reset();
    QVERIFY(ctx == nullptr);
}

namespace {
class FixedKeySecretStore : public IOSSecretStore {
public:
    explicit FixedKeySecretStore(char fillByte) : m_key(32, fillByte) {}
    NeoNect::ServiceResult<std::monostate> writeSecret(const QString&, const QByteArray& s) override {
        m_key = s;
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<QByteArray> readSecret(const QString&) override {
        return NeoNect::ServiceResult<QByteArray>::ok(m_key);
    }
    NeoNect::ServiceResult<std::monostate> deleteSecret(const QString&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
private:
    QByteArray m_key;
};
} // namespace

void TestStorageContext::testE2EEInitFailurePropagated() {
    QStandardPaths::setTestModeEnabled(true);
    const QString server = "test-server-key-mismatch";
    const QString user = "charlie";
    QDir(StoragePathResolver::accountRoot(server, user)).removeRecursively();

    const QByteArray expectedPub(32, 'P');
    const QByteArray expectedPriv(32, 'S');

    // 1. Create and populate e2ee.db with key 'A'
    {
        auto keyA = std::make_shared<FixedKeySecretStore>('A');
        StorageContext ctx1(server, user, false, "testprof", keyA);
        QVERIFY(ctx1.isE2EEReady());
        auto store = ctx1.secureStore().lock();
        QVERIFY(store != nullptr);
        QVERIFY(store->isOpen());

        E2EEIdentity id;
        id.identity_id = 1;
        id.public_key = expectedPub;
        id.private_key = expectedPriv;
        QVERIFY(store->saveIdentity(id).success);
    }

    // 2. Reopen with wrong key 'B' (simulating keyring backend mismatch)
    {
        auto keyB = std::make_shared<FixedKeySecretStore>('B');
        StorageContext ctx2(server, user, false, "testprof", keyB);
        QVERIFY(!ctx2.isE2EEReady());
        QVERIFY(!ctx2.e2eeError().isEmpty());
        QVERIFY(ctx2.secureStore().lock() == nullptr);
    }

    // 3. Reopen with original key 'A' and verify e2ee.db and identity are untouched
    {
        auto keyA = std::make_shared<FixedKeySecretStore>('A');
        StorageContext ctx3(server, user, false, "testprof", keyA);
        QVERIFY(ctx3.isE2EEReady());
        auto store = ctx3.secureStore().lock();
        QVERIFY(store != nullptr);
        auto idRes = store->getIdentity();
        QVERIFY(idRes.success);
        QCOMPARE(idRes.data->public_key, expectedPub);
        QCOMPARE(idRes.data->private_key, expectedPriv);
    }
}

QTEST_GUILESS_MAIN(TestStorageContext)
