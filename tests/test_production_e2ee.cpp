#include "../src/core/application.h"
#include "../src/crypto/session/SecurePreKeyStoreAdapter.h"
#include "../src/crypto/KeyEncoding.h"
#include "../src/services/prekeyservice.h"
#include "test_production_e2ee.h"
#include "../src/core/messaging/MessageStorage.h"
#include "../src/core/messaging/MessageService.h"
#include "../src/core/messaging/OfflineQueue.h"
#include "../src/crypto/session/SessionManager.h"
#include "../src/services/messageservice.h"
#include "../src/services/relayservice.h"
#include "../src/storage/e2ee/SecureE2EEStore.h"
#include "../src/storage/e2ee/MasterKeyProvider.h"
#include "test_secure_storage.h" // For MockSecretStore
#include "../src/storage/e2ee/PlatformSecretStore.h"
#include "mocks/mockhttptransport.h"
#include "../src/crypto/x3dh/X3DH.h"
#include "../src/crypto/doubleratchet/DoubleRatchet.h"
#include "../src/crypto/doubleratchet/AEAD.h"
#include "../src/crypto/OpenSSLBackend.h"
#include "../src/crypto/XEdDSAAdapter.h"
#include "../src/storage/settingsrepository.h"
#include "../src/common/types.h"
#include <QSignalSpy>

using namespace NeoNect;
using namespace NeoNect::Core::Messaging;
using namespace NeoNect::Crypto;
using namespace NeoNect::Crypto::Session;

class DummySecretStore : public NeoNect::Storage::IOSSecretStore {
    QByteArray m_key;
public:
    DummySecretStore() {
        m_key = QByteArray::fromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    }
    NeoNect::ServiceResult<QByteArray> readSecret(const QString& name) override {
        Q_UNUSED(name);
        return NeoNect::ServiceResult<QByteArray>::ok(m_key);
    }
    NeoNect::ServiceResult<std::monostate> writeSecret(const QString& name, const QByteArray& secret) override {
        Q_UNUSED(name);
        m_key = secret;
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> deleteSecret(const QString& name) override {
        Q_UNUSED(name);
        m_key.clear();
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
};


class ProxyMessageQueue : public Core::Messaging::IMessageQueue {
public:
    std::shared_ptr<Core::Messaging::IMessageQueue> realQueue;
    bool failEnqueue = false;

    ProxyMessageQueue(std::shared_ptr<Core::Messaging::IMessageQueue> q) : realQueue(std::move(q)) {}

    bool enqueue(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) override {
        if (failEnqueue) return false;
        return realQueue->enqueue(messageId, envelopeBytes, recipientUsername, recipientDeviceId);
    }
    bool updateState(const QString& messageId, Core::Messaging::QueueState newState) override {
        return realQueue->updateState(messageId, newState);
    }
    bool incrementRetry(const QString& messageId) override {
        return realQueue->incrementRetry(messageId);
    }
    std::optional<Core::Messaging::QueueEntry> getEntry(const QString& messageId) override {
        return realQueue->getEntry(messageId);
    }
    std::vector<Core::Messaging::QueueEntry> getPendingEntries(int limit, qint64 afterCreatedAt, const QString& afterMessageId) override {
        return realQueue->getPendingEntries(limit, afterCreatedAt, afterMessageId);
    }
};

void TestProductionE2EE::initTestCase() {
    QFile::remove("test_prod_e2ee.db");
    QFile::remove("test_prod_secure.db");
}

void TestProductionE2EE::cleanupTestCase() {
    QFile::remove("test_prod_e2ee.db");
    QFile::remove("test_prod_secure.db");
}

void TestProductionE2EE::testOutgoingProductionPath() {
    // 1. Initialize Mock Transport and Storage
    auto mockTransport = std::make_shared<Testing::MockHttpTransport>(true, false);
    mockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"success\":true}", 200);
    QSignalSpy spyReq(mockTransport.get(), &Testing::MockHttpTransport::rawRequestData);
    auto settingsStore = std::make_shared<Storage::SettingsRepository>("test");
    settingsStore->setAuthToken("fake_token");
    settingsStore->setDeviceId("fake_device");

    // 2. Initialize Crypto Core
    auto platformSecretStore = std::make_shared<DummySecretStore>();
    auto keyProvider = std::make_shared<Storage::MasterKeyProvider>(platformSecretStore);
    auto secureStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    auto initRes = secureStore->initialize("test_prod_secure.db");
    QVERIFY(initRes.success);

    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    auto x3dh = std::make_shared<X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<DoubleRatchet::AEAD>(backend.get());

    // 3. Prepare Dummy Session so encryption succeeds
    Storage::E2EESession s;
    s.session_id = "bob:default_device";
    s.remote_identity_key = QByteArray(32, 'I');

    // Add local identity first to satisfy FK
    Storage::E2EEIdentity id;
    id.public_key = QByteArray(32, 'P');
    id.private_key = QByteArray(32, 'V');
    secureStore->saveIdentity(id);

    s.local_identity_id = 1;
    s.PN = 0;
    s.Ns = 0;
    s.Nr = 0;
    s.RK = QByteArray(32, 'R');
    auto dhs = backend->GenerateX25519KeyPair();
    QByteArray dhsBytes;
    dhsBytes.append(QByteArray(reinterpret_cast<const char*>(dhs.first.data.data()), 32));
    dhsBytes.append(QByteArray(reinterpret_cast<const char*>(dhs.second.data.data()), 32));
    s.DHs = dhsBytes;
    s.DHr = QByteArray(32, 'D');
    s.CKs = QByteArray(32, 'C');
    s.CKr = QByteArray(32, 'C');
    auto saveRes = secureStore->saveSession(s);
    if (!saveRes.success) {
        qDebug() << "saveSession failed:" << saveRes.message;
    }
    QVERIFY(saveRes.success);

    // 4. Construct the Graph
    auto messageStorage = std::make_shared<SqliteMessageStorage>("test_prod_e2ee.db");
    auto messageQueue = std::make_shared<MessageQueue>("test_prod_e2ee.db");

    std::shared_ptr<Services::RelayService> relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, nullptr);

    auto offlineQueueService = std::make_shared<OfflineQueueService>(messageQueue,
        [relayService](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) {
            qDebug() << "OfflineQueue->RelayService: sendEncryptedEnvelope!";
            relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env); return true;
            return true;
        });

    auto sessionManager = std::make_shared<SessionManager>(
        secureStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [offlineQueueService](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            qDebug() << "SessionManager->OfflineQueue: onEnvelopeReady!";
            if (offlineQueueService) {
                offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
                return true;
            }
            return false;
        }, nullptr);

    auto coreMessageService = std::make_shared<NeoNect::Core::Messaging::MessageService>(messageStorage, sessionManager, messageQueue);
    auto legacyMessageService = std::make_unique<Services::MessageService>(messageStorage);
    legacyMessageService->setCurrentUserId("alice");

    // 5. Connect Adapters (Same as application.cpp)
    auto ptr = coreMessageService;
    QObject::connect(legacyMessageService.get(), &Services::MessageService::transmitMessage, relayService.get(), [ptr](const NeoNect::Domain::Message& msg) {
        qDebug() << "Adapter: received transmitMessage! Converting and passing to CoreMessageService...";
        NeoNect::Core::Messaging::Message coreMsg;
        coreMsg.messageId = msg.id;
        coreMsg.conversationId = msg.conversationId;
        coreMsg.senderId = msg.senderId;
        coreMsg.receiverId = msg.conversationId.startsWith("dms:") ? msg.conversationId.mid(4) : msg.conversationId;
        coreMsg.timestamp = msg.timestamp;
        coreMsg.plaintext = msg.text;
        coreMsg.state = NeoNect::Core::Messaging::MessageState::CREATED;

        coreMsg.senderId = "alice"; // mock sender for consistency

        bool success = ptr->sendMessage(coreMsg);
        qDebug() << "coreMessageService->sendMessage returned:" << success;
    });

    QSignalSpy spyRelayStatus(relayService.get(), &Services::RelayService::messageTransmissionStatus);
    QSignalSpy spyTransmit(legacyMessageService.get(), &Services::MessageService::transmitMessage);

    // 6. Execute Trigger
    qDebug() << "TRIGGER: legacyMessageService->sendMessage...";
    legacyMessageService->sendMessage("dms:bob", "Hello Bob", "text");

    // Wait for async events
    qDebug() << "WAITING for spyTransmit...";
    spyTransmit.wait(1000);
    qDebug() << "WAITING for spyRelayStatus...";
    spyRelayStatus.wait(1000);
    qDebug() << "WAIT DONE.";

    // 7. Verify
    // Queue should have entry, transmission should have emitted status
    QVERIFY(spyRelayStatus.count() >= 1);
    QList<QVariant> args = spyRelayStatus.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("bob"));
    QVERIFY(args.at(2).toBool() == true); // success
}

void TestProductionE2EE::testFirstMessageProductionPath() {
    QFile::remove("test_prod_alice_secure.db");
    QFile::remove("test_prod_bob_secure.db");
    QFile::remove("test_prod_alice_e2ee.db");
    // 1. Initialize Bob's environment (for real keys)
    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();

    auto bobIdentity = backend->GenerateX25519KeyPair();
    auto bobSpkKp = backend->GenerateX25519KeyPair();
    auto bobOpkKp = backend->GenerateX25519KeyPair();

    Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    std::copy(bobIdentity.second.data.data(), bobIdentity.second.data.data() + 32, bobIk.publicKey.data.data());
    bobIk.privateKey.data.resize(32);
    std::copy(bobIdentity.first.data.data(), bobIdentity.first.data.data() + 32, bobIk.privateKey.data.data());

    Crypto::SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
    bobSpk.publicKey.data.resize(32);
    std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());

    QByteArray spkBytes = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
    bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

    Crypto::OneTimePreKey bobOpk;
    bobOpk.id = 1;
    bobOpk.publicKey.data.resize(32);
    std::copy(bobOpkKp.second.data.data(), bobOpkKp.second.data.data() + 32, bobOpk.publicKey.data.data());
    bobOpk.privateKey.data.resize(32);
    std::copy(bobOpkKp.first.data.data(), bobOpkKp.first.data.data() + 32, bobOpk.privateKey.data.data());

    // 2. Mock Transport for Alice
    auto mockTransport = std::make_shared<Testing::MockHttpTransport>(true, false);
    mockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"success\":true}", 200);

    QJsonObject spkJson;
    spkJson["key_id"] = static_cast<int>(bobSpk.id);
    spkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpk.publicKey.data.data()), 32).toBase64());
    spkJson["signature"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpk.signature.data.data()), 64).toBase64());

    QJsonObject opkJson;
    opkJson["key_id"] = static_cast<int>(bobOpk.id);
    opkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobOpk.publicKey.data.data()), 32).toBase64());

    QJsonObject bundleJson;
    bundleJson["identity_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32).toBase64());
    bundleJson["signed_curve_prekey"] = spkJson;
    bundleJson["one_time_curve_prekey"] = opkJson;

    mockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(bundleJson).toJson(), 200);

    // 3. Initialize Alice's Crypto Core
    auto platformSecretStore = std::make_shared<DummySecretStore>();
    auto keyProvider = std::make_shared<Storage::MasterKeyProvider>(platformSecretStore);
    auto secureStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(secureStore->initialize("test_prod_alice_secure.db").success);

    Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    secureStore->saveIdentity(aliceId);

    auto x3dh = std::make_shared<X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<DoubleRatchet::AEAD>(backend.get());

    auto preKeyAdapter = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(secureStore);
    auto preKeyService = std::make_shared<Services::PreKeyService>(mockTransport, preKeyAdapter);

    // 4. Initialize Core Messaging
    auto settingsStore = std::make_shared<Storage::SettingsRepository>("test");
    auto relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, nullptr, nullptr);
    auto messageStorage = std::make_shared<SqliteMessageStorage>("test_prod_alice_e2ee.db");
    auto messageQueue = std::make_shared<MessageQueue>("test_prod_alice_e2ee.db");

    auto offlineQueueService = std::make_shared<OfflineQueueService>(messageQueue,
        [relayService](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env); return true;
            return true;
        });

    auto sessionManager = std::make_shared<SessionManager>(
        secureStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [offlineQueueService](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            if (offlineQueueService) {
                offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
                return true;
            }
            return false;
        }, nullptr);

    auto coreMessageService = std::make_shared<Core::Messaging::MessageService>(messageStorage, sessionManager, messageQueue);

    coreMessageService->setPreKeyClaimRequestCallback([preKeyService](const QString& targetUser, const QString& targetDevice, auto resultCb) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(preKeyService.get(), &Services::PreKeyService::preKeyBundleClaimed,
            [resultCb, targetUser, targetDevice, connection](const QString& retUser, const QString& retDev, std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                if (retUser == targetUser && retDev == targetDevice) {
                    QObject::disconnect(*connection);
                    resultCb(bundle);
                }
            });
        preKeyService->claimPreKeys(targetUser, targetDevice);
    });

    // relayService already instantiated above

    QSignalSpy spyRelayStatus(relayService.get(), &Services::RelayService::messageTransmissionStatus);
    QSignalSpy spyReq(mockTransport.get(), &Testing::MockHttpTransport::rawRequestData);

    // 5. Send message!
    Core::Messaging::Message msg;
    msg.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = "dms:bob";
    msg.senderId = "alice";
    msg.receiverId = "bob";
    msg.plaintext = "Hello Bob!";
    msg.state = Core::Messaging::MessageState::CREATED;

    bool sent = coreMessageService->sendMessage(msg);
    QVERIFY(sent); // Accepted

    // Wait for the async claim to finish and relay to send!
    QVERIFY(spyRelayStatus.count() >= 1 || spyRelayStatus.wait(2000));
    QVERIFY(spyRelayStatus.count() >= 1);

    auto args = spyRelayStatus.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("bob"));
    QVERIFY(args.at(2).toBool() == true); // Success

    // 6. Intercept envelope from HTTP Mock
    QByteArray envelopeBase64;
    for (const auto& reqList : spyReq) {
        QByteArray req = reqList[0].toByteArray();
        if (req.contains("ciphertext")) {
            QJsonDocument doc = QJsonDocument::fromJson(req);
            envelopeBase64 = doc.object()["ciphertext"].toString().toUtf8();
        }
    }
    QByteArray envBytes = QByteArray::fromBase64(envelopeBase64);

    // 7. Bob processes envelope
    auto bobStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(bobStore->initialize("test_prod_bob_secure.db").success);
    Storage::E2EEIdentity bId;
    bId.identity_id = 1;
    bId.version = 1;
    bId.created_at = QDateTime::currentMSecsSinceEpoch();
    bId.public_key = QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32);
    bId.private_key = QByteArray(reinterpret_cast<const char*>(bobIk.privateKey.data.data()), 32);
    bobStore->saveIdentity(bId);

    // Oh wait, Bob needs his OPK and SPK in the prekey store.
    auto bobPreKeyStore = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(bobStore);
    bobPreKeyStore->storeSignedPreKey(std::move(bobSpk));
    std::vector<Crypto::OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    bobPreKeyStore->storeOneTimePreKeys(std::move(opks));

    QByteArray bobDecryptedPlaintext;
    auto bobSessionManager = std::make_shared<SessionManager>(
        bobStore, x3dh, ratchet, aead, backend, bobPreKeyStore, xeddsa, nullptr,
        [&](const QString& sid, const QByteArray& pt) {
            bobDecryptedPlaintext = pt;
        });

    auto handleRes = bobSessionManager->handleEnvelope(envBytes, Transport::TransportMetadata{});
    if (!handleRes.success) { qDebug() << "Bob handleEnvelope failed:" << handleRes.message; } QVERIFY(handleRes.success);

    // 8. Verify Bob's decryption
    QJsonDocument ptDoc = QJsonDocument::fromJson(bobDecryptedPlaintext);
    QCOMPARE(ptDoc.object()["plaintext"].toString(), QString("Hello Bob!"));

    // 9. Verify Second Message Uses Established Session
    Core::Messaging::Message msg2;
    msg2.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg2.conversationId = "dms:bob";
    msg2.senderId = "alice";
    msg2.receiverId = "bob";
    msg2.plaintext = "Second message";
    msg2.state = Core::Messaging::MessageState::CREATED;

    QVERIFY(coreMessageService->sendMessage(msg2));
    QVERIFY(spyRelayStatus.count() >= 1 || spyRelayStatus.wait(2000));
}

void TestProductionE2EE::testFirstMessageFailures() {
    QFile::remove("test_prod_alice_secure_fail.db");
    QFile::remove("test_prod_alice_e2ee_fail.db");
    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();

    // Set up mock transport for failures
    auto mockTransport = std::make_shared<Testing::MockHttpTransport>(true, false);

    // 3. Initialize Alice's Crypto Core
    auto platformSecretStore = std::make_shared<DummySecretStore>();
    auto keyProvider = std::make_shared<Storage::MasterKeyProvider>(platformSecretStore);
    auto secureStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(secureStore->initialize("test_prod_alice_secure_fail.db").success);

    Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    secureStore->saveIdentity(aliceId);

    auto x3dh = std::make_shared<X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<DoubleRatchet::AEAD>(backend.get());

    auto preKeyAdapter = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(secureStore);
    auto preKeyService = std::make_shared<Services::PreKeyService>(mockTransport, preKeyAdapter);

    // 4. Initialize Core Messaging with a fake queue that can fail
    auto messageStorage = std::make_shared<SqliteMessageStorage>("test_prod_alice_e2ee_fail.db");
    auto messageQueue = std::make_shared<MessageQueue>("test_prod_alice_e2ee_fail.db");
    auto proxyMessageQueue = std::make_shared<ProxyMessageQueue>(messageQueue);

    auto settingsStore = std::make_shared<Storage::SettingsRepository>("test");
    auto relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, nullptr, nullptr);


    auto offlineQueueService = std::make_shared<OfflineQueueService>(proxyMessageQueue,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env); return true;
        });

    auto sessionManager = std::make_shared<SessionManager>(
        secureStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            if (offlineQueueService) {
                return offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
            }
            return false;
        }, nullptr);

    auto coreMessageService = std::make_shared<Core::Messaging::MessageService>(messageStorage, sessionManager, messageQueue);

    coreMessageService->setPreKeyClaimRequestCallback([preKeyService](const QString& targetUser, const QString& targetDevice, auto resultCb) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(preKeyService.get(), &Services::PreKeyService::preKeyBundleClaimed,
            [resultCb, targetUser, targetDevice, connection](const QString& retUser, const QString& retDev, std::optional<Crypto::X3DH::BobPreKeyBundle> bundle) {
                if (retUser == targetUser && retDev == targetDevice) {
                    QObject::disconnect(*connection);
                    resultCb(bundle);
                }
            });
        preKeyService->claimPreKeys(targetUser, targetDevice);
    });

    QSignalSpy spyRelayStatus(relayService.get(), &Services::RelayService::messageTransmissionStatus);


    // Scenario 1: PreKey claim failure -> message fails cleanly -> no session persisted
    mockTransport->setSimulatedResponse("/api/v1/keys/claim", "{}", 404);

    Core::Messaging::Message msg1;
    msg1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg1.state = Core::Messaging::MessageState::CREATED;
    msg1.conversationId = "dms:bob_fail";
    msg1.senderId = "alice";
    msg1.receiverId = "bob_fail";
    msg1.plaintext = "Failing message";
    msg1.state = Core::Messaging::MessageState::CREATED;

    QVERIFY(coreMessageService->sendMessage(msg1));


    auto msg1State = messageStorage->getMessage(msg1.messageId);
    QVERIFY(msg1State.has_value());
    QCOMPARE(msg1State.value().state, Core::Messaging::MessageState::FAILED);
    QVERIFY(!sessionManager->hasSession("bob_fail:default_device"));

    // Scenario 2: Invalid/malformed PreKey Bundle -> message fails -> no session persisted
    QJsonObject badBundle;
    badBundle["identity_key"] = "NOT_BASE64!!!!";
    mockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(badBundle).toJson(), 200);

    msg1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg1.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(coreMessageService->sendMessage(msg1));

    msg1State = messageStorage->getMessage(msg1.messageId);
    QCOMPARE(msg1State.value().state, Core::Messaging::MessageState::FAILED);
    QVERIFY(!sessionManager->hasSession("bob_fail:default_device"));

    // Scenario 4: Queue acceptance failure -> message fails -> no durable session persisted
    // First, let's create a valid Bob bundle
    auto bobIdentity = backend->GenerateX25519KeyPair();
    auto bobSpkKp = backend->GenerateX25519KeyPair();
    Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    std::copy(bobIdentity.second.data.data(), bobIdentity.second.data.data() + 32, bobIk.publicKey.data.data());
    bobIk.privateKey.data.resize(32);
    std::copy(bobIdentity.first.data.data(), bobIdentity.first.data.data() + 32, bobIk.privateKey.data.data());

    Crypto::SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
    bobSpk.publicKey.data.resize(32);
    std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
    QByteArray spkBytes = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
    bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

    QJsonObject spkJson;
    spkJson["key_id"] = 1;
    spkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpk.publicKey.data.data()), 32).toBase64());
    spkJson["signature"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpk.signature.data.data()), 64).toBase64());

    QJsonObject validBundle;
    validBundle["identity_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32).toBase64());
    validBundle["signed_curve_prekey"] = spkJson;

    mockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(validBundle).toJson(), 200);

    proxyMessageQueue->failEnqueue = true; // Inject failure in the send lambda!
    msg1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg1.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(coreMessageService->sendMessage(msg1));

    msg1State = messageStorage->getMessage(msg1.messageId);
    QCOMPARE(msg1State.value().state, Core::Messaging::MessageState::FAILED);
    // Because queue failed, SessionManager returned false, session creation reverted!
    QVERIFY(!sessionManager->hasSession("bob_fail:default_device"));
}

void TestProductionE2EE::testApplicationIntegrationFirstMessage() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(QDir(baseDir).filePath("messages_apptest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_apptest.db"));
    QFile::remove(QDir(baseDir).filePath("messages_apptest_guest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_apptest_guest.db"));

    int argc = 3;
    const char* argv[] = {"NeoNectTests", "--mock", "--profile=apptest"};
    NeoNect::Application app(argc, const_cast<char**>(argv));

    auto mockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(app.m_transport);
    QVERIFY(mockTransport);

    // Create Bob's Identity
    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();

    auto bobKp = backend->GenerateX25519KeyPair();
    NeoNect::Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    std::copy(bobKp.second.data.data(), bobKp.second.data.data() + 32, bobIk.publicKey.data.data());
    std::copy(bobKp.first.data.data(), bobKp.first.data.data() + 32, bobIk.privateKey.data.data());

    auto bobSpkKp = backend->GenerateX25519KeyPair();
    auto opkKp = backend->GenerateX25519KeyPair();

    QByteArray spkBytes;
    spkBytes.append(0x05);
    spkBytes.append(reinterpret_cast<const char*>(bobSpkKp.second.data.data()), bobSpkKp.second.data.size());

    auto spkSig = xeddsa->sign(bobIk.privateKey, NeoNect::Crypto::ByteView{reinterpret_cast<const uint8_t*>(spkBytes.data()), (size_t)spkBytes.size()});

    QJsonObject spkJson;
    spkJson["key_id"] = 1;
    spkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpkKp.second.data.data()), 32).toBase64());
    spkJson["signature"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(spkSig.data.data()), 64).toBase64());

    QJsonObject opkJson;
    opkJson["key_id"] = 1;
    opkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(opkKp.second.data.data()), 32).toBase64());

    QJsonObject bundleJson;
    bundleJson["identity_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32).toBase64());
    bundleJson["signed_curve_prekey"] = spkJson;
    bundleJson["one_time_curve_prekey"] = opkJson;

    mockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(bundleJson).toJson(), 200);
    mockTransport->setSimulatedResponse("/api/v1/messages/relay", "{\"status\":\"success\"}", 200);

        // We MUST initialize Alice's identity key in the app's secure store!
    NeoNect::Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    auto saveIdRes = app.m_secureStore->saveIdentity(aliceId);
    if(!saveIdRes.success) qDebug() << "saveIdentity FAILED:" << saveIdRes.message;
    QVERIFY(saveIdRes.success);

    // Alice sends first message
    NeoNect::Core::Messaging::Message msg;
    msg.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.receiverId = "bob_app";
    msg.senderId = "guest";
    msg.conversationId = "dms:bob_app";
    msg.plaintext = "Integration Test Message";

    QSignalSpy spyRelayStatus(app.m_relayService.get(), &NeoNect::Services::RelayService::messageTransmissionStatus);

    qDebug() << "About to send message! bobId=" << msg.receiverId;
    QVERIFY(app.m_coreMessageService->sendMessage(msg));

    QVERIFY(spyRelayStatus.count() >= 1 || spyRelayStatus.wait(2000));
    QVERIFY(spyRelayStatus.count() >= 1);

    QVERIFY(app.m_sessionManager->hasSession("bob_app:default_device"));
}

void TestProductionE2EE::testApplicationIntegrationIncomingPath() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(QDir(baseDir).filePath("messages_apptest_bob_guest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_apptest_bob_guest.db"));
    QFile::remove(QDir(baseDir).filePath("messages_apptest_alice_guest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_apptest_alice_guest.db"));
    QFile::remove("test_prod_alice_secure_standalone.db");

    // 1. Start Bob
    int argc = 3;
    const char* argvBob[] = {"NeoNectTests", "--mock", "--profile=apptest_bob"};
    NeoNect::Application bobApp(argc, const_cast<char**>(argvBob));
    auto bobMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(bobApp.m_transport);
    QVERIFY(bobMockTransport);

    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    NeoNect::Storage::E2EEIdentity bobId;
    bobId.identity_id = 1;
    auto bobKp = backend->GenerateX25519KeyPair();
    bobId.public_key = QByteArray(reinterpret_cast<const char*>(bobKp.second.data.data()), 32);
    bobId.private_key = QByteArray(reinterpret_cast<const char*>(bobKp.first.data.data()), 32);
    QVERIFY(bobApp.m_secureStore->saveIdentity(bobId).success);

    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();
    NeoNect::Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    std::copy(bobKp.second.data.data(), bobKp.second.data.data() + 32, bobIk.publicKey.data.data());
    std::copy(bobKp.first.data.data(), bobKp.first.data.data() + 32, bobIk.privateKey.data.data());

    auto bobSpkKp = backend->GenerateX25519KeyPair();
    auto bobOpkKp = backend->GenerateX25519KeyPair();

    NeoNect::Crypto::SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.publicKey.data.resize(32);
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
    QByteArray spkBytes = NeoNect::Crypto::KeyEncoding::Encode(bobSpk.publicKey);
    NeoNect::Crypto::ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
    bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

    NeoNect::Crypto::OneTimePreKey bobOpk;
    bobOpk.id = 1;
    bobOpk.publicKey.data.resize(32);
    bobOpk.privateKey.data.resize(32);
    std::copy(bobOpkKp.second.data.data(), bobOpkKp.second.data.data() + 32, bobOpk.publicKey.data.data());
    std::copy(bobOpkKp.first.data.data(), bobOpkKp.first.data.data() + 32, bobOpk.privateKey.data.data());

    // 2. Generate a valid InitialEnvelope using standalone Alice E2EE components (just like in testFirstMessageProductionPath)
    auto alicePlatformSecretStore = std::make_shared<DummySecretStore>();
    auto aliceKeyProvider = std::make_shared<NeoNect::Storage::MasterKeyProvider>(alicePlatformSecretStore);
    auto aliceSecureStore = std::make_shared<NeoNect::Storage::SecureE2EEStore>(aliceKeyProvider);
    QVERIFY(aliceSecureStore->initialize("test_prod_alice_secure_standalone.db").success);

    NeoNect::Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    aliceSecureStore->saveIdentity(aliceId);

    auto aliceX3dh = std::make_shared<NeoNect::Crypto::X3DH::X3DHImpl>();
    auto aliceRatchet = std::make_shared<NeoNect::Crypto::DoubleRatchet::Engine>(backend);
    auto aliceAead = std::make_shared<NeoNect::Crypto::DoubleRatchet::AEAD>(backend.get());
    auto alicePreKeyAdapter = std::make_shared<NeoNect::Crypto::Session::SecurePreKeyStoreAdapter>(aliceSecureStore);

    QByteArray interceptedData;
    auto aliceSessionManager = std::make_shared<NeoNect::Crypto::Session::SessionManager>(
        aliceSecureStore, aliceX3dh, aliceRatchet, aliceAead, backend, alicePreKeyAdapter, xeddsa,
        [&interceptedData](const QString&, const QString&, const QString&, const QByteArray& env) -> bool {
            interceptedData = env;
            return true;
        },
        nullptr
    );

    NeoNect::Crypto::X3DH::BobPreKeyBundle bundle;
    bundle.identityKey.data.resize(32);
    std::copy(bobIk.publicKey.data.data(), bobIk.publicKey.data.data() + 32, bundle.identityKey.data.data());
    bundle.signedPreKey = bobSpk.publicKey;
    bundle.signedPreKeySignature = bobSpk.signature;
    bundle.signedPreKeyId = 1;
    bundle.oneTimePreKeyId = 1;
    bundle.oneTimePreKey = bobOpk.publicKey;
    NeoNect::Crypto::Session::SecurePreKeyStoreAdapter(bobApp.m_secureStore).storeSignedPreKey(std::move(bobSpk));
    std::vector<NeoNect::Crypto::OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    NeoNect::Crypto::Session::SecurePreKeyStoreAdapter(bobApp.m_secureStore).storeOneTimePreKeys(std::move(opks));

    QJsonObject payloadObj;
    payloadObj["messageId"] = "msg-1234";
    payloadObj["conversationId"] = "dms:bob_app";
    payloadObj["senderId"] = "apptest_alice";
    payloadObj["plaintext"] = "Hello Bob Incoming!";
    payloadObj["timestamp"] = 123456789;

    auto res = aliceSessionManager->createSession("bob_app", "default_device", bundle, "msg-1234", QJsonDocument(payloadObj).toJson(QJsonDocument::Compact));
    QVERIFY2(res.success, res.message.toUtf8().constData());

    QVERIFY(!interceptedData.isEmpty());

    // 3. Deliver that envelope through Bob's production RelayService incoming path
    QSignalSpy spyBobRecv(bobApp.m_messageService.get(), &NeoNect::Services::MessageService::messageAdded);

    QJsonObject relayObj;
    relayObj["id"] = 1001;
    relayObj["ciphertext"] = QString::fromLatin1(interceptedData.toBase64());
    relayObj["sender_device_id"] = "apptest_alice:default_device";

    QMetaObject::invokeMethod(bobApp.m_relayService.get(), "onWebSocketMessageReceived", Q_ARG(QString, QString::fromUtf8(QJsonDocument(relayObj).toJson())));

    // 4. Verify Bob receives the message
    QVERIFY(spyBobRecv.count() >= 1 || spyBobRecv.wait(2000));
    QVERIFY(spyBobRecv.count() >= 1);

    QList<QVariant> args = spyBobRecv.takeFirst();
    QVariantMap msgMap = args.at(1).toMap();
    QCOMPARE(msgMap["text"].toString(), QString("Hello Bob Incoming!"));
    QCOMPARE(msgMap["senderId"].toString(), QString("apptest_alice"));

    // Verify Bob's storage
    auto bobMsg = bobApp.m_messageStorage->getMessage("msg-1234");
    QVERIFY(bobMsg.has_value());
    QCOMPARE(bobMsg.value().plaintext, QString("Hello Bob Incoming!"));
}
void TestProductionE2EE::testBidirectionalEstablishedSession() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // 1. Cleanup databases for both Alice and Bob
    QFile::remove(QDir(baseDir).filePath("messages_bidi_alice_guest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_bidi_alice_guest.db"));
    QFile::remove(QDir(baseDir).filePath("messages_bidi_bob_guest.db"));
    QFile::remove(QDir(baseDir).filePath("e2ee_bidi_bob_guest.db"));

    // 2. Start Alice Application
    int argcAlice = 3;
    const char* argvAlice[] = {"NeoNectTests", "--mock", "--profile=bidi_alice"};
    NeoNect::Application aliceApp(argcAlice, const_cast<char**>(argvAlice));
    auto aliceMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(aliceApp.m_transport);
    QVERIFY(aliceMockTransport);

    // 3. Start Bob Application
    int argcBob = 3;
    const char* argvBob[] = {"NeoNectTests", "--mock", "--profile=bidi_bob"};
    NeoNect::Application bobApp(argcBob, const_cast<char**>(argvBob));
    auto bobMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(bobApp.m_transport);
    QVERIFY(bobMockTransport);

    aliceApp.m_storage->setAuthToken("fake_token");
    aliceApp.m_storage->setDeviceId("default_device");
    bobApp.m_storage->setAuthToken("fake_token");
    bobApp.m_storage->setDeviceId("default_device");

    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();

    // 4. Generate & Save Identities
    NeoNect::Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    QVERIFY(aliceApp.m_secureStore->saveIdentity(aliceId).success);

    NeoNect::Storage::E2EEIdentity bobId;
    bobId.identity_id = 1;
    auto bobKp = backend->GenerateX25519KeyPair();
    bobId.public_key = QByteArray(reinterpret_cast<const char*>(bobKp.second.data.data()), 32);
    bobId.private_key = QByteArray(reinterpret_cast<const char*>(bobKp.first.data.data()), 32);
    QVERIFY(bobApp.m_secureStore->saveIdentity(bobId).success);

    NeoNect::Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    std::copy(bobKp.second.data.data(), bobKp.second.data.data() + 32, bobIk.publicKey.data.data());
    std::copy(bobKp.first.data.data(), bobKp.first.data.data() + 32, bobIk.privateKey.data.data());

    // 5. Generate Bob PreKeys
    auto bobSpkKp = backend->GenerateX25519KeyPair();
    auto bobOpkKp = backend->GenerateX25519KeyPair();

    NeoNect::Crypto::SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.publicKey.data.resize(32);
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
    QByteArray spkBytes = NeoNect::Crypto::KeyEncoding::Encode(bobSpk.publicKey);
    NeoNect::Crypto::ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
    bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

    NeoNect::Crypto::OneTimePreKey bobOpk;
    bobOpk.id = 1;
    bobOpk.publicKey.data.resize(32);
    bobOpk.privateKey.data.resize(32);
    std::copy(bobOpkKp.second.data.data(), bobOpkKp.second.data.data() + 32, bobOpk.publicKey.data.data());
    std::copy(bobOpkKp.first.data.data(), bobOpkKp.first.data.data() + 32, bobOpk.privateKey.data.data());

    // Configure Alice Mock Server to serve Bob's PreKeys
    QJsonObject spkJson;
    spkJson["key_id"] = 1;
    spkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpkKp.second.data.data()), 32).toBase64());
    spkJson["signature"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobSpk.signature.data.data()), 64).toBase64());

    QJsonObject opkJson;
    opkJson["key_id"] = 1;
    opkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobOpkKp.second.data.data()), 32).toBase64());

    QJsonObject bundleJson;
    bundleJson["identity_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32).toBase64());
    bundleJson["signed_curve_prekey"] = spkJson;
    bundleJson["one_time_curve_prekey"] = opkJson;

    NeoNect::Crypto::Session::SecurePreKeyStoreAdapter(bobApp.m_secureStore).storeSignedPreKey(std::move(bobSpk));
    std::vector<NeoNect::Crypto::OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    NeoNect::Crypto::Session::SecurePreKeyStoreAdapter(bobApp.m_secureStore).storeOneTimePreKeys(std::move(opks));

    aliceMockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(bundleJson).toJson(), 200);
    aliceMockTransport->setSimulatedResponse("/api/v1/messages/relay", "{\"status\":\"success\"}", 200);
    bobMockTransport->setSimulatedResponse("/api/v1/messages/relay", "{\"status\":\"success\"}", 200);

    // Capture ciphertexts
    QSignalSpy spyAliceReq(aliceMockTransport.get(), &NeoNect::Testing::MockHttpTransport::rawRequestData);
    QSignalSpy spyBobReq(bobMockTransport.get(), &NeoNect::Testing::MockHttpTransport::rawRequestData);

    // Helpers to extract ciphertext and deliver
    auto extractCiphertext = [](QSignalSpy& spy) -> QString {
        QString envelopeBase64;
        for (const auto& reqList : spy) {
            QByteArray req = reqList[0].toByteArray();
            qDebug() << "[TEST] Extracted request:" << req;
            if (req.contains("ciphertext")) {
                QJsonDocument doc = QJsonDocument::fromJson(req);
                envelopeBase64 = doc.object()["ciphertext"].toString();
            }
        }
        return envelopeBase64;
    };

    auto deliverTo = [](NeoNect::Application& targetApp, const QString& cipher, const QString& senderStr) {
        QJsonObject relayObj;
        relayObj["id"] = static_cast<int>(QRandomGenerator::global()->generate());
        relayObj["ciphertext"] = cipher;
        relayObj["sender_device_id"] = senderStr + ":default_device";
        QMetaObject::invokeMethod(targetApp.m_relayService.get(), "onWebSocketMessageReceived", Q_ARG(QString, QString::fromUtf8(QJsonDocument(relayObj).toJson())));
    };

    QSignalSpy spyAliceRecv(aliceApp.m_messageService.get(), &NeoNect::Services::MessageService::messageAdded);
    QSignalSpy spyBobRecv(bobApp.m_messageService.get(), &NeoNect::Services::MessageService::messageAdded);

    // 6. Alice sends M1 (Initial X3DH + Ratchet) to Bob
    NeoNect::Core::Messaging::Message m1;
    m1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m1.receiverId = "bidi_bob";
    m1.senderId = "bidi_alice";
    m1.conversationId = "dms:bidi_bob";
    m1.plaintext = "Alice to Bob 1";
    
    QVERIFY(aliceApp.m_coreMessageService->sendMessage(m1));
    
    QTRY_VERIFY([&]() {
        auto msgState = aliceApp.m_messageStorage->getMessage(m1.messageId);
        return msgState.has_value() && (msgState->state == NeoNect::Core::Messaging::MessageState::SENT || msgState->state == NeoNect::Core::Messaging::MessageState::FAILED);
    }());
    
    auto msg1State = aliceApp.m_messageStorage->getMessage(m1.messageId);
    QVERIFY(msg1State.has_value());
    if (msg1State->state == NeoNect::Core::Messaging::MessageState::FAILED) {
        qDebug() << "[TEST] M1 FAILED to create session or enqueue!";
    }
    QCOMPARE(msg1State->state, NeoNect::Core::Messaging::MessageState::SENT);

    QTRY_VERIFY(!extractCiphertext(spyAliceReq).isEmpty());
    QString cipherM1 = extractCiphertext(spyAliceReq);
    spyAliceReq.clear();
    QVERIFY(!cipherM1.isEmpty());

    deliverTo(bobApp, cipherM1, "bidi_alice");
    QVERIFY(spyBobRecv.wait(2000) || spyBobRecv.count() >= 1);
    QVERIFY(spyBobRecv.count() >= 1);
    QCOMPARE(spyBobRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Alice to Bob 1"));

    // 7. Bob sends M2 (Established Session Ratchet) back to Alice
    NeoNect::Core::Messaging::Message m2;
    m2.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m2.receiverId = "bidi_alice";
    m2.senderId = "bidi_bob";
    m2.conversationId = "dms:bidi_alice";
    m2.plaintext = "Bob to Alice 1";
    
    QVERIFY(bobApp.m_coreMessageService->sendMessage(m2));
    QTRY_VERIFY(!extractCiphertext(spyBobReq).isEmpty());
    QString cipherM2 = extractCiphertext(spyBobReq);
    spyBobReq.clear();
    QVERIFY(!cipherM2.isEmpty());

    deliverTo(aliceApp, cipherM2, "bidi_bob");
    QVERIFY(spyAliceRecv.wait(2000) || spyAliceRecv.count() >= 1);
    QVERIFY(spyAliceRecv.count() >= 1);
    QCOMPARE(spyAliceRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Bob to Alice 1"));

    // 8. Alice sends M3 and M4 (Out of order delivery)
    NeoNect::Core::Messaging::Message m3;
    m3.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m3.receiverId = "bidi_bob";
    m3.senderId = "bidi_alice";
    m3.conversationId = "dms:bidi_bob";
    m3.plaintext = "Alice to Bob 2 (Delayed)";
    
    QVERIFY(aliceApp.m_coreMessageService->sendMessage(m3));
    QTRY_VERIFY(!extractCiphertext(spyAliceReq).isEmpty());
    QString cipherM3 = extractCiphertext(spyAliceReq);
    spyAliceReq.clear();

    NeoNect::Core::Messaging::Message m4;
    m4.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m4.receiverId = "bidi_bob";
    m4.senderId = "bidi_alice";
    m4.conversationId = "dms:bidi_bob";
    m4.plaintext = "Alice to Bob 3 (Early)";
    
    QVERIFY(aliceApp.m_coreMessageService->sendMessage(m4));
    QTRY_VERIFY(!extractCiphertext(spyAliceReq).isEmpty());
    QString cipherM4 = extractCiphertext(spyAliceReq);
    spyAliceReq.clear();

    // Deliver M4 first
    deliverTo(bobApp, cipherM4, "bidi_alice");
    QVERIFY(spyBobRecv.wait(2000) || spyBobRecv.count() >= 1);
    QVERIFY(spyBobRecv.count() >= 1);
    QCOMPARE(spyBobRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Alice to Bob 3 (Early)"));

    // Deliver M3 later
    deliverTo(bobApp, cipherM3, "bidi_alice");
    QVERIFY(spyBobRecv.wait(2000) || spyBobRecv.count() >= 1);
    QVERIFY(spyBobRecv.count() >= 1);
    QCOMPARE(spyBobRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Alice to Bob 2 (Delayed)"));

    // 9. Replay Attack (Deliver M3 again)
    deliverTo(bobApp, cipherM3, "bidi_alice");
    QTest::qWait(100);
    QVERIFY(spyBobRecv.count() == 0);

    // 10. Tampering Attack
    NeoNect::Core::Messaging::Message m5;
    m5.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m5.receiverId = "bidi_alice";
    m5.senderId = "bidi_bob";
    m5.conversationId = "dms:bidi_alice";
    m5.plaintext = "Bob to Alice 2 (Tampered)";
    
    QVERIFY(bobApp.m_coreMessageService->sendMessage(m5));
    QTRY_VERIFY(!extractCiphertext(spyBobReq).isEmpty());
    QString cipherM5 = extractCiphertext(spyBobReq);
    spyBobReq.clear();
    
    QByteArray rawCipherM5 = QByteArray::fromBase64(cipherM5.toLatin1());
    if (rawCipherM5.size() > 10) {
        rawCipherM5[rawCipherM5.size() - 5] = rawCipherM5[rawCipherM5.size() - 5] ^ 0x01;
    }
    deliverTo(aliceApp, QString::fromLatin1(rawCipherM5.toBase64()), "bidi_bob");
    
    QTest::qWait(100);
    QVERIFY(spyAliceRecv.count() == 0); // Should fail to decrypt
}
