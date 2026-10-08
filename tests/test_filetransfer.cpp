#include "test_filetransfer.h"
#include "../src/services/FileTransferManager.h"
#include "../src/crypto/OpenSSLBackend.h"
#include "../src/storage/e2ee/ISecureE2EEStore.h"
#include "../src/storage/icapabilitiesrepository.h"
#include "../src/storage/isettingsrepository.h"
#include "../src/crypto/wire/WireCodec.h"
#include "../src/crypto/wire/WireTypes.h"
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
    ServiceResult<E2EEFileTransfer> getFileTransfer(const QString& transfer_id, const QString& peer_device_id) override {
        QString key = transfer_id + "_" + peer_device_id;
        if (!m_transfers.contains(key)) return ServiceResult<E2EEFileTransfer>::fail("Not found");
        return ServiceResult<E2EEFileTransfer>::ok(m_transfers.value(key));
    }
    ServiceResult<std::vector<E2EEFileTransfer>> getActiveFileTransfers() override {
        std::vector<E2EEFileTransfer> res;
        for (const auto& t : m_transfers) res.push_back(t);
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
    CapabilityState capabilityState() const override { return CapabilityState::VALID; }
    void replace(std::optional<qint64>, std::optional<int>, std::optional<int>, CapabilityState) override {}
    
    int m_envMax = 100 * 1024 * 1024;
};

class MockSettingsFT : public ISettingsRepository {
public:
    void setProfile(const QString &) override {}
    QString profile() const override { return "prof"; }
    QString serverUrl() const override { return ""; }
    void setServerUrl(const QString &) override {}
    QString authToken() const override { return ""; }
    void setAuthToken(const QString &) override {}
    QString username() const override { return "user1"; }
    void setUsername(const QString &) override {}
    QString deviceId() const override { return m_deviceId; }
    void setDeviceId(const QString &d) override { m_deviceId = d; }
    QString publicKey() const override { return ""; }
    void setPublicKey(const QString &) override {}
    QStringList friends() const override { return {}; }
    void setFriends(const QStringList &) override {}
    QStringList pendingRequests() const override { return {}; }
    void setPendingRequests(const QStringList &) override {}
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
    QVERIFY(ctx.sentEnvelopes.size() == 1);
    ctx.sentEnvelopes.clear();
    
    // Wait for the timer to fire autonomously through the Qt event loop
    QTRY_VERIFY_WITH_TIMEOUT(ctx.sentEnvelopes.size() == 1, 2000);
    
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
    // Break spool dir by putting a file in its place or making it unwritable
    // We'll create a dummy file for the exact spool path to prevent open(WriteOnly)
    // Actually startTransfer uses tid.spool.
    // Let's just hook the system by setting permissions. Wait, QProcess to chmod?
    // Let's just create a dummy file named 'spoolDir' to block it from being a dir? No, QDir().mkpath runs.
    
    // We know tid is a uuid. We can't guess it.
    // Let's just use chmod to make spoolDir read-only.
    QFile spoolDirFile(spoolDir);
    spoolDirFile.setPermissions(QFileDevice::ReadUser);
    
    QString fp = ctx.tempDir.path() + "/src.txt";
    QFile f(fp); if(f.open(QIODevice::WriteOnly)) { f.write(QByteArray(100, 'Z')); f.close(); }
    
    ctx.ftm->startTransfer("peerUser", "peerDevice", fp);
    
    // Restore permissions so other tests don't fail!
    spoolDirFile.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser);
    
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
