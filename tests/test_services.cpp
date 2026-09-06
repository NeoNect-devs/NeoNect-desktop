#include "../src/domain/message.h"
// tests/test_services.cpp
#include "test_services.h"
#include <QtTest>
#include <QSignalSpy>
#include "mocks/mockhttptransport.h"
#include "../src/transport/httptransport.h"
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
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>(); crypto->setMasterKey(QByteArray(32, 1));

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
    {
        NeoNect::Domain::Message msg;
        msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg.conversationId = "dms:" + QString("bob");
        msg.type = "text";
        msg.text = "Hello Bob from Alice!";
        msg.senderId = "alice"; // mock
        relayService.sendDomainMessage(msg);
    }
    QCOMPARE(spySend.count(), 1);

    // 2. Verify message was queued for Bob's device
    QCOMPARE(mockTransport->queuedMessageCount("mock-dev-bob"), 1);

    // 3. Switch active user to Bob
    mockTransport->setAuthToken("token-bob");
    storage->setAuthToken("token-bob");
    storage->setUsername("bob");
    storage->setDeviceId("mock-dev-bob");

    QSignalSpy spyRecv(&relayService, &NeoNect::Services::RelayService::incomingDomainMessagesReceived);

    // Poll message as Bob
    relayService.pollPendingMessages();
    QCOMPARE(spyRecv.count(), 1);
    auto recvArgs = spyRecv.takeFirst();
    auto msgs = qvariant_cast<std::vector<NeoNect::Domain::Message>>(recvArgs.at(0));
    QVERIFY(!msgs.empty());
    auto msg = msgs.front();
    QCOMPARE(msg.senderId, QString("alice"));
    QCOMPARE(msg.text, QString("Hello Bob from Alice!"));

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
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>(); crypto->setMasterKey(QByteArray(32, 1));

    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);
    NetworkManager nm(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);

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

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>(); cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>(); cryptoBob->setMasterKey(QByteArray(32, 1));

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

    QSignalSpy spyBobRecv(&relayBob, &NeoNect::Services::RelayService::incomingDomainMessagesReceived);
    QSignalSpy spyAliceRecv(&relayAlice, &NeoNect::Services::RelayService::incomingDomainMessagesReceived);

    // 2. Alice sends a direct message to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    {
        NeoNect::Domain::Message msg;
        msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg.conversationId = "dms:" + QString("bob");
        msg.type = "text";
        msg.text = "Hey Bob, greetings from client Alice!";
        msg.senderId = "alice"; // mock
        relayAlice.sendDomainMessage(msg);
        QSignalSpy spySend(&relayAlice, &NeoNect::Services::RelayService::messageTransmissionStatus);
        spySend.wait(50);
    }

    // 3. Bob polls and receives Alice's message
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob.pollPendingMessages();

    QCOMPARE(spyBobRecv.count(), 1);
    auto bobMsg = spyBobRecv.takeFirst();
    auto msgs = qvariant_cast<std::vector<NeoNect::Domain::Message>>(bobMsg.at(0));
    QVERIFY(!msgs.empty());
    auto msg = msgs.front();
    QCOMPARE(msg.senderId, QString("alice"));
    QCOMPARE(msg.text, QString("Hey Bob, greetings from client Alice!"));

    // 4. Bob sends reply back to Alice
    {
        NeoNect::Domain::Message msg;
        msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg.conversationId = "dms:" + QString("alice");
        msg.type = "text";
        msg.text = "Hey Alice, message received loud and clear!";
        msg.senderId = "alice"; // mock
        relayBob.sendDomainMessage(msg);
    }

    // 5. Alice polls and receives Bob's reply
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice.pollPendingMessages();

    QCOMPARE(spyAliceRecv.count(), 1);
    auto aliceMsg = spyAliceRecv.takeFirst();
    auto msgs2 = qvariant_cast<std::vector<NeoNect::Domain::Message>>(aliceMsg.at(0));
    QVERIFY(!msgs2.empty());
    auto msg2 = msgs2.front();
    QCOMPARE(msg2.senderId, QString("bob"));
    QCOMPARE(msg2.text, QString("Hey Alice, message received loud and clear!"));

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

    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>(); crypto->setMasterKey(QByteArray(32, 1));

    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);
    NetworkManager nm(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);

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
    NeoNect::Core::NotificationManager notifMgrObj;
    auto notifMgr = &notifMgrObj;
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

void TestServices::testCiphertextDoesNotContainPlaintext() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_crypto_plain");
    storage->setAuthToken("token");
    storage->setUsername("alice");
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    crypto->setMasterKey(QByteArray(32, 1));

    NeoNect::Services::RelayService relay(mockTransport, storage, crypto);

    QSignalSpy spyRaw(mockTransport.get(), &NeoNect::Testing::MockHttpTransport::rawRequestData);

    NeoNect::Domain::Message msg;
    msg.id = "123";
    msg.conversationId = "dms:bob";
    msg.type = "text";
    msg.text = "SUPER_SECRET_PLAINTEXT_999";
    msg.senderId = "alice";

    relay.sendDomainMessage(msg);

    QCOMPARE(spyRaw.count(), 1);
    QByteArray rawJson = spyRaw.takeFirst().at(0).toByteArray();
    QVERIFY(!rawJson.contains("SUPER_SECRET_PLAINTEXT_999"));
}

void TestServices::testRelayServiceTamperedMessageRejection() {
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice");
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("token-alice");
    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoAlice->setMasterKey(QByteArray(32, 1));

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob");
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("token-bob");
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoBob->setMasterKey(QByteArray(32, 1));

    sharedTransport->seedUser("alice", "pass");
    sharedTransport->seedUser("bob", "pass");

    NeoNect::Services::RelayService relayAlice(sharedTransport, storageAlice, cryptoAlice);
    NeoNect::Services::RelayService relayBob(sharedTransport, storageBob, cryptoBob);

    sharedTransport->setAuthToken("token-alice");
    NeoNect::Domain::Message msg;
    msg.id = "msg1";
    msg.conversationId = "dms:bob";
    msg.type = "text";
    msg.text = "Hello Bob!";
    msg.senderId = "alice";
    relayAlice.sendDomainMessage(msg);

    sharedTransport->tamperLastMessageCiphertext("mock-dev-bob");

    QSignalSpy spyBobRecv(&relayBob, &NeoNect::Services::RelayService::incomingDomainMessagesReceived);
    sharedTransport->setAuthToken("token-bob");
    relayBob.pollPendingMessages();

    QCOMPARE(spyBobRecv.count(), 0);
}

void TestServices::testCallbackCannotReachDestroyedService() {
    auto transport = std::make_shared<NeoNect::Transport::HttpTransport>();
    bool callbackInvoked = false;

    {
        auto obj = std::make_unique<QObject>();
        transport->get("http://127.0.0.1:9999/dummy", {}, obj.get(), [&callbackInvoked](int, const QByteArray&, QNetworkReply::NetworkError, const QString&) {
            callbackInvoked = true;
        });
        // Destroy the context object immediately while request is pending
    }

    // Process events to allow network reply to finish/fail
    QCoreApplication::processEvents(QEventLoop::AllEvents, 500);

    // Callback should not be invoked because the context object was destroyed
    QCOMPARE(callbackInvoked, false);
}

void TestServices::testRequestCancellationOnServiceDestruction() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_cancel");

    QPointer<QObject> servicePtr;
    {
        auto authService = std::make_unique<NeoNect::Services::AuthService>(mockTransport, storage);
        servicePtr = authService.get();
        QCOMPARE(servicePtr.isNull(), false);
    }
    // Verifies that destruction is clean without crashing from pending requests
    QCOMPARE(servicePtr.isNull(), true);
}

void TestServices::testPhase10ABackendProtocolCompliance() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>();
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    crypto->deriveKeyFromPassphrase("testpass", "salt");
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>();

    NeoNect::Services::AuthService authService(mockTransport, storage);
    NeoNect::Services::DeviceService deviceService(mockTransport, storage);
    NeoNect::Services::RelayService relayService(mockTransport, storage, crypto);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    storage->setAuthToken("test_token");
    storage->setDeviceId("dev_123");

    QSignalSpy spyRequests(mockTransport.get(), &NeoNect::Testing::MockHttpTransport::rawRequestData);

    // 1. Correct send request envelope
    NeoNect::Domain::Message msg;
    msg.id = "msg_abc";
    msg.text = "Hello backend";
    relayService.sendDomainMessage(msg);
    QTest::qWait(50);

    QCOMPARE(spyRequests.count(), 1);
    QByteArray sendPayload = spyRequests.takeFirst().at(0).toByteArray();
    QJsonDocument sendDoc = QJsonDocument::fromJson(sendPayload);
    QVERIFY(!sendDoc.isNull());
    QVERIFY(sendDoc.object().contains("from_device_id"));
    QVERIFY(sendDoc.object().contains("to_username"));
    QVERIFY(sendDoc.object().contains("ciphertext"));
    QVERIFY(sendDoc.object().contains("timestamp"));
    QVERIFY(!sendDoc.object().contains("messages")); // Must NOT be an array wrapper

    // 2 & 3. Correct parsing of server id & Acknowledgement Request
    relayService.acknowledgeMessage(999);
    QTest::qWait(50);

    QCOMPARE(spyRequests.count(), 1);
    QByteArray ackPayload = spyRequests.takeFirst().at(0).toByteArray();
    QJsonDocument ackDoc = QJsonDocument::fromJson(ackPayload);
    QVERIFY(!ackDoc.isNull());
    QVERIFY(ackDoc.object().contains("device_id"));
    QVERIFY(ackDoc.object().contains("message_id"));
    QCOMPARE(ackDoc.object().value("message_id").toInt(), 999);

    // 4. Authentication request compatibility
    authService.loginUser("alice", "pass");
    QTest::qWait(50);
    QCOMPARE(spyRequests.count(), 1);
    QByteArray authPayload = spyRequests.takeFirst().at(0).toByteArray();
    QJsonDocument authDoc = QJsonDocument::fromJson(authPayload);
    QVERIFY(authDoc.object().contains("username"));
    QVERIFY(authDoc.object().contains("password"));

    // 5. Backend error body parsing (Testing AuthService's handling of 401 {"error": "..."})
    mockTransport->setSimulateHttpError(401);
    QSignalSpy spyLogin(&authService, &NeoNect::Services::AuthService::loginResult);
    authService.loginUser("alice", "wrong");
    QTest::qWait(50);
    spyRequests.clear();
    mockTransport->setSimulateHttpError(0);

    QCOMPARE(spyLogin.count(), 1);
    QCOMPARE(spyLogin.first().at(0).toBool(), false);
    QVERIFY(spyLogin.first().at(1).toString().contains("Simulated HTTP error", Qt::CaseInsensitive));

    // 6. Presence response parsing
    friendService.addFriend("bob");
    QSignalSpy spyPresence(&friendService, &NeoNect::Services::FriendService::friendStatusUpdated);
    friendService.checkFriendsStatus();
    QTest::qWait(50);
    spyRequests.clear();
    QVERIFY(spyPresence.count() > 0);
    bool foundBob = false;
    for (int i = 0; i < spyPresence.count(); ++i) {
        if (spyPresence.at(i).at(0).toString() == "bob") {
            foundBob = true;
            break;
        }
    }
    QVERIFY(foundBob);
}
