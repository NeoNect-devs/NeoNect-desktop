// tests/test_storage.cpp
#include "test_storage.h"
#include <QtTest>
#include "../src/storage/settingsrepository.h"

void TestStorage::testProfileIsolation() {
    NeoNect::Storage::SettingsRepository repoAlice("unit_alice");
    NeoNect::Storage::SettingsRepository repoBob("unit_bob");

    repoAlice.setUsername("alice");
    repoAlice.setAuthToken("token_alice_123");

    repoBob.setUsername("bob");
    repoBob.setAuthToken("token_bob_456");

    QCOMPARE(repoAlice.username(), "alice");
    QCOMPARE(repoAlice.authToken(), "token_alice_123");

    QCOMPARE(repoBob.username(), "bob");
    QCOMPARE(repoBob.authToken(), "token_bob_456");

    repoAlice.clearSession();
    repoBob.clearSession();
}

void TestStorage::testGettersAndSetters() {
    NeoNect::Storage::SettingsRepository repo("unit_test_main");

    repo.setServerUrl("http://custom.node:9000");
    QCOMPARE(repo.serverUrl(), "http://custom.node:9000");

    repo.setDeviceId("dev-id-unit-1");
    QCOMPARE(repo.deviceId(), "dev-id-unit-1");

    repo.setPublicKey("BASE64_KEY_12345");
    QCOMPARE(repo.publicKey(), "BASE64_KEY_12345");

    QStringList friends = {"alice", "bob", "charlie"};
    repo.setFriends(friends);
    QCOMPARE(repo.friends(), friends);

    repo.clearSession();
}

void TestStorage::testClearSession() {
    NeoNect::Storage::SettingsRepository repo("unit_test_clear");

    repo.setDeviceId("dev-persistent");
    repo.setUsername("logged_in_user");
    repo.setAuthToken("session_token_xyz");

    repo.clearSession();

    QVERIFY(repo.authToken().isEmpty());
    QVERIFY(repo.username().isEmpty());
    QCOMPARE(repo.deviceId(), "dev-persistent");
}

void TestStorage::testBookmarks() {
    NeoNect::Storage::SettingsRepository repo("unit_test_bm");
    repo.setBookmarks({}); // clear

    QCOMPARE(repo.bookmarks().size(), 0);

    QVariantMap bm1;
    bm1["name"] = "My Server";
    bm1["serverUrl"] = "http://localhost:8080";
    bm1["username"] = "alice";
    bm1["password"] = "password123";

    repo.addBookmark(bm1);
    QVariantList list = repo.bookmarks();
    QCOMPARE(list.size(), 1);

    QVariantMap saved = list.at(0).toMap();
    QCOMPARE(saved.value("name").toString(), "My Server");
    QCOMPARE(saved.value("username").toString(), "alice");
    QVERIFY(!saved.value("id").toString().isEmpty());

    QString id = saved.value("id").toString();

    // Update bookmark
    saved["name"] = "Updated Server Name";
    repo.updateBookmark(saved);

    list = repo.bookmarks();
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.at(0).toMap().value("name").toString(), "Updated Server Name");

    // Remove bookmark
    repo.removeBookmark(id);
    QCOMPARE(repo.bookmarks().size(), 0);
}

