// tests/test_services.cpp
#include "test_services.h"
#include <QtTest>
#include <QSignalSpy>
#include "mocks/mockhttptransport.h"
#include "../src/storage/settingsrepository.h"
#include "../src/crypto/cryptoservice.h"
#include "../src/services/authservice.h"
#include "../src/services/deviceservice.h"
#include "../src/services/relayservice.h"
#include "../src/services/friendservice.h"
#include "../src/core/networkmanager.h"
#include "../src/core/notificationmanager.h"

void TestServices::testAuthServiceFlow() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_service_auth");
    storage->clearSession();
    storage->setFriends({});
    NeoNect::Services::AuthService authService(mockTransport, storage);

    // 1. Test verifyServer
    QSignalSpy spyVerify(&authService, &NeoNect::Services::AuthService::verificationResult);
    authService.verifyServer("http://localhost:8080");
    QCOMPARE(spyVerify.count(), 1);
    QCOMPARE(spyVerify.takeFirst().at(0).toBool(), true);

    // 2. Test checkUsernameAvailability
    QSignalSpy spyAvail(&authService, &NeoNect::Services::AuthService::availabilityResult);
    authService.checkUsernameAvailability("new_user_123");
    QCOMPARE(spyAvail.count(), 1);
    auto availArgs = spyAvail.takeFirst();
    QCOMPARE(availArgs.at(0).toString(), "new_user_123");
    QCOMPARE(availArgs.at(1).toBool(), true); // available

    // 3. Test registerUser with Danisa complexity requirement (letters + digits, >= 8)
    QSignalSpy spyReg(&authService, &NeoNect::Services::AuthService::registrationResult);
    authService.registerUser("new_user_123", "secret_pass_123");
    QCOMPARE(spyReg.count(), 1);
    QCOMPARE(spyReg.takeFirst().at(0).toBool(), true);

    // 4. Test loginUser
    QSignalSpy spyLogin(&authService, &NeoNect::Services::AuthService::loginResult);
    authService.loginUser("new_user_123", "secret_pass_123");
    QCOMPARE(spyLogin.count(), 1);
    QCOMPARE(spyLogin.takeFirst().at(0).toBool(), true);
    QVERIFY(!storage->authToken().isEmpty());
    QCOMPARE(storage->username(), "new_user_123");

    // 5. Test logoutUser
    authService.logoutUser();
    QVERIFY(storage->authToken().isEmpty());
}

void TestServices::testDeviceServiceFlow() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_service_device");
    storage->clearSession();
    mockTransport->setAuthToken("mock-valid-session-token");
    storage->setAuthToken("mock-valid-session-token");

    NeoNect::Services::DeviceService deviceService(mockTransport, storage);

    QSignalSpy spyReg(&deviceService, &NeoNect::Services::DeviceService::deviceRegistrationResult);
    deviceService.registerDevice("unit-dev-id-99", "MOCK_PUB_KEY_99");
    QCOMPARE(spyReg.count(), 1);
    QCOMPARE(spyReg.takeFirst().at(0).toBool(), true);

    QSignalSpy spyFetch(&deviceService, &NeoNect::Services::DeviceService::deviceKeyFetched);
    deviceService.fetchDevicePublicKey("unit-dev-id-99");
    QCOMPARE(spyFetch.count(), 1);
    auto fetchArgs = spyFetch.takeFirst();
    QCOMPARE(fetchArgs.at(0).toString(), "unit-dev-id-99");
    QCOMPARE(fetchArgs.at(1).toString(), "MOCK_PUB_KEY_99");

    // Test fetchRecipientKeys (Danisa /api/v1/relay/keys)
    QSignalSpy spyRelayKeys(&deviceService, &NeoNect::Services::DeviceService::recipientKeysFetched);
    deviceService.fetchRecipientKeys("alex");
    QCOMPARE(spyRelayKeys.count(), 1);
    auto relayArgs = spyRelayKeys.takeFirst();
    QCOMPARE(relayArgs.at(0).toString(), "alex");

    // Test revokeDevice (Danisa DELETE /api/v1/device)
    QSignalSpy spyRevoke(&deviceService, &NeoNect::Services::DeviceService::deviceRevocationResult);
    deviceService.revokeDevice("unit-dev-id-99");
    QCOMPARE(spyRevoke.count(), 1);
    QCOMPARE(spyRevoke.takeFirst().at(0).toBool(), true);

    storage->clearSession();
}

void TestServices::testRelayServiceFlowAndDeduplication() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_service_relay");
    storage->clearSession();
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();

    mockTransport->seedUser("alice", "password123");
    mockTransport->seedUser("bob", "password123");

    // Set Alice as active session
    mockTransport->setAuthToken("token-alice");
    storage->setAuthToken("token-alice");
    storage->setUsername("alice");
    storage->setDeviceId("dev-alice");

    NeoNect::Services::RelayService relayService(mockTransport, storage, crypto);

    // 1. Send relay message from Alice to Bob
    QSignalSpy spySend(&relayService, &NeoNect::Services::RelayService::secureMessageTransmitted);
    relayService.sendRelayMessage("bob", "Hello Bob from Alice!");
    QCOMPARE(spySend.count(), 1);
    QCOMPARE(spySend.takeFirst().at(1).toBool(), true);

    // 2. Verify message was queued for Bob's device
    QCOMPARE(mockTransport->queuedMessageCount("mock-dev-bob"), 1);

    // 3. Switch active user to Bob
    mockTransport->setAuthToken("token-bob");
    storage->setAuthToken("token-bob");
    storage->setUsername("bob");
    storage->setDeviceId("mock-dev-bob");

    QSignalSpy spyRecv(&relayService, &NeoNect::Services::RelayService::incomingRelayMessageReceived);

    // Poll message as Bob
    relayService.pollPendingMessages();
    QCOMPARE(spyRecv.count(), 1);
    auto recvArgs = spyRecv.takeFirst();
    QCOMPARE(recvArgs.at(0).toString(), "alice");
    QCOMPARE(recvArgs.at(1).toString(), "bob");
    QCOMPARE(recvArgs.at(2).toString(), "Hello Bob from Alice!");

    // 4. Test Deduplication: second poll immediately should not emit duplicate
    relayService.pollPendingMessages();
    QCOMPARE(spyRecv.count(), 0);

    storage->clearSession();
}

void TestServices::testFriendServiceFlow() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_service_friend");
    storage->clearSession();
    storage->setFriends({});
    storage->setUsername("alice");

    mockTransport->seedUser("david", "password123");

    NeoNect::Services::FriendService friendService(mockTransport, storage);
    friendService.loadFriends();

    QSignalSpy spyAdd(&friendService, &NeoNect::Services::FriendService::addFriendResult);
    friendService.addFriend("david");

    QCOMPARE(spyAdd.count(), 1);
    QCOMPARE(spyAdd.takeFirst().at(0).toBool(), true);
    QVERIFY(friendService.friends().contains("david", Qt::CaseInsensitive));

    // Test Presence tracking
    QSignalSpy spyStatus(&friendService, &NeoNect::Services::FriendService::friendStatusUpdated);
    friendService.updateLastSeen("david");
    QCOMPARE(spyStatus.count(), 1);
    auto statusArgs = spyStatus.takeFirst();
    QCOMPARE(statusArgs.at(0).toString(), "david");
    QCOMPARE(statusArgs.at(1).toString(), "online");

    // Test Danisa Presence Endpoint check
    mockTransport->setAuthToken("mock-valid-token");
    storage->setAuthToken("mock-valid-token");
    friendService.checkFriendsStatus();
    QVERIFY(spyStatus.count() >= 1);

    storage->clearSession();
}

void TestServices::testNetworkManagerFacadeIntegration() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_facade_profile");
    storage->clearSession();
    storage->setFriends({});
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();

    NetworkManager nm(mockTransport, storage, crypto);

    QSignalSpy spyVerify(&nm, &NetworkManager::verificationResult);
    nm.verifyServer("http://localhost:8080");
    QCOMPARE(spyVerify.count(), 1);
    QCOMPARE(spyVerify.takeFirst().at(0).toBool(), true);

    QSignalSpy spyRevoke(&nm, &NetworkManager::deviceRevocationResult);
    nm.revokeDevice("test-dev-123");
    QCOMPARE(spyRevoke.count(), 1);

    storage->clearSession();
}

void TestServices::testTwoClientChatExchange() {
    // 1. Shared mock transport simulating network backbone
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "pass123");
    sharedTransport->seedUser("bob", "pass123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");

    NeoNect::Services::RelayService relayAlice(sharedTransport, storageAlice, cryptoAlice);
    NeoNect::Services::RelayService relayBob(sharedTransport, storageBob, cryptoBob);

    QSignalSpy spyBobRecv(&relayBob, &NeoNect::Services::RelayService::incomingRelayMessageReceived);
    QSignalSpy spyAliceRecv(&relayAlice, &NeoNect::Services::RelayService::incomingRelayMessageReceived);

    // 2. Alice sends a direct message to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice.sendRelayMessage("bob", "Hey Bob, greetings from client Alice!");

    // 3. Bob polls and receives Alice's message
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob.pollPendingMessages();

    QCOMPARE(spyBobRecv.count(), 1);
    auto bobMsg = spyBobRecv.takeFirst();
    QCOMPARE(bobMsg.at(0).toString(), "alice");
    QCOMPARE(bobMsg.at(1).toString(), "bob");
    QCOMPARE(bobMsg.at(2).toString(), "Hey Bob, greetings from client Alice!");

    // 4. Bob sends reply back to Alice
    relayBob.sendRelayMessage("alice", "Hey Alice, message received loud and clear!");

    // 5. Alice polls and receives Bob's reply
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice.pollPendingMessages();

    QCOMPARE(spyAliceRecv.count(), 1);
    auto aliceMsg = spyAliceRecv.takeFirst();
    QCOMPARE(aliceMsg.at(0).toString(), "bob");
    QCOMPARE(aliceMsg.at(1).toString(), "alice");
    QCOMPARE(aliceMsg.at(2).toString(), "Hey Alice, message received loud and clear!");

    storageAlice->clearSession();
    storageBob->clearSession();
}

void TestServices::testBookmarkConnectFlow() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    mockTransport->seedUser("alice", "password123");

    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_bm_connect_profile");
    // Ensure that even if persistent storage had a token, NetworkManager starts unauthenticated (NO auto-login)
    storage->setAuthToken("old_stale_token");
    storage->setBookmarks({});

    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();

    NetworkManager nm(mockTransport, storage, crypto);

    // 1. Verify that auto-login is completely removed: token starts empty!
    QVERIFY(nm.token().isEmpty());
    QVERIFY(nm.currentUsername().isEmpty());

    // 2. Save a bookmark
    nm.saveBookmark("Test Node", "http://localhost:8080", "alice", "password123");
    QVariantList bms = nm.bookmarks();
    QCOMPARE(bms.size(), 1);
    QString bmId = bms.at(0).toMap().value("id").toString();
    QVERIFY(!bmId.isEmpty());

    // 3. Connect via bookmark
    QSignalSpy spyLogin(&nm, &NetworkManager::loginResult);
    QSignalSpy spyToken(&nm, &NetworkManager::tokenChanged);

    nm.connectBookmark(bmId);

    QCOMPARE(spyLogin.count(), 1);
    QCOMPARE(spyLogin.takeFirst().at(0).toBool(), true);
    QVERIFY(spyToken.count() >= 1);
    QVERIFY(!nm.token().isEmpty());
    QCOMPARE(nm.currentUsername(), "alice");

    // 4. Logout via logout() alias
    nm.logout();
    QVERIFY(nm.token().isEmpty());
    QVERIFY(nm.currentUsername().isEmpty());

    storage->clearSession();
}

void TestServices::testNotificationManagerFlow() {
    auto notifMgr = NeoNect::Core::NotificationManager::instance();
    notifMgr->clearAll();
    notifMgr->setNotificationsEnabled(true);
    notifMgr->setDndEnabled(false);
    notifMgr->setSoundEnabled(false); // Silent in automated test

    QCOMPARE(notifMgr->notificationsEnabled(), true);
    QCOMPARE(notifMgr->soundEnabled(), false);
    QCOMPARE(notifMgr->dndEnabled(), false);
    QCOMPARE(notifMgr->unreadCount(), 0);

    // 1. Test showNotification
    QSignalSpy spyTrigger(notifMgr, &NeoNect::Core::NotificationManager::notificationTriggered);
    notifMgr->showNotification("Alex", "Hello from Alex!", "message", "alex", "A", 3000);

    QCOMPARE(spyTrigger.count(), 1);
    auto notif = spyTrigger.takeFirst().at(0).toMap();
    QCOMPARE(notif.value("title").toString(), "Alex");
    QCOMPARE(notif.value("body").toString(), "Hello from Alex!");
    QCOMPARE(notif.value("channel").toString(), "alex");
    QCOMPARE(notifMgr->unreadCount(), 1);
    QCOMPARE(notifMgr->activeNotifications().size(), 1);

    QString notifId = notif.value("notifId").toString();
    QVERIFY(!notifId.isEmpty());

    // 2. Test showMessageNotification
    notifMgr->showMessageNotification("Beatrice", "Voice message sent", "beatrice", "B", "voice");
    QCOMPARE(spyTrigger.count(), 1);
    QCOMPARE(notifMgr->unreadCount(), 2);
    QCOMPARE(notifMgr->activeNotifications().size(), 2);
    spyTrigger.clear();

    // 3. Test dismissNotification
    QSignalSpy spyDismiss(notifMgr, &NeoNect::Core::NotificationManager::notificationDismissed);
    notifMgr->dismissNotification(notifId);
    QCOMPARE(spyDismiss.count(), 1);
    QCOMPARE(spyDismiss.takeFirst().at(0).toString(), notifId);
    QCOMPARE(notifMgr->activeNotifications().size(), 1);

    // 4. Test DND mode disables notification dispatch
    notifMgr->setDndEnabled(true);
    notifMgr->showNotification("Charlie", "Muted message", "message");
    QCOMPARE(spyTrigger.count(), 0); // Blocked by DND

    // 5. Test clearAll
    notifMgr->clearAll();
    QCOMPARE(notifMgr->activeNotifications().size(), 0);
    QCOMPARE(notifMgr->unreadCount(), 0);

    // 6. Test screenCorner
    QSignalSpy spyCorner(notifMgr, &NeoNect::Core::NotificationManager::screenCornerChanged);
    notifMgr->setScreenCorner("top-right");
    QCOMPARE(notifMgr->screenCorner(), "top-right");
    QCOMPARE(spyCorner.count(), 1);
    notifMgr->setScreenCorner("bottom-right");
    QCOMPARE(notifMgr->screenCorner(), "bottom-right");

    // 7. Test resetUnreadCount
    notifMgr->setDndEnabled(false);
    notifMgr->showNotification("Test", "Unread counter test", "message");
    QCOMPARE(notifMgr->unreadCount(), 1);
    notifMgr->resetUnreadCount();
    QCOMPARE(notifMgr->unreadCount(), 0);
    notifMgr->clearAll();

    // Restore settings
    notifMgr->setDndEnabled(false);
    notifMgr->setSoundEnabled(true);
}
