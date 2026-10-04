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
    s.session_id = "sess_bob";
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
    
    auto settingsStore = std::make_shared<Storage::SettingsRepository>("test");
    auto relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, nullptr, nullptr);
    bool shouldQueueFail = false;
    
    auto offlineQueueService = std::make_shared<OfflineQueueService>(messageQueue,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env); return true;
        });
    
    auto sessionManager = std::make_shared<SessionManager>(
        secureStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            if (shouldQueueFail) return false;
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
    QVERIFY(!sessionManager->hasSession("sess_bob_fail_default_device"));
    
    // Scenario 2: Invalid/malformed PreKey Bundle -> message fails -> no session persisted
    QJsonObject badBundle;
    badBundle["identity_key"] = "NOT_BASE64!!!!";
    mockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(badBundle).toJson(), 200);
    
    msg1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg1.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(coreMessageService->sendMessage(msg1));
    
    msg1State = messageStorage->getMessage(msg1.messageId);
    QCOMPARE(msg1State.value().state, Core::Messaging::MessageState::FAILED);
    QVERIFY(!sessionManager->hasSession("sess_bob_fail_default_device"));

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

    shouldQueueFail = true; // Inject failure in the send lambda!
    msg1.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg1.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(coreMessageService->sendMessage(msg1));
    
    msg1State = messageStorage->getMessage(msg1.messageId);
    QCOMPARE(msg1State.value().state, Core::Messaging::MessageState::FAILED);
    // Because queue failed, SessionManager returned false, session creation reverted!
    QVERIFY(!sessionManager->hasSession("sess_bob_fail_default_device"));
}
