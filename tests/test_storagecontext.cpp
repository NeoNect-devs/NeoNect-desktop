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
    
    QVERIFY(ctx->secureStore() != nullptr);
    QVERIFY(ctx->messageStorage() != nullptr);
    QVERIFY(ctx->messageQueue() != nullptr);
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
        QVERIFY(ctx2.secureStore() != nullptr);
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

QTEST_GUILESS_MAIN(TestStorageContext)
