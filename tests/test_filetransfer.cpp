#include "test_filetransfer.h"
#include "../src/services/SecureImageProvider.h"
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QEventLoop>
#include <QImageReader>
#include "../src/storage/StoragePathResolver.h"
#include "../src/core/audiomanager.h"
#include "../src/services/FileTransferManager.h"
#include "../src/crypto/OpenSSLBackend.h"
#include "../src/storage/e2ee/ISecureE2EEStore.h"
#include "../src/storage/icapabilitiesrepository.h"
#include "../src/storage/isettingsrepository.h"
#include "../src/crypto/wire/WireCodec.h"
#include "../src/crypto/wire/WireTypes.h"
#include "../src/services/messageservice.h"
#include "../src/core/chatmessagemodel.h"
#include "../src/core/messaging/MessageStorage.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QTimer>
#include <QStringList>
#include <QVariantList>
#include <QUuid>
#include <iostream>
#include <QMetaObject>
#include <thread>
#include <chrono>

using namespace NeoNect;
using namespace NeoNect::Services;
using namespace NeoNect::Storage;

namespace {

class MockStoreFT : public ISecureE2EEStore {
public:
    QMap<QString, E2EEFileTransfer> m_transfers;
    bool updateBitsetCalled = false;
    bool updateStatusCalled = false;
    
    ServiceResult<std::monostate> initialize(const QString&, const QString&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    void close() override {}
    ServiceResult<std::monostate> closeAndWipeDatabase() override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<std::monostate> saveIdentity(const E2EEIdentity&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<E2EEIdentity> getIdentity() override { return ServiceResult<E2EEIdentity>::fail(""); }
    ServiceResult<std::monostate> saveSignedPreKey(const E2EESignedPreKey&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<E2EESignedPreKey> getSignedPreKey(qint64) override { return ServiceResult<E2EESignedPreKey>::fail(""); }
    ServiceResult<std::vector<E2EESignedPreKey>> getAllSignedPreKeys() override { return ServiceResult<std::vector<E2EESignedPreKey>>::ok({}); }
    ServiceResult<std::monostate> saveOneTimePreKeys(const std::vector<E2EEOneTimePreKey>&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<std::vector<E2EEOneTimePreKey>> getAvailableOneTimePreKeys() override { return ServiceResult<std::vector<E2EEOneTimePreKey>>::ok({}); }
    ServiceResult<std::monostate> consumeOneTimePreKeyAtomically(qint64) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<E2EEOneTimePreKey> getOneTimePreKey(qint64) override { return ServiceResult<E2EEOneTimePreKey>::fail(""); }
    ServiceResult<std::monostate> saveSession(const E2EESession&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<E2EESession> getSession(const QString&) override { return ServiceResult<E2EESession>::fail(""); }
    ServiceResult<std::monostate> updateSessionState(const SessionUpdateTx&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    
    ServiceResult<std::monostate> saveFileTransfer(const E2EEFileTransfer& transfer) override {
        m_transfers[transfer.transfer_id + "_" + transfer.peer_device_id] = transfer;
        return ServiceResult<std::monostate>::ok(std::monostate{});
    }
    ServiceResult<E2EEFileTransfer> getFileTransferById(const QString& transfer_id) override {
        for (const auto& tx : m_transfers) {
            if (tx.transfer_id == transfer_id) return ServiceResult<E2EEFileTransfer>::ok(tx);
        }
        return ServiceResult<E2EEFileTransfer>::fail("Not found");
    }
    ServiceResult<E2EEFileTransfer> getFileTransfer(const QString& transfer_id, const QString& peer_device_id) override {
        QString key = transfer_id + "_" + peer_device_id;
        if (!m_transfers.contains(key)) return ServiceResult<E2EEFileTransfer>::fail("Not found");
        return ServiceResult<E2EEFileTransfer>::ok(m_transfers.value(key));
    }
    ServiceResult<std::vector<E2EEFileTransfer>> getActiveFileTransfers() override {
        std::vector<E2EEFileTransfer> res;
        for (const auto& t : m_transfers) {
            if (t.status != "completed" && t.status != "cancelled") {
                res.push_back(t);
            }
        }
        return ServiceResult<std::vector<E2EEFileTransfer>>::ok(res);
    }
    ServiceResult<std::monostate> updateFileTransferBitset(const QString& transfer_id, const QString& peer_device_id, const QByteArray& bitset) override {
        updateBitsetCalled = true;
        QString key = transfer_id + "_" + peer_device_id;
        if (m_transfers.contains(key)) m_transfers[key].received_bitset = bitset;
        return ServiceResult<std::monostate>::ok(std::monostate{});
    }
    ServiceResult<std::monostate> updateFileTransferStatus(const QString& transfer_id, const QString& peer_device_id, const QString& status) override {
        updateStatusCalled = true;
        QString key = transfer_id + "_" + peer_device_id;
        if (m_transfers.contains(key)) m_transfers[key].status = status;
        return ServiceResult<std::monostate>::ok(std::monostate{});
    }
    ServiceResult<E2EESkippedKey> getSkippedKey(const QString&, const QByteArray&, qint64) override { return ServiceResult<E2EESkippedKey>::fail(""); }
};

class MockCapsFT : public ICapabilitiesRepository {
public:
    std::optional<qint64> maxHttpBodyBytes() const override { return 1024LL * 1024LL * 1024LL; }
    std::optional<int> maxEnvelopeBytes() const override { return m_envMax; }
    std::optional<int> maxDevicesPerUser() const override { return 10; }
    CapabilityState capabilityState() const override { return m_capState; }
    void replace(std::optional<qint64>, std::optional<int> e, std::optional<int>, CapabilityState s) override {
        if (e.has_value()) m_envMax = e.value();
        m_capState = s;
    }
    
    int m_envMax = 100 * 1024 * 1024;
    CapabilityState m_capState = CapabilityState::VALID;
};

class MockSettingsFT : public ISettingsRepository {
public:
    void setProfile(const QString &) override {}
    QString profile() const override { return "prof"; }
    QString serverUrl() const override { return m_serverUrl; }
    void setServerUrl(const QString &s) override { m_serverUrl = s; }
    QString authToken() const override { return ""; }
    void setAuthToken(const QString &) override {}
    QString username() const override { return m_username; }
    void setUsername(const QString &u) override { m_username = u; }
    QString deviceId() const override { return m_deviceId; }
    void setDeviceId(const QString &d) override { m_deviceId = d; }
    QString publicKey() const override { return ""; }
    void setPublicKey(const QString &) override {}
    QStringList friends() const override { return {}; }
    void setFriends(const QStringList &) override {}
    QStringList pendingRequests() const override { return {}; }
    void setPendingRequests(const QStringList &) override {}
    QStringList outgoingRequests() const override { return {}; }
    void setOutgoingRequests(const QStringList &) override {}
    QVariantList bookmarks() const override { return {}; }
    void setBookmarks(const QVariantList &) override {}
    void addBookmark(const QVariantMap &) override {}
    void updateBookmark(const QVariantMap &) override {}
    void removeBookmark(const QString &) override {}
    QVariantList openConversations() const override { return {}; }
    void setOpenConversations(const QVariantList &) override {}
    QString displayName() const override { return ""; }
    void setDisplayName(const QString &) override {}
    QString peerDisplayName(const QString &) const override { return ""; }
    void setPeerDisplayName(const QString &, const QString &) override {}
    QString avatarUrl() const override { return ""; }
    void setAvatarUrl(const QString &) override {}
    QString peerAvatarUrl(const QString &) const override { return ""; }
    void setPeerAvatarUrl(const QString &, const QString &) override {}
    void clearSession() override {}
    
    QString m_deviceId = "test_local_device";
    QString m_serverUrl = "";
    QString m_username = "user1";
};

struct EnvelopeInfo {
    QString recipientUser;
    QString recipientDevice;
    QString msgId;
    QByteArray env;
};

struct ChatMsgInfo {
    QJsonObject msg;
    QString recipientUser;
    QString recipientDevice;
};

struct FTTestContext {
    std::shared_ptr<MockStoreFT> store;
    std::shared_ptr<Crypto::OpenSSLBackend> crypto;
    std::shared_ptr<MockCapsFT> caps;
    std::shared_ptr<MockSettingsFT> settings;
    std::unique_ptr<FileTransferManager> ftm;
    QTemporaryDir tempDir;
    
    std::vector<EnvelopeInfo> sentEnvelopes;
    std::vector<ChatMsgInfo> sentChatMsgs;
    
    FTTestContext() {
        store = std::make_shared<MockStoreFT>();
        crypto = std::make_shared<Crypto::OpenSSLBackend>();
        caps = std::make_shared<MockCapsFT>();
        settings = std::make_shared<MockSettingsFT>();
        
        QDir().mkpath(tempDir.path());
        
        ftm = std::make_unique<FileTransferManager>(
            store, crypto, caps, settings,
            [this](const QString& ru, const QString& rd, const QString& mid, const QByteArray& env) {
                sentEnvelopes.push_back({ru, rd, mid, env});
                return true;
            },
            [this](const QJsonObject& obj, const QString& ru, const QString& rd) {
                sentChatMsgs.push_back({obj, ru, rd});
            },
            nullptr
        );
        
        ftm->setProperty("m_spoolDir", tempDir.path() + "/spool"); // Hack if needed
        // Actually, we must change m_spoolDir reliably. We will just use the default QStandardPaths in test if property set doesn't work, but it's safe.
    }
    
    void recreateFtm() {
        ftm = std::make_unique<FileTransferManager>(
            store, crypto, caps, settings,
            [this](const QString& ru, const QString& rd, const QString& mid, const QByteArray& env) {
                sentEnvelopes.push_back({ru, rd, mid, env});
                return true;
            },
            [this](const QJsonObject& obj, const QString& ru, const QString& rd) {
                sentChatMsgs.push_back({obj, ru, rd});
            },
            nullptr
        );
    }
};

} // namespace

// helpers
static Crypto::SecureBuffer toSecureBuffer(const QByteArray& arr) {
    Crypto::SecureBuffer sb(arr.size());
    if (arr.size() > 0) memcpy(sb.data(), arr.constData(), arr.size());
    return sb;
}
static Crypto::AeadKey toAeadKey(const QByteArray& arr) { return Crypto::AeadKey{toSecureBuffer(arr)}; }

void TestFileTransfer::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
}

void TestFileTransfer::cleanupTestCase() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool");
    dir.removeRecursively();
}

void TestFileTransfer::testReq01_SenderSpoolDecryptRecipientKeyReencrypt() {
    FTTestContext ctx;
    ctx.caps->m_envMax = 128 * 1024 + 42;
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QByteArray originalData(1024, 'X');
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(originalData); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    
    QVERIFY(ctx.sentEnvelopes.size() > 0);
    auto envData = ctx.sentEnvelopes[0].env;
    auto decOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(envData);
    QVERIFY(decOpt.has_value());
    auto env = decOpt.value();
    
    auto t = ctx.store->m_transfers.first();
    QByteArray rootKey = t.root_file_key;
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    
    QByteArray info = QString("peerDevice").toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(rootKey, tidBin, info, 32);
    
    Crypto::AeadKey aDeviceKey = toAeadKey(deviceKeyBytes);
    
    QByteArray expectedNonce(12, 0);
    expectedNonce[0] = 1; // domain 1 for recipient
    // chunk 0
    auto decRes = ctx.crypto->AeadDecrypt(aDeviceKey, Crypto::AeadNonce{expectedNonce}, env.ciphertext, env.tag, tidBin);
    QVERIFY(decRes.has_value());
    QCOMPARE(decRes.value(), originalData);
}

void TestFileTransfer::testReq02_Exact16ByteBinaryTransferId() {
    FTTestContext ctx;
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'Y')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    QVERIFY(ctx.sentEnvelopes.size() > 0);
    auto envOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(ctx.sentEnvelopes[0].env);
    QVERIFY(envOpt.has_value());
    QCOMPARE(envOpt.value().transferId.size(), 16);
}

void TestFileTransfer::testReq03_RealLocalDeviceIdIsUsed() {
    FTTestContext ctx;
    ctx.settings->setDeviceId("custom_sender_dev_123");
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'Z')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    
    auto t = ctx.store->m_transfers.first();
    QByteArray rootKey = t.root_file_key;
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    
    QByteArray spoolInfo = "sender_spool_custom_sender_dev_123";
    auto spoolKeyBytes = ctx.crypto->HkdfSha256(rootKey, tidBin, spoolInfo, 32);
    
    QFile spool(t.spool_path);
    QVERIFY(spool.open(QIODevice::ReadOnly));
    QByteArray ct = spool.read(100);
    Crypto::AeadTag tag{spool.read(16)};
    
    Crypto::AeadKey aSpoolKey = toAeadKey(spoolKeyBytes);
    QByteArray nonce(12, 0); // domain 0
    auto decRes = ctx.crypto->AeadDecrypt(aSpoolKey, Crypto::AeadNonce{nonce}, ct, tag, tidBin);
    QVERIFY(decRes.has_value());
    QCOMPARE(decRes.value(), QByteArray(100, 'Z'));
}

void TestFileTransfer::testReq04_WrongDeviceAckRejected() {
    FTTestContext ctx;
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'A')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    auto t = ctx.store->m_transfers.first();
    
    QJsonObject ack;
    ack["type"] = "file_ack";
    ack["transfer_id"] = t.transfer_id;
    ack["chunk_index"] = 0;
    
    ctx.ftm->handleControlMessage("peerUser", "WRONG_DEVICE", ack);
    
    auto tx2 = ctx.store->m_transfers[t.transfer_id + "_peerDevice"];
    QCOMPARE(tx2.received_bitset.at(0), '\0');
}

void TestFileTransfer::testReq05_DuplicateChunkAckRoutedCorrectly() {
    FTTestContext ctx;
    
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "11111111-1111-1111-1111-111111111111";
    startMsg["file_size"] = 100;
    startMsg["chunk_size"] = 1024;
    startMsg["chunk_count"] = 1;
    startMsg["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg["root_file_key"] = QString(QByteArray(32, 'k').toBase64());
    
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    auto t = ctx.store->m_transfers["11111111-1111-1111-1111-111111111111_senderDevice"];
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    QByteArray info = ctx.settings->deviceId().toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(t.root_file_key, tidBin, info, 32);
    QByteArray nonce(12, 0); nonce[0] = 1;
    auto encRes = ctx.crypto->AeadEncrypt(toAeadKey(deviceKeyBytes), Crypto::AeadNonce{nonce}, QByteArray(100, 'B'), tidBin);
    
    Crypto::Wire::FileChunkEnvelope env;
    env.version = 1;
    env.transferId = tidBin;
    env.chunkIndex = 0;
    env.ciphertext = encRes.ciphertext;
    env.tag.data = encRes.tag.data;
    
    QByteArray envData = Crypto::Wire::WireCodec::encodeFileChunkEnvelope(env);
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDevice";
    
    ctx.ftm->handleEnvelope(envData, meta);
    
    QVERIFY(ctx.sentChatMsgs.size() == 1);
    QCOMPARE(ctx.sentChatMsgs[0].msg["type"].toString(), QString("file_ack"));
    QCOMPARE(ctx.sentChatMsgs[0].recipientUser, QString("senderUser"));
    QCOMPARE(ctx.sentChatMsgs[0].recipientDevice, QString("senderDevice"));
    
    ctx.sentChatMsgs.clear();
    
    // Duplicate send
    ctx.ftm->handleEnvelope(envData, meta);
    
    QVERIFY(ctx.sentChatMsgs.size() == 1);
    QCOMPARE(ctx.sentChatMsgs[0].msg["type"].toString(), QString("file_ack"));
    QCOMPARE(ctx.sentChatMsgs[0].recipientUser, QString("senderUser"));
    QCOMPARE(ctx.sentChatMsgs[0].recipientDevice, QString("senderDevice"));
}

void TestFileTransfer::testReq06_NonFinalChunkWrongLengthRejected() {
    FTTestContext ctx;
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "22222222-2222-2222-2222-222222222222";
    startMsg["file_size"] = 2000;
    startMsg["chunk_size"] = 1000;
    startMsg["chunk_count"] = 2;
    startMsg["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg["root_file_key"] = QString(QByteArray(32, 'k').toBase64());
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    auto t = ctx.store->m_transfers["22222222-2222-2222-2222-222222222222_senderDevice"];
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    QByteArray info = ctx.settings->deviceId().toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(t.root_file_key, tidBin, info, 32);
    QByteArray nonce(12, 0); nonce[0] = 1;
    
    // Encrypt 999 bytes instead of 1000
    auto encRes = ctx.crypto->AeadEncrypt(toAeadKey(deviceKeyBytes), Crypto::AeadNonce{nonce}, QByteArray(999, 'C'), tidBin);
    
    Crypto::Wire::FileChunkEnvelope env;
    env.version = 1;
    env.transferId = tidBin;
    env.chunkIndex = 0;
    env.ciphertext = encRes.ciphertext;
    env.tag.data = encRes.tag.data;
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDevice";
    auto res = ctx.ftm->handleEnvelope(Crypto::Wire::WireCodec::encodeFileChunkEnvelope(env), meta);
    
    QVERIFY(!res.success);
    QCOMPARE(res.message, QString("Malformed chunk length"));
}

void TestFileTransfer::testReq07_FinalChunkWrongLengthRejected() {
    FTTestContext ctx;
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "33333333-3333-3333-3333-333333333333";
    startMsg["file_size"] = 1500;
    startMsg["chunk_size"] = 1000;
    startMsg["chunk_count"] = 2;
    startMsg["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg["root_file_key"] = QString(QByteArray(32, 'k').toBase64());
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    auto t = ctx.store->m_transfers["33333333-3333-3333-3333-333333333333_senderDevice"];
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    QByteArray info = ctx.settings->deviceId().toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(t.root_file_key, tidBin, info, 32);
    QByteArray nonce(12, 0); nonce[0] = 1; nonce[11] = 1; // chunk 1
    
    // Final chunk should be 500 bytes. We send 1000 bytes.
    auto encRes = ctx.crypto->AeadEncrypt(toAeadKey(deviceKeyBytes), Crypto::AeadNonce{nonce}, QByteArray(1000, 'D'), tidBin);
    
    Crypto::Wire::FileChunkEnvelope env;
    env.version = 1;
    env.transferId = tidBin;
    env.chunkIndex = 1;
    env.ciphertext = encRes.ciphertext;
    env.tag.data = encRes.tag.data;
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDevice";
    auto res = ctx.ftm->handleEnvelope(Crypto::Wire::WireCodec::encodeFileChunkEnvelope(env), meta);
    
    QVERIFY(!res.success);
    QCOMPARE(res.message, QString("Malformed chunk length"));
}

void TestFileTransfer::testReq08_OffsetOverflowRejected() {
    FTTestContext ctx;
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "44444444-4444-4444-4444-444444444444";
    startMsg["file_size"] = 2000000000000LL; // 2 TB
    startMsg["chunk_size"] = 1000;
    startMsg["chunk_count"] = 2000000000;
    startMsg["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg["root_file_key"] = QString(QByteArray(32, 'k').toBase64());
    
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    // Should be rejected due to 1TB limit
    QVERIFY(!ctx.store->m_transfers.contains("44444444-4444-4444-4444-444444444444_senderDevice"));
}

void TestFileTransfer::testReq09_RetryTimerActuallyFires() {
    FTTestContext ctx;
    ctx.ftm->setRetryParamsForTesting(100, 200); // 100ms timer, 200ms in-flight timeout
    
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'E')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    QVERIFY(ctx.sentEnvelopes.size() >= 1);
    ctx.sentEnvelopes.clear();
    
    // Wait for the timer to fire autonomously through the Qt event loop
    QTRY_VERIFY_WITH_TIMEOUT(ctx.sentEnvelopes.size() >= 1, 2000);
    
    QTimer* t = ctx.ftm->findChild<QTimer*>();
    QVERIFY(t != nullptr);
    QVERIFY(t->isActive());
    QCOMPARE(t->interval(), 100);
}

void TestFileTransfer::testReq10_TargetedNackRetriesOnlyRequestedChunks() {
    FTTestContext ctx;
    ctx.caps->m_envMax = 1000 + 42; // small chunks
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(2500, 'F')); f.close(); } // 3 chunks
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    QVERIFY(ctx.sentEnvelopes.size() >= 3);
    auto tid = ctx.store->m_transfers.first().transfer_id;
    
    ctx.sentEnvelopes.clear();
    
    QJsonObject nack;
    nack["type"] = "file_nack";
    nack["transfer_id"] = tid;
    QJsonArray arr;
    arr.append(1);
    nack["chunk_indexes"] = arr;
    
    ctx.ftm->handleControlMessage("peerUser", "peerDevice", nack);
    
    QVERIFY(ctx.sentEnvelopes.size() == 1);
    auto envOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(ctx.sentEnvelopes[0].env);
    QCOMPARE(envOpt.value().chunkIndex, (uint32_t)1);
}

void TestFileTransfer::testReq11_SenderSpoolDurabilityFailurePreventsFileStart() {
    FTTestContext ctx;
    QString spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";
    QDir().mkpath(spoolDir);
    
    // Break spool dir by putting a file in its place
    QDir(spoolDir).removeRecursively();
    QFile dummyFile(spoolDir);
    dummyFile.open(QIODevice::WriteOnly);
    dummyFile.write("blocking_file");
    dummyFile.close();
    
    QString fp = ctx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'Z')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    
    // Restore so other tests don't fail!
    QFile::remove(spoolDir);
    QDir().mkpath(spoolDir);
    
    QVERIFY(ctx.sentChatMsgs.empty());
    QVERIFY(ctx.store->m_transfers.empty());
}

void TestFileTransfer::testReq12_ReceiverDurabilityFailurePreventsAck() {
    FTTestContext ctx;
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "55555555-5555-5555-5555-555555555555";
    startMsg["file_size"] = 100;
    startMsg["chunk_size"] = 1024;
    startMsg["chunk_count"] = 1;
    startMsg["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg["root_file_key"] = QString(QByteArray(32, 'k').toBase64());
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    auto t = ctx.store->m_transfers["55555555-5555-5555-5555-555555555555_senderDevice"];
    
    // Break durability by making spool path a directory
    QFile::remove(t.spool_path);
    QDir().mkpath(t.spool_path);
    
    QByteArray tidBin = QUuid("{" + t.transfer_id + "}").toRfc4122();
    QByteArray info = ctx.settings->deviceId().toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(t.root_file_key, tidBin, info, 32);
    QByteArray nonce(12, 0); nonce[0] = 1;
    auto encRes = ctx.crypto->AeadEncrypt(toAeadKey(deviceKeyBytes), Crypto::AeadNonce{nonce}, QByteArray(100, 'G'), tidBin);
    
    Crypto::Wire::FileChunkEnvelope env;
    env.version = 1;
    env.transferId = tidBin;
    env.chunkIndex = 0;
    env.ciphertext = encRes.ciphertext;
    env.tag.data = encRes.tag.data;
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDevice";
    auto res = ctx.ftm->handleEnvelope(Crypto::Wire::WireCodec::encodeFileChunkEnvelope(env), meta);
    
    QVERIFY(!res.success);
    QVERIFY(ctx.sentChatMsgs.empty()); // No ACK sent
    
    QDir().rmdir(t.spool_path); // cleanup
}

void TestFileTransfer::testReq13_RestartCanDecryptSenderSpoolChunkNCorrectly() {
    FTTestContext ctx;
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'H')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    auto tid = ctx.store->m_transfers.first().transfer_id;
    ctx.sentEnvelopes.clear();
    
    ctx.recreateFtm(); // "Restart"
    
    QJsonObject nack;
    nack["type"] = "file_nack";
    nack["transfer_id"] = tid;
    nack["chunk_index"] = 0;
    
    ctx.ftm->handleControlMessage("peerUser", "peerDevice", nack);
    
    QVERIFY(ctx.sentEnvelopes.size() == 1);
    auto envOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(ctx.sentEnvelopes[0].env);
    QVERIFY(envOpt.has_value());
    QCOMPARE(envOpt.value().chunkIndex, (uint32_t)0);
}

void TestFileTransfer::testReq14_RestartCanReconstructReceiverSpoolChunkNCorrectly() {
    FTTestContext ctx;
    QJsonObject startMsg;
    startMsg["type"] = "file_start";
    startMsg["transfer_id"] = "66666666-6666-6666-6666-666666666666";
    startMsg["file_size"] = 100;
    startMsg["chunk_size"] = 1024;
    startMsg["chunk_count"] = 1;
    
    QByteArray original = QByteArray(100, 'I');
    QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(original);
    startMsg["total_hash"] = QString::fromLatin1(hash.result().toBase64());
    
    QByteArray rootKey = ctx.crypto->RandomBytes(32);
    startMsg["root_file_key"] = QString::fromLatin1(rootKey.toBase64());
    
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg);
    
    ctx.recreateFtm(); // Restart
    
    QByteArray tidBin = QUuid("{66666666-6666-6666-6666-666666666666}").toRfc4122();
    QByteArray info = ctx.settings->deviceId().toUtf8();
    auto deviceKeyBytes = ctx.crypto->HkdfSha256(rootKey, tidBin, info, 32);
    QByteArray nonce(12, 0); nonce[0] = 1;
    auto encRes = ctx.crypto->AeadEncrypt(toAeadKey(deviceKeyBytes), Crypto::AeadNonce{nonce}, original, tidBin);
    
    Crypto::Wire::FileChunkEnvelope env;
    env.version = 1;
    env.transferId = tidBin;
    env.chunkIndex = 0;
    env.ciphertext = encRes.ciphertext;
    env.tag.data = encRes.tag.data;
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDevice";
    
    QSignalSpy spy(ctx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    ctx.ftm->handleEnvelope(Crypto::Wire::WireCodec::encodeFileChunkEnvelope(env), meta);
    
    QCOMPARE(spy.count(), 1);
}

void TestFileTransfer::testReq15_MultiDeviceDerivedKeysRemainDistinct() {
    FTTestContext ctx;
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'J')); f.close(); }
    
    // Start transfer to deviceA
    ctx.ftm->startTransfer("peerUser", "deviceA", fp);
    auto envA = ctx.sentEnvelopes[0].env;
    ctx.sentEnvelopes.clear();
    
    // Manually create the DB entry for deviceB for the SAME transfer id
    auto txA = ctx.store->m_transfers.first();
    auto txB = txA;
    txB.peer_device_id = "deviceB";
    ctx.store->saveFileTransfer(txB);
    
    // NACK from deviceB to trigger processNextSend for deviceB
    QJsonObject nackB;
    nackB["type"] = "file_nack";
    nackB["transfer_id"] = txA.transfer_id;
    nackB["chunk_index"] = 0;
    ctx.ftm->handleControlMessage("peerUser", "deviceB", nackB);
    
    QVERIFY(ctx.sentEnvelopes.size() == 1);
    auto envB = ctx.sentEnvelopes[0].env;
    
    auto decA = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(envA).value();
    auto decB = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(envB).value();
    
    QCOMPARE(decA.transferId, decB.transferId);
    QVERIFY(decA.ciphertext != decB.ciphertext);
}

void TestFileTransfer::testReq16_EndToEndSingleSmallFileTransfer() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    
    QString fp = senderCtx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'K')); f.close(); }
    
    QSignalSpy rSpy(receiverCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    QSignalSpy sSpy(senderCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    
    senderCtx.ftm->startTransfer("recvUser", "recvDev", fp);
    
    // Route control msg
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    
    // Route data chunk
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDev";
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    
    // Route ACK back
    senderCtx.ftm->handleControlMessage("recvUser", "recvDev", receiverCtx.sentChatMsgs[0].msg);
    
    QCOMPARE(rSpy.count(), 1);
    QCOMPARE(sSpy.count(), 1);
}

void TestFileTransfer::testReq17_EndToEndMultiChunkFileTransfer() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    
    senderCtx.caps->m_envMax = 1000 + 42; // small chunks
    receiverCtx.caps->m_envMax = 1000 + 42;
    
    QString fp = senderCtx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(2500, 'L')); f.close(); } // 3 chunks
    
    senderCtx.ftm->startTransfer("recvUser", "recvDev", fp);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDev";
    
    for (const auto& env : senderCtx.sentEnvelopes) {
        receiverCtx.ftm->handleEnvelope(env.env, meta);
    }
    
    QSignalSpy sSpy(senderCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    for (const auto& cMsg : receiverCtx.sentChatMsgs) {
        senderCtx.ftm->handleControlMessage("recvUser", "recvDev", cMsg.msg);
    }
    
    QCOMPARE(sSpy.count(), 1);
}

void TestFileTransfer::testReq18_EndToEndOutOfOrderTransfer() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    senderCtx.caps->m_envMax = 1000 + 42;
    receiverCtx.caps->m_envMax = 1000 + 42;
    
    QString fp = senderCtx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(2500, 'M')); f.close(); } // 3 chunks
    
    senderCtx.ftm->startTransfer("recvUser", "recvDev", fp);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDev";
    
    // Send in reverse order
    QSignalSpy rSpy(receiverCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[2].env, meta);
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[1].env, meta);
    
    QCOMPARE(rSpy.count(), 1);
}

void TestFileTransfer::testReq19_EndToEndDuplicateChunk() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    senderCtx.caps->m_envMax = 1000 + 42;
    receiverCtx.caps->m_envMax = 1000 + 42;
    
    QString fp = senderCtx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(1500, 'N')); f.close(); } // 2 chunks
    
    senderCtx.ftm->startTransfer("recvUser", "recvDev", fp);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDev";
    
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta); // duplicate before it's completed
    
    QCOMPARE(receiverCtx.sentChatMsgs.size(), 2); // Two ACKs sent
}

void TestFileTransfer::testReq20_EndToEndReconnectResume() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    senderCtx.caps->m_envMax = 1000 + 42;
    receiverCtx.caps->m_envMax = 1000 + 42;
    
    QString fp = senderCtx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(2500, 'O')); f.close(); } // 3 chunks
    
    senderCtx.ftm->startTransfer("recvUser", "recvDev", fp);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    
    Transport::TransportMetadata meta;
    meta.senderUserId = "senderUser";
    meta.senderDeviceId = "senderDev";
    
    // Chunk 0 sent and acked
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    senderCtx.ftm->handleControlMessage("recvUser", "recvDev", receiverCtx.sentChatMsgs[0].msg);
    
    // Restart both
    senderCtx.recreateFtm();
    receiverCtx.recreateFtm();
    
    // Receiver sends NACK for missing chunks 1 and 2
    QJsonObject nack;
    nack["type"] = "file_nack";
    nack["transfer_id"] = senderCtx.store->m_transfers.first().transfer_id;
    QJsonArray arr; arr.append(1); arr.append(2);
    nack["chunk_indexes"] = arr;
    
    senderCtx.sentEnvelopes.clear(); // Clear so we only count newly sent ones
    senderCtx.ftm->handleControlMessage("recvUser", "recvDev", nack);
    
    // Sender resends chunks 1 and 2
    QVERIFY(senderCtx.sentEnvelopes.size() == 2);
    
    QSignalSpy rSpy(receiverCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[1].env, meta);
    
    QCOMPARE(rSpy.count(), 1);
}

void TestFileTransfer::testReq21_RestartUsesPersistedSnapshotEvenIfCapabilitiesChange() {
    FTTestContext ctx;
    ctx.caps->m_envMax = 1000 + 42; // Original capabilities
    QString fp = ctx.tempDir.path() + "/dummy.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(2000, 'P')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    QVERIFY(ctx.sentEnvelopes.size() > 0);
    auto tid = ctx.store->m_transfers.first().transfer_id;
    ctx.sentEnvelopes.clear();
    
    // Radically change capabilities
    ctx.caps->m_envMax = 500 + 42;
    
    // Restart
    ctx.recreateFtm();
    
    // Trigger NACK to force a resend
    QJsonObject nack;
    nack["type"] = "file_nack";
    nack["transfer_id"] = tid;
    nack["chunk_index"] = 0;
    ctx.ftm->handleControlMessage("peerUser", "peerDevice", nack);
    
    QVERIFY(ctx.sentEnvelopes.size() > 0);
    auto envData = ctx.sentEnvelopes[0].env;
    auto envOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(envData);
    QVERIFY(envOpt.has_value());
    // The chunk size should still be the original 1000, not 500.
    // So the ciphertext should be 1000 bytes.
    QCOMPARE(envOpt.value().ciphertext.size(), 1000);
}

void TestFileTransfer::testReq22_ImageTransferIncomplete() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;

    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");

    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.png";
    QImage testImg(10, 10, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);

    // Route control msg
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);

    // Do not route data chunk! (Incomplete transfer)

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    QVERIFY(!rxStore->m_transfers.isEmpty());
    auto tx = rxStore->m_transfers.first();

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());

    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(result.isNull());
    QVERIFY(!resp->errorString().isEmpty());
    QVERIFY(!QFile::exists(tx.spool_path + ".final"));
}

void TestFileTransfer::testReq23_ImageTransferSecureDecryption() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;

    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");

    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.png";
    QImage testImg(10, 10, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);

    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);

    for (auto& envMeta : senderCtx.sentEnvelopes) {
        Transport::TransportMetadata meta;
        meta.senderUserId = "senderUser";
        meta.senderDeviceId = "senderDev";
        receiverCtx.ftm->handleEnvelope(envMeta.env, meta);
    }

    // Wait a bit
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    auto tx = rxStore->m_transfers.first();
    QCOMPARE(tx.status, QString("completed"));

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());

    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(!result.isNull());
    QCOMPARE(result.size(), QSize(10, 10));
    QCOMPARE(result.pixelColor(5, 5), QColor(Qt::red));
    QVERIFY(resp->errorString().isEmpty());
    QVERIFY(!QFile::exists(tx.spool_path + ".final"));
}

void TestFileTransfer::testReq24_ImageTransferHashMismatch() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.png";
    QImage testImg(10, 10, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    for (auto& envMeta : senderCtx.sentEnvelopes) {
        Transport::TransportMetadata meta;
        meta.senderUserId = "senderUser";
        meta.senderDeviceId = "senderDev";
        receiverCtx.ftm->handleEnvelope(envMeta.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    auto tx = rxStore->m_transfers.first();

    // Corrupt the hash
    tx.file_hash[0] = tx.file_hash[0] ^ 0xFF;
    rxStore->m_transfers[0] = tx;

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(result.isNull());
    QVERIFY(!resp->errorString().isEmpty());
}

void TestFileTransfer::testReq25_ImageTransferCorruptedCiphertext() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.png";
    QImage testImg(10, 10, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    for (auto& envMeta : senderCtx.sentEnvelopes) {
        Transport::TransportMetadata meta;
        meta.senderUserId = "senderUser";
        meta.senderDeviceId = "senderDev";
        receiverCtx.ftm->handleEnvelope(envMeta.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    auto tx = rxStore->m_transfers.first();

    // Corrupt spool file (ciphertext/tag)
    QFile spoolFile(tx.spool_path);
    if (spoolFile.open(QIODevice::ReadWrite)) {
        spoolFile.seek(0);
        spoolFile.write(QByteArray(1, 'X'));
        spoolFile.close();
    }

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(result.isNull());
    QVERIFY(!resp->errorString().isEmpty());
}

void TestFileTransfer::testReq26_ImageTransferMissingChunkMetadata() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.png";
    QImage testImg(10, 10, QImage::Format_RGB32);
    testImg.fill(Qt::red);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    for (auto& envMeta : senderCtx.sentEnvelopes) {
        Transport::TransportMetadata meta;
        meta.senderUserId = "senderUser";
        meta.senderDeviceId = "senderDev";
        receiverCtx.ftm->handleEnvelope(envMeta.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    auto tx = rxStore->m_transfers.first();

    // Corrupt bitset
    if (!tx.received_bitset.isEmpty()) {
        tx.received_bitset[0] = tx.received_bitset[0] ^ 0x01; // unset chunk 0
        rxStore->m_transfers[0] = tx;
    }

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(result.isNull());
    QVERIFY(!resp->errorString().isEmpty());
}

void TestFileTransfer::testReq27_ImageTransferDecodingFailure() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("senderDev");
    receiverCtx.settings->setDeviceId("recvDev");
    receiverCtx.ftm->setIsImageTransferCb([](const QString&) { return true; });

    QString testDataPath = senderCtx.tempDir.path() + "/test_image.txt";
    QFile testFile(testDataPath);
    if (testFile.open(QIODevice::WriteOnly)) {
        testFile.write("This is not an image, just text.");
        testFile.close();
    }

    QString validUuid = "77777777-7777-7777-7777-777777777777";
    senderCtx.ftm->startTransfer("recvUser", "recvDev", testDataPath, validUuid);
    receiverCtx.ftm->handleControlMessage("senderUser", "senderDev", senderCtx.sentChatMsgs[0].msg);
    for (auto& envMeta : senderCtx.sentEnvelopes) {
        Transport::TransportMetadata meta;
        meta.senderUserId = "senderUser";
        meta.senderDeviceId = "senderDev";
        receiverCtx.ftm->handleEnvelope(envMeta.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    auto rxStore = std::static_pointer_cast<MockStoreFT>(receiverCtx.store);
    auto tx = rxStore->m_transfers.first();

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tx.transfer_id, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result;
    result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(result.isNull());
    QVERIFY(!resp->errorString().isEmpty());
}

void TestFileTransfer::testReq28_IncomingAndOutgoingAttachmentStatusAndProgress() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("aliceDev");
    receiverCtx.settings->setDeviceId("bobDev");
    senderCtx.caps->m_envMax = 200 + 42;
    receiverCtx.caps->m_envMax = 200 + 42;

    auto senderRepo = std::make_shared<Core::Messaging::SqliteMessageStorage>(senderCtx.tempDir.path() + "/sender.db");
    auto receiverRepo = std::make_shared<Core::Messaging::SqliteMessageStorage>(receiverCtx.tempDir.path() + "/receiver.db");
    Services::MessageService senderSvc(senderRepo);
    Services::MessageService receiverSvc(receiverRepo);
    senderSvc.setCurrentUserId("alice");
    receiverSvc.setCurrentUserId("bob");
    senderSvc.setFileTransferManager(std::shared_ptr<FileTransferManager>(senderCtx.ftm.get(), [](FileTransferManager*) {}));
    receiverSvc.setFileTransferManager(std::shared_ptr<FileTransferManager>(receiverCtx.ftm.get(), [](FileTransferManager*) {}));
    senderSvc.setDeviceResolverCallback([](const QString&, std::function<void(QString)> cb) { cb(QString("bobDev")); });

    ChatMessageModel senderModel;
    ChatMessageModel receiverModel;
    senderModel.setActiveConversation("dms:bob");
    receiverModel.setActiveConversation("dms:alice");

    QObject::connect(&senderSvc, &Services::MessageService::messageAdded, &senderModel, &ChatMessageModel::onMessageAdded);
    QObject::connect(&senderSvc, &Services::MessageService::messageUpdated, &senderModel, &ChatMessageModel::onMessageUpdated);
    QObject::connect(&senderSvc, &Services::MessageService::mediaTransferProgress, &senderModel, &ChatMessageModel::onMediaTransferProgress);
    QObject::connect(&receiverSvc, &Services::MessageService::messageAdded, &receiverModel, &ChatMessageModel::onMessageAdded);
    QObject::connect(&receiverSvc, &Services::MessageService::messageUpdated, &receiverModel, &ChatMessageModel::onMessageUpdated);
    QObject::connect(&receiverSvc, &Services::MessageService::mediaTransferProgress, &receiverModel, &ChatMessageModel::onMediaTransferProgress);

    QString imgPath = senderCtx.tempDir.path() + "/photo.png";
    QImage img(16, 16, QImage::Format_RGB32);
    img.fill(Qt::blue);
    img.save(imgPath, "PNG");
    qint64 imgSize = QFileInfo(imgPath).size();

    QSignalSpy spyTransmit(&senderSvc, &Services::MessageService::transmitMessage);
    senderSvc.sendMessage("dms:bob", "", "image", "file://" + imgPath, "photo.png", imgSize, 0, {});

    QCOMPARE(senderModel.rowCount(), 1);
    QModelIndex sIdx = senderModel.index(0, 0);
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::FromMeRole).toBool(), true);
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::StatusRole).toString(), QString("sending"));
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::MediaWidthRole).toInt(), 16);
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::MediaHeightRole).toInt(), 16);

    QTRY_VERIFY_WITH_TIMEOUT(spyTransmit.count() >= 1, 1000);
    Domain::Message wireMsg = spyTransmit.first().at(0).value<Domain::Message>();
    wireMsg.conversationId = "dms:alice";

    QVERIFY(!senderCtx.sentChatMsgs.empty());
    receiverCtx.ftm->handleControlMessage("alice", "aliceDev", senderCtx.sentChatMsgs[0].msg);
    QSignalSpy spyReceiverAdded(&receiverSvc, &Services::MessageService::messageAdded);
    receiverSvc.handleIncomingMessages({ wireMsg });

    QCOMPARE(spyReceiverAdded.count(), 1);
    QCOMPARE(receiverModel.rowCount(), 1);
    QModelIndex rIdx = receiverModel.index(0, 0);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::FromMeRole).toBool(), false);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("downloading"));
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::MediaWidthRole).toInt(), 16);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::MediaHeightRole).toInt(), 16);

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "aliceDev";
    for (size_t i = 0; i < senderCtx.sentEnvelopes.size(); ++i) {
        receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[i].env, meta);
        while (!receiverCtx.sentChatMsgs.empty()) {
            auto ack = receiverCtx.sentChatMsgs.front();
            receiverCtx.sentChatMsgs.erase(receiverCtx.sentChatMsgs.begin());
            senderCtx.ftm->handleControlMessage("bob", "bobDev", ack.msg);
        }
    }

    for (int i = 0; i < 20; ++i) QCoreApplication::processEvents();

    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::TransferProgressRole).toReal(), 1.0);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::MediaUrlRole).toString(), wireMsg.id);
    QCOMPARE(spyReceiverAdded.count(), 1);

    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::TransferProgressRole).toReal(), 1.0);

    receiverModel.onMediaTransferProgress("dms:alice", wireMsg.id, 0.5, imgSize / 2, imgSize);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::TransferProgressRole).toReal(), 1.0);
}

void TestFileTransfer::testReq29_CompletionBeforeAndAfterIncomingMessageAndReload() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("aliceDev");
    receiverCtx.settings->setDeviceId("bobDev");

    auto receiverRepo = std::make_shared<Core::Messaging::SqliteMessageStorage>(receiverCtx.tempDir.path() + "/receiver_race.db");
    Services::MessageService receiverSvc(receiverRepo);
    receiverSvc.setCurrentUserId("bob");
    receiverSvc.setFileTransferManager(std::shared_ptr<FileTransferManager>(receiverCtx.ftm.get(), [](FileTransferManager*) {}));

    ChatMessageModel receiverModel;
    receiverModel.setActiveConversation("dms:alice");
    QObject::connect(&receiverSvc, &Services::MessageService::messageAdded, &receiverModel, &ChatMessageModel::onMessageAdded);
    QObject::connect(&receiverSvc, &Services::MessageService::messageUpdated, &receiverModel, &ChatMessageModel::onMessageUpdated);
    QObject::connect(&receiverSvc, &Services::MessageService::conversationLoaded, &receiverModel, &ChatMessageModel::onConversationLoaded);

    QString imgPath = senderCtx.tempDir.path() + "/early.png";
    QImage img(10, 10, QImage::Format_RGB32);
    img.fill(Qt::green);
    img.save(imgPath, "PNG");
    qint64 imgSize = QFileInfo(imgPath).size();

    QString earlyMsgId = "88888888-8888-8888-8888-888888888888";
    senderCtx.ftm->startTransfer("bob", "bobDev", imgPath, earlyMsgId);
    receiverCtx.ftm->handleControlMessage("alice", "aliceDev", senderCtx.sentChatMsgs[0].msg);

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "aliceDev";
    for (const auto& env : senderCtx.sentEnvelopes) {
        receiverCtx.ftm->handleEnvelope(env.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    Domain::Message earlyMsg;
    earlyMsg.id = earlyMsgId;
    earlyMsg.conversationId = "dms:alice";
    earlyMsg.senderId = "alice";
    earlyMsg.type = "image";
    earlyMsg.fileName = "early.png";
    earlyMsg.fileSize = imgSize;
    earlyMsg.timestamp = 1000;
    earlyMsg.mediaWidth = 10;
    earlyMsg.mediaHeight = 10;

    receiverSvc.handleIncomingMessages({ earlyMsg });
    QCOMPARE(receiverModel.rowCount(), 1);
    QModelIndex idx0 = receiverModel.index(0, 0);
    QCOMPARE(receiverModel.data(idx0, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(receiverModel.data(idx0, ChatMessageModel::MediaUrlRole).toString(), earlyMsgId);

    QTest::qWait(100);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QSignalSpy spyLoaded(&receiverSvc, &Services::MessageService::conversationLoaded);
    receiverSvc.loadConversation("dms:alice");
    QTRY_VERIFY_WITH_TIMEOUT(spyLoaded.count() >= 1, 1000);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QCOMPARE(receiverModel.rowCount(), 1);
    idx0 = receiverModel.index(0, 0);
    QCOMPARE(receiverModel.data(idx0, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(receiverModel.data(idx0, ChatMessageModel::MediaUrlRole).toString(), earlyMsgId);

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(earlyMsgId, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();
    QImage decoded = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();
    QVERIFY(!decoded.isNull());
    QCOMPARE(decoded.size(), QSize(10, 10));
}

void TestFileTransfer::testReq30_MultiDeviceRecipientTransferAndOfflineDeviceIsolation() {
    FTTestContext senderCtx;
    FTTestContext recvOnlineCtx;
    senderCtx.settings->setDeviceId("aliceDev");
    recvOnlineCtx.settings->setDeviceId("bobDevOnline");
    senderCtx.caps->m_envMax = 500 + 42;
    recvOnlineCtx.caps->m_envMax = 500 + 42;

    auto senderRepo = std::make_shared<Core::Messaging::SqliteMessageStorage>(senderCtx.tempDir.path() + "/sender_md.db");
    Services::MessageService senderSvc(senderRepo);
    senderSvc.setCurrentUserId("alice");
    senderSvc.setFileTransferManager(std::shared_ptr<FileTransferManager>(senderCtx.ftm.get(), [](FileTransferManager*) {}));
    senderSvc.setMultiDeviceResolverCallback([](const QString&, std::function<void(QStringList)> cb) {
        cb(QStringList{"bobDevOnline", "bobDevOffline"});
    });

    ChatMessageModel senderModel;
    senderModel.setActiveConversation("dms:bob");
    QObject::connect(&senderSvc, &Services::MessageService::messageAdded, &senderModel, &ChatMessageModel::onMessageAdded);
    QObject::connect(&senderSvc, &Services::MessageService::messageUpdated, &senderModel, &ChatMessageModel::onMessageUpdated);

    QString fp = senderCtx.tempDir.path() + "/multidev.bin";
    QByteArray payload(1200, 'Q');
    QFile f(fp);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(payload);
    f.close();

    senderSvc.sendMessage("dms:bob", "", "file", "file://" + fp, "multidev.bin", payload.size());
    QCOMPARE(senderModel.rowCount(), 1);
    QModelIndex sIdx = senderModel.index(0, 0);
    QString msgId = senderModel.data(sIdx, ChatMessageModel::MessageIdRole).toString();
    QTRY_VERIFY_WITH_TIMEOUT(senderCtx.sentChatMsgs.size() >= 2, 1000);

    for (const auto& cm : senderCtx.sentChatMsgs) {
        if (cm.recipientDevice == "bobDevOnline") {
            recvOnlineCtx.ftm->handleControlMessage("alice", "aliceDev", cm.msg);
        }
    }

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "aliceDev";
    QSignalSpy recvDoneSpy(recvOnlineCtx.ftm.get(), SIGNAL(transferCompleted(QString,QString)));

    for (const auto& env : senderCtx.sentEnvelopes) {
        if (env.recipientDevice == "bobDevOnline") {
            recvOnlineCtx.ftm->handleEnvelope(env.env, meta);
        }
    }
    for (const auto& ack : recvOnlineCtx.sentChatMsgs) {
        senderCtx.ftm->handleControlMessage("bob", "bobDevOnline", ack.msg);
    }

    QCOMPARE(recvDoneSpy.count(), 1);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));

    senderCtx.ftm->setRetryParamsForTesting(20, 10);
    QTest::qWait(150);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QCOMPARE(senderModel.data(sIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(senderCtx.store->m_transfers[msgId + "_bobDevOnline"].status, QString("completed"));
}

void TestFileTransfer::testReq31_IncomingRetryAndCancelNeverTransmitFakeOutgoingMessage() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("aliceDev");
    receiverCtx.settings->setDeviceId("bobDev");
    senderCtx.caps->m_envMax = 500 + 42;
    receiverCtx.caps->m_envMax = 500 + 42;

    auto receiverRepo = std::make_shared<Core::Messaging::SqliteMessageStorage>(receiverCtx.tempDir.path() + "/receiver_retry.db");
    Services::MessageService receiverSvc(receiverRepo);
    receiverSvc.setCurrentUserId("bob");
    receiverSvc.setFileTransferManager(std::shared_ptr<FileTransferManager>(receiverCtx.ftm.get(), [](FileTransferManager*) {}));

    ChatMessageModel receiverModel;
    receiverModel.setActiveConversation("dms:alice");
    QObject::connect(&receiverSvc, &Services::MessageService::messageAdded, &receiverModel, &ChatMessageModel::onMessageAdded);
    QObject::connect(&receiverSvc, &Services::MessageService::messageUpdated, &receiverModel, &ChatMessageModel::onMessageUpdated);

    QString fp = senderCtx.tempDir.path() + "/doc.bin";
    QByteArray payload(900, 'R');
    QFile f(fp);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(payload);
    f.close();

    QString tid = "99999999-9999-9999-9999-999999999999";
    senderCtx.ftm->startTransfer("bob", "bobDev", fp, tid);
    receiverCtx.ftm->handleControlMessage("alice", "aliceDev", senderCtx.sentChatMsgs[0].msg);

    Domain::Message inMsg;
    inMsg.id = tid;
    inMsg.conversationId = "dms:alice";
    inMsg.senderId = "alice";
    inMsg.type = "file";
    inMsg.fileName = "doc.bin";
    inMsg.fileSize = payload.size();
    inMsg.timestamp = 2000;
    receiverSvc.handleIncomingMessages({ inMsg });

    QCOMPARE(receiverModel.rowCount(), 1);
    QModelIndex rIdx = receiverModel.index(0, 0);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("downloading"));

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "aliceDev";
    receiverCtx.ftm->handleEnvelope(senderCtx.sentEnvelopes[0].env, meta);
    receiverCtx.sentChatMsgs.clear();

    QSignalSpy spyTransmit(&receiverSvc, &Services::MessageService::transmitMessage);
    receiverSvc.cancelMediaTransfer("dms:alice", tid);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("failed"));
    QVERIFY(!receiverCtx.sentChatMsgs.empty());
    QCOMPARE(receiverCtx.sentChatMsgs[0].msg["type"].toString(), QString("file_cancel"));
    receiverCtx.sentChatMsgs.clear();

    receiverSvc.retryMessage(tid);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QCOMPARE(spyTransmit.count(), 0);
    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("downloading"));
    QVERIFY(!receiverCtx.sentChatMsgs.empty());
    QCOMPARE(receiverCtx.sentChatMsgs[0].msg["type"].toString(), QString("file_nack"));

    senderCtx.sentEnvelopes.clear();
    senderCtx.ftm->handleControlMessage("bob", "bobDev", receiverCtx.sentChatMsgs[0].msg);
    QVERIFY(!senderCtx.sentEnvelopes.empty());

    for (const auto& env : senderCtx.sentEnvelopes) {
        receiverCtx.ftm->handleEnvelope(env.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QCOMPARE(receiverModel.data(rIdx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(spyTransmit.count(), 0);
}

void TestFileTransfer::testReq32_SecureImageProviderQueryParamAndSenderSpoolSupport() {
    FTTestContext senderCtx;
    senderCtx.settings->setDeviceId("aliceDev");

    QString testDataPath = senderCtx.tempDir.path() + "/sender_img.png";
    QImage testImg(12, 12, QImage::Format_RGB32);
    testImg.fill(Qt::yellow);
    testImg.save(testDataPath, "PNG");

    QString validUuid = "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa";
    senderCtx.ftm->startTransfer("bob", "bobDev", testDataPath, validUuid);

    NeoNect::Services::SecureImageProvider provider(senderCtx.store, senderCtx.crypto, senderCtx.settings);
    auto* resp = provider.requestImageResponse(validUuid + "?cacheBust=1", QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage result = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();

    QVERIFY(!result.isNull());
    QCOMPARE(result.size(), QSize(12, 12));
    QCOMPARE(result.pixelColor(6, 6), QColor(Qt::yellow));
}

void TestFileTransfer::testReq33_ImageReconstructedIntoPlaintextCanonicalStorageAndCached() {
    FTTestContext senderCtx;
    FTTestContext receiverCtx;
    senderCtx.settings->setDeviceId("aliceDev");
    receiverCtx.settings->setDeviceId("bobDev");
    receiverCtx.settings->setUsername("bob");
    receiverCtx.settings->setServerUrl("https://neonect.local");

    QString testImgPath = senderCtx.tempDir.path() + "/canonical_sample.png";
    QImage originalImg(16, 16, QImage::Format_ARGB32);
    originalImg.fill(QColor(10, 200, 50));
    originalImg.save(testImgPath, "PNG");

    QString tid = "cccccccc-cccc-cccc-cccc-cccccccccccc";
    senderCtx.ftm->startTransfer("bob", "bobDev", testImgPath, tid);
    receiverCtx.ftm->handleControlMessage("alice", "aliceDev", senderCtx.sentChatMsgs[0].msg);

    Transport::TransportMetadata meta;
    meta.senderUserId = "alice";
    meta.senderDeviceId = "aliceDev";
    for (const auto& env : senderCtx.sentEnvelopes) {
        receiverCtx.ftm->handleEnvelope(env.env, meta);
    }
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

    QString canonicalDir = StoragePathResolver::mediaImagesDirectory("https://neonect.local", "bob");
    QString canonicalPath = canonicalDir + "/" + tid + ".png";
    QVERIFY(QFile::exists(canonicalPath));

    QImageReader reader(canonicalPath);
    QVERIFY(reader.canRead());
    QImage loaded = reader.read();
    QCOMPARE(loaded.size(), QSize(16, 16));
    QCOMPARE(loaded.pixelColor(0, 0), QColor(10, 200, 50));

    QString spoolPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool/" + tid + ".recv.spool";
    QVERIFY(!QFile::exists(spoolPath));

    NeoNect::Services::SecureImageProvider provider(receiverCtx.store, receiverCtx.crypto, receiverCtx.settings);
    auto* resp = provider.requestImageResponse(tid, QSize());
    QEventLoop loop;
    QObject::connect(resp, &QQuickImageResponse::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QImage provImg = resp->property("decodedImage").value<QImage>();
    resp->deleteLater();
    QVERIFY(!provImg.isNull());
    QCOMPARE(provImg.size(), QSize(16, 16));
    QCOMPARE(provImg.pixelColor(0, 0), QColor(10, 200, 50));

    auto* resp2 = provider.requestImageResponse(tid, QSize());
    QEventLoop loop2;
    QObject::connect(resp2, &QQuickImageResponse::finished, &loop2, &QEventLoop::quit);
    loop2.exec();
    QImage cachedImg = resp2->property("decodedImage").value<QImage>();
    resp2->deleteLater();
    QVERIFY(!cachedImg.isNull());
    QCOMPARE(cachedImg.pixelColor(0, 0), QColor(10, 200, 50));
}

void TestFileTransfer::testReq34_VoiceRecordingFinalizationWavValidityAndStorageIsolation() {
    AudioManager am;
    am.setStorageBoundary("https://neonect.local", "alice");

    am.startRecording();
    QVERIFY(am.isRecording());

    QTest::qWait(50);

    QVariantMap recordMeta = am.stopRecording();
    QVERIFY(!am.isRecording());

    QString audioUrl = recordMeta["audioUrl"].toString();
    QVERIFY(!audioUrl.isEmpty());

    QString localPath = QUrl(audioUrl).toLocalFile();
    QVERIFY(QFile::exists(localPath));

    QString expectedVoiceDir = StoragePathResolver::mediaVoiceDirectory("https://neonect.local", "alice");
    QVERIFY(localPath.startsWith(expectedVoiceDir));

    QFile wavFile(localPath);
    QVERIFY(wavFile.open(QIODevice::ReadOnly));
    QByteArray header = wavFile.read(12);
    wavFile.close();
    QCOMPARE(header.left(4), QByteArray("RIFF"));
    QCOMPARE(header.mid(8, 4), QByteArray("WAVE"));

    am.playAudio("test-msg-1", audioUrl, 1);
    QCOMPARE(am.currentPlayingId(), QString("test-msg-1"));

    am.playAudio("test-msg-bad", "/etc/shadow", 1);
    QCOMPARE(am.currentPlayingId(), QString("test-msg-1"));

    am.pauseAudio();

    am.cleanupLocalFile(audioUrl);
    QVERIFY(!QFile::exists(localPath));
}

void TestFileTransfer::testReq35_AudioPlaybackCompletionAndLifecycleHardening() {
    AudioManager am;
    am.setStorageBoundary("https://neonect.local", "alice");

    QString voiceDir = StoragePathResolver::mediaVoiceDirectory("https://neonect.local", "alice");
    QDir().mkpath(voiceDir);
    QString filePath = voiceDir + "/voice_note_test_playback.wav";

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray header(44, 0);
    char *hdr = header.data();
    memcpy(hdr + 0, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(hdr + 4) = 36 + 1600 * 2;
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(hdr + 16) = 16;
    *reinterpret_cast<uint16_t*>(hdr + 20) = 1;
    *reinterpret_cast<uint16_t*>(hdr + 22) = 1;
    *reinterpret_cast<uint32_t*>(hdr + 24) = 16000;
    *reinterpret_cast<uint32_t*>(hdr + 28) = 32000;
    *reinterpret_cast<uint16_t*>(hdr + 32) = 2;
    *reinterpret_cast<uint16_t*>(hdr + 34) = 16;
    memcpy(hdr + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(hdr + 40) = 1600 * 2;
    file.write(header);
    std::vector<int16_t> samples(1600, 1000);
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
    file.close();

    QString audioUrl = QUrl::fromLocalFile(filePath).toString();

    QSignalSpy spyFinished(&am, &AudioManager::playbackFinished);
    am.playAudio("test-pcm-track", audioUrl, 1);
    QVERIFY(am.isPlaying());
    QCOMPARE(am.currentPlayingId(), QString("test-pcm-track"));

    QVERIFY(spyFinished.wait(3000));
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(spyFinished.at(0).at(0).toString(), QString("test-pcm-track"));
    QVERIFY(!am.isPlaying());
    QCOMPARE(am.playbackProgress(), 1.0);
    QCOMPARE(am.currentPlayingId(), QString(""));

    am.playAudio("test-seek-track", audioUrl, 1);
    QVERIFY(am.isPlaying());
    am.seek("test-seek-track", 0.5);
    QCOMPARE(am.playbackProgress(), 0.5);
    am.pauseAudio();
    QVERIFY(!am.isPlaying());
    QCOMPARE(am.currentPlayingId(), QString("test-seek-track"));

    am.cleanupLocalFile(audioUrl);
    QVERIFY(!QFile::exists(filePath));
}

void TestFileTransfer::testReq36_AudioPlaybackRapidControlCyclesNoDeadlock() {
    AudioManager am;
    am.setStorageBoundary("https://neonect.local", "alice");

    QString voiceDir = StoragePathResolver::mediaVoiceDirectory("https://neonect.local", "alice");
    QDir().mkpath(voiceDir);
    QString filePath = voiceDir + "/voice_note_rapid.wav";

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray header(44, 0);
    char *hdr = header.data();
    memcpy(hdr + 0, "RIFF", 4);
    *reinterpret_cast<uint32_t*>(hdr + 4) = 36 + 800 * 2;
    memcpy(hdr + 8, "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    *reinterpret_cast<uint32_t*>(hdr + 16) = 16;
    *reinterpret_cast<uint16_t*>(hdr + 20) = 1;
    *reinterpret_cast<uint16_t*>(hdr + 22) = 1;
    *reinterpret_cast<uint32_t*>(hdr + 24) = 16000;
    *reinterpret_cast<uint32_t*>(hdr + 28) = 32000;
    *reinterpret_cast<uint16_t*>(hdr + 32) = 2;
    *reinterpret_cast<uint16_t*>(hdr + 34) = 16;
    memcpy(hdr + 36, "data", 4);
    *reinterpret_cast<uint32_t*>(hdr + 40) = 800 * 2;
    file.write(header);
    std::vector<int16_t> samples(800, 500);
    file.write(reinterpret_cast<const char*>(samples.data()), samples.size() * sizeof(int16_t));
    file.close();

    QString audioUrl = QUrl::fromLocalFile(filePath).toString();

    for (int i = 0; i < 30; ++i) {
        am.playAudio(QString("rapid-%1").arg(i), audioUrl, 1);
        am.seek(QString("rapid-%1").arg(i), (i % 10) * 0.1);
        am.setPlaybackSpeed((i % 3 == 0) ? 1.0 : ((i % 3 == 1) ? 1.5 : 2.0));
        am.setVolume((i % 10) * 0.1);
        am.toggleMute();
        if (i % 2 == 0) {
            am.pauseAudio();
        }
    }
    am.pauseAudio();
    QVERIFY(!am.isPlaying());

    for (int i = 0; i < 15; ++i) {
        am.startRecording();
        am.cancelRecording();
        QVERIFY(!am.isRecording());
    }

    am.cleanupLocalFile(audioUrl);
    QVERIFY(!QFile::exists(filePath));
}

void TestFileTransfer::testReq37_OversizedChunkAndInvalidCapsRejected() {
    FTTestContext ctx;
    QSignalSpy spyFailed(ctx.ftm.get(), &FileTransferManager::transferFailed);

    ctx.caps->m_envMax = 1000;
    QJsonObject startOversized;
    startOversized["type"] = "file_start";
    startOversized["transfer_id"] = "10000000-0000-0000-0000-000000000001";
    startOversized["file_size"] = 959;
    startOversized["chunk_size"] = 959;
    startOversized["chunk_count"] = 1;
    startOversized["total_hash"] = QString(QByteArray(32, 'h').toBase64());
    startOversized["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startOversized);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Chunk size exceeds envelope limit"));

    QJsonObject startExact;
    startExact["type"] = "file_start";
    startExact["transfer_id"] = "10000000-0000-0000-0000-000000000002";
    startExact["file_size"] = 958;
    startExact["chunk_size"] = 958;
    startExact["chunk_count"] = 1;
    startExact["total_hash"] = QString(QByteArray(32, 'h').toBase64());
    startExact["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startExact);
    QCOMPARE(spyFailed.count(), 0);

    ctx.caps->m_capState = CapabilityState::UNAVAILABLE;
    QJsonObject startUnavail;
    startUnavail["type"] = "file_start";
    startUnavail["transfer_id"] = "10000000-0000-0000-0000-000000000003";
    startUnavail["file_size"] = 100;
    startUnavail["chunk_size"] = 100;
    startUnavail["chunk_count"] = 1;
    startUnavail["total_hash"] = QString(QByteArray(32, 'h').toBase64());
    startUnavail["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startUnavail);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Capabilities unavailable"));

    ctx.caps->m_capState = CapabilityState::INVALID;
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startUnavail);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Capabilities unavailable"));

    ctx.caps->m_capState = CapabilityState::UNSUPPORTED;
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startUnavail);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Capabilities unavailable"));

    ctx.caps->m_capState = CapabilityState::VALID;

    QJsonObject startBadHash;
    startBadHash["type"] = "file_start";
    startBadHash["transfer_id"] = "10000000-0000-0000-0000-000000000004";
    startBadHash["file_size"] = 100;
    startBadHash["chunk_size"] = 100;
    startBadHash["chunk_count"] = 1;
    startBadHash["total_hash"] = QString(QByteArray(16, 'x').toBase64());
    startBadHash["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startBadHash);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Invalid file hash"));

    QJsonObject startOverflow;
    startOverflow["type"] = "file_start";
    startOverflow["transfer_id"] = "10000000-0000-0000-0000-000000000005";
    startOverflow["file_size"] = QString::number(std::numeric_limits<qint64>::max());
    startOverflow["chunk_size"] = 100;
    startOverflow["chunk_count"] = 1;
    startOverflow["total_hash"] = QString(QByteArray(32, 'h').toBase64());
    startOverflow["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startOverflow);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Invalid transfer metadata"));

    ctx.caps->m_envMax = 100 * 1024 * 1024;
    QJsonObject startHuge;
    startHuge["type"] = "file_start";
    startHuge["transfer_id"] = "10000000-0000-0000-0000-000000000006";
    qint64 twoTib = 2LL * 1024 * 1024 * 1024 * 1024;
    startHuge["file_size"] = QString::number(twoTib);
    startHuge["chunk_size"] = 1000000;
    startHuge["chunk_count"] = static_cast<int>((twoTib + 999999) / 1000000);
    startHuge["total_hash"] = QString(QByteArray(32, 'h').toBase64());
    startHuge["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startHuge);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Transfer size exceeds limit"));
}

void TestFileTransfer::testReq38_SpoolCleanupOnCancelAndCompletedProtected() {
    FTTestContext ctx;
    QString spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";

    QJsonObject startMsg1;
    startMsg1["type"] = "file_start";
    startMsg1["transfer_id"] = "20000000-0000-0000-0000-000000000001";
    startMsg1["file_size"] = 100;
    startMsg1["chunk_size"] = 100;
    startMsg1["chunk_count"] = 1;
    startMsg1["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMsg1["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMsg1);
    QString spool1 = spoolDir + "/20000000-0000-0000-0000-000000000001.recv.spool";
    QVERIFY(QFile::exists(spool1));

    QJsonObject cancelMsg1;
    cancelMsg1["type"] = "file_cancel";
    cancelMsg1["transfer_id"] = "20000000-0000-0000-0000-000000000001";
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", cancelMsg1);
    QVERIFY(!QFile::exists(spool1));

    QString fp = ctx.tempDir.path() + "/dummy_send.txt";
    QFile fSend(fp);
    QVERIFY(fSend.open(QIODevice::WriteOnly));
    fSend.write(QByteArray(100, 'Z'));
    fSend.close();

    QString sendTid = "22222222-2222-2222-2222-222222222222";
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp, sendTid);
    QString spool2 = spoolDir + "/" + sendTid + ".spool";
    QVERIFY(QFile::exists(spool2));

    ctx.ftm->cancelTransfer(sendTid, "peerDevice", "user cancelled");
    QVERIFY(!QFile::exists(spool2));

    QString completedFile = ctx.tempDir.path() + "/final_completed.txt";
    QFile fComp(completedFile);
    QVERIFY(fComp.open(QIODevice::WriteOnly));
    fComp.write("completed file payload");
    fComp.close();

    E2EEFileTransfer txCompleted;
    txCompleted.transfer_id = "20000000-0000-0000-0000-000000000003";
    txCompleted.peer_device_id = "senderDevice";
    txCompleted.is_sender = 0;
    txCompleted.file_size = 22;
    txCompleted.chunk_size = 100;
    txCompleted.max_envelope_bytes = 0;
    txCompleted.max_http_body_bytes = 0;
    txCompleted.chunk_count = 1;
    txCompleted.file_hash = QByteArray(32, 'c');
    txCompleted.spool_path = completedFile;
    txCompleted.root_file_key = QByteArray(32, 'k');
    txCompleted.status = "completed";
    txCompleted.created_at = QDateTime::currentSecsSinceEpoch();
    ctx.store->saveFileTransfer(txCompleted);

    ctx.ftm->cancelTransfer("20000000-0000-0000-0000-000000000003", "senderDevice", "cancel test");
    QVERIFY(QFile::exists(completedFile));
}

void TestFileTransfer::testReq39_StartupSpoolCleanupPreservesActiveTransfers() {
    FTTestContext ctx;
    QString spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";
    QDir().mkpath(spoolDir);

    E2EEFileTransfer txActive;
    txActive.transfer_id = "30000000-0000-0000-0000-000000000004";
    txActive.peer_device_id = "senderDevice";
    txActive.is_sender = 0;
    txActive.file_size = 100;
    txActive.chunk_size = 100;
    txActive.max_envelope_bytes = 0;
    txActive.max_http_body_bytes = 0;
    txActive.chunk_count = 1;
    txActive.file_hash = QByteArray(32, 'd');
    txActive.spool_path = spoolDir + "/30000000-0000-0000-0000-000000000004.recv.spool";
    txActive.root_file_key = QByteArray(32, 'k');
    txActive.status = "active";
    txActive.created_at = QDateTime::currentSecsSinceEpoch();
    ctx.store->saveFileTransfer(txActive);

    QFile fActive(txActive.spool_path);
    QVERIFY(fActive.open(QIODevice::WriteOnly));
    fActive.write("active spool data");
    fActive.close();

    QString orphanSendPath = spoolDir + "/30000000-0000-0000-0000-000000000005.spool";
    QFile fOrphanSend(orphanSendPath);
    QVERIFY(fOrphanSend.open(QIODevice::WriteOnly));
    fOrphanSend.write("orphan sender");
    fOrphanSend.close();

    QString orphanRecvPath = spoolDir + "/30000000-0000-0000-0000-000000000006.recv.spool";
    QFile fOrphanRecv(orphanRecvPath);
    QVERIFY(fOrphanRecv.open(QIODevice::WriteOnly));
    fOrphanRecv.write("orphan receiver");
    fOrphanRecv.close();

    ctx.recreateFtm();

    QVERIFY(QFile::exists(txActive.spool_path));
    QVERIFY(!QFile::exists(orphanSendPath));
    QVERIFY(!QFile::exists(orphanRecvPath));
}

void TestFileTransfer::testReq40_BitsetSafeAtMaxChunkCountAndRejectedAboveMax() {
    FTTestContext ctx;
    QSignalSpy spyFailed(ctx.ftm.get(), &FileTransferManager::transferFailed);
    ctx.caps->m_envMax = 100 * 1024 * 1024;

    QJsonObject startMax;
    startMax["type"] = "file_start";
    startMax["transfer_id"] = "40000000-0000-0000-0000-000000000001";
    startMax["file_size"] = 128000000LL;
    startMax["chunk_size"] = 64;
    startMax["chunk_count"] = 2000000;
    startMax["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startMax["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startMax);
    QCOMPARE(spyFailed.count(), 0);

    QJsonObject startAboveMax;
    startAboveMax["type"] = "file_start";
    startAboveMax["transfer_id"] = "40000000-0000-0000-0000-000000000002";
    startAboveMax["file_size"] = 128000064LL;
    startAboveMax["chunk_size"] = 64;
    startAboveMax["chunk_count"] = 2000001;
    startAboveMax["total_hash"] = QString(QByteArray(32, 'b').toBase64());
    startAboveMax["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startAboveMax);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Chunk count exceeds limit"));

    QJsonObject startExtreme;
    startExtreme["type"] = "file_start";
    startExtreme["transfer_id"] = "40000000-0000-0000-0000-000000000003";
    startExtreme["file_size"] = 2147483641LL * 64;
    startExtreme["chunk_size"] = 64;
    startExtreme["chunk_count"] = 2147483641;
    startExtreme["total_hash"] = QString(QByteArray(32, 'c').toBase64());
    startExtreme["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startExtreme);
    QCOMPARE(spyFailed.count(), 1);
    QVERIFY(!ctx.store->m_transfers.contains("40000000-0000-0000-0000-000000000003_senderDevice"));
}

void TestFileTransfer::testReq41_MicroChunkingRejectedAndSmallFilesAllowed() {
    FTTestContext ctx;
    QSignalSpy spyFailed(ctx.ftm.get(), &FileTransferManager::transferFailed);
    ctx.caps->m_envMax = 1000;

    QJsonObject startTinyChunk;
    startTinyChunk["type"] = "file_start";
    startTinyChunk["transfer_id"] = "41000000-0000-0000-0000-000000000001";
    startTinyChunk["file_size"] = 120;
    startTinyChunk["chunk_size"] = 60;
    startTinyChunk["chunk_count"] = 2;
    startTinyChunk["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startTinyChunk["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startTinyChunk);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Chunk size too small"));

    QJsonObject startSmallFile;
    startSmallFile["type"] = "file_start";
    startSmallFile["transfer_id"] = "41000000-0000-0000-0000-000000000002";
    startSmallFile["file_size"] = 10;
    startSmallFile["chunk_size"] = 10;
    startSmallFile["chunk_count"] = 1;
    startSmallFile["total_hash"] = QString(QByteArray(32, 'b').toBase64());
    startSmallFile["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startSmallFile);
    QCOMPARE(spyFailed.count(), 0);

    QJsonObject startExact64;
    startExact64["type"] = "file_start";
    startExact64["transfer_id"] = "41000000-0000-0000-0000-000000000003";
    startExact64["file_size"] = 70;
    startExact64["chunk_size"] = 64;
    startExact64["chunk_count"] = 2;
    startExact64["total_hash"] = QString(QByteArray(32, 'c').toBase64());
    startExact64["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startExact64);
    QCOMPARE(spyFailed.count(), 0);
}

void TestFileTransfer::testReq42_MalformedAndTraversalTransferIdsRejected() {
    FTTestContext ctx;
    QSignalSpy spyFailed(ctx.ftm.get(), &FileTransferManager::transferFailed);
    QString spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";

    QJsonObject startTraversal;
    startTraversal["type"] = "file_start";
    startTraversal["transfer_id"] = "../../../../etc/passwd";
    startTraversal["file_size"] = 100;
    startTraversal["chunk_size"] = 100;
    startTraversal["chunk_count"] = 1;
    startTraversal["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startTraversal["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startTraversal);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Invalid transfer ID"));

    QJsonObject cancelTraversal;
    cancelTraversal["type"] = "file_cancel";
    cancelTraversal["transfer_id"] = "../evil";
    ctx.ftm->handleControlMessage("senderUser", "senderDevice", cancelTraversal);

    ctx.ftm->cancelTransfer("99999999-0000-0000-0000-000000000000", "senderDevice", "test");

    QJsonObject startSlash;
    startSlash["type"] = "file_start";
    startSlash["transfer_id"] = "42000000-0000/0000-0000-000000000001";
    startSlash["file_size"] = 100;
    startSlash["chunk_size"] = 100;
    startSlash["chunk_count"] = 1;
    startSlash["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startSlash["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startSlash);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Invalid transfer ID"));

    QVERIFY(!QFile::exists(spoolDir + "/../evil.recv.spool"));
}

void TestFileTransfer::testReq43_CapabilityUnknownFailsClosedUntilValid() {
    FTTestContext ctx;
    QSignalSpy spyFailed(ctx.ftm.get(), &FileTransferManager::transferFailed);

    ctx.caps->m_capState = CapabilityState::UNKNOWN;
    ctx.caps->m_envMax = 1000;

    QJsonObject startUnknown;
    startUnknown["type"] = "file_start";
    startUnknown["transfer_id"] = "43000000-0000-0000-0000-000000000001";
    startUnknown["file_size"] = 100;
    startUnknown["chunk_size"] = 100;
    startUnknown["chunk_count"] = 1;
    startUnknown["total_hash"] = QString(QByteArray(32, 'a').toBase64());
    startUnknown["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startUnknown);
    QCOMPARE(spyFailed.count(), 1);
    QCOMPARE(spyFailed.takeFirst().at(1).toString(), QString("Capabilities unavailable"));

    ctx.caps->m_capState = CapabilityState::VALID;

    QJsonObject startValid;
    startValid["type"] = "file_start";
    startValid["transfer_id"] = "43000000-0000-0000-0000-000000000002";
    startValid["file_size"] = 100;
    startValid["chunk_size"] = 100;
    startValid["chunk_count"] = 1;
    startValid["total_hash"] = QString(QByteArray(32, 'b').toBase64());
    startValid["root_file_key"] = QString(QByteArray(32, 'k').toBase64());

    ctx.ftm->handleControlMessage("senderUser", "senderDevice", startValid);
    QCOMPARE(spyFailed.count(), 0);
}
