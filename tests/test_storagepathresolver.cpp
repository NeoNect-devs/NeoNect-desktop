#include "test_storagepathresolver.h"
#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include "../src/storage/StoragePathResolver.h"

void TestStoragePathResolver::testCanonicalServerUrl() {
    // Same canonical URL
    QCOMPARE(NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://example.com"),
             NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://EXAMPLE.COM/"));
             
    // Scheme defaults to https if missing
    QCOMPARE(NeoNect::Storage::StoragePathResolver::canonicalServerUrl("example.com"),
             NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://example.com"));
             
    // Default ports are removed
    QCOMPARE(NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://example.com:443"),
             NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://example.com"));
    QCOMPARE(NeoNect::Storage::StoragePathResolver::canonicalServerUrl("http://example.com:80"),
             NeoNect::Storage::StoragePathResolver::canonicalServerUrl("http://example.com"));
             
    // Path and query preserved, fragment removed
    QCOMPARE(NeoNect::Storage::StoragePathResolver::canonicalServerUrl("https://example.com/api?user=1#frag"),
             QString("https://example.com/api?user=1"));
}

void TestStoragePathResolver::testServerKey() {
    QString key1 = NeoNect::Storage::StoragePathResolver::serverKey("https://example.com");
    QString key2 = NeoNect::Storage::StoragePathResolver::serverKey("example.com/");
    QCOMPARE(key1, key2);

    QString key3 = NeoNect::Storage::StoragePathResolver::serverKey("https://other.com");
    QVERIFY(key1 != key3);
    
    // No raw URL in key
    QVERIFY(!key1.contains("example.com"));
}

void TestStoragePathResolver::testAccountKey() {
    QString key1 = NeoNect::Storage::StoragePathResolver::accountKey("Alice");
    QString key2 = NeoNect::Storage::StoragePathResolver::accountKey("alice ");
    QCOMPARE(key1, key2); // Canonicalization implies lowercasing and trimming

    QString key3 = NeoNect::Storage::StoragePathResolver::accountKey("bob");
    QVERIFY(key1 != key3);
    
    // No raw username in key
    QVERIFY(!key1.contains("alice"));
}

void TestStoragePathResolver::testChatKey() {
    QString key1 = NeoNect::Storage::StoragePathResolver::chatKey("dms:alice");
    QString key2 = NeoNect::Storage::StoragePathResolver::chatKey("dms:alice ");
    QCOMPARE(key1, key2);

    QString key3 = NeoNect::Storage::StoragePathResolver::chatKey("dms:bob");
    QVERIFY(key1 != key3);
    
    // No raw ID in key
    QVERIFY(!key1.contains("alice"));
}

void TestStoragePathResolver::testPathSafetyAndHierarchy() {
    QString serverUrl = "https://example.com";
    QString username = "Alice";
    
    QString appRoot = NeoNect::Storage::StoragePathResolver::appRoot();
    QString msgDb = NeoNect::Storage::StoragePathResolver::messageDbPath(serverUrl, username);
    QString e2eeDb = NeoNect::Storage::StoragePathResolver::e2eeDbPath(serverUrl, username);
    QString chatDir = NeoNect::Storage::StoragePathResolver::chatDirectory(serverUrl, username, "dms:bob");
    
    // Logical root delegated to AppDataLocation
    QString expectedRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(appRoot == expectedRoot || appRoot == QDir(expectedRoot).absolutePath());
    
    // Ensure subdirectories correctly contain root
    QVERIFY(msgDb.startsWith(appRoot));
    QVERIFY(e2eeDb.startsWith(appRoot));
    QVERIFY(chatDir.startsWith(appRoot));
    
    // Message DB is user-scoped
    QString accountRoot = NeoNect::Storage::StoragePathResolver::accountRoot(serverUrl, username);
    QCOMPARE(msgDb, accountRoot + "/messages.db");
    
    // Chat directory is under user root conceptually
    QVERIFY(chatDir.startsWith(accountRoot + "/chats/"));
    
    // No path traversal from external input
    QString maliciousUser = "../../../etc/passwd";
    QString maliciousKey = NeoNect::Storage::StoragePathResolver::accountKey(maliciousUser);
    QString maliciousPath = NeoNect::Storage::StoragePathResolver::accountRoot(serverUrl, maliciousUser);
    QVERIFY(!maliciousPath.contains(".."));
    QVERIFY(maliciousPath.contains(maliciousKey)); // Path should only contain the hash
}

void TestStoragePathResolver::testDeterministicOutput() {
    QString out1 = NeoNect::Storage::StoragePathResolver::messageDbPath("server", "user");
    QString out2 = NeoNect::Storage::StoragePathResolver::messageDbPath("server", "user");
    QCOMPARE(out1, out2);
}

QTEST_MAIN(TestStoragePathResolver)

