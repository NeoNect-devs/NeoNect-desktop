#include "FileTransferManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QFileInfo>
#include <QtEndian>
#include <QDateTime>
#include <QCryptographicHash>
#include "../crypto/wire/WireCodec.h"
#include "../crypto/wire/WireTypes.h"
#include <iostream>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace NeoNect {
namespace Services {

static Crypto::SecureBuffer toSecureBuffer(const QByteArray& arr) {
    Crypto::SecureBuffer sb(arr.size());
    if (arr.size() > 0) {
        memcpy(sb.data(), arr.constData(), arr.size());
    }
    return sb;
}

static Crypto::AeadKey toAeadKey(const QByteArray& arr) {
    return Crypto::AeadKey{toSecureBuffer(arr)};
}

FileTransferManager::FileTransferManager(
    std::weak_ptr<Storage::ISecureE2EEStore> store,
    std::shared_ptr<Crypto::ICryptoBackend> crypto,
    std::shared_ptr<Storage::ICapabilitiesRepository> capabilities,
    std::shared_ptr<Storage::ISettingsRepository> settings,
    SendEnvelopeCb sendEnvelopeCb,
    SendChatMsgCb sendChatMsgCb,
    QObject* parent)
    : QObject(parent), m_store(std::move(store)), m_crypto(std::move(crypto)),
      m_capabilities(std::move(capabilities)), m_settings(std::move(settings)), m_sendEnvelopeCb(std::move(sendEnvelopeCb)),
      m_sendChatMsgCb(std::move(sendChatMsgCb))
{
    m_spoolDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/spool";
    QDir().mkpath(m_spoolDir);
    m_retryTimer = new QTimer(this);
    connect(m_retryTimer, &QTimer::timeout, this, &FileTransferManager::onRetryTimeout);
    m_retryTimer->start(5000); // 5 seconds
}

QByteArray FileTransferManager::deriveSenderSpoolKey(const QByteArray& rootKey, const QByteArray& transferId, const QString& senderDeviceId) {
    QByteArray info = "sender_spool_" + senderDeviceId.toUtf8();
    auto res = m_crypto->HkdfSha256(rootKey, transferId, info, 32);
    return res.isEmpty() ? QByteArray() : res;
}

QByteArray FileTransferManager::deriveRecipientDeviceKey(const QByteArray& rootKey, const QByteArray& transferId, const QString& recipientDeviceId) {
    QByteArray info = recipientDeviceId.toUtf8();
    auto res = m_crypto->HkdfSha256(rootKey, transferId, info, 32);
    return res.isEmpty() ? QByteArray() : res;
}

QByteArray FileTransferManager::generateNonce(uint32_t chunkIndex, uint8_t domain) {
    QByteArray nonce(12, 0);
    nonce[0] = domain;
    uint32_t beIdx = qToBigEndian(chunkIndex);
    memcpy(nonce.data() + 8, &beIdx, 4);
    return nonce;
}

static QByteArray transferIdToBinary(const QString& tid) {
    return QUuid("{" + tid + "}").toRfc4122();
}

static QString binaryToTransferId(const QByteArray& bin) {
    return QUuid::fromRfc4122(bin).toString(QUuid::WithoutBraces);
}

void FileTransferManager::startTransfer(const QString& recipientUser, const QString& recipientDevice, const QString& filePath) {
    auto maxHttpOpt = m_capabilities->maxHttpBodyBytes();
    auto maxEnvOpt = m_capabilities->maxEnvelopeBytes();
    if (!maxHttpOpt || !maxEnvOpt) {
        emit transferFailed("", "Capabilities unavailable");
        return;
    }

    qint32 maxEnvBytes = maxEnvOpt.value();
    qint32 maxHttpBodyBytes = maxHttpOpt.value();

    QJsonObject dummyHttp;
    dummyHttp["from_device_id"] = m_settings->deviceId(); 
    dummyHttp["protocol_version"] = 1;
    dummyHttp["recipient_id"] = recipientUser;
    dummyHttp["recipient_device_id"] = recipientDevice;
    dummyHttp["ciphertext"] = "";
    
    QJsonDocument doc(dummyHttp);
    qint32 emptyCiphertextBodyBytes = doc.toJson(QJsonDocument::Compact).size();

    qint32 maxBase64Bytes = maxHttpBodyBytes - emptyCiphertextBodyBytes;
    qint32 maxRawEnvelopeFromHttp = (maxBase64Bytes / 4) * 3;

    qint32 maxRawEnvelope = std::min(maxEnvBytes, maxRawEnvelopeFromHttp);

    qint32 overhead = 42;
    qint32 maxChunkSize = maxRawEnvelope - overhead;
    if (maxChunkSize <= 0) {
        emit transferFailed("", "Chunk size too small");
        return;
    }

    QFileInfo fi(filePath);
    if (!fi.exists() || fi.size() < 0) {
        emit transferFailed("", "File invalid");
        return;
    }

    qint64 fileSize = fi.size();
    qint32 chunkSize = maxChunkSize;
    qint32 chunkCount = (fileSize + chunkSize - 1) / chunkSize;
    if (fileSize == 0) chunkCount = 0; 
    
    QUuid newUuid = QUuid::createUuid();
    QByteArray transferIdBin = newUuid.toRfc4122();
    QString transferIdStr = newUuid.toString(QUuid::WithoutBraces);
    QByteArray rootKey = m_crypto->RandomBytes(32);
    if (rootKey.isEmpty()) return;

    QFile src(filePath);
    if (!src.open(QIODevice::ReadOnly)) return;

    QString senderDeviceId = m_settings->deviceId();
    QByteArray spoolKey = deriveSenderSpoolKey(rootKey, transferIdBin, senderDeviceId);
    
    QString spoolPath = m_spoolDir + "/" + transferIdStr + ".spool";
    QFile spool(spoolPath);
    if (!spool.open(QIODevice::WriteOnly)) return;

    QCryptographicHash fileHash(QCryptographicHash::Sha256);
    Crypto::AeadKey aSpoolKey = toAeadKey(spoolKey);

    for (qint32 i = 0; i < chunkCount; ++i) {
        QByteArray ptChunk = src.read(chunkSize);
        if (ptChunk.isEmpty() && fileSize != 0) break;
        
        fileHash.addData(ptChunk);
        Crypto::AeadNonce aNonce{generateNonce(i, 0)};
        auto encRes = m_crypto->AeadEncrypt(aSpoolKey, aNonce, ptChunk, transferIdBin);
        if (!encRes.success) {
            return;
        }
        spool.write(encRes.ciphertext);
        spool.write(encRes.tag.data);
    }
    QByteArray totalHash = fileHash.result();

    spool.flush();
#ifdef Q_OS_LINUX
    if (fdatasync(spool.handle()) != 0) {
        spool.close();
        spool.remove();
        emit transferFailed("", "Spool durability failed");
        return;
    }
#endif
    spool.close();

    Storage::E2EEFileTransfer tx;
    tx.transfer_id = transferIdStr;
    tx.peer_device_id = recipientDevice;
    tx.is_sender = 1;
    tx.file_size = fileSize;
    tx.chunk_size = chunkSize;
    tx.max_envelope_bytes = maxEnvBytes;
    tx.max_http_body_bytes = maxHttpBodyBytes;
    tx.chunk_count = chunkCount;
    tx.file_hash = totalHash;
    tx.spool_path = spoolPath;
    tx.root_file_key = rootKey;
    tx.status = "active";
    tx.created_at = QDateTime::currentSecsSinceEpoch();
    tx.received_bitset = QByteArray((chunkCount + 7) / 8, '\0');

    auto store = m_store.lock();
    if (store) store->saveFileTransfer(tx);
    QJsonObject fileStart;
    fileStart["type"] = "file_start";
    fileStart["transfer_id"] = transferIdStr;
    fileStart["file_name"] = fi.fileName();
    fileStart["file_size"] = fileSize;
    fileStart["chunk_count"] = chunkCount;
    fileStart["chunk_size"] = chunkSize;
    fileStart["total_hash"] = QString::fromLatin1(totalHash.toBase64());
    fileStart["root_file_key"] = QString::fromLatin1(rootKey.toBase64());

    m_sendChatMsgCb(fileStart, recipientUser, recipientDevice);
    
    // Start transmission window
    m_activeUsers[transferIdStr] = recipientUser;
    processNextSend(transferIdStr, recipientDevice, recipientUser);
}

void FileTransferManager::handleControlMessage(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString type = obj["type"].toString();
    if (type == "file_start") handleFileStart(senderUser, senderDevice, obj);
    else if (type == "file_ack") handleFileAck(senderUser, senderDevice, obj);
    else if (type == "file_nack") handleFileNack(senderUser, senderDevice, obj);
    else if (type == "file_cancel") handleFileCancel(senderUser, senderDevice, obj);
}

void FileTransferManager::handleFileStart(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    qint64 fileSize = obj["file_size"].toVariant().toLongLong();
    qint32 chunkCount = obj["chunk_count"].toInt();
    qint32 chunkSize = obj["chunk_size"].toInt();
    QByteArray hash = QByteArray::fromBase64(obj["total_hash"].toString().toLatin1());
    QByteArray rootKey = QByteArray::fromBase64(obj["root_file_key"].toString().toLatin1());

    if (fileSize < 0 || chunkCount < 0 || chunkSize <= 0) return;
    qint64 safeChunkSize = chunkSize;
    qint64 expectedCount = (fileSize + safeChunkSize - 1) / safeChunkSize;
    if (fileSize == 0) expectedCount = 0;
    if (chunkCount != expectedCount) return;
    if (rootKey.size() != 32) return;

    qint64 required_spool_size = static_cast<qint64>(chunkCount) * (safeChunkSize + 16);
    if (required_spool_size < 0) return; // overflow
    if (required_spool_size > 1LL * 1024 * 1024 * 1024 * 1024) return; // 1 TB limit arbitrary to prevent abuse

    QString spoolPath = m_spoolDir + "/" + tid + ".recv.spool";
    QFile spool(spoolPath);
    if (spool.open(QIODevice::WriteOnly)) {
        if (!spool.resize(required_spool_size)) {
            spool.close();
            spool.remove();
            return;
        }
        spool.close();
    }

    QByteArray bitset((chunkCount + 7) / 8, 0);

    Storage::E2EEFileTransfer tx;
    tx.transfer_id = tid;
    tx.peer_device_id = senderDevice;
    tx.is_sender = 0;
    tx.file_size = fileSize;
    tx.chunk_size = chunkSize;
    tx.max_envelope_bytes = 0;
    tx.max_http_body_bytes = 0;
    tx.chunk_count = chunkCount;
    tx.file_hash = hash;
    tx.spool_path = spoolPath;
    tx.root_file_key = rootKey;
    tx.received_bitset = bitset;
    tx.status = "active";
    tx.created_at = QDateTime::currentSecsSinceEpoch();

    auto store = m_store.lock();
    if (store) store->saveFileTransfer(tx);
}

void FileTransferManager::handleFileAck(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    qint32 idx = obj["chunk_index"].toInt();
    
    auto store = m_store.lock();
    if (!store) return;
    auto txRes = store->getFileTransfer(tid, senderDevice);
    if (!txRes.success) return;
    auto tx = txRes.data.value();
    
    if (idx < 0 || idx >= tx.chunk_count) return;

    int byteIdx = idx / 8;
    int bitIdx = idx % 8;
    
    if (tx.received_bitset.size() <= byteIdx) {
        int oldSize = tx.received_bitset.size();
        tx.received_bitset.resize((tx.chunk_count + 7) / 8);
        for(int i = oldSize; i < tx.received_bitset.size(); ++i) {
            tx.received_bitset[i] = '\0';
        }
    }

    tx.received_bitset[byteIdx] = tx.received_bitset[byteIdx] | (1 << bitIdx);
    store->updateFileTransferBitset(tid, senderDevice, tx.received_bitset);
    
    processNextSend(tid, senderDevice, senderUser);
}

void FileTransferManager::handleFileNack(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    
    auto store = m_store.lock();
    if (!store) return;
    auto txRes = store->getFileTransfer(tid, senderDevice);
    if (!txRes.success) return;
    auto tx = txRes.data.value();
    if (!tx.is_sender) return;

    if (obj.contains("chunk_index")) {
        m_inFlight[tid].remove(obj["chunk_index"].toInt());
    }
    if (obj.contains("chunk_indexes")) {
        for (const auto& val : obj["chunk_indexes"].toArray()) {
            m_inFlight[tid].remove(val.toInt());
        }
    }
    processNextSend(tid, senderDevice, senderUser);
}

void FileTransferManager::handleFileCancel(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    auto store = m_store.lock();
    if (store) store->updateFileTransferStatus(tid, senderDevice, "cancelled");
}

void FileTransferManager::cancelTransfer(const QString& transferId, const QString& peerDevice, const QString& reason) {
    auto store = m_store.lock();
    if (store) store->updateFileTransferStatus(transferId, peerDevice, "cancelled");
}

void FileTransferManager::onRetryTimeout() {
    auto store = m_store.lock();
    if (!store) return;
    
    // We iterate over m_inFlight, which is only populated for senders
    for (auto it = m_inFlight.begin(); it != m_inFlight.end(); ++it) {
        QString tid = it.key();
        if (it.value().isEmpty()) continue;
        // get the peer device ID by looking at the DB? Or we can query DB for this transfer directly
        auto txsRes = store->getActiveFileTransfers();
        if (!txsRes.success) continue;
        for (const auto& tx : txsRes.data.value()) {
            if (tx.transfer_id == tid && tx.is_sender) {
                QString recipientUser = m_activeUsers.value(tid);
                if (recipientUser.isEmpty()) continue; // We cannot retry if we lost the user in memory (restart case)
                processNextSend(tid, tx.peer_device_id, recipientUser);
                break;
            }
        }
    }
}

void FileTransferManager::processNextSend(const QString& transferId, const QString& peerDevice, const QString& recipientUser) {
    auto store = m_store.lock();
    if (!store) return;
    auto txRes = store->getFileTransfer(transferId, peerDevice);
    if (!txRes.success) return;
    auto tx = txRes.data.value();
    if (tx.status != "active") return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int inFlightCount = 0;
    auto& inFlight = m_inFlight[transferId];
    
    if (tx.received_bitset.size() < (tx.chunk_count + 7) / 8) {
        int oldSize = tx.received_bitset.size();
        tx.received_bitset.resize((tx.chunk_count + 7) / 8);
        for(int i = oldSize; i < tx.received_bitset.size(); ++i) {
            tx.received_bitset[i] = '\0';
        }
    }

    for (auto it = inFlight.begin(); it != inFlight.end();) {
        if ((tx.received_bitset[it.key()/8] & (1 << (it.key()%8))) != 0) {
            it = inFlight.erase(it);
        } else if (now - it.value() > m_inFlightTimeoutMs) { 
            it = inFlight.erase(it);
        } else {
            inFlightCount++;
            ++it;
        }
    }

    int windowSize = 5;
    bool allAcked = true;
    for (int i = 0; i < tx.chunk_count; ++i) {
        if ((tx.received_bitset[i/8] & (1 << (i%8))) == 0) {
            allAcked = false;
            if (inFlightCount < windowSize && !inFlight.contains(i)) {
                if (m_retryCounts[transferId][i] > 5) {
                    store->updateFileTransferStatus(transferId, peerDevice, "failed");
                    emit transferFailed(transferId, "Too many retries");
                    return;
                }
                m_retryCounts[transferId][i]++;
                sendChunk(tx, i, recipientUser);
                inFlight[i] = now;
                inFlightCount++;
            }
        }
    }
    
    if (allAcked) {
        store->updateFileTransferStatus(transferId, peerDevice, "completed");
        emit transferCompleted(transferId, tx.spool_path);
    }
}

void FileTransferManager::sendChunk(const Storage::E2EEFileTransfer& tx, uint32_t chunkIndex, const QString& recipientUser) {
    QFile spool(tx.spool_path);
    if (!spool.open(QIODevice::ReadOnly)) return;
    
    qint64 offset = static_cast<qint64>(chunkIndex) * (static_cast<qint64>(tx.chunk_size) + 16);
    if (!spool.seek(offset)) return;
    
    qint32 ptSize = tx.chunk_size;
    if (chunkIndex == tx.chunk_count - 1) {
        qint32 rem = tx.file_size % tx.chunk_size;
        if (rem != 0) ptSize = rem;
    }
    
    QByteArray spoolCt = spool.read(ptSize);
    QByteArray spoolTagData = spool.read(16);

    QString senderDeviceId = m_settings->deviceId();
    QByteArray spoolKey = deriveSenderSpoolKey(tx.root_file_key, transferIdToBinary(tx.transfer_id), senderDeviceId);
    Crypto::AeadKey aSpoolKey = toAeadKey(spoolKey);
    Crypto::AeadNonce spoolNonce{generateNonce(chunkIndex, 0)};
    Crypto::AeadTag spoolTag{spoolTagData};

    auto decRes = m_crypto->AeadDecrypt(aSpoolKey, spoolNonce, spoolCt, spoolTag, transferIdToBinary(tx.transfer_id));
    if (!decRes.has_value()) return;

    QByteArray pt = decRes.value();

    QByteArray deviceKey = deriveRecipientDeviceKey(tx.root_file_key, transferIdToBinary(tx.transfer_id), tx.peer_device_id);
    Crypto::AeadKey aDeviceKey = toAeadKey(deviceKey);
    Crypto::AeadNonce deviceNonce{generateNonce(chunkIndex, 1)};
    auto encRes = m_crypto->AeadEncrypt(aDeviceKey, deviceNonce, pt, transferIdToBinary(tx.transfer_id));
    if (!encRes.success) return;

    Crypto::Wire::FileChunkEnvelope chunkEnv;
    chunkEnv.version = Crypto::Wire::WireCodec::CURRENT_VERSION;
    chunkEnv.transferId = transferIdToBinary(tx.transfer_id);
    chunkEnv.chunkIndex = chunkIndex;
    chunkEnv.ciphertext = encRes.ciphertext;
    chunkEnv.tag.data = encRes.tag.data;
    
    QByteArray envBytes = Crypto::Wire::WireCodec::encodeFileChunkEnvelope(chunkEnv);
    QString msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sendEnvelopeCb(recipientUser, tx.peer_device_id, msgId, envBytes);
}

NeoNect::VoidResult FileTransferManager::handleEnvelope(const QByteArray &envelope, const Transport::TransportMetadata &metadata) {
    auto envOpt = Crypto::Wire::WireCodec::decodeFileChunkEnvelope(envelope);
    if (!envOpt) return VoidResult::fail("Invalid FILE_CHUNK envelope");
    auto& chunkEnv = envOpt.value();
    QString tid = binaryToTransferId(chunkEnv.transferId);

    auto store = m_store.lock();
    if (!store) return VoidResult::fail("Store not available");

    auto txRes = store->getFileTransfer(tid, metadata.senderDeviceId);
    if (!txRes.success) return VoidResult::fail("Transfer not found");

    auto tx = txRes.data.value();
    if (tx.is_sender) return VoidResult::fail("Sender cannot receive chunks");
    if (tx.status != "active") return VoidResult::fail("Transfer not active");
    if (chunkEnv.chunkIndex >= tx.chunk_count) return VoidResult::fail("Invalid chunk index");

    int byteIdx = chunkEnv.chunkIndex / 8;
    int bitIdx = chunkEnv.chunkIndex % 8;
    if ((tx.received_bitset[byteIdx] & (1 << bitIdx)) != 0) {
        // Duplicate chunk, ACK and ignore
        QJsonObject ack;
        ack["type"] = "file_ack";
        ack["transfer_id"] = tid;
        ack["chunk_index"] = static_cast<int>(chunkEnv.chunkIndex);
        m_sendChatMsgCb(ack, metadata.senderUserId, metadata.senderDeviceId); 
        return VoidResult::ok(std::monostate{});
    }

    QString localDeviceId = m_settings->deviceId();
    QByteArray deviceKey = deriveRecipientDeviceKey(tx.root_file_key, chunkEnv.transferId, localDeviceId); 
    Crypto::AeadNonce aNonce{generateNonce(chunkEnv.chunkIndex, 1)};
    Crypto::AeadKey aDeviceKey = toAeadKey(deviceKey);
    
    qint32 expectedCtSize = tx.chunk_size;
    if (chunkEnv.chunkIndex == static_cast<uint32_t>(tx.chunk_count - 1)) {
        qint32 rem = tx.file_size % tx.chunk_size;
        if (rem != 0) expectedCtSize = rem;
    }
    if (chunkEnv.ciphertext.size() != expectedCtSize) return VoidResult::fail("Malformed chunk length");

    auto decRes = m_crypto->AeadDecrypt(aDeviceKey, aNonce, chunkEnv.ciphertext, chunkEnv.tag, chunkEnv.transferId);
    if (!decRes.has_value()) return VoidResult::fail("Chunk authentication failed");

    QFile spool(tx.spool_path);
    if (!spool.open(QIODevice::ReadWrite)) return VoidResult::fail("Failed to open spool");
    qint64 offset = static_cast<qint64>(chunkEnv.chunkIndex) * (static_cast<qint64>(tx.chunk_size) + 16);
    if (!spool.seek(offset)) return VoidResult::fail("Seek failed");
    
    spool.write(chunkEnv.ciphertext);
    spool.write(chunkEnv.tag.data);
    if (!spool.flush()) {
        spool.close();
        store->updateFileTransferStatus(tid, metadata.senderDeviceId, "failed");
        return VoidResult::fail("Spool flush failed");
    }
#ifdef Q_OS_LINUX
    if (fdatasync(spool.handle()) != 0) {
        spool.close();
        store->updateFileTransferStatus(tid, metadata.senderDeviceId, "failed");
        return VoidResult::fail("Spool durability failed");
    }
#endif
    spool.close();

    tx.received_bitset[byteIdx] = tx.received_bitset[byteIdx] | (1 << bitIdx);
    auto updateRes = store->updateFileTransferBitset(tid, tx.peer_device_id, tx.received_bitset);
    if (!updateRes.success) {
        store->updateFileTransferStatus(tid, metadata.senderDeviceId, "failed");
        return VoidResult::fail("DB commit failed");
    }

    QJsonObject ack;
    ack["type"] = "file_ack";
    ack["transfer_id"] = tid;
    ack["chunk_index"] = static_cast<int>(chunkEnv.chunkIndex);
    m_sendChatMsgCb(ack, metadata.senderUserId, metadata.senderDeviceId); 

    bool done = true;
    for (int i = 0; i < tx.chunk_count; ++i) {
        if ((tx.received_bitset[i/8] & (1 << (i%8))) == 0) {
            done = false; break;
        }
    }
    if (done) reconstructFile(tx);

    return VoidResult::ok(std::monostate{});
}

void FileTransferManager::reconstructFile(const Storage::E2EEFileTransfer& tx) {
    QString outPath = m_spoolDir + "/" + tx.transfer_id + ".final";
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) return;
    
    QFile spool(tx.spool_path);
    if (!spool.open(QIODevice::ReadOnly)) return;

    QCryptographicHash fileHash(QCryptographicHash::Sha256);
    QString localDeviceId = m_settings->deviceId();
    QByteArray deviceKey = deriveRecipientDeviceKey(tx.root_file_key, transferIdToBinary(tx.transfer_id), localDeviceId); 
    Crypto::AeadKey aDeviceKey = toAeadKey(deviceKey);

    for (int i = 0; i < tx.chunk_count; ++i) {
        qint64 offset = static_cast<qint64>(i) * (static_cast<qint64>(tx.chunk_size) + 16);
        spool.seek(offset);
        
        qint32 ptSize = tx.chunk_size;
        if (i == tx.chunk_count - 1) {
            qint32 rem = tx.file_size % tx.chunk_size;
            if (rem != 0) ptSize = rem;
        }
        
        QByteArray ct = spool.read(ptSize);
        Crypto::AeadTag tag{spool.read(16)};
        Crypto::AeadNonce aNonce{generateNonce(i, 1)};
        
        auto decRes = m_crypto->AeadDecrypt(aDeviceKey, aNonce, ct, tag, transferIdToBinary(tx.transfer_id));
        if (!decRes.has_value()) {
            out.close();
            out.remove();
            return;
        }
        out.write(decRes.value());
        fileHash.addData(decRes.value());
    }
    
    if (fileHash.result() != tx.file_hash) {
        out.close();
        out.remove();
        return;
    }
    out.flush();
#ifdef Q_OS_LINUX
    fdatasync(out.handle());
#endif
    out.close();
    
    auto store = m_store.lock();
    if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "completed");
    
    emit transferCompleted(tx.transfer_id, outPath);
}

}
}
