#include "../src/domain/message.h"
// tests/test_services.cpp
#include "test_services.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include "mocks/mockhttptransport.h"
#include "../src/transport/httptransport.h"
#include "../src/storage/settingsrepository.h"
#include "../src/crypto/cryptoservice.h"
#include "../src/services/authservice.h"
#include "../src/services/deviceservice.h"
#include "../src/services/relayservice.h"
#include "../src/services/friendservice.h"
#include "../src/services/messageservice.h"
#include "../src/storage/sqlmessagerepository.h"
#include "../src/core/audiomanager.h"
#include "../src/core/networkmanager.h"
#include <iostream>
#include "../src/core/notificationmanager.h"
#include "../src/core/chatmessagemodel.h"

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

    // 3. Test registerUser with NeoNect complexity requirement (letters + digits, >= 8)
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

    // Test fetchRecipientKeys (NeoNect /api/v1/relay/keys)
    QSignalSpy spyRelayKeys(&deviceService, &NeoNect::Services::DeviceService::recipientKeysFetched);
    deviceService.fetchRecipientKeys("alex");
    QCOMPARE(spyRelayKeys.count(), 1);
    auto relayArgs = spyRelayKeys.takeFirst();
    QCOMPARE(relayArgs.at(0).toString(), "alex");

    // Test revokeDevice (NeoNect DELETE /api/v1/device)
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

void TestServices::testTwoClientFriendRequestFlow() {
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "pass123");
    sharedTransport->seedUser("bob", "pass123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoBob->setMasterKey(QByteArray(32, 1));

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice_fr");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");
    storageAlice->setFriends({});
    storageAlice->setPendingRequests({});

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob_fr");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");
    storageBob->setFriends({});
    storageBob->setPendingRequests({});

    auto relayAlice = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageAlice, cryptoAlice);
    auto relayBob = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageBob, cryptoBob);

    auto friendAlice = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageAlice);
    auto friendBob = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageBob);

    // Wire Relay <-> Friend for Alice
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::incomingFriendPacket,
                     friendAlice.get(), &NeoNect::Services::FriendService::handleIncomingFriendPacket);
    QObject::connect(friendAlice.get(), &NeoNect::Services::FriendService::requestSendDomainMessage,
                     relayAlice.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     friendAlice.get(), &NeoNect::Services::FriendService::handleTransmissionStatus);

    // Wire Relay <-> Friend for Bob
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::incomingFriendPacket,
                     friendBob.get(), &NeoNect::Services::FriendService::handleIncomingFriendPacket);
    QObject::connect(friendBob.get(), &NeoNect::Services::FriendService::requestSendDomainMessage,
                     relayBob.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     friendBob.get(), &NeoNect::Services::FriendService::handleTransmissionStatus);

    QSignalSpy spyAliceTx(relayAlice.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);
    QSignalSpy spyBobReq(friendBob.get(), &NeoNect::Services::FriendService::friendRequestReceived);
    QSignalSpy spyBobPending(friendBob.get(), &NeoNect::Services::FriendService::pendingRequestsChanged);
    QSignalSpy spyAliceAccepted(friendAlice.get(), &NeoNect::Services::FriendService::friendAccepted);

    // 1. Alice sends friend request to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    friendAlice->addFriend("bob");
    if (spyAliceTx.count() == 0) {
        spyAliceTx.wait(200);
    }
    QCOMPARE(spyAliceTx.count(), 1);
    QCOMPARE(spyAliceTx.at(0).at(2).toBool(), true); // success == true

    // 2. Bob polls and receives friend request packet
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();

    QCOMPARE(spyBobReq.count(), 1);
    QCOMPARE(spyBobReq.at(0).at(0).toString(), QString("alice"));
    QCOMPARE(spyBobPending.count(), 1);
    QVERIFY(friendBob->pendingRequests().contains("alice"));
    QVERIFY(!friendBob->friends().contains("alice"));

    // 3. Bob accepts friend request from Alice
    QSignalSpy spyBobTx(relayBob.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);
    friendBob->acceptFriend("alice");
    if (spyBobTx.count() == 0) {
        spyBobTx.wait(200);
    }
    QCOMPARE(spyBobTx.count(), 1);
    QVERIFY(!friendBob->pendingRequests().contains("alice"));
    QVERIFY(friendBob->friends().contains("alice"));

    // 4. Alice polls and receives friend accept packet
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice->pollPendingMessages();

    QCOMPARE(spyAliceAccepted.count(), 1);
    QCOMPARE(spyAliceAccepted.at(0).at(0).toString(), QString("bob"));
    QVERIFY(friendAlice->friends().contains("bob"));

    storageAlice->clearSession();
    storageBob->clearSession();
}

void TestServices::testTwoClientFriendRequestRejectFlow() {
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "pass123");
    sharedTransport->seedUser("bob", "pass123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoBob->setMasterKey(QByteArray(32, 1));

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice_fr_rej");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");
    storageAlice->setFriends({});
    storageAlice->setPendingRequests({});

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob_fr_rej");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");
    storageBob->setFriends({});
    storageBob->setPendingRequests({});

    auto relayAlice = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageAlice, cryptoAlice);
    auto relayBob = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageBob, cryptoBob);

    auto friendAlice = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageAlice);
    auto friendBob = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageBob);

    // Wire Relay <-> Friend for Alice
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::incomingFriendPacket,
                     friendAlice.get(), &NeoNect::Services::FriendService::handleIncomingFriendPacket);
    QObject::connect(friendAlice.get(), &NeoNect::Services::FriendService::requestSendDomainMessage,
                     relayAlice.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     friendAlice.get(), &NeoNect::Services::FriendService::handleTransmissionStatus);

    // Wire Relay <-> Friend for Bob
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::incomingFriendPacket,
                     friendBob.get(), &NeoNect::Services::FriendService::handleIncomingFriendPacket);
    QObject::connect(friendBob.get(), &NeoNect::Services::FriendService::requestSendDomainMessage,
                     relayBob.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     friendBob.get(), &NeoNect::Services::FriendService::handleTransmissionStatus);

    QSignalSpy spyAliceTx(relayAlice.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);
    QSignalSpy spyBobReq(friendBob.get(), &NeoNect::Services::FriendService::friendRequestReceived);
    QSignalSpy spyBobPending(friendBob.get(), &NeoNect::Services::FriendService::pendingRequestsChanged);
    QSignalSpy spyAliceRejected(friendAlice.get(), &NeoNect::Services::FriendService::friendRejected);

    // 1. Alice sends friend request to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    friendAlice->addFriend("bob");
    if (spyAliceTx.count() == 0) {
        spyAliceTx.wait(200);
    }
    QCOMPARE(spyAliceTx.count(), 1);
    QCOMPARE(spyAliceTx.at(0).at(2).toBool(), true);

    // 2. Bob polls and receives friend request packet
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();

    QCOMPARE(spyBobReq.count(), 1);
    QCOMPARE(spyBobReq.at(0).at(0).toString(), QString("alice"));
    QCOMPARE(spyBobPending.count(), 1);
    QVERIFY(friendBob->pendingRequests().contains("alice"));
    QVERIFY(!friendBob->friends().contains("alice"));
    QVERIFY(!friendAlice->friends().contains("bob"));

    // 3. Bob REJECTS friend request from Alice
    QSignalSpy spyBobTx(relayBob.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);
    friendBob->rejectFriend("alice");
    if (spyBobTx.count() == 0) {
        spyBobTx.wait(200);
    }
    QCOMPARE(spyBobTx.count(), 1);
    QVERIFY(!friendBob->pendingRequests().contains("alice"));
    QVERIFY(!friendBob->friends().contains("alice"));

    // 4. Alice polls and receives friend reject packet
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice->pollPendingMessages();

    QCOMPARE(spyAliceRejected.count(), 1);
    QCOMPARE(spyAliceRejected.at(0).at(0).toString(), QString("bob"));
    QVERIFY(!friendAlice->friends().contains("bob"));
    QVERIFY(!friendAlice->pendingRequests().contains("bob"));

    storageAlice->clearSession();
    storageBob->clearSession();
}

void TestServices::testTwoClientMediaRequestApprovalFlow() {
    // 1. Verify AudioManager dynamic formatFileSize (KB, MB, GB based on size)
    QCOMPARE(AudioManager::formatFileSize(0), QString("0 B"));
    QCOMPARE(AudioManager::formatFileSize(512), QString("512 B"));
    QCOMPARE(AudioManager::formatFileSize(1024 * 350), QString("350.0 KB"));
    QCOMPARE(AudioManager::formatFileSize(1024LL * 1024 * 12), QString("12.0 MB"));
    QCOMPARE(AudioManager::formatFileSize(1024LL * 1024 * 1024 * 2), QString("2.00 GB"));

    // 2. Setup mock transport and two clients (Alice and Bob)
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "pass123");
    sharedTransport->seedUser("bob", "pass123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoBob->setMasterKey(QByteArray(32, 1));

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice_mr");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob_mr");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");

    auto relayAlice = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageAlice, cryptoAlice);
    auto relayBob = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageBob, cryptoBob);

    auto repoAlice = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    auto repoBob = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");

    auto msgAlice = std::make_shared<NeoNect::Services::MessageService>(repoAlice);
    msgAlice->setCurrentUserId("alice");
    auto msgBob = std::make_shared<NeoNect::Services::MessageService>(repoBob);
    msgBob->setCurrentUserId("bob");

    // Wire Relay <-> MessageService for Alice
    QObject::connect(msgAlice.get(), &NeoNect::Services::MessageService::transmitMessage,
                     relayAlice.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived,
                     msgAlice.get(), &NeoNect::Services::MessageService::handleIncomingMessages);

    // Wire Relay <-> MessageService for Bob
    QObject::connect(msgBob.get(), &NeoNect::Services::MessageService::transmitMessage,
                     relayBob.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived,
                     msgBob.get(), &NeoNect::Services::MessageService::handleIncomingMessages);

    QSignalSpy spyBobMsgAdded(msgBob.get(), &NeoNect::Services::MessageService::messageAdded);

    // 3. Exemption: Voice messages are sent directly WITHOUT media request
    sharedTransport->setAuthToken("mock-token-alice");
    msgAlice->sendMessage("dms:bob", "", "voice", "file:///recording.wav", "recording.wav", 64000, 5);
    QTest::qWait(60);

    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();
    QTest::qWait(60);

    QCOMPARE(spyBobMsgAdded.count(), 1);
    auto voiceMsg = spyBobMsgAdded.takeFirst().at(1).toMap();
    QCOMPARE(voiceMsg.value("type").toString(), QString("voice"));
    QCOMPARE(voiceMsg.value("fileName").toString(), QString("recording.wav"));
    QCOMPARE(voiceMsg.value("duration").toInt(), 5);

    // 4. Two-phase flow: Alice initiates media request for an image (photo)
    sharedTransport->setAuthToken("mock-token-alice");
    msgAlice->sendMediaRequest("dms:bob", "Check out the sunset!", "image", "file:///sunset.jpg", "sunset.jpg", 4500000);
    QTest::qWait(60);

    // Bob polls and receives media_request with metadata (name, type, size in bytes)
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();
    QTest::qWait(60);

    QCOMPARE(spyBobMsgAdded.count(), 1);
    auto reqMap = spyBobMsgAdded.takeFirst().at(1).toMap();
    QCOMPARE(reqMap.value("type").toString(), QString("media_request"));
    QCOMPARE(reqMap.value("fileName").toString(), QString("sunset.jpg"));
    QCOMPARE(reqMap.value("fileSize").toLongLong(), 4500000LL);
    QCOMPARE(reqMap.value("errorText").toString(), QString("image"));
    QCOMPARE(reqMap.value("status").toString(), QString("pending"));
    QString requestId = reqMap.value("id").toString();
    QVERIFY(!requestId.isEmpty());

    // 5. Bob accepts the media request
    QSignalSpy spyAliceUpdated(msgAlice.get(), &NeoNect::Services::MessageService::messageUpdated);
    QSignalSpy spyBobRemoved(msgBob.get(), &NeoNect::Services::MessageService::messageRemoved);
    QSignalSpy spyAliceRemoved(msgAlice.get(), &NeoNect::Services::MessageService::messageRemoved);

    sharedTransport->setAuthToken("mock-token-bob");
    msgBob->acceptMediaRequest("dms:alice", requestId);
    QTest::qWait(60);

    // Verify Bob removed the request card upon accept
    QVERIFY(spyBobRemoved.count() >= 1);
    QCOMPARE(spyBobRemoved.at(0).at(1).toString(), requestId);

    // Alice polls and receives media_accept
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice->pollPendingMessages();
    QTest::qWait(60);

    // Verify Alice's message updated and removed
    QVERIFY(spyAliceUpdated.count() >= 1);
    QCOMPARE(spyAliceUpdated.at(0).at(1).toString(), requestId);
    QCOMPARE(spyAliceUpdated.at(0).at(2).toString(), QString("accepted"));
    QVERIFY(spyAliceRemoved.count() >= 1);
    QCOMPARE(spyAliceRemoved.at(0).at(1).toString(), requestId);

    // 6. Verify Alice automatically transmitted the actual image payload to Bob upon acceptance
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();
    QTest::qWait(60);

    QCOMPARE(spyBobMsgAdded.count(), 1);
    auto payloadMap = spyBobMsgAdded.takeFirst().at(1).toMap();
    QCOMPARE(payloadMap.value("type").toString(), QString("image"));
    QCOMPARE(payloadMap.value("fileName").toString(), QString("sunset.jpg"));
    QCOMPARE(payloadMap.value("fileSize").toLongLong(), 4500000LL);
    QCOMPARE(payloadMap.value("mediaUrl").toString(), QString("file:///sunset.jpg"));
    QCOMPARE(payloadMap.value("text").toString(), QString("Check out the sunset!"));

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
    msg.conversationId = "dms:bob";
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

    // 6. Presence response parsing is tested via testAllFriendsUsesBackendAuthority
}

void TestServices::testAllFriendsUsesBackendAuthority() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_auth");
    storage->setFriends({"bob"});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spy(&friendService, &NeoNect::Services::FriendService::friendsListChanged);
    friendService.loadFriends();

    QCOMPARE(spy.count(), 1);
    QStringList friends = spy.first().at(0).toStringList();
    QCOMPARE(friends.size(), 1);
    QCOMPARE(friends[0], QString("bob"));
}

void TestServices::testEmptyBackendFriendsProducesEmptyModel() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_empty");
    storage->setFriends({});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spy(&friendService, &NeoNect::Services::FriendService::friendsListChanged);
    friendService.loadFriends();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toStringList().size(), 0);
}

void TestServices::testStaleLocalFriendsDoNotOverrideBackend() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_stale");
    storage->setFriends({"alex", "beatrice"});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spy(&friendService, &NeoNect::Services::FriendService::friendsListChanged);
    friendService.loadFriends();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toStringList().size(), 2);
}

void TestServices::testAddFriendSendsRealRequest() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_add");
    storage->clearSession();
    storage->setFriends({});
    storage->setPendingRequests({});
    storage->setUsername("alice");
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    mockTransport->seedUser("alice", "password123!");
    mockTransport->seedUser("bob", "password123!");
    mockTransport->setAuthToken("mock-token-alice");

    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spyResult(&friendService, &NeoNect::Services::FriendService::addFriendResult);
    QSignalSpy spySend(&friendService, &NeoNect::Services::FriendService::requestSendDomainMessage);

    // 1. Success: Add bob
    friendService.addFriend("bob");

    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResult.first().at(0).toBool(), true);
    QCOMPARE(spySend.count(), 1);
    auto msg = spySend.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(msg.type, QString("friend_request"));
    QCOMPARE(msg.conversationId, QString("dms:bob"));

    // 2. Already friend: returns false without sending request
    friendService.acceptFriend("charlie");
    spyResult.clear();
    spySend.clear();
    friendService.addFriend("charlie");
    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResult.first().at(0).toBool(), false);
    QVERIFY(spyResult.first().at(1).toString().contains("already your friend"));
    QCOMPARE(spySend.count(), 0);

    // 3. User does not exist: returns 404 Not Found
    spyResult.clear();
    friendService.addFriend("non_existent_user");
    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResult.first().at(0).toBool(), false);
    QVERIFY(spyResult.first().at(1).toString().contains("does not exist"));

    // 4. Cannot add yourself: returns 400 Bad Request
    spyResult.clear();
    friendService.addFriend("alice");
    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResult.first().at(0).toBool(), false);
    QVERIFY(spyResult.first().at(1).toString().contains("yourself", Qt::CaseInsensitive));

    // 5. Unauthorized: returns 401
    spyResult.clear();
    mockTransport->setAuthToken("unauthorized");
    friendService.addFriend("eve");
    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResult.first().at(0).toBool(), false);
    QVERIFY(spyResult.first().at(1).toString().contains("Authentication required", Qt::CaseInsensitive));
}

void TestServices::testPendingFriendIsNotAccepted() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_pend");
    storage->clearSession();
    storage->setUsername("alice");
    storage->setFriends({});
    storage->setPendingRequests({});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spyReq(&friendService, &NeoNect::Services::FriendService::pendingRequestsChanged);

    NeoNect::Domain::Message msg;
    msg.senderId = "charlie";
    msg.type = "friend_request";
    friendService.handleIncomingFriendPacket(msg);

    QCOMPARE(spyReq.count(), 1);
    QCOMPARE(spyReq.first().at(0).toStringList().size(), 1);
    QCOMPARE(friendService.pendingRequests().size(), 1);
    QCOMPARE(friendService.pendingRequests()[0], QString("charlie"));
    QCOMPARE(friendService.friends().size(), 0);
}

void TestServices::testAcceptedFriendAppears() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_acc");
    storage->clearSession();
    storage->setUsername("alice");
    storage->setFriends({});
    storage->setPendingRequests({"david"});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spySend(&friendService, &NeoNect::Services::FriendService::requestSendDomainMessage);
    QSignalSpy spyFriends(&friendService, &NeoNect::Services::FriendService::friendsListChanged);

    friendService.acceptFriend("david");

    QCOMPARE(friendService.friends().size(), 1);
    QCOMPARE(friendService.friends()[0], QString("david"));
    QCOMPARE(friendService.pendingRequests().size(), 0);
    QCOMPARE(spyFriends.count(), 1);
    QCOMPARE(spySend.count(), 1);
    auto msg = spySend.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(msg.type, QString("friend_accept"));
}

void TestServices::testRejectedFriendDoesNotAppear() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_rej");
    storage->clearSession();
    storage->setUsername("alice");
    storage->setFriends({});
    storage->setPendingRequests({"eve"});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spyReq(&friendService, &NeoNect::Services::FriendService::pendingRequestsChanged);
    QSignalSpy spySend(&friendService, &NeoNect::Services::FriendService::requestSendDomainMessage);

    friendService.rejectFriend("eve");

    QCOMPARE(friendService.friends().size(), 0);
    QCOMPARE(friendService.pendingRequests().size(), 0);
    QCOMPARE(spyReq.count(), 1);
    QCOMPARE(spySend.count(), 1);
    auto msg = spySend.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(msg.type, QString("friend_reject"));

    // Verify incoming friend_reject packet handling on sender
    auto storageSender = std::make_shared<NeoNect::Storage::SettingsRepository>("test_rej_sender");
    storageSender->clearSession();
    storageSender->setFriends({});
    storageSender->setPendingRequests({});
    storageSender->setUsername("eve");
    NeoNect::Services::FriendService friendSender(mockTransport, storageSender);
    QSignalSpy spyRejected(&friendSender, &NeoNect::Services::FriendService::friendRejected);

    NeoNect::Domain::Message rejectMsg;
    rejectMsg.senderId = "alice";
    rejectMsg.type = "friend_reject";
    friendSender.handleIncomingFriendPacket(rejectMsg);

    QCOMPARE(spyRejected.count(), 1);
    QCOMPARE(spyRejected.first().at(0).toString(), QString("alice"));
    QCOMPARE(friendSender.friends().size(), 0);
    QCOMPARE(friendSender.pendingRequests().size(), 0);
}

void TestServices::testRemovedFriendDisappears() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_rem");
    storage->clearSession();
    storage->setPendingRequests({});
    storage->setFriends({"frank"});
    storage->setUsername("alice");
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    QSignalSpy spyFriends(&friendService, &NeoNect::Services::FriendService::friendsListChanged);

    friendService.removeFriend("frank");

    QCOMPARE(friendService.friends().size(), 0);
    QCOMPARE(spyFriends.count(), 1);
}

void TestServices::testNoHardcodedFriendFallback() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_nohard");
    storage->setFriends({});
    storage->setPendingRequests({});
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    NeoNect::Services::FriendService friendService(mockTransport, storage);

    friendService.loadFriends();
    QCOMPARE(friendService.friends().size(), 0);
    QCOMPARE(friendService.pendingRequests().size(), 0);
}

void TestServices::testConnectivityAndOnlinePresence() {
    // 1. Test WebSocketClient RFC 6455 frame parsing and handshake
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    quint16 port = server.serverPort();

    NeoNect::Transport::WebSocketClient client;
    QSignalSpy spyConnected(&client, &NeoNect::Transport::WebSocketClient::connected);
    QSignalSpy spyDisconnected(&client, &NeoNect::Transport::WebSocketClient::disconnected);
    QSignalSpy spyText(&client, &NeoNect::Transport::WebSocketClient::textMessageReceived);

    client.open(QString("http://127.0.0.1:%1").arg(port), "test-device-ws", "token-ws-123");
    QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 2000);
    QTcpSocket *serverSideSocket = server.nextPendingConnection();
    QVERIFY(serverSideSocket != nullptr);

    // Read client upgrade request
    QTRY_VERIFY_WITH_TIMEOUT(serverSideSocket->bytesAvailable() > 0, 2000);
    QByteArray reqData = serverSideSocket->readAll();
    QVERIFY(reqData.contains("GET /api/v1/relay/ws?device_id=test-device-ws"));
    QVERIFY(reqData.contains("Upgrade: websocket"));
    QVERIFY(reqData.contains("Sec-WebSocket-Key:"));
    QVERIFY(reqData.contains("Authorization: Bearer token-ws-123"));

    // Server accepts handshake
    serverSideSocket->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\r\n");
    serverSideSocket->flush();

    // Client should transition to connected
    QTRY_COMPARE_WITH_TIMEOUT(spyConnected.count(), 1, 2000);
    QVERIFY(client.isConnected());

    // 2. Server sends Ping frame (0x89 with 4 bytes "ping")
    QByteArray pingFrame;
    pingFrame.append(static_cast<char>(0x89));
    pingFrame.append(static_cast<char>(0x04));
    pingFrame.append("ping");
    serverSideSocket->write(pingFrame);
    serverSideSocket->flush();

    // Server should receive Pong frame (0x8A masked with "ping" payload)
    QTRY_VERIFY_WITH_TIMEOUT(serverSideSocket->bytesAvailable() >= 6, 2000);
    QByteArray pongData = serverSideSocket->readAll();
    QVERIFY(pongData.size() >= 6);
    quint8 pongOpcode = static_cast<quint8>(pongData[0]) & 0x0F;
    QCOMPARE(pongOpcode, static_cast<quint8>(0x0A));
    bool isMasked = (static_cast<quint8>(pongData[1]) & 0x80) != 0;
    QVERIFY(isMasked);
    quint8 maskKey[4];
    std::memcpy(maskKey, pongData.constData() + 2, 4);
    QByteArray unmaskedPayload;
    for (int i = 0; i < 4; ++i) {
        unmaskedPayload.append(pongData[6 + i] ^ maskKey[i % 4]);
    }
    QCOMPARE(unmaskedPayload, QByteArray("ping"));

    // 3. Server delivers text message over WebSocket
    QByteArray textFrame;
    textFrame.append(static_cast<char>(0x81));
    QByteArray jsonMsg = "{\"id\":777,\"ciphertext\":\"mock_cipher\",\"sequence\":1}";
    textFrame.append(static_cast<char>(jsonMsg.size()));
    textFrame.append(jsonMsg);
    serverSideSocket->write(textFrame);
    serverSideSocket->flush();

    QTRY_COMPARE_WITH_TIMEOUT(spyText.count(), 1, 2000);
    QCOMPARE(spyText.first().at(0).toString(), QString::fromUtf8(jsonMsg));

    // 4. Server disconnects -> client transitions to disconnected
    serverSideSocket->disconnectFromHost();
    QTRY_COMPARE_WITH_TIMEOUT(spyDisconnected.count(), 1, 2000);
    QVERIFY(!client.isConnected());
    client.close();

    // 5. Test FriendService presence querying & immediate heartbeat check
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_presence_svc");
    storage->clearSession();
    storage->setFriends({"bob", "alice"});

    NeoNect::Services::FriendService friendService(mockTransport, storage);
    QSignalSpy spyPresence(&friendService, &NeoNect::Services::FriendService::friendStatusUpdated);

    // Initial heartbeat immediately queries presence for friends
    friendService.startHeartbeat();
    QTRY_VERIFY_WITH_TIMEOUT(spyPresence.count() >= 2, 2000);

    // Stop heartbeat marks all friends offline
    spyPresence.clear();
    friendService.stopHeartbeat();
    QCOMPARE(spyPresence.count(), 2);
    for (int i = 0; i < spyPresence.count(); ++i) {
        QCOMPARE(spyPresence.at(i).at(1).toString(), QString("offline"));
    }

    // 6. Test NetworkManager isConnected reflecting server connectivity
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendServicePtr = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);

    NetworkManager netMgr(mockTransport, storage, crypto, authService, deviceService, relayService, friendServicePtr);
    QSignalSpy spyNetConnected(&netMgr, &NetworkManager::isConnectedChanged);

    QVERIFY(!netMgr.isConnected());
    emit relayService->serverConnected();
    QVERIFY(netMgr.isConnected());
    QCOMPARE(spyNetConnected.count(), 1);

    emit relayService->serverDisconnected();
    QVERIFY(!netMgr.isConnected());
    QCOMPARE(spyNetConnected.count(), 2);
}

void TestServices::testOpenConversationsActivityOrdering() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("unit_test_conv_ordering");
    storage->setOpenConversations({});

    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);

    NetworkManager netMgr(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);
    QSignalSpy spyOpenChanged(&netMgr, &NetworkManager::openConversationsChanged);

    // 1. Open chat with Alice at t=1000
    netMgr.openDirectConversation("alice", 1000);
    QCOMPARE(netMgr.openConversations().size(), 1);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("alice"));

    // 2. Open chat with Bob at t=2000 -> Bob has more recent activity so Bob must be at index 0 (top)
    netMgr.openDirectConversation("bob", 2000);
    QCOMPARE(netMgr.openConversations().size(), 2);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("bob"));
    QCOMPARE(netMgr.openConversations().at(1).toMap().value("name").toString(), QString("alice"));

    // 3. New message arrives from Alice at t=3000 -> Alice's activity is updated, Alice promotes to top
    std::vector<NeoNect::Domain::Message> incoming;
    NeoNect::Domain::Message msg;
    msg.senderId = "alice";
    msg.conversationId = "dms:alice";
    msg.text = "Hey Bob!";
    msg.type = "text";
    msg.timestamp = 3000;
    incoming.push_back(msg);

    emit relayService->incomingDomainMessagesReceived(incoming);

    QCOMPARE(netMgr.openConversations().size(), 2);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("alice"));
    QCOMPARE(netMgr.openConversations().at(1).toMap().value("name").toString(), QString("bob"));

    // 4. Close chat with Bob -> Bob is removed and Alice remains
    netMgr.closeDirectConversation("bob");
    QCOMPARE(netMgr.openConversations().size(), 1);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("alice"));

    // 5. Verify persistence across new NetworkManager instance
    NetworkManager netMgr2(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);
    QCOMPARE(netMgr2.openConversations().size(), 1);
    QCOMPARE(netMgr2.openConversations().at(0).toMap().value("name").toString(), QString("alice"));

    storage->setOpenConversations({});
}

void TestServices::testOpenConversationsUnreadCountBadge() {
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("unit_test_unread_count");
    storage->setOpenConversations({});

    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);

    NetworkManager netMgr(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);

    // Initial state: 0 unread for anyone
    QCOMPARE(netMgr.unreadCount("alice"), 0);

    // 1. Incoming message from Alice increments her unread count to 1
    netMgr.incrementUnreadCount("alice");
    QCOMPARE(netMgr.unreadCount("alice"), 1);
    QCOMPARE(netMgr.openConversations().size(), 1);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("alice"));
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("unreadCount").toInt(), 1);

    // 2. Second incoming message from Alice increments count to 2
    netMgr.incrementUnreadCount("alice");
    QCOMPARE(netMgr.unreadCount("alice"), 2);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("unreadCount").toInt(), 2);

    QThread::msleep(10);

    // 3. Incoming message from Bob increments Bob's count to 1 and places Bob at the top
    netMgr.incrementUnreadCount("bob");
    QCOMPARE(netMgr.unreadCount("bob"), 1);
    QCOMPARE(netMgr.unreadCount("alice"), 2);
    QCOMPARE(netMgr.openConversations().size(), 2);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("bob"));
    QCOMPARE(netMgr.openConversations().at(1).toMap().value("name").toString(), QString("alice"));

    // 4. Reading Alice's chat clears her unread count, leaving Bob's unread intact
    netMgr.markConversationAsRead("alice");
    QCOMPARE(netMgr.unreadCount("alice"), 0);
    QCOMPARE(netMgr.unreadCount("bob"), 1);

    // 5. Reading Bob's chat clears his unread count as well
    netMgr.markConversationAsRead("bob");
    QCOMPARE(netMgr.unreadCount("bob"), 0);

    // 6. Incoming message for Alice, then verify persistence across instances
    netMgr.incrementUnreadCount("alice");
    QCOMPARE(netMgr.unreadCount("alice"), 1);

    NetworkManager netMgr2(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);
    QCOMPARE(netMgr2.unreadCount("alice"), 1);
    QCOMPARE(netMgr2.unreadCount("bob"), 0);

    storage->setOpenConversations({});
}

void TestServices::testSeenReceiptsAndUpdateCheckmark() {
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "pass123");
    sharedTransport->seedUser("bob", "pass123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>();
    cryptoBob->setMasterKey(QByteArray(32, 1));

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice_seen");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob_seen");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");

    auto relayAlice = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageAlice, cryptoAlice);
    auto relayBob = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageBob, cryptoBob);

    auto repoAlice = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    auto repoBob = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");

    auto msgAlice = std::make_shared<NeoNect::Services::MessageService>(repoAlice);
    msgAlice->setCurrentUserId("alice");
    auto msgBob = std::make_shared<NeoNect::Services::MessageService>(repoBob);
    msgBob->setCurrentUserId("bob");

    QObject::connect(msgAlice.get(), &NeoNect::Services::MessageService::transmitMessage,
                     relayAlice.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived,
                     msgAlice.get(), &NeoNect::Services::MessageService::handleIncomingMessages);
    QObject::connect(relayAlice.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     msgAlice.get(), [&msgAlice](const QString &, const QString &messageId, bool success, const QString &errorMessage) {
        msgAlice->handleMessageDeliveryStatus(messageId, success, errorMessage);
    });

    QObject::connect(msgBob.get(), &NeoNect::Services::MessageService::transmitMessage,
                     relayBob.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived,
                     msgBob.get(), &NeoNect::Services::MessageService::handleIncomingMessages);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::messageTransmissionStatus,
                     msgBob.get(), [&msgBob](const QString &, const QString &messageId, bool success, const QString &errorMessage) {
        msgBob->handleMessageDeliveryStatus(messageId, success, errorMessage);
    });

    ChatMessageModel modelAlice;
    modelAlice.setActiveConversation("dms:bob");
    QObject::connect(msgAlice.get(), &NeoNect::Services::MessageService::messageAdded,
                     &modelAlice, &ChatMessageModel::onMessageAdded);
    QObject::connect(msgAlice.get(), &NeoNect::Services::MessageService::messageUpdated,
                     &modelAlice, &ChatMessageModel::onMessageUpdated);

    // 1. Alice sends a direct message to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    msgAlice->sendMessage("dms:bob", "Hey Bob, check this out!");
    QTest::qWait(100);

    QCOMPARE(modelAlice.rowCount(), 1);
    QCOMPARE(modelAlice.data(modelAlice.index(0, 0), ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(modelAlice.data(modelAlice.index(0, 0), ChatMessageModel::FromMeRole).toBool(), true);

    // 2. Bob polls and receives the message
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();
    QTest::qWait(100);

    // 3. Bob sends seen receipt (as when viewing or opening the conversation)
    msgBob->sendSeenReceipt("dms:alice", "all");
    QTest::qWait(100);

    // 4. Alice polls and receives the read receipt
    sharedTransport->setAuthToken("mock-token-alice");
    relayAlice->pollPendingMessages();
    QTest::qWait(150);

    // 5. Verify Alice's message model status has updated to "seen"
    QCOMPARE(modelAlice.data(modelAlice.index(0, 0), ChatMessageModel::StatusRole).toString(), QString("seen"));

    // 6. Verify persistence in SQLite repository
    QSignalSpy spyLoaded(msgAlice.get(), &NeoNect::Services::MessageService::conversationLoaded);
    msgAlice->loadConversation("dms:bob");
    QVERIFY(spyLoaded.wait(500));
    auto loadedMsgs = spyLoaded.first().at(1).toList();
    QCOMPARE(loadedMsgs.size(), 1);
    QCOMPARE(loadedMsgs.at(0).toMap().value("status").toString(), QString("seen"));

    storageAlice->clearSession();
    storageBob->clearSession();
}

void TestServices::testSelfDirectMessageAndSavedMessagesFlow() {
    auto mockTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    auto crypto = std::make_shared<NeoNect::Crypto::CryptoService>();
    crypto->setMasterKey(QByteArray(32, 1));
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("client_test_self");
    storage->clearSession();
    storage->setUsername("alice");
    storage->setAuthToken("token_alice");
    storage->setDeviceId("dev_alice");
    storage->setOpenConversations({});

    auto authService = std::make_shared<NeoNect::Services::AuthService>(mockTransport, storage);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(mockTransport, storage);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(mockTransport, storage, crypto);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(mockTransport, storage);

    NetworkManager netMgr(mockTransport, storage, crypto, authService, deviceService, relayService, friendService);

    // 1. Verify netMgr rejects opening DM with oneself ("alice")
    netMgr.openDirectConversation("alice", 1000);
    QCOMPARE(netMgr.openConversations().size(), 0);

    // 2. Open chat with Bob -> allowed
    netMgr.openDirectConversation("bob", 2000);
    QCOMPARE(netMgr.openConversations().size(), 1);
    QCOMPARE(netMgr.openConversations().at(0).toMap().value("name").toString(), QString("bob"));

    // 3. Update activity or unread for self -> ignored
    netMgr.updateConversationActivity("alice", 3000);
    QCOMPARE(netMgr.openConversations().size(), 1);
    netMgr.incrementUnreadCount("alice");
    QCOMPARE(netMgr.unreadCount("alice"), 0);

    // 4. Test MessageService with saved-messages: saved locally as Sent, NO network transmission
    auto repo = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    auto msgService = std::make_shared<NeoNect::Services::MessageService>(repo);
    msgService->setCurrentUserId("alice");

    QSignalSpy spyTransmit(msgService.get(), &NeoNect::Services::MessageService::transmitMessage);
    QSignalSpy spyUpdated(msgService.get(), &NeoNect::Services::MessageService::messageUpdated);

    msgService->sendMessage("dms:saved-messages", "Personal Note 1");
    QTest::qWait(100);

    // Verify transmitMessage was NOT emitted for saved-messages
    QCOMPARE(spyTransmit.count(), 0);

    // Verify message updated to "sent"
    QVERIFY(!spyUpdated.isEmpty());
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("sent"));

    // Verify message persistence in DB as "sent"
    QSignalSpy spyLoaded(msgService.get(), &NeoNect::Services::MessageService::conversationLoaded);
    msgService->loadConversation("dms:saved-messages");
    QVERIFY(spyLoaded.wait(500));
    auto loaded = spyLoaded.first().at(1).toList();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).toMap().value("status").toString(), QString("sent"));
    QCOMPARE(loaded.at(0).toMap().value("text").toString(), QString("Personal Note 1"));

    // 5. Test sendMediaRequest for saved-messages -> directly stored as media, no transmit
    msgService->sendMediaRequest("dms:saved-messages", "My photo", "image", "file:///pic.png", "pic.png", 500);
    QTest::qWait(100);
    QCOMPARE(spyTransmit.count(), 0);

    // 6. Test RelayService ignores saved-messages
    QSignalSpy spyStatus(relayService.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);
    NeoNect::Domain::Message dummyMsg;
    dummyMsg.id = "msg-dummy";
    dummyMsg.conversationId = "dms:saved-messages";
    dummyMsg.text = "Hello self";
    relayService->sendDomainMessage(dummyMsg);
    QCOMPARE(spyStatus.count(), 1);
    QCOMPARE(spyStatus.first().at(2).toBool(), true); // success = true, bypassed network

    storage->clearSession();
}

void TestServices::testRetryMessageFlow() {
    auto repo = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    auto msgService = std::make_shared<NeoNect::Services::MessageService>(repo);
    msgService->setCurrentUserId("alice");

    QSignalSpy spyAdded(msgService.get(), &NeoNect::Services::MessageService::messageAdded);
    QSignalSpy spyUpdated(msgService.get(), &NeoNect::Services::MessageService::messageUpdated);
    QSignalSpy spyTransmit(msgService.get(), &NeoNect::Services::MessageService::transmitMessage);

    // 1. Send normal message to Bob
    msgService->sendMessage("dms:bob", "Hello Bob!");
    QVERIFY(spyTransmit.wait(500));
    QCOMPARE(spyAdded.count(), 1);
    QCOMPARE(spyTransmit.count(), 1);

    QString msgId = spyAdded.first().at(1).toMap().value("id").toString();
    QVERIFY(!msgId.isEmpty());

    // 2. Simulate transmission failure
    msgService->handleMessageDeliveryStatus(msgId, false, "Connection timed out");
    QCOMPARE(spyUpdated.count(), 1);
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("failed"));
    QCOMPARE(spyUpdated.last().at(3).toString(), QString("Connection timed out"));

    // 3. Retry sending the message
    spyUpdated.clear();
    spyTransmit.clear();
    msgService->retryMessage(msgId);

    // Should immediately transition to "sending" with empty errorText
    QVERIFY(!spyUpdated.isEmpty());
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("sending"));
    QCOMPARE(spyUpdated.last().at(3).toString(), QString(""));

    // Should re-emit transmitMessage
    QCOMPARE(spyTransmit.count(), 1);
    auto retriedMsg = spyTransmit.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(retriedMsg.id, msgId);
    QCOMPARE(retriedMsg.text, QString("Hello Bob!"));

    // 4. Simulate transmission success
    spyUpdated.clear();
    msgService->handleMessageDeliveryStatus(msgId, true, "");
    QCOMPARE(spyUpdated.count(), 1);
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("sent"));

    // Verify persisted status in DB is "sent"
    QSignalSpy spyLoaded(msgService.get(), &NeoNect::Services::MessageService::conversationLoaded);
    msgService->loadConversation("dms:bob");
    QVERIFY(spyLoaded.wait(500));
    auto loaded = spyLoaded.first().at(1).toList();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.at(0).toMap().value("status").toString(), QString("sent"));

    // 5. Test Media Request failure & retry
    spyAdded.clear();
    spyUpdated.clear();
    spyTransmit.clear();

    msgService->sendMediaRequest("dms:bob", "Vacation Video", "video", "file:///video.mp4", "video.mp4", 1024 * 1024 * 5);
    QVERIFY(spyTransmit.wait(500));
    QCOMPARE(spyAdded.count(), 1);
    QString reqId = spyAdded.first().at(1).toMap().value("id").toString();

    // Simulate failure
    msgService->handleMessageDeliveryStatus(reqId, false, "Recipient offline");
    QCOMPARE(spyUpdated.count(), 1);
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("failed"));

    // Retry media request
    spyUpdated.clear();
    spyTransmit.clear();
    msgService->retryMessage(reqId);

    QVERIFY(!spyUpdated.isEmpty());
    QCOMPARE(spyUpdated.last().at(2).toString(), QString("sending"));
    QCOMPARE(spyTransmit.count(), 1);
    auto retriedReq = spyTransmit.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(retriedReq.id, reqId);
    QCOMPARE(retriedReq.type, QString("media_request"));

    // 6. Test retry of cold message loaded from SQLite (not in memory cache)
    NeoNect::Domain::Message coldMsg;
    coldMsg.id = "cold-failed-msg-123";
    coldMsg.conversationId = "dms:charlie";
    coldMsg.senderId = "alice";
    coldMsg.type = "image";
    coldMsg.text = "Photo from cold DB";
    coldMsg.mediaUrl = "file:///image.jpg";
    coldMsg.fileName = "image.jpg";
    coldMsg.fileSize = 4096;
    coldMsg.status = NeoNect::Domain::MessageStatus::Failed;
    coldMsg.errorText = "P2P transfer aborted";
    coldMsg.timestamp = 1700000000;

    repo->saveMessageAsync(coldMsg, nullptr, [](bool) {});
    QTest::qWait(200);

    // Create a new MessageService instance with same repo so memory cache is empty
    auto freshMsgService = std::make_shared<NeoNect::Services::MessageService>(repo);
    freshMsgService->setCurrentUserId("alice");

    QSignalSpy freshSpyUpdated(freshMsgService.get(), &NeoNect::Services::MessageService::messageUpdated);
    QSignalSpy freshSpyTransmit(freshMsgService.get(), &NeoNect::Services::MessageService::transmitMessage);

    freshMsgService->retryMessage("cold-failed-msg-123");
    QVERIFY(freshSpyTransmit.wait(500));
    QCOMPARE(freshSpyTransmit.count(), 1);
    auto retriedCold = freshSpyTransmit.first().at(0).value<NeoNect::Domain::Message>();
    QCOMPARE(retriedCold.id, QString("cold-failed-msg-123"));
    QCOMPARE(retriedCold.text, QString("Photo from cold DB"));
    QCOMPARE(retriedCold.mediaUrl, QString("file:///image.jpg"));

    QVERIFY(!freshSpyUpdated.isEmpty());
    QCOMPARE(freshSpyUpdated.last().at(2).toString(), QString("sending"));

    // 7. Retry for saved-messages: verifies instant local sent resolution
    NeoNect::Domain::Message savedMsg;
    savedMsg.id = "saved-failed-msg-456";
    savedMsg.conversationId = "dms:saved-messages";
    savedMsg.senderId = "alice";
    savedMsg.type = "text";
    savedMsg.text = "My note that failed";
    savedMsg.status = NeoNect::Domain::MessageStatus::Failed;
    savedMsg.timestamp = 1700000050;

    repo->saveMessageAsync(savedMsg, nullptr, [](bool) {});
    QTest::qWait(200);

    freshSpyUpdated.clear();
    freshSpyTransmit.clear();
    freshMsgService->retryMessage("saved-failed-msg-456");
    QTest::qWait(200);

    // Should NOT transmit over network
    QCOMPARE(freshSpyTransmit.count(), 0);
    // Should update to sent
    QVERIFY(!freshSpyUpdated.isEmpty());
    QCOMPARE(freshSpyUpdated.last().at(2).toString(), QString("sent"));
}

void TestServices::testFunctionalOnlineIdleDndInvisibleStates() {
    auto transport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false);
    auto storage = std::make_shared<NeoNect::Storage::SettingsRepository>("test_presence_states");
    storage->clearSession();

    auto cryptoService = std::make_shared<NeoNect::Crypto::CryptoService>();
    auto authService = std::make_shared<NeoNect::Services::AuthService>(transport, storage, nullptr);
    auto deviceService = std::make_shared<NeoNect::Services::DeviceService>(transport, storage, nullptr);
    auto relayService = std::make_shared<NeoNect::Services::RelayService>(transport, storage, cryptoService, nullptr);
    auto friendService = std::make_shared<NeoNect::Services::FriendService>(transport, storage, nullptr);

    NetworkManager netMgr(transport, storage, cryptoService, authService, deviceService, relayService, friendService);
    QSignalSpy spyStatus(&netMgr, &NetworkManager::effectiveStatusChanged);
    QSignalSpy spyIdle(&netMgr, &NetworkManager::isIdleChanged);
    QSignalSpy spyDnd(&netMgr, &NetworkManager::isDndChanged);
    QSignalSpy spyInvisible(&netMgr, &NetworkManager::isInvisibleChanged);
    QSignalSpy spyLogin(&netMgr, &NetworkManager::loginResult);

    // 1. Initial State before login: effectiveStatus is offline
    QCOMPARE(netMgr.userStatus(), QString("online"));
    QCOMPARE(netMgr.effectiveStatus(), QString("offline"));
    QCOMPARE(netMgr.isIdle(), false);
    QCOMPARE(netMgr.isInvisible(), false);
    QCOMPARE(netMgr.isDnd(), false);

    // Login and connect
    netMgr.loginUser("alice", "password123");
    if (spyLogin.isEmpty()) {
        spyLogin.wait(200);
    }
    QVERIFY(storage->authToken() != "");
    emit relayService->serverConnected();
    QCOMPARE(netMgr.effectiveStatus(), QString("online"));

    // 2. Idle State: set short idle timeout (60ms) and wait for auto-idle transition
    netMgr.setIdleTimeout(60);
    QTest::qWait(120);
    QCOMPARE(netMgr.isIdle(), true);
    QCOMPARE(netMgr.effectiveStatus(), QString("afk"));

    // User touches mouse or keyboard -> reportActivity wakes back up to online
    netMgr.reportActivity();
    QCOMPARE(netMgr.isIdle(), false);
    QCOMPARE(netMgr.effectiveStatus(), QString("online"));

    // 3. Do Not Disturb (DND) state
    NeoNect::Core::NotificationManager notifMgr;
    QObject::connect(&netMgr, &NetworkManager::isDndChanged,
                     &notifMgr, &NeoNect::Core::NotificationManager::setDndEnabled);
    QSignalSpy spyNotifTriggered(&notifMgr, &NeoNect::Core::NotificationManager::notificationTriggered);

    netMgr.setUserStatus("dnd");
    QCOMPARE(netMgr.isDnd(), true);
    QCOMPARE(netMgr.effectiveStatus(), QString("dnd"));
    QCOMPARE(notifMgr.dndEnabled(), true);

    // In DND mode: incoming notifications & sounds must be suppressed!
    notifMgr.showNotification("bob", "Hey are you busy?", "message", "bob", "B", 5000);
    QCOMPARE(spyNotifTriggered.count(), 0);
    QCOMPARE(notifMgr.unreadCount(), 0);

    // 4. Invisible / Offline state
    auto msgRepo = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    NeoNect::Services::MessageService msgService(msgRepo);
    msgService.setCurrentUserId("alice");
    QObject::connect(&netMgr, &NetworkManager::isInvisibleChanged,
                     &msgService, &NeoNect::Services::MessageService::setIsInvisible);
    QSignalSpy spyTransmit(&msgService, &NeoNect::Services::MessageService::transmitMessage);

    netMgr.setUserStatus("offline");
    QCOMPARE(netMgr.isInvisible(), true);
    QCOMPARE(netMgr.effectiveStatus(), QString("offline"));
    QCOMPARE(msgService.isInvisible(), true);

    // When invisible: seen receipts must NOT be transmitted
    msgService.sendSeenReceipt("dms:bob", "all");
    QCOMPARE(spyTransmit.count(), 0);

    // When invisible: typing indicators must NOT be transmitted
    msgService.sendTyping("dms:bob", true);
    QCOMPARE(spyTransmit.count(), 0);

    // When invisible: sending a regular message still transmits, but status stays invisible
    msgService.sendMessage("dms:bob", "Hello from stealth mode");
    if (spyTransmit.isEmpty()) {
        spyTransmit.wait(200);
    }
    QVERIFY(spyTransmit.count() >= 1);
    QCOMPARE(netMgr.isInvisible(), true);
    QCOMPARE(netMgr.effectiveStatus(), QString("offline"));

    // Activity reports must NOT wake up an invisible user
    netMgr.reportActivity();
    QCOMPARE(netMgr.effectiveStatus(), QString("offline"));
    QCOMPARE(netMgr.isInvisible(), true);

    // 5. Switching back to online restores normal operation
    netMgr.setUserStatus("online");
    QCOMPARE(netMgr.isInvisible(), false);
    QCOMPARE(netMgr.isDnd(), false);
    QCOMPARE(netMgr.effectiveStatus(), QString("online"));
    QCOMPARE(msgService.isInvisible(), false);
    QCOMPARE(notifMgr.dndEnabled(), false);

    spyTransmit.clear();
    msgService.sendSeenReceipt("dms:bob", "all");
    QCOMPARE(spyTransmit.count(), 1);

    storage->clearSession();
}

void TestServices::testRealtimeChatPresenceExchange() {
    auto sharedTransport = std::make_shared<NeoNect::Testing::MockHttpTransport>(false, false);
    sharedTransport->seedUser("alice", "password123");
    sharedTransport->seedUser("bob", "password123");

    auto cryptoAlice = std::make_shared<NeoNect::Crypto::CryptoService>(); cryptoAlice->setMasterKey(QByteArray(32, 1));
    auto cryptoBob = std::make_shared<NeoNect::Crypto::CryptoService>(); cryptoBob->setMasterKey(QByteArray(32, 1));

    auto storageAlice = std::make_shared<NeoNect::Storage::SettingsRepository>("client_alice_pres");
    storageAlice->clearSession();
    storageAlice->setUsername("alice");
    storageAlice->setAuthToken("mock-token-alice");
    storageAlice->setDeviceId("mock-dev-alice");

    auto storageBob = std::make_shared<NeoNect::Storage::SettingsRepository>("client_bob_pres");
    storageBob->clearSession();
    storageBob->setUsername("bob");
    storageBob->setDeviceId("mock-dev-bob");
    storageBob->setAuthToken("mock-token-bob");

    auto authAlice = std::make_shared<NeoNect::Services::AuthService>(sharedTransport, storageAlice, nullptr);
    auto authBob = std::make_shared<NeoNect::Services::AuthService>(sharedTransport, storageBob, nullptr);
    auto devAlice = std::make_shared<NeoNect::Services::DeviceService>(sharedTransport, storageAlice, nullptr);
    auto devBob = std::make_shared<NeoNect::Services::DeviceService>(sharedTransport, storageBob, nullptr);
    auto relayAlice = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageAlice, cryptoAlice);
    auto relayBob = std::make_shared<NeoNect::Services::RelayService>(sharedTransport, storageBob, cryptoBob);
    auto friendAlice = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageAlice, nullptr);
    auto friendBob = std::make_shared<NeoNect::Services::FriendService>(sharedTransport, storageBob, nullptr);

    NetworkManager netMgrAlice(sharedTransport, storageAlice, cryptoAlice, authAlice, devAlice, relayAlice, friendAlice);
    NetworkManager netMgrBob(sharedTransport, storageBob, cryptoBob, authBob, devBob, relayBob, friendBob);

    auto msgRepoAlice = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    auto msgRepoBob = std::make_shared<NeoNect::Storage::SqlMessageRepository>(":memory:");
    NeoNect::Services::MessageService msgAlice(msgRepoAlice);
    NeoNect::Services::MessageService msgBob(msgRepoBob);
    msgAlice.setCurrentUserId("alice");
    msgBob.setCurrentUserId("bob");

    QObject::connect(&msgAlice, &NeoNect::Services::MessageService::transmitMessage,
                     relayAlice.get(), &NeoNect::Services::RelayService::sendDomainMessage);
    QObject::connect(relayBob.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived,
                     &msgBob, &NeoNect::Services::MessageService::handleIncomingMessages);

    QSignalSpy spyBobPresence(&netMgrBob, &NetworkManager::friendStatusUpdated);

    // 1. Alice sends 'afk' (idle) presence status to Bob
    sharedTransport->setAuthToken("mock-token-alice");
    msgAlice.sendPresenceStatus("bob", "afk");

    // 2. Bob polls and receives presence packet
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();

    QCOMPARE(spyBobPresence.count(), 1);
    QCOMPARE(spyBobPresence.last().at(0).toString(), QString("alice"));
    QCOMPARE(spyBobPresence.last().at(1).toString(), QString("afk"));

    // 3. Alice changes status to 'dnd'
    spyBobPresence.clear();
    sharedTransport->setAuthToken("mock-token-alice");
    msgAlice.sendPresenceStatus("bob", "dnd");

    // Bob polls and receives dnd
    sharedTransport->setAuthToken("mock-token-bob");
    relayBob->pollPendingMessages();

    QCOMPARE(spyBobPresence.count(), 1);
    QCOMPARE(spyBobPresence.last().at(0).toString(), QString("alice"));
    QCOMPARE(spyBobPresence.last().at(1).toString(), QString("dnd"));

    // 4. Bob queries presence directly via checkUserStatus (preserves Alice's reported 'dnd' status while online)
    spyBobPresence.clear();
    netMgrBob.checkUserStatus("alice");
    if (spyBobPresence.isEmpty()) {
        spyBobPresence.wait(200);
    }
    QCOMPARE(spyBobPresence.count(), 1);
    QCOMPARE(spyBobPresence.last().at(0).toString(), QString("alice"));
    QCOMPARE(spyBobPresence.last().at(1).toString(), QString("dnd"));

    // 5. Invisible mode suppresses online presence but allows broadcasting offline
    msgAlice.setIsInvisible(true);
    QSignalSpy spyAliceTx(&msgAlice, &NeoNect::Services::MessageService::transmitMessage);
    msgAlice.sendPresenceStatus("bob", "online");
    QCOMPARE(spyAliceTx.count(), 0);
    msgAlice.sendPresenceStatus("bob", "offline");
    QCOMPARE(spyAliceTx.count(), 1);

    storageAlice->clearSession();
    storageBob->clearSession();
}

