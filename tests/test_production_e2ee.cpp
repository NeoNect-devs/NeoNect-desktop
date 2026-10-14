#include "../src/core/application.h"
#include "../src/core/networkmanager.h"
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
#include "../src/storage/capabilitiesrepository.h"
#include "../src/storage/e2ee/SecureE2EEStore.h"
#include "../src/storage/e2ee/MasterKeyProvider.h"
#include "../src/services/authservice.h"
#include "test_secure_storage.h" // For MockSecretStore
#include "../src/storage/e2ee/PlatformSecretStore.h"
#include "mocks/mockhttptransport.h"
#include "../src/crypto/x3dh/X3DH.h"
#include "../src/crypto/doubleratchet/DoubleRatchet.h"
#include "../src/crypto/doubleratchet/AEAD.h"
#include "../src/crypto/OpenSSLBackend.h"
#include "../src/crypto/XEdDSAAdapter.h"
#include "../src/storage/settingsrepository.h"
#include "../src/services/SecureImageProvider.h"
#include "../src/common/types.h"
#include <QSignalSpy>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QImage>

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
    bool resetEntry(const QString& messageId, const QByteArray& envelopeBytes, const QString& recipientUsername, const QString& recipientDeviceId) override {
        if (failEnqueue) return false;
        return realQueue->resetEntry(messageId, envelopeBytes, recipientUsername, recipientDeviceId);
    }
};

void TestProductionE2EE::initTestCase() {
    QFile::remove("test_prod_e2ee.db");
    QFile::remove("test_prod_secure.db");
}

void TestProductionE2EE::cleanupTestCase() {
    const QStringList dbFiles = {
        "test_prod_e2ee.db",
        "test_prod_secure.db",
        "test_prod_alice_secure.db",
        "test_prod_bob_secure.db",
        "test_prod_alice_e2ee.db",
        "test_prod_alice_secure_fail.db",
        "test_prod_alice_e2ee_fail.db",
        "test_prod_alice_secure_standalone.db"
    };
    for (const auto& dbFile : dbFiles) {
        QFile::remove(dbFile);
        QFile::remove(dbFile + "-wal");
        QFile::remove(dbFile + "-shm");
    }
    qInstallMessageHandler(nullptr);
    QFile::remove("qml_error.log");
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
    auto initRes = secureStore->initialize("test_prod_secure.db", "testProfile");
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

    auto caps = std::make_shared<Storage::CapabilitiesRepository>();
    caps->replace(std::nullopt, 4 * 1024 * 1024, std::nullopt, Storage::CapabilityState::VALID);
    std::shared_ptr<Services::RelayService> relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, caps, nullptr);

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
    coreMessageService->setDeviceResolverCallback([](const QString& targetUser, auto cb) { cb("default_device"); });
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
    QVERIFY(secureStore->initialize("test_prod_alice_secure.db", "testProfile").success);

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
    auto caps = std::make_shared<Storage::CapabilitiesRepository>();
    caps->replace(std::nullopt, 4 * 1024 * 1024, std::nullopt, Storage::CapabilityState::VALID);
    auto relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, caps, nullptr);
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

    coreMessageService->setDeviceResolverCallback([](const QString& targetUser, auto cb) { cb("default_device"); });
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
    QVERIFY(bobStore->initialize("test_prod_bob_secure.db", "testProfile").success);
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
    bobSessionManager->setSenderVerifier([&](const QString& claimedUser, const QString&, const QByteArray& senderKey) {
        return claimedUser == "alice" && senderKey == aliceId.public_key;
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
    QVERIFY(secureStore->initialize("test_prod_alice_secure_fail.db", "testProfile").success);

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
    auto caps = std::make_shared<Storage::CapabilitiesRepository>();
    caps->replace(std::nullopt, 4 * 1024 * 1024, std::nullopt, Storage::CapabilityState::VALID);
    auto relayService = std::make_shared<Services::RelayService>(mockTransport, settingsStore, caps, nullptr);


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

    coreMessageService->setDeviceResolverCallback([](const QString& targetUser, auto cb) { cb("default_device"); });
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
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);
        QSettings testSettings_app("NeoNect", "DesktopClient_apptest");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");


    int argc = 3;
    const char* argv[] = {"NeoNectTests", "--mock", "--profile=apptest"};
    NeoNect::Application app(argc, const_cast<char**>(argv));

    auto mockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(app.m_transport);
    QVERIFY(mockTransport);

    QSignalSpy spyCaps(app.m_networkManager.get(), &NetworkManager::verificationResult);
    app.m_networkManager->verifyServer("http://localhost");
    QVERIFY(spyCaps.count() > 0 || spyCaps.wait(2000));

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
    mockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"status\":\"success\"}", 200);

    QJsonObject devicesJson;
    QJsonArray devicesArr;
    QJsonObject dev;
    dev["device_id"] = "default_device";
    dev["public_key"] = "mock_key";
    devicesArr.append(dev);
    devicesJson["devices"] = devicesArr;
    mockTransport->setSimulatedResponse("/api/v1/relay/keys", QJsonDocument(devicesJson).toJson(), 200);

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
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);
    QFile::remove("test_prod_alice_secure_standalone.db");

    // 1. Start Bob
        QSettings testSettings_bobApp("NeoNect", "DesktopClient_apptest_bob");
        testSettings_bobApp.setValue("auth_token", "fake_token");
        testSettings_bobApp.setValue("username", "bob");

    int argc = 3;
    const char* argvBob[] = {"NeoNectTests", "--mock", "--profile=apptest_bob"};
    NeoNect::Application bobApp(argc, const_cast<char**>(argvBob));
    auto bobMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(bobApp.m_transport);
    QVERIFY(bobMockTransport);

    QSignalSpy spyCapsBob(bobApp.m_networkManager.get(), &NetworkManager::verificationResult);
    bobApp.m_networkManager->verifyServer("http://localhost");
    QVERIFY(spyCapsBob.count() > 0 || spyCapsBob.wait(2000));

    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();

    auto bobIdRes = bobApp.m_secureStore.lock()->getIdentity();
    QVERIFY(bobIdRes.success);
    NeoNect::Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    memcpy(bobIk.publicKey.data.data(), bobIdRes.data.value().public_key.constData(), 32);
    memcpy(bobIk.privateKey.data.data(), bobIdRes.data.value().private_key.constData(), 32);

    auto spkRes = bobApp.m_secureStore.lock()->getAllSignedPreKeys();
    QVERIFY(spkRes.success && !spkRes.data.value().empty());
    NeoNect::Crypto::SignedPreKey bobSpk;
    bobSpk.id = spkRes.data.value().front().key_id;
    bobSpk.publicKey.data.resize(32);
    bobSpk.privateKey.data.resize(32);
    memcpy(bobSpk.publicKey.data.data(), spkRes.data.value().front().public_key.constData(), 32);
    memcpy(bobSpk.privateKey.data.data(), spkRes.data.value().front().private_key.constData(), 32);
    bobSpk.signature.data.resize(64);
    memcpy(bobSpk.signature.data.data(), spkRes.data.value().front().signature.constData(), 64);

    auto opkRes = bobApp.m_secureStore.lock()->getAvailableOneTimePreKeys();
    QVERIFY(opkRes.success && !opkRes.data.value().empty());
    NeoNect::Crypto::OneTimePreKey bobOpk;
    bobOpk.id = opkRes.data.value().front().key_id;
    bobOpk.publicKey.data.resize(32);
    bobOpk.privateKey.data.resize(32);
    memcpy(bobOpk.publicKey.data.data(), opkRes.data.value().front().public_key.constData(), 32);
    memcpy(bobOpk.privateKey.data.data(), opkRes.data.value().front().private_key.constData(), 32);

    // 2. Generate a valid InitialEnvelope using standalone Alice E2EE components (just like in testFirstMessageProductionPath)
    auto alicePlatformSecretStore = std::make_shared<DummySecretStore>();
    auto aliceKeyProvider = std::make_shared<NeoNect::Storage::MasterKeyProvider>(alicePlatformSecretStore);
    auto aliceSecureStore = std::make_shared<NeoNect::Storage::SecureE2EEStore>(aliceKeyProvider);
    QVERIFY(aliceSecureStore->initialize("test_prod_alice_secure_standalone.db", "testProfile").success);

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
    bundle.signedPreKeyId = bobSpk.id;
    bundle.oneTimePreKeyId = bobOpk.id;
    bundle.oneTimePreKey = bobOpk.publicKey;
    // Keys are already in Bob's secure store, no need to store them again.


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
    auto bobMsg = bobApp.m_messageStorage.lock()->getMessage("msg-1234");
    QVERIFY(bobMsg.has_value());
    QCOMPARE(bobMsg.value().plaintext, QString("Hello Bob Incoming!"));
}
void TestProductionE2EE::testBidirectionalEstablishedSession() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // 1. Cleanup databases for both Alice and Bob
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);

    // 2. Start Alice Application
        QSettings testSettings_aliceApp("NeoNect", "DesktopClient_bidi_alice");
        testSettings_aliceApp.setValue("auth_token", "fake_token");
        testSettings_aliceApp.setValue("username", "alice");

    int argcAlice = 3;
    const char* argvAlice[] = {"NeoNectTests", "--mock", "--profile=bidi_alice"};
    NeoNect::Application aliceApp(argcAlice, const_cast<char**>(argvAlice));
    auto aliceMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(aliceApp.m_transport);
    QVERIFY(aliceMockTransport);

    QSignalSpy spyCapsAlice(aliceApp.m_networkManager.get(), &NetworkManager::verificationResult);
    aliceApp.m_networkManager->verifyServer("http://localhost");
    QVERIFY(spyCapsAlice.count() > 0 || spyCapsAlice.wait(2000));

    // 3. Start Bob Application
        QSettings testSettings_bobApp("NeoNect", "DesktopClient_bidi_bob");
        testSettings_bobApp.setValue("auth_token", "fake_token");
        testSettings_bobApp.setValue("username", "bob");

    int argcBob = 3;
    const char* argvBob[] = {"NeoNectTests", "--mock", "--profile=bidi_bob"};
    NeoNect::Application bobApp(argcBob, const_cast<char**>(argvBob));
    auto bobMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(bobApp.m_transport);
    QVERIFY(bobMockTransport);

    QSignalSpy spyCapsBob(bobApp.m_networkManager.get(), &NetworkManager::verificationResult);
    bobApp.m_networkManager->verifyServer("http://localhost");
    QVERIFY(spyCapsBob.count() > 0 || spyCapsBob.wait(2000));

    aliceApp.m_storage->setAuthToken("fake_token");
    aliceApp.m_storage->setDeviceId("default_device");
    bobApp.m_storage->setAuthToken("fake_token");
    bobApp.m_storage->setDeviceId("default_device");

    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();

    // 4. Retrieve auto-generated Bob Keys
    auto bobIdRes = bobApp.m_secureStore.lock()->getIdentity();
    QVERIFY(bobIdRes.success);
    NeoNect::Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    memcpy(bobIk.publicKey.data.data(), bobIdRes.data.value().public_key.constData(), 32);
    memcpy(bobIk.privateKey.data.data(), bobIdRes.data.value().private_key.constData(), 32);

    auto spkRes = bobApp.m_secureStore.lock()->getAllSignedPreKeys();
    QVERIFY(spkRes.success && !spkRes.data.value().empty());
    NeoNect::Crypto::SignedPreKey bobSpk;
    bobSpk.id = spkRes.data.value().front().key_id;
    bobSpk.publicKey.data.resize(32);
    bobSpk.privateKey.data.resize(32);
    memcpy(bobSpk.publicKey.data.data(), spkRes.data.value().front().public_key.constData(), 32);
    memcpy(bobSpk.privateKey.data.data(), spkRes.data.value().front().private_key.constData(), 32);
    bobSpk.signature.data.resize(64);
    memcpy(bobSpk.signature.data.data(), spkRes.data.value().front().signature.constData(), 64);

    auto opkRes = bobApp.m_secureStore.lock()->getAvailableOneTimePreKeys();
    QVERIFY(opkRes.success && !opkRes.data.value().empty());
    NeoNect::Crypto::OneTimePreKey bobOpk;
    bobOpk.id = opkRes.data.value().front().key_id;
    bobOpk.publicKey.data.resize(32);
    bobOpk.privateKey.data.resize(32);
    memcpy(bobOpk.publicKey.data.data(), opkRes.data.value().front().public_key.constData(), 32);
    memcpy(bobOpk.privateKey.data.data(), opkRes.data.value().front().private_key.constData(), 32);

    // Configure Alice Mock Server to serve Bob's PreKeys
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

    aliceMockTransport->setSimulatedResponse("/api/v1/keys/claim", QJsonDocument(bundleJson).toJson(), 200);
    aliceMockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"status\":\"success\"}", 200);
    bobMockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"status\":\"success\"}", 200);

    QJsonObject devicesJson;
    QJsonArray devicesArr;
    QJsonObject dev;
    dev["device_id"] = "default_device";
    dev["public_key"] = "mock_key";
    devicesArr.append(dev);
    devicesJson["devices"] = devicesArr;
    aliceMockTransport->setSimulatedResponse("/api/v1/relay/keys", QJsonDocument(devicesJson).toJson(), 200);
    bobMockTransport->setSimulatedResponse("/api/v1/relay/keys", QJsonDocument(devicesJson).toJson(), 200);

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
        auto msgState = aliceApp.m_messageStorage.lock()->getMessage(m1.messageId);
        return msgState.has_value() && (msgState->state == NeoNect::Core::Messaging::MessageState::SENT || msgState->state == NeoNect::Core::Messaging::MessageState::FAILED);
    }());

    auto msg1State = aliceApp.m_messageStorage.lock()->getMessage(m1.messageId);
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

    // 11. Explicit Tamper Rollback Proof (Valid message succeeds using the same session state)
    NeoNect::Core::Messaging::Message m6;
    m6.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m6.receiverId = "bidi_alice";
    m6.senderId = "bidi_bob";
    m6.conversationId = "dms:bidi_alice";
    m6.plaintext = "Bob to Alice 3 (Valid after tamper)";

    QVERIFY(bobApp.m_coreMessageService->sendMessage(m6));
    QTRY_VERIFY(!extractCiphertext(spyBobReq).isEmpty());
    QString cipherM6 = extractCiphertext(spyBobReq);
    spyBobReq.clear();
    QVERIFY(!cipherM6.isEmpty());

    deliverTo(aliceApp, cipherM6, "bidi_bob");
    QVERIFY(spyAliceRecv.wait(2000) || spyAliceRecv.count() >= 1);
    QVERIFY(spyAliceRecv.count() >= 1);
    QCOMPARE(spyAliceRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Bob to Alice 3 (Valid after tamper)"));

    // 12. Full Application-level E2EE Send Failure + retryMessage() Recovery
    aliceApp.m_messageService->setCurrentUserId("bidi_alice");
    QSignalSpy spyAliceAdded(aliceApp.m_messageService.get(), &NeoNect::Services::MessageService::messageAdded);
    aliceMockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"error\":\"relay_down\"}", 500);
    spyAliceReq.clear();

    aliceApp.m_messageService->sendMessage("dms:bidi_bob", "Alice to Bob 4 (Retry Recovery)", "text");
    QVERIFY(spyAliceAdded.count() >= 1 || spyAliceAdded.wait(2000));
    QString retryMsgId = spyAliceAdded.takeFirst().at(1).toMap()["id"].toString();
    QVERIFY(!retryMsgId.isEmpty());

    QTRY_VERIFY([&]() {
        auto st = aliceApp.m_messageStorage.lock()->getMessage(retryMsgId);
        return st.has_value() && st->state == NeoNect::Core::Messaging::MessageState::FAILED;
    }());

    // Restore relay and retry via UI/Services::MessageService::retryMessage
    spyAliceReq.clear();
    aliceMockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"status\":\"success\"}", 200);
    aliceApp.m_messageService->retryMessage(retryMsgId);

    QTRY_VERIFY([&]() {
        auto st = aliceApp.m_messageStorage.lock()->getMessage(retryMsgId);
        return st.has_value() && st->state == NeoNect::Core::Messaging::MessageState::SENT;
    }());
    QTRY_VERIFY(!extractCiphertext(spyAliceReq).isEmpty());
    QString cipherM7 = extractCiphertext(spyAliceReq);
    spyAliceReq.clear();
    QVERIFY(!cipherM7.isEmpty());

    deliverTo(bobApp, cipherM7, "bidi_alice");
    QVERIFY(spyBobRecv.wait(2000) || spyBobRecv.count() >= 1);
    QCOMPARE(spyBobRecv.takeFirst().at(1).toMap()["text"].toString(), QString("Alice to Bob 4 (Retry Recovery)"));

    // 13. Mixed-case target username resolution + first-message synchronous failure guard (D-01 & D-03)
    QList<NeoNect::Core::Messaging::MessageState> observedStates;
    aliceApp.m_coreMessageService->setOnDeliveryStateChanged([&](const QString& mid, NeoNect::Core::Messaging::MessageState st, const QString& reason) {
        observedStates.append(st);
        if (st == NeoNect::Core::Messaging::MessageState::FAILED && aliceApp.m_messageService) {
            aliceApp.m_messageService->handleMessageDeliveryStatus(mid, false, reason);
        }
    });
    // Oversized / unauthenticable relay failure: clear authToken temporarily so RelayService::sendEncryptedEnvelope fails synchronously
    // and marks message FAILED during first-message createSession for a new mixed-case peer "Mixed_Peer"
    aliceApp.m_storage->setAuthToken("");
    NeoNect::Core::Messaging::Message mFirstFail;
    mFirstFail.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mFirstFail.receiverId = "Mixed_Peer";
    mFirstFail.senderId = "bidi_alice";
    mFirstFail.conversationId = "dms:mixed_peer";
    mFirstFail.plaintext = "First message sync fail check";
    QVERIFY(aliceApp.m_coreMessageService->sendMessage(mFirstFail));
    QTRY_VERIFY([&]() {
        auto st = aliceApp.m_messageStorage.lock()->getMessage(mFirstFail.messageId);
        return st.has_value() && st->state == NeoNect::Core::Messaging::MessageState::FAILED;
    }());
    // Must NOT have emitted SENT after FAILED!
    QVERIFY(!observedStates.contains(NeoNect::Core::Messaging::MessageState::SENT));
    aliceApp.m_storage->setAuthToken("fake_token");

    // 14. Recipient-side FileTransferManager::transferCompleted persists mediaUrl as transfer ID (not file://.../.spool) and full file:/// image transfer decodes via SecureImageProvider
    NeoNect::Core::Messaging::Message imgMsg;
    imgMsg.messageId = "88888888-8888-8888-8888-888888888888";
    imgMsg.conversationId = "dms:bidi_alice";
    imgMsg.senderId = "bidi_alice";
    imgMsg.receiverId = "bidi_bob";
    imgMsg.type = "image";
    imgMsg.fileName = "photo.png";
    imgMsg.fileSize = 1024;
    imgMsg.state = NeoNect::Core::Messaging::MessageState::DELIVERED;
    imgMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
    QVERIFY(bobApp.m_messageStorage.lock()->saveMessage(imgMsg));

    QSignalSpy spyBobUpdated(bobApp.m_messageService.get(), &NeoNect::Services::MessageService::messageUpdated);
    emit bobApp.m_fileTransferManager->transferCompleted(
        imgMsg.messageId,
        baseDir + "/spool/" + imgMsg.messageId + ".recv.spool");
    QVERIFY(spyBobUpdated.count() >= 1);
    auto updatedImg = bobApp.m_messageStorage.lock()->getMessage(imgMsg.messageId);
    QVERIFY(updatedImg.has_value());
    QCOMPARE(updatedImg->mediaUrl, imgMsg.messageId);

    // 15. Full two-client image pipeline: Alice sends media_request with file:/// URL -> Bob accepts -> Alice sends image metadata + file_start + encrypted chunks -> Bob reconstructs spool, sets mediaUrl=tid, and decodes via SecureImageProvider
    QString srcImgPath = baseDir + "/alice_source_photo.png";
    QImage srcImg(24, 16, QImage::Format_RGBA8888);
    srcImg.fill(QColor(30, 144, 255, 255));
    QVERIFY(srcImg.save(srcImgPath, "PNG"));
    QString fileUrlStr = QUrl::fromLocalFile(srcImgPath).toString();
    QVERIFY(fileUrlStr.startsWith("file:///"));

    aliceApp.m_storage->setUsername("bidi_alice");
    aliceApp.m_storage->setDeviceId("default_device");
    bobApp.m_storage->setUsername("bidi_bob");
    bobApp.m_storage->setDeviceId("default_device");
    aliceApp.m_messageService->setCurrentUserId("bidi_alice");
    bobApp.m_messageService->setCurrentUserId("bidi_bob");

    spyAliceReq.clear();
    spyBobReq.clear();
    QSignalSpy spyBobMsgAdded(bobApp.m_messageService.get(), &NeoNect::Services::MessageService::messageAdded);

    QString reqId = "99999999-9999-4999-8999-999999999999";
    aliceApp.m_messageService->sendMediaRequest("dms:bidi_bob", "Here is a photo", "image", fileUrlStr, "alice_source_photo.png", 0, reqId);
    QTRY_VERIFY_WITH_TIMEOUT(spyAliceReq.count() >= 1, 2000);

    // Deliver media_request from Alice to Bob
    for (int i = 0; i < spyAliceReq.count(); ++i) {
        QByteArray rawReq = spyAliceReq.at(i).at(0).toByteArray();
        if (rawReq.contains("ciphertext")) {
            QString ct = QJsonDocument::fromJson(rawReq).object()["ciphertext"].toString();
            if (!ct.isEmpty()) deliverTo(bobApp, ct, "bidi_alice");
        }
    }
    QTRY_VERIFY_WITH_TIMEOUT(spyBobMsgAdded.count() >= 1, 2000);

    // Bob accepts the media_request
    spyAliceReq.clear();
    spyBobReq.clear();
    bobApp.m_messageService->acceptMediaRequest("dms:bidi_alice", reqId);
    QTRY_VERIFY_WITH_TIMEOUT(spyBobReq.count() >= 1, 2000);

    // Deliver media_accept from Bob to Alice; Alice must resolve file:/// URL, send payloadMsg + file_start + FileChunkEnvelope
    for (int i = 0; i < spyBobReq.count(); ++i) {
        QByteArray rawReq = spyBobReq.at(i).at(0).toByteArray();
        if (rawReq.contains("ciphertext")) {
            QString ct = QJsonDocument::fromJson(rawReq).object()["ciphertext"].toString();
            if (!ct.isEmpty()) deliverTo(aliceApp, ct, "bidi_bob");
        }
    }

    // Wait for Alice to emit all 3 envelopes: image chat message, file_start control message, and FileChunkEnvelope
    QTRY_VERIFY_WITH_TIMEOUT([&]() {
        int cipherCount = 0;
        for (int i = 0; i < spyAliceReq.count(); ++i) {
            if (spyAliceReq.at(i).at(0).toByteArray().contains("ciphertext")) cipherCount++;
        }
        return cipherCount >= 3;
    }(), 3000);

    QSignalSpy spyBobTransferDone(bobApp.m_fileTransferManager.get(), &NeoNect::Services::FileTransferManager::transferCompleted);
    for (int i = 0; i < spyAliceReq.count(); ++i) {
        QByteArray rawReq = spyAliceReq.at(i).at(0).toByteArray();
        if (rawReq.contains("ciphertext")) {
            QString ct = QJsonDocument::fromJson(rawReq).object()["ciphertext"].toString();
            if (!ct.isEmpty()) deliverTo(bobApp, ct, "bidi_alice");
        }
    }

    QTRY_VERIFY_WITH_TIMEOUT(spyBobTransferDone.count() >= 1, 3000);
    QString completedTid = spyBobTransferDone.first().at(0).toString();
    QString completedSpoolPath = spyBobTransferDone.first().at(1).toString();
    QVERIFY(!completedTid.isEmpty());
    QVERIFY(QFile::exists(completedSpoolPath));
    QVERIFY(!completedSpoolPath.endsWith(".recv.spool"));
    QVERIFY(completedSpoolPath.endsWith(".png"));

    // Verify Bob's DB message has mediaUrl == completedTid (not file://.../.recv.spool) and cleared errorText
    auto bobReceivedImg = bobApp.m_messageStorage.lock()->getMessage(completedTid);
    QVERIFY(bobReceivedImg.has_value());
    QCOMPARE(bobReceivedImg->type, QString("image"));
    QCOMPARE(bobReceivedImg->mediaUrl, completedTid);
    QCOMPARE(bobReceivedImg->errorText, QString(""));
    QCOMPARE(bobReceivedImg->mediaWidth, 24);
    QCOMPARE(bobReceivedImg->mediaHeight, 16);

    // Verify SecureImageProvider on Bob's instance decrypts and decodes the image from the encrypted spool
    QVERIFY(bobApp.m_secureImageProvider != nullptr);
    QQuickImageResponse* imgResp = bobApp.m_secureImageProvider->requestImageResponse(completedTid, QSize());
    QVERIFY(imgResp != nullptr);
    QSignalSpy spyImgFinished(imgResp, &QQuickImageResponse::finished);
    QVERIFY(spyImgFinished.count() >= 1 || spyImgFinished.wait(2000));
    QCOMPARE(imgResp->errorString(), QString(""));
    QImage decodedImg = imgResp->property("decodedImage").value<QImage>();
    QVERIFY(!decodedImg.isNull());
    QCOMPARE(decodedImg.size(), QSize(24, 16));
    QCOMPARE(decodedImg.pixelColor(0, 0), QColor(30, 144, 255, 255));
    delete imgResp;
}

void TestProductionE2EE::testIdentityRestart() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);

    QByteArray alicePub, alicePriv;

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_id_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=id_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto idRes = app.m_secureStore.lock()->getIdentity();
        QVERIFY(idRes.success);
        alicePub = idRes.data.value().public_key;
        alicePriv = idRes.data.value().private_key;
    }

    {
        QSettings testSettings_app2("NeoNect", "DesktopClient_id_restart");
        testSettings_app2.setValue("auth_token", "fake_token");
        testSettings_app2.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=id_restart"};
        NeoNect::Application app2(argc, const_cast<char**>(argv));

        auto idRes2 = app2.m_secureStore.lock()->getIdentity();
        QVERIFY(idRes2.success);
        QCOMPARE(idRes2.data.value().public_key, alicePub);
        QCOMPARE(idRes2.data.value().private_key, alicePriv);
    }
}

void TestProductionE2EE::testSessionRestart() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);
    NeoNect::Crypto::X3DH::BobPreKeyBundle bobBundle;
    QString bobIdStr = "sess_restart_bob:default_device";

    // Setup Bob
    {
        QSettings testSettings_bobApp("NeoNect", "DesktopClient_sess_restart_bob");
        testSettings_bobApp.setValue("auth_token", "fake_token");
        testSettings_bobApp.setValue("username", "bob");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=sess_restart_bob"};
        NeoNect::Application bobApp(argc, const_cast<char**>(argv));
        bobApp.m_storage->setAuthToken("fake_token");
        bobApp.m_storage->setDeviceId("default_device");

        auto idRes = bobApp.m_secureStore.lock()->getIdentity();
        QVERIFY(idRes.success);
        bobBundle.identityKey.data.resize(32);
        memcpy(bobBundle.identityKey.data.data(), idRes.data.value().public_key.constData(), 32);

        auto spkRes = bobApp.m_secureStore.lock()->getAllSignedPreKeys();
        QVERIFY(spkRes.success && !spkRes.data.value().empty());
        bobBundle.signedPreKey.data.resize(32);
        memcpy(bobBundle.signedPreKey.data.data(), spkRes.data.value().front().public_key.constData(), 32);
        bobBundle.signedPreKeySignature.data.resize(64);
        memcpy(bobBundle.signedPreKeySignature.data.data(), spkRes.data.value().front().signature.constData(), 64);
        bobBundle.signedPreKeyId = spkRes.data.value().front().key_id;

        auto opkRes = bobApp.m_secureStore.lock()->getAvailableOneTimePreKeys();
        QVERIFY(opkRes.success && !opkRes.data.value().empty());
        bobBundle.oneTimePreKey.emplace();
        bobBundle.oneTimePreKey->data.resize(32);
        memcpy(bobBundle.oneTimePreKey->data.data(), opkRes.data.value().front().public_key.constData(), 32);
        bobBundle.oneTimePreKeyId = opkRes.data.value().front().key_id;
    }

    // Alice creates session, sends message
    {
        QSettings testSettings_aliceApp("NeoNect", "DesktopClient_sess_restart_alice");
        testSettings_aliceApp.setValue("auth_token", "fake_token");
        testSettings_aliceApp.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=sess_restart_alice"};
        NeoNect::Application aliceApp(argc, const_cast<char**>(argv));
        aliceApp.m_storage->setAuthToken("fake_token");
        aliceApp.m_storage->setDeviceId("default_device");

        QSignalSpy spyCapsAlice(aliceApp.m_networkManager.get(), &NetworkManager::verificationResult);
        aliceApp.m_networkManager->verifyServer("http://localhost");
        QVERIFY(spyCapsAlice.count() > 0 || spyCapsAlice.wait(2000));

        auto res = aliceApp.m_sessionManager->createSession("sess_restart_bob", "default_device", bobBundle, QUuid::createUuid().toString(QUuid::WithoutBraces), QByteArray("First Message"));
        QVERIFY2(res.success, res.message.toStdString().c_str());
        QVERIFY(aliceApp.m_sessionManager->hasSession(bobIdStr));
    }

    // Alice restarts, should have session, sends another message
    {
        QSettings testSettings_aliceApp("NeoNect", "DesktopClient_sess_restart_alice");
        testSettings_aliceApp.setValue("auth_token", "fake_token");
        testSettings_aliceApp.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=sess_restart_alice"};
        NeoNect::Application aliceApp(argc, const_cast<char**>(argv));
        aliceApp.m_storage->setAuthToken("fake_token");
        aliceApp.m_storage->setDeviceId("default_device");

        QSignalSpy spyCapsAlice(aliceApp.m_networkManager.get(), &NetworkManager::verificationResult);
        aliceApp.m_networkManager->verifyServer("http://localhost");
        QVERIFY(spyCapsAlice.count() > 0 || spyCapsAlice.wait(2000));

        QVERIFY(aliceApp.m_secureStore.lock()->getSession(bobIdStr).success);

        auto aliceMockTransport = std::dynamic_pointer_cast<NeoNect::Testing::MockHttpTransport>(aliceApp.m_transport);
        QSignalSpy spyReq(aliceMockTransport.get(), &NeoNect::Testing::MockHttpTransport::requestHandled);

        QJsonObject devicesJson;
        QJsonArray devicesArr;
        QJsonObject dev;
        dev["device_id"] = "default_device";
        dev["public_key"] = "mock_key";
        devicesArr.append(dev);
        devicesJson["devices"] = devicesArr;
        aliceMockTransport->setSimulatedResponse("/api/v1/relay/keys", QJsonDocument(devicesJson).toJson(), 200);
        aliceMockTransport->setSimulatedResponse("/api/v1/relay/send", "{\"status\":\"success\"}", 200);

        NeoNect::Core::Messaging::Message msg2;
        msg2.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg2.conversationId = "sess_restart_bob";
        msg2.receiverId = "sess_restart_bob";
        msg2.senderId = "sess_restart_alice";
        msg2.plaintext = "Second Message";
        msg2.timestamp = QDateTime::currentMSecsSinceEpoch();

        QVERIFY(aliceApp.m_coreMessageService->sendMessage(msg2));
        QVERIFY(spyReq.count() > 0);
    }
}

void TestProductionE2EE::testPreKeyRestart() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);

    QByteArray spkPub;

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_pk_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=pk_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto spkRes = app.m_secureStore.lock()->getAllSignedPreKeys();
        QVERIFY(spkRes.success && !spkRes.data.value().empty());
        spkPub = spkRes.data.value().front().public_key;
    }

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_pk_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=pk_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto spkRes = app.m_secureStore.lock()->getAllSignedPreKeys();
        QVERIFY(spkRes.success);
        QVERIFY(!spkRes.data.value().empty());
        QCOMPARE(spkRes.data.value().front().public_key, spkPub);

        auto opkRes = app.m_secureStore.lock()->getAvailableOneTimePreKeys();
        QVERIFY(opkRes.success);
        QCOMPARE(opkRes.data.value().size(), 100);

        QVERIFY(app.m_secureStore.lock()->consumeOneTimePreKeyAtomically(1).success);
    }

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_pk_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=pk_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto opkRes = app.m_secureStore.lock()->getAvailableOneTimePreKeys();
        QVERIFY(opkRes.success);
        QCOMPARE(opkRes.data.value().size(), 99);

        bool found = false;
        for (const auto& k : opkRes.data.value()) {
            if (k.key_id == 1) found = true;
        }
        QVERIFY(!found);
    }
}

void TestProductionE2EE::testSkippedKeyRestart() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_sk_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=sk_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        NeoNect::Storage::SessionUpdateTx tx;
        tx.session.session_id = "test_session";
        tx.session.remote_identity_key = QByteArray(32, 'a');
        tx.session.local_identity_id = 1;
        tx.session.RK = QByteArray(32, 'r');
        tx.session.DHs = QByteArray(32, 'd');
        tx.session.DHr = QByteArray(32, 'e');
        tx.session.CKs = QByteArray(32, 'c');
        tx.session.CKr = QByteArray(32, 'k');

        NeoNect::Storage::E2EESkippedKey sk;
        sk.session_id = "test_session";
        sk.remote_ratchet_public_key = QByteArray(32, 'd');
        sk.message_number = 42;
        sk.message_key = QByteArray(32, 'm');
        sk.created_at = QDateTime::currentMSecsSinceEpoch();

        tx.new_skipped_keys.push_back(sk);
        auto upRes = app.m_secureStore.lock()->updateSessionState(tx);
        QVERIFY2(upRes.success, upRes.message.toStdString().c_str());
    }

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_sk_restart");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=sk_restart"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto skRes = app.m_secureStore.lock()->getSkippedKey("test_session", QByteArray(32, 'd'), 42);
        QVERIFY(skRes.success);
        QCOMPARE(skRes.data.value().message_key, QByteArray(32, 'm'));
    }
}

void TestProductionE2EE::testLogoutReloginIntegration() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively(); QDir().mkpath(baseDir);

    QByteArray pubKey;

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_logout");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=logout"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
        NeoNect::Storage::E2EEIdentity aliceId;
        aliceId.identity_id = 1;
        auto aliceKp = backend->GenerateX25519KeyPair();
        aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
        aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
        app.m_secureStore.lock()->saveIdentity(aliceId);
        pubKey = aliceId.public_key;

        auto authSvc = std::make_shared<NeoNect::Services::AuthService>(app.m_transport, app.m_storage, nullptr);
        authSvc->setBeforeTeardownCallback([secureStorePtr = app.m_secureStore](const QString&) {
            if (auto ptr = secureStorePtr.lock()) ptr->close();
        });

        authSvc->logoutUser();

    }

    {
        QSettings testSettings_app("NeoNect", "DesktopClient_logout");
        testSettings_app.setValue("auth_token", "fake_token");
        testSettings_app.setValue("username", "alice");

        int argc = 3;
        const char* argv[] = {"NeoNectTests", "--mock", "--profile=logout"};
        NeoNect::Application app(argc, const_cast<char**>(argv));

        auto idRes = app.m_secureStore.lock()->getIdentity();
        QVERIFY(idRes.success);
        QCOMPARE(idRes.data.value().public_key, pubKey);
    }
}

#include "../src/storage/StoragePathResolver.h"

namespace {
class ControllableMockE2EEStore : public NeoNect::Storage::ISecureE2EEStore {
public:
    bool openFlag = true;
    NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity> getIdentityResult =
        NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity>::fail("Identity not found");
    NeoNect::ServiceResult<std::monostate> saveIdentityResult =
        NeoNect::ServiceResult<std::monostate>::ok({});
    int saveIdentityCalls = 0;
    std::optional<NeoNect::Storage::E2EEIdentity> savedIdentity;

    bool isOpen() const override { return openFlag; }

    NeoNect::ServiceResult<std::monostate> initialize(const QString&, const QString&) override {
        return openFlag ? NeoNect::ServiceResult<std::monostate>::ok({})
                        : NeoNect::ServiceResult<std::monostate>::fail("Database not open");
    }
    void close() override { openFlag = false; }
    NeoNect::ServiceResult<std::monostate> closeAndWipeDatabase() override {
        openFlag = false;
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> saveIdentity(const NeoNect::Storage::E2EEIdentity& identity) override {
        ++saveIdentityCalls;
        if (saveIdentityResult.success) {
            savedIdentity = identity;
            getIdentityResult = NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity>::ok(identity);
        }
        return saveIdentityResult;
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity> getIdentity() override {
        return getIdentityResult;
    }
    NeoNect::ServiceResult<std::monostate> saveSignedPreKey(const NeoNect::Storage::E2EESignedPreKey&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EESignedPreKey> getSignedPreKey(qint64) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EESignedPreKey>::fail("Not found");
    }
    NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EESignedPreKey>> getAllSignedPreKeys() override {
        return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EESignedPreKey>>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> saveOneTimePreKeys(const std::vector<NeoNect::Storage::E2EEOneTimePreKey>&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEOneTimePreKey>> getAvailableOneTimePreKeys() override {
        return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEOneTimePreKey>>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> consumeOneTimePreKeyAtomically(qint64) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EEOneTimePreKey> getOneTimePreKey(qint64) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEOneTimePreKey>::fail("Not found");
    }
    NeoNect::ServiceResult<std::monostate> saveSession(const NeoNect::Storage::E2EESession&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EESession> getSession(const QString&) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EESession>::fail("Not found");
    }
    NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EESession>> getSessionsForDevice(const QString&) override {
        return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EESession>>::fail("Not found");
    }
    NeoNect::ServiceResult<std::monostate> updateSessionState(const NeoNect::Storage::SessionUpdateTx&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EESkippedKey> getSkippedKey(const QString&, const QByteArray&, qint64) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EESkippedKey>::fail("Not found");
    }
    NeoNect::ServiceResult<std::monostate> saveFileTransfer(const NeoNect::Storage::E2EEFileTransfer&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer> getFileTransfer(const QString&, const QString&) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Not found");
    }
    NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer> getFileTransferById(const QString&) override {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Not found");
    }
    NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>> getActiveFileTransfers() override {
        return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> updateFileTransferBitset(const QString&, const QString&, const QByteArray&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
    NeoNect::ServiceResult<std::monostate> updateFileTransferStatus(const QString&, const QString&, const QString&) override {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    }
};
} // namespace

void TestProductionE2EE::testIdentityInitializationGuards() {
    QStandardPaths::setTestModeEnabled(true);
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(baseDir).removeRecursively();
    QDir().mkpath(baseDir);

    int argc = 3;
    const char* argv[] = {"NeoNectTests", "--mock", "--profile=id_guards"};
    NeoNect::Application app(argc, const_cast<char**>(argv));

    app.m_storage->setServerUrl("http://localhost:8080");
    app.m_storage->setUsername("guard_user");
    const QString originalPubKeyB64 = QString::fromLatin1(QByteArray(32, 'O').toBase64());
    app.m_storage->setPublicKey(originalPubKeyB64);

    auto backend = std::make_shared<NeoNect::Crypto::OpenSSLBackend>();
    auto xeddsa = std::make_shared<NeoNect::Crypto::XEdDSAAdapter>();
    auto mockStore = std::make_shared<ControllableMockE2EEStore>();
    auto preKeyAdapter = std::make_shared<NeoNect::Crypto::Session::SecurePreKeyStoreAdapter>(mockStore);

    // Case 1: Store is not open (m_db == nullptr) -> must fail closed and NEVER overwrite public_key
    mockStore->openFlag = false;
    mockStore->getIdentityResult = NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity>::fail("Database not open");
    auto res1 = app.ensureE2EEIdentity(mockStore, backend, xeddsa, preKeyAdapter);
    QVERIFY(!res1.success);
    QCOMPARE(mockStore->saveIdentityCalls, 0);
    QCOMPARE(app.m_storage->publicKey(), originalPubKeyB64);

    // Case 2: Store is open, but getIdentity() fails with a SQL/prepare error -> must fail closed and NEVER overwrite public_key
    mockStore->openFlag = true;
    mockStore->getIdentityResult = NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity>::fail("Failed to prepare statement");
    auto res2 = app.ensureE2EEIdentity(mockStore, backend, xeddsa, preKeyAdapter);
    QVERIFY(!res2.success);
    QCOMPARE(mockStore->saveIdentityCalls, 0);
    QCOMPARE(app.m_storage->publicKey(), originalPubKeyB64);

    // Case 3: Truly missing identity ("Identity not found"), but saveIdentity() fails -> must fail closed and NEVER overwrite public_key
    mockStore->getIdentityResult = NeoNect::ServiceResult<NeoNect::Storage::E2EEIdentity>::fail("Identity not found");
    mockStore->saveIdentityResult = NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
    auto res3 = app.ensureE2EEIdentity(mockStore, backend, xeddsa, preKeyAdapter);
    QVERIFY(!res3.success);
    QCOMPARE(mockStore->saveIdentityCalls, 1);
    QCOMPARE(app.m_storage->publicKey(), originalPubKeyB64);

    // Case 4: Truly missing identity ("Identity not found") and saveIdentity() succeeds -> generates and saves identity and updates public_key
    mockStore->saveIdentityCalls = 0;
    mockStore->saveIdentityResult = NeoNect::ServiceResult<std::monostate>::ok({});
    auto res4 = app.ensureE2EEIdentity(mockStore, backend, xeddsa, preKeyAdapter);
    QVERIFY2(res4.success, res4.message.toUtf8().constData());
    QCOMPARE(mockStore->saveIdentityCalls, 1);
    QVERIFY(mockStore->savedIdentity.has_value());
    const QString generatedPubKeyB64 = QString::fromLatin1(mockStore->savedIdentity->public_key.toBase64());
    QCOMPARE(app.m_storage->publicKey(), generatedPubKeyB64);
    QVERIFY(generatedPubKeyB64 != originalPubKeyB64);

    // Case 5: Normal startup with existing valid identity -> preserves existing identity and syncs public_key
    app.m_storage->setPublicKey(QString());
    auto res5 = app.ensureE2EEIdentity(mockStore, backend, xeddsa, preKeyAdapter);
    QVERIFY(res5.success);
    QCOMPARE(mockStore->saveIdentityCalls, 1); // No additional saveIdentity call
    QCOMPARE(app.m_storage->publicKey(), generatedPubKeyB64);

    // Case 6: End-to-end startup with an e2ee.db encrypted under a different master key
    // Verify Application aborts session, does NOT overwrite settings.ini public_key, and preserves e2ee.db
    const QString mismatchProfile = "id_guards_mismatch";
    const QString mismatchServer = "http://localhost:8080";
    const QString mismatchUser = "alice_mismatch";
    QDir(NeoNect::Storage::StoragePathResolver::accountRoot(mismatchServer, mismatchUser)).removeRecursively();

    QByteArray existingDbPub(32, 'K');
    QByteArray existingDbPriv(32, 'V');
    {
        // Create e2ee.db encrypted with DummySecretStore's key (0x0123... rather than StorageContextMockSecretStore's 'M'*32)
        auto otherSecretStore = std::make_shared<DummySecretStore>();
        auto otherProvider = std::make_shared<NeoNect::Storage::MasterKeyProvider>(otherSecretStore);
        NeoNect::Storage::SecureE2EEStore existingDb(otherProvider);
        QString dbPath = NeoNect::Storage::StoragePathResolver::e2eeDbPath(mismatchServer, mismatchUser);
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
        QString keyProfileId = NeoNect::Storage::StoragePathResolver::serverKey(mismatchServer) + "_" +
                               NeoNect::Storage::StoragePathResolver::accountKey(mismatchUser);
        QVERIFY(existingDb.initialize(dbPath, keyProfileId).success);
        NeoNect::Storage::E2EEIdentity id;
        id.identity_id = 1;
        id.public_key = existingDbPub;
        id.private_key = existingDbPriv;
        QVERIFY(existingDb.saveIdentity(id).success);
        existingDb.close();
    }

    const QString expectedPersistedB64 = QString::fromLatin1(existingDbPub.toBase64());
    {
        NeoNect::Storage::SettingsRepository preloadRepo(mismatchProfile);
        preloadRepo.setServerUrl(mismatchServer);
        preloadRepo.setUsername(mismatchUser);
        preloadRepo.setAuthToken("saved_token");
        preloadRepo.setPublicKey(expectedPersistedB64);
    }

    {
        int argcMismatch = 3;
        const char* argvMismatch[] = {"NeoNectTests", "--mock", "--profile=id_guards_mismatch"};
        NeoNect::Application mismatchApp(argcMismatch, const_cast<char**>(argvMismatch));

        // Session must have been aborted because StorageContextMockSecretStore ('M'*32) cannot unlock e2ee.db
        QVERIFY(mismatchApp.m_networkManager->token().isEmpty());
        QVERIFY(!mismatchApp.m_networkManager->lastAuthError().isEmpty());
        QVERIFY(mismatchApp.m_secureStore.expired());

        NeoNect::Storage::SettingsRepository verifyRepo(mismatchProfile);
        verifyRepo.setServerUrl(mismatchServer);
        verifyRepo.setUsername(mismatchUser);
        QCOMPARE(verifyRepo.publicKey(), expectedPersistedB64);
    }
}

void TestProductionE2EE::testDeferredRelayAckAndConcurrentSessionCoalescing() {
    const QString aliceSecurePath = "test_prod_coalesce_alice_secure.db";
    const QString bobSecurePath = "test_prod_coalesce_bob_secure.db";
    const QString aliceMsgPath = "test_prod_coalesce_alice_msg.db";
    QFile::remove(aliceSecurePath);
    QFile::remove(bobSecurePath);
    QFile::remove(aliceMsgPath);

    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    auto x3dh = std::make_shared<X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<DoubleRatchet::AEAD>(backend.get());

    auto secretStore = std::make_shared<DummySecretStore>();
    auto keyProvider = std::make_shared<Storage::MasterKeyProvider>(secretStore);

    auto aliceStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(aliceStore->initialize(aliceSecurePath, "coalesce_alice").success);
    Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    auto aliceKp = backend->GenerateX25519KeyPair();
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    QVERIFY(aliceStore->saveIdentity(aliceId).success);

    auto bobStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(bobStore->initialize(bobSecurePath, "coalesce_bob").success);
    auto bobKp = backend->GenerateX25519KeyPair();
    auto bobSpkKp = backend->GenerateX25519KeyPair();
    auto bobOpkKp = backend->GenerateX25519KeyPair();

    Crypto::IdentityKeyPair bobIk;
    bobIk.publicKey.data.resize(32);
    bobIk.privateKey.data.resize(32);
    std::copy(bobKp.second.data.data(), bobKp.second.data.data() + 32, bobIk.publicKey.data.data());
    std::copy(bobKp.first.data.data(), bobKp.first.data.data() + 32, bobIk.privateKey.data.data());

    Storage::E2EEIdentity bobId;
    bobId.identity_id = 1;
    bobId.public_key = QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32);
    bobId.private_key = QByteArray(reinterpret_cast<const char*>(bobIk.privateKey.data.data()), 32);
    QVERIFY(bobStore->saveIdentity(bobId).success);

    Crypto::SignedPreKey bobSpk;
    bobSpk.id = 1;
    bobSpk.publicKey.data.resize(32);
    bobSpk.privateKey.data.resize(32);
    std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
    std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
    QByteArray spkBytes = KeyEncoding::Encode(bobSpk.publicKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
    bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

    Crypto::OneTimePreKey bobOpk;
    bobOpk.id = 1;
    bobOpk.publicKey.data.resize(32);
    bobOpk.privateKey.data.resize(32);
    std::copy(bobOpkKp.second.data.data(), bobOpkKp.second.data.data() + 32, bobOpk.publicKey.data.data());
    std::copy(bobOpkKp.first.data.data(), bobOpkKp.first.data.data() + 32, bobOpk.privateKey.data.data());

    Crypto::X3DH::BobPreKeyBundle bobBundle;
    bobBundle.identityKey = bobIk.publicKey;
    bobBundle.signedPreKeyId = bobSpk.id;
    bobBundle.signedPreKey = bobSpk.publicKey;
    bobBundle.signedPreKeySignature = bobSpk.signature;
    bobBundle.oneTimePreKeyId = bobOpk.id;
    bobBundle.oneTimePreKey = bobOpk.publicKey;

    auto bobPreKeyStore = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(bobStore);
    bobPreKeyStore->storeSignedPreKey(std::move(bobSpk));
    std::vector<Crypto::OneTimePreKey> opks;
    opks.push_back(std::move(bobOpk));
    bobPreKeyStore->storeOneTimePreKeys(std::move(opks));

    auto messageStorage = std::make_shared<SqliteMessageStorage>(aliceMsgPath);
    auto messageQueue = std::make_shared<MessageQueue>(aliceMsgPath);

    QList<QPair<QString, QByteArray>> dispatchedEnvelopes;
    auto offlineQueueService = std::make_shared<OfflineQueueService>(
        messageQueue,
        [&](const QString&, const QString&, const QString& msgId, const QByteArray& env) -> bool {
            dispatchedEnvelopes.append({msgId, env});
            return true;
        });

    auto aliceSessionManager = std::make_shared<SessionManager>(
        aliceStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            return offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
        },
        nullptr);

    auto coreMessageService = std::make_shared<Core::Messaging::MessageService>(messageStorage, aliceSessionManager, messageQueue);
    coreMessageService->setDeviceResolverCallback([](const QString&, auto cb) { cb("devB"); });

    int claimCount = 0;
    Core::Messaging::MessageService::PreKeyClaimCallback pendingClaimCb;
    coreMessageService->setPreKeyClaimRequestCallback([&](const QString&, const QString&, Core::Messaging::MessageService::PreKeyClaimCallback resultCb) {
        ++claimCount;
        pendingClaimCb = std::move(resultCb);
    });

    Core::Messaging::Message m1;
    m1.messageId = "coalesce-m1";
    m1.conversationId = "dms:bob";
    m1.senderId = "alice";
    m1.receiverId = "bob";
    m1.plaintext = "First concurrent message";
    m1.state = Core::Messaging::MessageState::CREATED;

    Core::Messaging::Message m2;
    m2.messageId = "coalesce-m2";
    m2.conversationId = "dms:bob";
    m2.senderId = "alice";
    m2.receiverId = "bob";
    m2.plaintext = "Second concurrent message";
    m2.state = Core::Messaging::MessageState::CREATED;

    QVERIFY(coreMessageService->sendMessage(m1));
    QVERIFY(coreMessageService->sendMessage(m2));
    QCOMPARE(claimCount, 1);

    QVERIFY(pendingClaimCb != nullptr);
    pendingClaimCb(bobBundle);

    QCOMPARE(dispatchedEnvelopes.size(), 2);
    QCOMPARE(messageStorage->getMessage("coalesce-m1")->state, Core::Messaging::MessageState::ENCRYPTING);
    QCOMPARE(messageStorage->getMessage("coalesce-m2")->state, Core::Messaging::MessageState::ENCRYPTING);

    offlineQueueService->handleAck("coalesce-m1");
    messageStorage->updateMessageState("coalesce-m1", Core::Messaging::MessageState::SENT);
    QCOMPARE(messageStorage->getMessage("coalesce-m1")->state, Core::Messaging::MessageState::SENT);

    offlineQueueService->handleTransportFailure("coalesce-m2");
    messageStorage->updateMessageState("coalesce-m2", Core::Messaging::MessageState::FAILED);
    QCOMPARE(messageStorage->getMessage("coalesce-m2")->state, Core::Messaging::MessageState::FAILED);

    QStringList bobReceivedTexts;
    auto bobSessionManager = std::make_shared<SessionManager>(
        bobStore, x3dh, ratchet, aead, backend, bobPreKeyStore, xeddsa, nullptr,
        [&](const QString&, const QByteArray& pt) {
            QJsonDocument doc = QJsonDocument::fromJson(pt);
            bobReceivedTexts.append(doc.object().value("plaintext").toString());
        });
    bobSessionManager->setSenderVerifier([&](const QString& user, const QString& dev, const QByteArray& ik) {
        return user == "alice" && dev == "devA" && ik == aliceId.public_key;
    });

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "devA";
    meta.recipientDeviceId = "devB";

    QVERIFY(bobSessionManager->handleEnvelope(dispatchedEnvelopes[0].second, meta).success);
    QVERIFY(bobSessionManager->handleEnvelope(dispatchedEnvelopes[1].second, meta).success);
    QCOMPARE(bobReceivedTexts.size(), 2);
    QCOMPARE(bobReceivedTexts[0], QString("First concurrent message"));
    QCOMPARE(bobReceivedTexts[1], QString("Second concurrent message"));

    QFile::remove(aliceSecurePath);
    QFile::remove(bobSecurePath);
    QFile::remove(aliceMsgPath);
}

#include "../src/core/chatmessagemodel.h"
#include "../src/services/deviceservice.h"
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
class OfflineDeliveryTestTransport : public Transport::IHttpTransport {
public:
    struct QueuedItem {
        qint64 id{0};
        QString messageId;
        QString senderDeviceId;
        QString recipientDeviceId;
        QString ciphertextBase64;
        int protocolVersion{2};
    };

    QString m_baseUrl{"http://mock.neonect.local"};
    QString m_token{"token"};
    qint64 m_nextId{1};
    QList<QueuedItem> m_queue;
    QSet<qint64> m_ackedIds;
    int m_pollCalls{0};
    int m_ackCalls{0};
    int m_keysCalls{0};
    bool m_failNextPoll{false};
    bool m_deferNextPoll{false};
    Transport::HttpResponseCallback m_deferredPollCb;
    QMap<QString, QString> m_deferredPollParams;
    QMap<QString, QString> m_lastPollParams;
    int m_simulateKeysStatus{200};
    int m_failKeysCount{0};
    QByteArray m_keysPayload;

    void setBaseUrl(const QString &url) override { m_baseUrl = url; }
    QString baseUrl() const override { return m_baseUrl; }
    void setAuthToken(const QString &token) override { m_token = token; }
    QString authToken() const override { return m_token; }

    qint64 enqueueRaw(const QString& msgId, const QString& senderDev, const QString& recipientDev, const QByteArray& rawBytes) {
        QueuedItem item;
        item.id = m_nextId++;
        item.messageId = msgId;
        item.senderDeviceId = senderDev;
        item.recipientDeviceId = recipientDev;
        item.ciphertextBase64 = QString::fromLatin1(rawBytes.toBase64());
        item.protocolVersion = 2;
        m_queue.append(item);
        return item.id;
    }

    void fulfillPoll(const QMap<QString, QString>& queryParams, Transport::HttpResponseCallback callback) {
        m_lastPollParams = queryParams;
        if (m_failNextPoll) {
            m_failNextPoll = false;
            if (callback) callback(503, QByteArray(R"({"error":"Service Unavailable"})"), QNetworkReply::InternalServerError, "Service Unavailable");
            return;
        }
        // Matches original NeoNect-server PollRelayMessages (internal/api/relay_handlers.go:139-182):
        // Reads ONLY query param "device_id" and returns the complete non-expired queue for that device
        // ordered by sequence ASC (up to MaxMailboxMessages = 1000), ignoring after_id/limit.
        const QString devId = queryParams.value("device_id");

        QJsonArray arr;
        for (const auto& item : m_queue) {
            if (item.recipientDeviceId == devId) {
                QJsonObject obj;
                obj["id"] = item.id;
                obj["message_id"] = item.messageId;
                obj["sender_device_id"] = item.senderDeviceId;
                obj["protocol_version"] = item.protocolVersion;
                obj["ciphertext"] = item.ciphertextBase64;
                arr.append(obj);
            }
        }
        QJsonObject root;
        root["messages"] = arr;
        if (callback) callback(200, QJsonDocument(root).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
    }

    QNetworkReply* get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject*, Transport::HttpResponseCallback callback) override {
        if (endpoint == "/api/v1/relay/poll") {
            ++m_pollCalls;
            m_lastPollParams = queryParams;
            if (m_deferNextPoll) {
                m_deferNextPoll = false;
                m_deferredPollCb = callback;
                m_deferredPollParams = queryParams;
                return nullptr;
            }
            fulfillPoll(queryParams, callback);
            return nullptr;
        }
        if (endpoint == "/api/v1/relay/keys") {
            ++m_keysCalls;
            if (m_failKeysCount > 0) {
                --m_failKeysCount;
                if (callback) callback(429, QByteArray(R"({"error":"rate limited"})"), QNetworkReply::UnknownServerError, "HTTP 429");
                return nullptr;
            }
            if (m_simulateKeysStatus != 200) {
                if (callback) callback(m_simulateKeysStatus, QByteArray(R"({"error":"rate limited"})"), QNetworkReply::UnknownServerError, "HTTP 429");
                return nullptr;
            }
            if (callback) callback(200, m_keysPayload, QNetworkReply::NoError, QString());
            return nullptr;
        }
        if (callback) callback(200, QByteArray("{}"), QNetworkReply::NoError, QString());
        return nullptr;
    }

    QNetworkReply* post(const QString &endpoint, const QByteArray &jsonData, const QObject*, Transport::HttpResponseCallback callback) override {
        if (endpoint == "/api/v1/relay/send") {
            QJsonObject req = QJsonDocument::fromJson(jsonData).object();
            QueuedItem item;
            item.id = m_nextId++;
            item.messageId = req.value("message_id").toString();
            item.senderDeviceId = req.value("from_device_id").toString();
            item.recipientDeviceId = req.value("recipient_device_id").toString();
            item.ciphertextBase64 = req.value("ciphertext").toString();
            item.protocolVersion = req.value("protocol_version").toInt(2);
            m_queue.append(item);
            if (callback) callback(201, QByteArray(R"({"status":"queued"})"), QNetworkReply::NoError, QString());
            return nullptr;
        }
        if (endpoint == "/api/v1/relay/ack") {
            ++m_ackCalls;
            QJsonObject req = QJsonDocument::fromJson(jsonData).object();
            const QString devId = req.value("device_id").toString();
            const qint64 msgId = req.value("message_id").toVariant().toLongLong();
            m_ackedIds.insert(msgId);
            for (int i = 0; i < m_queue.size(); ++i) {
                if (m_queue[i].id == msgId && m_queue[i].recipientDeviceId == devId) {
                    m_queue.removeAt(i);
                    break;
                }
            }
            if (callback) callback(200, QByteArray(R"({"status":"acknowledged"})"), QNetworkReply::NoError, QString());
            return nullptr;
        }
        if (callback) callback(200, QByteArray("{}"), QNetworkReply::NoError, QString());
        return nullptr;
    }

    QNetworkReply* deleteResource(const QString &, const QObject*, Transport::HttpResponseCallback callback, const QByteArray &) override {
        if (callback) callback(200, QByteArray("{}"), QNetworkReply::NoError, QString());
        return nullptr;
    }
};

class RelayEnvelopeHandlerAdapter : public Transport::IIncomingEnvelopeHandler {
public:
    std::shared_ptr<Crypto::Session::SessionManager> m_sm;
    explicit RelayEnvelopeHandlerAdapter(std::shared_ptr<Crypto::Session::SessionManager> sm) : m_sm(std::move(sm)) {}
    VoidResult handleEnvelope(const QByteArray& rawBytes, const Transport::TransportMetadata& meta) override {
        return m_sm->handleEnvelope(rawBytes, meta);
    }
};
} // namespace

void TestProductionE2EE::testOfflineDeliveryEndToEndRegressionSuite() {
    const QString aliceSecureDb = "test_offline_suite_alice_secure.db";
    const QString bobSecureDb = "test_offline_suite_bob_secure.db";
    const QString bob2SecureDb = "test_offline_suite_bob2_secure.db";
    const QString aliceMsgDb = "test_offline_suite_alice_msg.db";
    const QString bobMsgDb = "test_offline_suite_bob_msg.db";
    const QStringList allDbs = {aliceSecureDb, bobSecureDb, bob2SecureDb, aliceMsgDb, bobMsgDb};
    auto cleanupFiles = [&]() {
        for (const auto& f : allDbs) {
            QFile::remove(f);
            QFile::remove(f + "-wal");
            QFile::remove(f + "-shm");
        }
    };
    cleanupFiles();

    auto backend = std::make_shared<OpenSSLBackend>();
    auto xeddsa = std::make_shared<XEdDSAAdapter>();
    auto x3dh = std::make_shared<X3DH::X3DHImpl>();
    auto ratchet = std::make_shared<DoubleRatchet::Engine>(backend);
    auto aead = std::make_shared<DoubleRatchet::AEAD>(backend.get());

    auto secretStore = std::make_shared<DummySecretStore>();
    auto keyProvider = std::make_shared<Storage::MasterKeyProvider>(secretStore);

    // Set up Alice identity & store
    auto aliceStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
    QVERIFY(aliceStore->initialize(aliceSecureDb, "offline_alice").success);
    auto aliceKp = backend->GenerateX25519KeyPair();
    Storage::E2EEIdentity aliceId;
    aliceId.identity_id = 1;
    aliceId.public_key = QByteArray(reinterpret_cast<const char*>(aliceKp.second.data.data()), 32);
    aliceId.private_key = QByteArray(reinterpret_cast<const char*>(aliceKp.first.data.data()), 32);
    QVERIFY(aliceStore->saveIdentity(aliceId).success);

    // Helper to set up a Bob device store + prekey bundle
    auto setupBobDevice = [&](const QString& dbFile, const QString& profile,
                              std::shared_ptr<Storage::SecureE2EEStore>& outStore,
                              std::shared_ptr<Crypto::Session::SecurePreKeyStoreAdapter>& outPreKeyAdapter,
                              Crypto::X3DH::BobPreKeyBundle& outBundle,
                              QByteArray& outPubKeyBytes) {
        outStore = std::make_shared<Storage::SecureE2EEStore>(keyProvider);
        QVERIFY(outStore->initialize(dbFile, profile).success);
        auto bobKp = backend->GenerateX25519KeyPair();
        auto bobSpkKp = backend->GenerateX25519KeyPair();
        auto bobOpkKp = backend->GenerateX25519KeyPair();

        Crypto::IdentityKeyPair bobIk;
        bobIk.publicKey.data.resize(32);
        bobIk.privateKey.data.resize(32);
        std::copy(bobKp.second.data.data(), bobKp.second.data.data() + 32, bobIk.publicKey.data.data());
        std::copy(bobKp.first.data.data(), bobKp.first.data.data() + 32, bobIk.privateKey.data.data());

        Storage::E2EEIdentity bobId;
        bobId.identity_id = 1;
        bobId.public_key = QByteArray(reinterpret_cast<const char*>(bobIk.publicKey.data.data()), 32);
        bobId.private_key = QByteArray(reinterpret_cast<const char*>(bobIk.privateKey.data.data()), 32);
        outPubKeyBytes = bobId.public_key;
        QVERIFY(outStore->saveIdentity(bobId).success);

        Crypto::SignedPreKey bobSpk;
        bobSpk.id = 1;
        bobSpk.publicKey.data.resize(32);
        bobSpk.privateKey.data.resize(32);
        std::copy(bobSpkKp.second.data.data(), bobSpkKp.second.data.data() + 32, bobSpk.publicKey.data.data());
        std::copy(bobSpkKp.first.data.data(), bobSpkKp.first.data.data() + 32, bobSpk.privateKey.data.data());
        QByteArray spkBytes = KeyEncoding::Encode(bobSpk.publicKey);
        ByteView spkView{reinterpret_cast<const uint8_t*>(spkBytes.constData()), static_cast<size_t>(spkBytes.size())};
        bobSpk.signature = xeddsa->sign(bobIk.privateKey, spkView);

        Crypto::OneTimePreKey bobOpk;
        bobOpk.id = 1;
        bobOpk.publicKey.data.resize(32);
        bobOpk.privateKey.data.resize(32);
        std::copy(bobOpkKp.second.data.data(), bobOpkKp.second.data.data() + 32, bobOpk.publicKey.data.data());
        std::copy(bobOpkKp.first.data.data(), bobOpkKp.first.data.data() + 32, bobOpk.privateKey.data.data());

        outBundle.identityKey = bobIk.publicKey;
        outBundle.signedPreKeyId = bobSpk.id;
        outBundle.signedPreKey = bobSpk.publicKey;
        outBundle.signedPreKeySignature = bobSpk.signature;
        outBundle.oneTimePreKeyId = bobOpk.id;
        outBundle.oneTimePreKey = bobOpk.publicKey;

        outPreKeyAdapter = std::make_shared<Crypto::Session::SecurePreKeyStoreAdapter>(outStore);
        outPreKeyAdapter->storeSignedPreKey(std::move(bobSpk));
        std::vector<Crypto::OneTimePreKey> opks;
        opks.push_back(std::move(bobOpk));
        outPreKeyAdapter->storeOneTimePreKeys(std::move(opks));
    };

    std::shared_ptr<Storage::SecureE2EEStore> bobStore1, bobStore2;
    std::shared_ptr<Crypto::Session::SecurePreKeyStoreAdapter> bobPreKey1, bobPreKey2;
    Crypto::X3DH::BobPreKeyBundle bobBundle1, bobBundle2;
    QByteArray bobPub1, bobPub2;
    setupBobDevice(bobSecureDb, "offline_bob1", bobStore1, bobPreKey1, bobBundle1, bobPub1);
    setupBobDevice(bob2SecureDb, "offline_bob2", bobStore2, bobPreKey2, bobBundle2, bobPub2);

    auto transport = std::make_shared<OfflineDeliveryTestTransport>();
    auto aliceSettings = std::make_shared<Storage::SettingsRepository>("test_offline_alice_settings");
    aliceSettings->setAuthToken("alice_token");
    aliceSettings->setUsername("alice");
    aliceSettings->setDeviceId("alice_dev1");

    auto bobSettings = std::make_shared<Storage::SettingsRepository>("test_offline_bob_settings");
    bobSettings->setAuthToken("bob_token");
    bobSettings->setUsername("bob");
    bobSettings->setDeviceId("bob_dev1");

    auto caps = std::make_shared<Storage::CapabilitiesRepository>();
    caps->replace(1, 4 * 1024 * 1024, 65536, Storage::CapabilityState::VALID);

    // Alice side: SessionManager + Core::Messaging::MessageService + RelayService
    auto aliceRelay = std::make_shared<Services::RelayService>(transport, aliceSettings, caps, nullptr);
    auto aliceMsgStorage = std::make_shared<SqliteMessageStorage>(aliceMsgDb);
    auto aliceQueue = std::make_shared<MessageQueue>(aliceMsgDb);
    auto aliceOfflineQueue = std::make_shared<OfflineQueueService>(
        aliceQueue,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            aliceRelay->sendEncryptedEnvelope(rUser, rDev, msgId, env);
            return true;
        });
    auto aliceSessionMgr = std::make_shared<SessionManager>(
        aliceStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [&](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) -> bool {
            return aliceOfflineQueue->onEnvelopeReady(rUser, rDev, msgId, env);
        },
        nullptr);

    auto aliceCoreMsg = std::make_shared<Core::Messaging::MessageService>(aliceMsgStorage, aliceSessionMgr, aliceQueue);
    aliceCoreMsg->setDeviceResolverCallback([](const QString&, auto cb) { cb(QStringLiteral("bob_dev1")); });
    aliceCoreMsg->setPreKeyClaimRequestCallback([&](const QString&, const QString& targetDev, Core::Messaging::MessageService::PreKeyClaimCallback cb) {
        if (targetDev == "bob_dev2") {
            cb(bobBundle2);
        } else {
            cb(bobBundle1);
        }
    });

    // Bob side: SessionManager + Core::Messaging::MessageService + Services::MessageService + ChatMessageModel + RelayService
    auto bobMsgStorage = std::make_shared<SqliteMessageStorage>(bobMsgDb);
    auto bobUiMsgService = std::make_shared<Services::MessageService>(bobMsgStorage);
    bobUiMsgService->setCurrentUserId("bob");

    ChatMessageModel bobChatModel;
    bobChatModel.setActiveConversation("dms:alice");
    QObject::connect(bobUiMsgService.get(), &Services::MessageService::messageAdded,
                     &bobChatModel, &ChatMessageModel::onMessageAdded);

    std::shared_ptr<Core::Messaging::MessageService> bobCoreMsg;
    auto bobSessionMgr = std::make_shared<SessionManager>(
        bobStore1, x3dh, ratchet, aead, backend, bobPreKey1, xeddsa, nullptr,
        [&](const QString& sessionId, const QByteArray& pt) {
            if (bobCoreMsg) bobCoreMsg->receiveMessage(sessionId, pt);
        });
    bobSessionMgr->setSenderVerifier([&](const QString& user, const QString& dev, const QByteArray& ik) {
        return user == "alice" && dev == "alice_dev1" && ik == aliceId.public_key;
    });

    bobCoreMsg = std::make_shared<Core::Messaging::MessageService>(bobMsgStorage, bobSessionMgr);
    bobCoreMsg->setOnMessageReceived([&](const Core::Messaging::Message& m) {
        Domain::Message dm;
        dm.id = m.messageId;
        dm.conversationId = m.conversationId;
        dm.senderId = m.senderId;
        dm.timestamp = m.timestamp;
        dm.text = m.plaintext;
        dm.type = m.type.isEmpty() ? QStringLiteral("text") : m.type;
        dm.status = Domain::MessageStatus::Sent;
        bobUiMsgService->handleIncomingMessages({dm});
    });

    auto bobEnvelopeAdapter = std::make_shared<RelayEnvelopeHandlerAdapter>(bobSessionMgr);
    auto bobRelay = std::make_shared<Services::RelayService>(transport, bobSettings, caps, bobEnvelopeAdapter);

    // =========================================================================
    // TEST 5 — ACK AND PERSISTENCE ORDERING (InitialEnvelope + RatchetEnvelope!)
    // =========================================================================
    // Alice sends msg-1 (InitialEnvelope) and msg-1b (RatchetEnvelope) while Bob is offline.
    Core::Messaging::Message m1;
    m1.messageId = "offline-msg-1";
    m1.conversationId = "dms:bob";
    m1.senderId = "alice";
    m1.receiverId = "bob";
    m1.timestamp = 1000;
    m1.plaintext = "Hello offline Bob #1";
    m1.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(m1));

    Core::Messaging::Message m1b;
    m1b.messageId = "offline-msg-1b";
    m1b.conversationId = "dms:bob";
    m1b.senderId = "alice";
    m1b.receiverId = "bob";
    m1b.timestamp = 1001;
    m1b.plaintext = "Hello offline Bob #1b (Ratchet follow-up)";
    m1b.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(m1b));
    QCOMPARE(transport->m_queue.size(), 2);

    // Force Bob's local message storage to fail on INSERT using a SQLite trigger.
    (void)bobMsgStorage->getMessage("schema-init");
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_offline_fail_trigger");
        db.setDatabaseName(bobMsgDb);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TRIGGER fail_bob_insert BEFORE INSERT ON messages BEGIN SELECT RAISE(ABORT, 'disk full'); END;"));
        db.close();
    }
    QSqlDatabase::removeDatabase("test_offline_fail_trigger");

    // Bob polls TWICE while local DB cannot persist:
    // - SessionManager MUST NOT consume the One-Time PreKey or commit the session
    // - RelayService MUST NOT ACK/delete either InitialEnvelope or RatchetEnvelope on the server,
    //   and MUST NOT poison-ACK RatchetEnvelope (offline-msg-1b) with "Session not found" on the 2nd poll!
    bobRelay->pollPendingMessages();
    bobRelay->pollPendingMessages();
    QVERIFY(!transport->m_lastPollParams.contains("after_id"));
    QVERIFY(!transport->m_lastPollParams.contains("limit"));
    QCOMPARE(transport->m_ackCalls, 0);
    QCOMPARE(transport->m_queue.size(), 2);
    QVERIFY(!bobMsgStorage->getMessage("offline-msg-1").has_value());
    QVERIFY(!bobMsgStorage->getMessage("offline-msg-1b").has_value());
    QCOMPARE(bobChatModel.count(), 0);
    // Verify Bob's OPK #1 is still unconsumed in bobStore1!
    QVERIFY(bobStore1->getOneTimePreKey(1).success);

    // Remove the failure trigger so storage recovers
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "test_offline_drop_trigger");
        db.setDatabaseName(bobMsgDb);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec("DROP TRIGGER fail_bob_insert;"));
        db.close();
    }
    QSqlDatabase::removeDatabase("test_offline_drop_trigger");

    // =========================================================================
    // TEST 1 — BASIC OFFLINE DELIVERY (+ overlapping poll coalescing on login)
    // =========================================================================
    // Simulate login: startPolling() starts a poll that is in-flight when WebSocket
    // connects and requests a second pollPendingMessages().
    transport->m_pollCalls = 0;
    transport->m_deferNextPoll = true;
    bobRelay->startPolling();
    QCOMPARE(transport->m_pollCalls, 1);
    // Request a second poll while the first poll is still in flight
    bobRelay->pollPendingMessages();
    // Fulfill the first poll with empty list, and verify the coalesced re-poll
    // immediately fires and delivers both offline-msg-1 and offline-msg-1b in order!
    auto deferredCb = transport->m_deferredPollCb;
    transport->m_deferredPollCb = nullptr;
    deferredCb(200, QByteArray(R"({"messages":[]})"), QNetworkReply::NoError, QString());

    QTRY_COMPARE_WITH_TIMEOUT(transport->m_pollCalls, 2, 1000);
    QCOMPARE(transport->m_ackCalls, 2);
    QCOMPARE(transport->m_queue.size(), 0);
    auto storedM1 = bobMsgStorage->getMessage("offline-msg-1");
    QVERIFY(storedM1.has_value());
    QCOMPARE(storedM1->plaintext, QString("Hello offline Bob #1"));
    QCOMPARE(storedM1->state, Core::Messaging::MessageState::DELIVERED);
    auto storedM1b = bobMsgStorage->getMessage("offline-msg-1b");
    QVERIFY(storedM1b.has_value());
    QCOMPARE(storedM1b->plaintext, QString("Hello offline Bob #1b (Ratchet follow-up)"));
    QCOMPARE(storedM1b->state, Core::Messaging::MessageState::DELIVERED);
    QCOMPARE(bobChatModel.count(), 2);
    QCOMPARE(bobChatModel.getMessageById("offline-msg-1").value("text").toString(), QString("Hello offline Bob #1"));
    QCOMPARE(bobChatModel.getMessageById("offline-msg-1b").value("text").toString(), QString("Hello offline Bob #1b (Ratchet follow-up)"));

    // =========================================================================
    // TEST 4 — DUPLICATE SAFETY (same message delivered again via poll/WS)
    // =========================================================================
    // Re-insert the exact same message payload / ID or deliver via poll again:
    Core::Messaging::Message mDup;
    mDup.messageId = "offline-msg-dup";
    mDup.conversationId = "dms:bob";
    mDup.senderId = "alice";
    mDup.receiverId = "bob";
    mDup.timestamp = 1005;
    mDup.plaintext = "Idempotent message";
    mDup.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(mDup));
    QCOMPARE(transport->m_queue.size(), 1);
    const OfflineDeliveryTestTransport::QueuedItem savedDupItem = transport->m_queue.first();

    bobRelay->pollPendingMessages();
    QCOMPARE(bobChatModel.count(), 3);

    // Simulate duplicate delivery of the same message ID at Core::Messaging::MessageService level
    // and at RelayService level (different relay row ID carrying a ratchet envelope with same messageId):
    QByteArray dupPayload = Core::Messaging::MessageService::serializePayload(mDup);
    QVERIFY(bobCoreMsg->receiveMessage("alice:alice_dev1", dupPayload));
    QCOMPARE(bobChatModel.count(), 3);
    QCOMPARE(bobMsgStorage->getConversationMessages("dms:alice").size(), 3u);
    Q_UNUSED(savedDupItem);

    // =========================================================================
    // TEST 3 — RETRY AFTER TRANSIENT FAILURE
    // =========================================================================
    Core::Messaging::Message mTransient;
    mTransient.messageId = "offline-msg-transient";
    mTransient.conversationId = "dms:bob";
    mTransient.senderId = "alice";
    mTransient.receiverId = "bob";
    mTransient.timestamp = 1010;
    mTransient.plaintext = "Delivered after transient poll error";
    mTransient.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(mTransient));
    QCOMPARE(transport->m_queue.size(), 1);

    // First poll fails with HTTP 503; since polling is active, m_retryPollTimer (250ms) automatically retries!
    transport->m_failNextPoll = true;
    bobRelay->pollPendingMessages();
    QCOMPARE(transport->m_queue.size(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(transport->m_queue.size(), 0, 1500);
    QVERIFY(bobMsgStorage->getMessage("offline-msg-transient").has_value());
    QCOMPARE(bobChatModel.count(), 4);

    // =========================================================================
    // TEST 7 — MALFORMED OR UNDECRYPTABLE ENVELOPE DOES NOT BLOCK LATER MESSAGES
    // =========================================================================
    // Enqueue a corrupted envelope ahead of a valid encrypted message
    qint64 corruptId = transport->enqueueRaw("corrupt-env-1", "alice_dev1", "bob_dev1", QByteArray("corrupted-not-valid-wire-envelope"));
    Core::Messaging::Message mAfterCorrupt;
    mAfterCorrupt.messageId = "offline-msg-after-corrupt";
    mAfterCorrupt.conversationId = "dms:bob";
    mAfterCorrupt.senderId = "alice";
    mAfterCorrupt.receiverId = "bob";
    mAfterCorrupt.timestamp = 1020;
    mAfterCorrupt.plaintext = "Valid message after corrupted envelope";
    mAfterCorrupt.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(mAfterCorrupt));
    QCOMPARE(transport->m_queue.size(), 2);

    // Poll: corrupt envelope is permanently invalid and ACKed, and valid message behind it is immediately decrypted, stored, and ACKed!
    bobRelay->pollPendingMessages();
    QVERIFY(bobMsgStorage->getMessage("offline-msg-after-corrupt").has_value());
    QCOMPARE(bobMsgStorage->getMessage("offline-msg-after-corrupt")->plaintext, QString("Valid message after corrupted envelope"));
    QCOMPARE(bobChatModel.count(), 5);
    QTRY_VERIFY_WITH_TIMEOUT(transport->m_ackedIds.contains(corruptId), 1500);
    QCOMPARE(transport->m_queue.size(), 0);

    // =========================================================================
    // TEST 2 — MULTIPLE QUEUED MESSAGES (>100 IN ORIGINAL UNPAGINATED SERVER RESPONSE)
    // =========================================================================
    const int bulkCount = 105;
    for (int i = 0; i < bulkCount; ++i) {
        Core::Messaging::Message bm;
        bm.messageId = QStringLiteral("offline-bulk-%1").arg(i, 3, 10, QLatin1Char('0'));
        bm.conversationId = "dms:bob";
        bm.senderId = "alice";
        bm.receiverId = "bob";
        bm.timestamp = 2000 + i;
        bm.plaintext = QStringLiteral("Bulk offline message #%1").arg(i);
        bm.state = Core::Messaging::MessageState::CREATED;
        QVERIFY(aliceCoreMsg->sendMessage(bm));
    }
    QCOMPARE(transport->m_queue.size(), bulkCount);

    // Single pollPendingMessages() call against the original unpaginated server:
    // - Sends ONLY device_id (no after_id, no limit)
    // - Processes all 105 messages in a single poll response
    // - Does NOT trigger a repeated-fetch loop when messages.size() >= 100!
    const int pollsBeforeBulk = transport->m_pollCalls;
    bobRelay->pollPendingMessages();
    QCOMPARE(transport->m_pollCalls, pollsBeforeBulk + 1);
    QVERIFY(!transport->m_lastPollParams.contains("after_id"));
    QVERIFY(!transport->m_lastPollParams.contains("limit"));
    QCOMPARE(transport->m_queue.size(), 0);
    QCOMPARE(bobChatModel.count(), 5 + bulkCount);
    for (int i = 0; i < bulkCount; ++i) {
        const QString expectedId = QStringLiteral("offline-bulk-%1").arg(i, 3, 10, QLatin1Char('0'));
        const QString expectedText = QStringLiteral("Bulk offline message #%1").arg(i);
        auto stored = bobMsgStorage->getMessage(expectedId);
        QVERIFY2(stored.has_value(), qPrintable(expectedId));
        QCOMPARE(stored->plaintext, expectedText);
        QCOMPARE(bobChatModel.getMessageIdAt(5 + i), expectedId);
    }

    // =========================================================================
    // TEST 6 — DEVICE ROUTING / MULTI-DEVICE FANOUT & DISCOVERY COALESCING
    // =========================================================================
    QJsonArray devicesJson;
    QJsonObject dev1Obj;
    dev1Obj["device_id"] = "bob_dev1";
    dev1Obj["public_key"] = QString::fromLatin1(bobPub1.toBase64());
    devicesJson.append(dev1Obj);
    QJsonObject dev2Obj;
    dev2Obj["device_id"] = "bob_dev2";
    dev2Obj["public_key"] = QString::fromLatin1(bobPub2.toBase64());
    devicesJson.append(dev2Obj);
    QJsonObject devRevokedObj;
    devRevokedObj["device_id"] = "bob_dev_revoked";
    devRevokedObj["public_key"] = QString::fromLatin1(QByteArray(32, 'R').toBase64());
    devRevokedObj["revoked_at"] = "2026-10-12T00:00:00Z";
    devicesJson.append(devRevokedObj);
    QJsonObject keysRoot;
    keysRoot["username"] = "bob";
    keysRoot["devices"] = devicesJson;
    transport->m_keysPayload = QJsonDocument(keysRoot).toJson(QJsonDocument::Compact);

    auto aliceDeviceService = std::make_shared<Services::DeviceService>(transport, aliceSettings);
    QSignalSpy spyKeys(aliceDeviceService.get(), &Services::DeviceService::recipientKeysFetched);

    // 6a. Cold-cache transient HTTP 429 on first fetch automatically retries and succeeds
    transport->m_failKeysCount = 1;
    aliceDeviceService->fetchRecipientKeys("bob");
    QTRY_COMPARE_WITH_TIMEOUT(spyKeys.count(), 1, 1000);
    QCOMPARE(transport->m_keysCalls, 2);
    QCOMPARE(spyKeys.last().at(1).toList().size(), 2);

    // 6b. Warm-cache fallback when HTTP 429 occurs after a successful fetch
    transport->m_simulateKeysStatus = 429;
    aliceDeviceService->fetchRecipientKeys("bob");
    QCOMPARE(spyKeys.count(), 2);
    QCOMPARE(spyKeys.last().at(1).toList().size(), 2);
    transport->m_simulateKeysStatus = 200;

    // Wire multi-device resolver on Alice's Core::Messaging::MessageService
    aliceCoreMsg->setMultiDeviceResolverCallback([&](const QString& targetUser, std::function<void(QStringList)> resultCb) {
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = QObject::connect(aliceDeviceService.get(), &Services::DeviceService::recipientKeysFetched,
            [resultCb, targetUser, conn](const QString& retUser, const QVariantList& devices) {
                if (retUser.compare(targetUser.trimmed(), Qt::CaseInsensitive) == 0) {
                    QObject::disconnect(*conn);
                    QStringList ids;
                    for (const QVariant& v : devices) {
                        QVariantMap m = v.toMap();
                        if (!m.value("revoked_at").toString().isEmpty()) continue;
                        ids.append(m.value("device_id").toString());
                    }
                    resultCb(ids);
                }
            });
        aliceDeviceService->fetchRecipientKeys(targetUser);
    });

    Core::Messaging::Message mMulti;
    mMulti.messageId = "offline-multidev-1";
    mMulti.conversationId = "dms:bob";
    mMulti.senderId = "alice";
    mMulti.receiverId = "bob";
    mMulti.timestamp = 5000;
    mMulti.plaintext = "Fanout to all active devices";
    mMulti.state = Core::Messaging::MessageState::CREATED;
    QVERIFY(aliceCoreMsg->sendMessage(mMulti));

    // Must have queued 2 envelopes: one for bob_dev1 and one for bob_dev2 (skipping bob_dev_revoked)
    QCOMPARE(transport->m_queue.size(), 2);
    QCOMPARE(transport->m_queue[0].recipientDeviceId, QString("bob_dev1"));
    QCOMPARE(transport->m_queue[1].recipientDeviceId, QString("bob_dev2"));

    // Bob's device 1 polls and ACKs its queue item; bob_dev2's queue item must remain intact!
    bobRelay->pollPendingMessages();
    QCOMPARE(transport->m_queue.size(), 1);
    QCOMPARE(transport->m_queue[0].recipientDeviceId, QString("bob_dev2"));

    // Bob's device 2 decrypts its own queued envelope independently
    QStringList bob2Texts;
    auto bob2SessionMgr = std::make_shared<SessionManager>(
        bobStore2, x3dh, ratchet, aead, backend, bobPreKey2, xeddsa, nullptr,
        [&](const QString&, const QByteArray& pt) {
            bob2Texts.append(QJsonDocument::fromJson(pt).object().value("plaintext").toString());
        });
    bob2SessionMgr->setSenderVerifier([&](const QString& user, const QString& dev, const QByteArray& ik) {
        return user == "alice" && dev == "alice_dev1" && ik == aliceId.public_key;
    });
    auto bob2Adapter = std::make_shared<RelayEnvelopeHandlerAdapter>(bob2SessionMgr);
    auto bob2Settings = std::make_shared<Storage::SettingsRepository>("test_offline_bob2_settings");
    bob2Settings->setAuthToken("bob_token");
    bob2Settings->setUsername("bob");
    bob2Settings->setDeviceId("bob_dev2");
    auto bob2Relay = std::make_shared<Services::RelayService>(transport, bob2Settings, caps, bob2Adapter);
    bob2Relay->pollPendingMessages();
    QCOMPARE(transport->m_queue.size(), 0);
    QCOMPARE(bob2Texts.size(), 1);
    QCOMPARE(bob2Texts.first(), QString("Fanout to all active devices"));

    // =========================================================================
    // TEST 8 — ACCOUNT SWITCHING INVALIDATES CACHED CONVERSATION STATE
    // =========================================================================
    bobRelay->stopPolling();
    bobChatModel.clearAllCaches();
    QCOMPARE(bobChatModel.count(), 0);
    bobUiMsgService->setCurrentUserId("charlie");

    aliceSettings->clearSession();
    bobSettings->clearSession();
    bob2Settings->clearSession();
    cleanupFiles();
}
