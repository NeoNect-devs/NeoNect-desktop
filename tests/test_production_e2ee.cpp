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
            relayService->sendEncryptedEnvelope(rUser, rDev, msgId, env);
            return true;
        });

    auto sessionManager = std::make_shared<SessionManager>(
        secureStore, x3dh, ratchet, aead, backend, nullptr, xeddsa,
        [offlineQueueService](const QString& rUser, const QString& rDev, const QString& msgId, const QByteArray& env) {
            qDebug() << "SessionManager->OfflineQueue: onEnvelopeReady!";
            offlineQueueService->onEnvelopeReady(rUser, rDev, msgId, env);
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
