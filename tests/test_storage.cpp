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

void TestStorage::testOpenConversationsPersistence() {
    NeoNect::Storage::SettingsRepository repo("unit_test_open_convs");
    repo.setOpenConversations({});

    QCOMPARE(repo.openConversations().size(), 0);

    QVariantList convs;
    QVariantMap c1;
    c1["name"] = "alice";
    c1["lastActivity"] = 1000;
    QVariantMap c2;
    c2["name"] = "bob";
    c2["lastActivity"] = 2000;
    convs.append(c1);
    convs.append(c2);

    repo.setOpenConversations(convs);

    // Verify persistence across repository instantiation with same profile
    NeoNect::Storage::SettingsRepository repoReopened("unit_test_open_convs");
    QVariantList fetched = repoReopened.openConversations();
    QCOMPARE(fetched.size(), 2);
    QCOMPARE(fetched.at(0).toMap().value("name").toString(), "alice");
    QCOMPARE(fetched.at(0).toMap().value("lastActivity").toLongLong(), 1000LL);
    QCOMPARE(fetched.at(1).toMap().value("name").toString(), "bob");
    QCOMPARE(fetched.at(1).toMap().value("lastActivity").toLongLong(), 2000LL);

    repo.setOpenConversations({});
    QCOMPARE(repoReopened.openConversations().size(), 0);
}

void TestStorage::testAccountScopedConversations() {
    NeoNect::Storage::SettingsRepository repo("test_account_scope");

    // Alice logs in and adds a conversation
    repo.setUsername("alice");
    repo.setAuthToken("token_alice");
    QVariantList aliceConvs;
    QVariantMap c1;
    c1["name"] = "charlie";
    aliceConvs.append(c1);
    repo.setOpenConversations(aliceConvs);
    QCOMPARE(repo.openConversations().size(), 1);
    QCOMPARE(repo.openConversations().at(0).toMap().value("name").toString(), "charlie");

    // Alice logs out
    repo.clearSession();

    // Bob logs in - should have 0 open conversations initially
    repo.setUsername("bob");
    repo.setAuthToken("token_bob");
    QCOMPARE(repo.openConversations().size(), 0);

    // Bob adds his own conversation
    QVariantList bobConvs;
    QVariantMap c2;
    c2["name"] = "david";
    bobConvs.append(c2);
    repo.setOpenConversations(bobConvs);
    QCOMPARE(repo.openConversations().size(), 1);
    QCOMPARE(repo.openConversations().at(0).toMap().value("name").toString(), "david");

    // Bob logs out and Alice logs back in
    repo.clearSession();
    repo.setUsername("alice");
    QCOMPARE(repo.openConversations().size(), 1);
    QCOMPARE(repo.openConversations().at(0).toMap().value("name").toString(), "charlie");

    // Cleanup
    repo.setOpenConversations({});
    repo.setUsername("bob");
    repo.setOpenConversations({});
    repo.clearSession();
}

void TestStorage::testAccountScopedFriends() {
    NeoNect::Storage::SettingsRepository repo("test_account_scoped_friends");
    repo.clearSession();

    // Alice logs in and adds friends
    repo.setUsername("alice");
    repo.setFriends({"friend1", "friend2"});
    repo.setPendingRequests({"pending1"});
    QCOMPARE(repo.friends().size(), 2);
    QCOMPARE(repo.pendingRequests().size(), 1);

    // Alice logs out
    repo.clearSession();

    // Bob logs in - should have 0 friends and 0 pending requests
    repo.setUsername("bob");
    QCOMPARE(repo.friends().size(), 0);
    QCOMPARE(repo.pendingRequests().size(), 0);

    // Bob adds his own friend
    repo.setFriends({"friend_of_bob"});
    QCOMPARE(repo.friends().size(), 1);
    QCOMPARE(repo.friends().at(0), QString("friend_of_bob"));

    // Switch back to Alice - should still have Alice's friends
    repo.clearSession();
    repo.setUsername("alice");
    QCOMPARE(repo.friends().size(), 2);
    QCOMPARE(repo.pendingRequests().size(), 1);

    // Cleanup
    repo.setFriends({});
    repo.setPendingRequests({});
    repo.setUsername("bob");
    repo.setFriends({});
    repo.setPendingRequests({});
    repo.clearSession();
}


