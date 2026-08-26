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

void TestServices::testAuthServiceFlow() {
    auto mockTransport = std::make_shared<Avila::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<Avila::Storage::SettingsRepository>("test_service_auth");
    storage->clearSession();
    storage->setFriends({});
    Avila::Services::AuthService authService(mockTransport, storage);

    // 1. Test verifyServer
    QSignalSpy spyVerify(&authService, &Avila::Services::AuthService::verificationResult);
    authService.verifyServer("http://localhost:8090");
    QCOMPARE(spyVerify.count(), 1);
    QCOMPARE(spyVerify.takeFirst().at(0).toBool(), true);

    // 2. Test checkUsernameAvailability
    QSignalSpy spyAvail(&authService, &Avila::Services::AuthService::availabilityResult);
    authService.checkUsernameAvailability("new_user_123");
    QCOMPARE(spyAvail.count(), 1);
    auto availArgs = spyAvail.takeFirst();
    QCOMPARE(availArgs.at(0).toString(), "new_user_123");
    QCOMPARE(availArgs.at(1).toBool(), true); // available

    // 3. Test registerUser
    QSignalSpy spyReg(&authService, &Avila::Services::AuthService::registrationResult);
    authService.registerUser("new_user_123", "secret_pass_123");
    QCOMPARE(spyReg.count(), 1);
    QCOMPARE(spyReg.takeFirst().at(0).toBool(), true);

    // 4. Test loginUser
    QSignalSpy spyLogin(&authService, &Avila::Services::AuthService::loginResult);
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
    auto mockTransport = std::make_shared<Avila::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<Avila::Storage::SettingsRepository>("test_service_device");
    storage->clearSession();
    mockTransport->setAuthToken("mock-valid-session-token");
    storage->setAuthToken("mock-valid-session-token");

    Avila::Services::DeviceService deviceService(mockTransport, storage);

    QSignalSpy spyReg(&deviceService, &Avila::Services::DeviceService::deviceRegistrationResult);
    deviceService.registerDevice("unit-dev-id-99", "MOCK_PUB_KEY_99");
    QCOMPARE(spyReg.count(), 1);
    QCOMPARE(spyReg.takeFirst().at(0).toBool(), true);

    QSignalSpy spyFetch(&deviceService, &Avila::Services::DeviceService::deviceKeyFetched);
    deviceService.fetchDevicePublicKey("unit-dev-id-99");
    QCOMPARE(spyFetch.count(), 1);
    auto fetchArgs = spyFetch.takeFirst();
    QCOMPARE(fetchArgs.at(0).toString(), "unit-dev-id-99");
    QCOMPARE(fetchArgs.at(1).toString(), "MOCK_PUB_KEY_99");

    storage->clearSession();
}

void TestServices::testRelayServiceFlowAndDeduplication() {
    auto mockTransport = std::make_shared<Avila::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<Avila::Storage::SettingsRepository>("test_service_relay");
    storage->clearSession();
    auto crypto = std::make_shared<Avila::Crypto::CryptoService>();

    mockTransport->seedUser("alice", "pass");
    mockTransport->seedUser("bob", "pass");

    // Set Alice as active session
    mockTransport->setAuthToken("token-alice");
    storage->setAuthToken("token-alice");
    storage->setUsername("alice");
    storage->setDeviceId("dev-alice");

    Avila::Services::RelayService relayService(mockTransport, storage, crypto);

    // 1. Send relay message from Alice to Bob
    QSignalSpy spySend(&relayService, &Avila::Services::RelayService::secureMessageTransmitted);
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

    QSignalSpy spyRecv(&relayService, &Avila::Services::RelayService::incomingRelayMessageReceived);

    // Poll message as Bob
    relayService.pollPendingMessages();
    QCOMPARE(spyRecv.count(), 1);
    auto recvArgs = spyRecv.takeFirst();
    QCOMPARE(recvArgs.at(0).toString(), "alice");
    QCOMPARE(recvArgs.at(1).toString(), "Hello Bob from Alice!");

    // 4. Test Deduplication: second poll immediately should not emit duplicate
    relayService.pollPendingMessages();
    QCOMPARE(spyRecv.count(), 0);

    storage->clearSession();
}

void TestServices::testFriendServiceFlow() {
    auto mockTransport = std::make_shared<Avila::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<Avila::Storage::SettingsRepository>("test_service_friend");
    storage->clearSession();
    storage->setFriends({});
    storage->setUsername("alice");

    mockTransport->seedUser("david", "pass");

    Avila::Services::FriendService friendService(mockTransport, storage);
    friendService.loadFriends();

    QSignalSpy spyAdd(&friendService, &Avila::Services::FriendService::addFriendResult);
    friendService.addFriend("david");

    QCOMPARE(spyAdd.count(), 1);
    QCOMPARE(spyAdd.takeFirst().at(0).toBool(), true);
    QVERIFY(friendService.friends().contains("david", Qt::CaseInsensitive));

    // Test Presence tracking
    QSignalSpy spyStatus(&friendService, &Avila::Services::FriendService::friendStatusUpdated);
    friendService.updateLastSeen("david");
    QCOMPARE(spyStatus.count(), 1);
    auto statusArgs = spyStatus.takeFirst();
    QCOMPARE(statusArgs.at(0).toString(), "david");
    QCOMPARE(statusArgs.at(1).toString(), "online");

    storage->clearSession();
}

void TestServices::testNetworkManagerFacadeIntegration() {
    auto mockTransport = std::make_shared<Avila::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<Avila::Storage::SettingsRepository>("test_facade_profile");
    storage->clearSession();
    storage->setFriends({});
    auto crypto = std::make_shared<Avila::Crypto::CryptoService>();

    NetworkManager nm(mockTransport, storage, crypto);

    QSignalSpy spyVerify(&nm, &NetworkManager::verificationResult);
    nm.verifyServer("http://localhost:8090");
    QCOMPARE(spyVerify.count(), 1);
    QCOMPARE(spyVerify.takeFirst().at(0).toBool(), true);

    storage->clearSession();
}
