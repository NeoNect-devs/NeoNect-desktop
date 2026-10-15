#include "FileTransferManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QFileInfo>
#include <QUrl>
#include <QtEndian>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QImageReader>
#include <QSet>
#include "../crypto/wire/WireCodec.h"
#include "../crypto/wire/WireTypes.h"
#include "../storage/StoragePathResolver.h"
#include <iostream>

#if defined(Q_OS_UNIX)
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
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
    QDir spoolDir(m_spoolDir);
    const auto tmpFiles = spoolDir.entryList(QStringList() << "*.tmp*", QDir::Files);
    for (const auto &tmpFile : tmpFiles) {
        spoolDir.remove(tmpFile);
    }
    auto storeLock = m_store.lock();
    if (storeLock) {
        auto activeRes = storeLock->getActiveFileTransfers();
        if (activeRes.success) {
            QSet<QString> activeSpoolFiles;
            for (const auto& tx : activeRes.data.value()) {
                if (tx.status != "completed" && tx.status != "cancelled") {
                    if (!tx.spool_path.isEmpty()) {
                        activeSpoolFiles.insert(QFileInfo(tx.spool_path).fileName());
                    }
                    activeSpoolFiles.insert(tx.transfer_id + ".spool");
                    activeSpoolFiles.insert(tx.transfer_id + ".recv.spool");
                }
            }
            const auto spoolFiles = spoolDir.entryList(QStringList() << "*.spool", QDir::Files);
            for (const auto& sf : spoolFiles) {
                if (!activeSpoolFiles.contains(sf)) {
                    spoolDir.remove(sf);
                }
            }
        }
    }
    m_retryTimer = new QTimer(this);
    connect(m_retryTimer, &QTimer::timeout, this, &FileTransferManager::onRetryTimeout);
    m_retryTimer->start(5000);
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

static bool atomicReplaceFile(const QString& sourcePath, const QString& targetPath) {
#if defined(Q_OS_UNIX)
    return ::rename(QFile::encodeName(sourcePath).constData(), QFile::encodeName(targetPath).constData()) == 0;
#elif defined(Q_OS_WIN)
    return MoveFileExW(
        reinterpret_cast<LPCWSTR>(sourcePath.utf16()),
        reinterpret_cast<LPCWSTR>(targetPath.utf16()),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return QFile::rename(sourcePath, targetPath);
#endif
}

qint64 FileTransferManager::writeData(QFile& file, const char* data, qint64 len, const QString& stage) {
    if (m_writeHook) {
        return m_writeHook(file, data, len, stage);
    }
    return file.write(data, len);
}

bool FileTransferManager::replaceFile(const QString& sourcePath, const QString& targetPath) {
    if (m_replaceHook) {
        return m_replaceHook(sourcePath, targetPath);
    }
    return atomicReplaceFile(sourcePath, targetPath);
}

bool FileTransferManager::syncDirectory(const QString& dirPath) {
    if (m_dirSyncHook) {
        return m_dirSyncHook(dirPath);
    }
#ifdef Q_OS_LINUX
    int dirFd = ::open(QFile::encodeName(dirPath).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0) {
        return false;
    }
    int syncErr = 0;
    int res = 0;
    if (m_fsyncHook) {
        res = m_fsyncHook(dirFd, syncErr);
    } else {
        res = ::fsync(dirFd);
        syncErr = errno;
    }
    int closeRes = ::close(dirFd);
    if (res != 0 && syncErr != EINVAL && syncErr != ENOTSUP) {
        return false;
    }
    if (closeRes != 0) {
        return false;
    }
#endif
    return true;
}

static constexpr qint32 MAX_CHUNK_COUNT = 2000000;
static constexpr qint32 MIN_CHUNK_SIZE_MULTI_CHUNK = 64;

static bool isValidTransferId(const QString& tid) {
    if (tid.size() != 36) return false;
    for (int i = 0; i < 36; ++i) {
        QChar c = tid.at(i);
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return false;
            }
        }
    }
    QUuid u = QUuid::fromString(tid);
    return !u.isNull();
}

static bool isContainedInSpoolDir(const QString& filePath, const QString& spoolDir) {
    if (filePath.isEmpty() || spoolDir.isEmpty()) return false;
    QDir dir(spoolDir);
    QString canonicalDir = dir.canonicalPath();
    if (canonicalDir.isEmpty()) {
        canonicalDir = QDir::cleanPath(dir.absolutePath());
    }
    QFileInfo fi(filePath);
    QString canonicalFile = fi.canonicalFilePath();
    if (canonicalFile.isEmpty()) {
        QString canonicalParent = fi.dir().canonicalPath();
        if (canonicalParent.isEmpty()) {
            canonicalParent = QDir::cleanPath(fi.dir().absolutePath());
        }
        return canonicalParent == canonicalDir || canonicalParent.startsWith(canonicalDir + "/");
    }
    return canonicalFile.startsWith(canonicalDir + "/");
}

static inline qsizetype safeBitsetSize(qint32 chunkCount) {
    if (chunkCount <= 0) return 0;
    return static_cast<qsizetype>((static_cast<qint64>(chunkCount) + 7) / 8);
}

static QByteArray transferIdToBinary(const QString& tid) {
    return QUuid("{" + tid + "}").toRfc4122();
}

static QString binaryToTransferId(const QByteArray& bin) {
    return QUuid::fromRfc4122(bin).toString(QUuid::WithoutBraces);
}

void FileTransferManager::startTransfer(const QString& recipientUser, const QString& recipientDevice, const QString& filePath, const QString& overrideTransferId) {
    if (!overrideTransferId.isEmpty() && !isValidTransferId(overrideTransferId)) {
        emit transferFailed(overrideTransferId, "Invalid transfer ID");
        return;
    }
    QUuid newUuid;
    if (overrideTransferId.isEmpty()) {
        newUuid = QUuid::createUuid();
    } else {
        newUuid = QUuid("{" + overrideTransferId + "}");
    }
    QByteArray transferIdBin = newUuid.toRfc4122();
    QString transferIdStr = newUuid.toString(QUuid::WithoutBraces);

    auto maxHttpOpt = m_capabilities->maxHttpBodyBytes();
    auto maxEnvOpt = m_capabilities->maxEnvelopeBytes();
    if (!maxHttpOpt || !maxEnvOpt) {
        emit transferFailed(transferIdStr, "Capabilities unavailable");
        return;
    }

    qint32 maxEnvBytes = maxEnvOpt.value();
    qint64 maxHttpBodyBytes = maxHttpOpt.value();

    QJsonObject dummyHttp;
    dummyHttp["from_device_id"] = m_settings->deviceId();
    dummyHttp["protocol_version"] = 2;
    dummyHttp["recipient_device_id"] = recipientDevice;
    dummyHttp["message_id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    dummyHttp["timestamp"] = QDateTime::currentSecsSinceEpoch();
    dummyHttp["ciphertext"] = "";
    
    QJsonDocument doc(dummyHttp);
    qint64 emptyCiphertextBodyBytes = doc.toJson(QJsonDocument::Compact).size();

    qint64 maxBase64Bytes = maxHttpBodyBytes - emptyCiphertextBodyBytes;
    qint64 maxRawEnvelopeFromHttp = (maxBase64Bytes / 4) * 3;

    qint64 maxRawEnvelope64 = std::min<qint64>(maxEnvBytes, maxRawEnvelopeFromHttp);
    qint32 maxRawEnvelope = static_cast<qint32>(std::clamp<qint64>(maxRawEnvelope64, 0, std::numeric_limits<qint32>::max()));

    qint32 overhead = 42;
    qint32 maxChunkSize = maxRawEnvelope - overhead;
    if (maxChunkSize <= 0) {
        emit transferFailed(transferIdStr, "Chunk size too small");
        return;
    }

    QString resolvedFilePath = filePath;
    if (resolvedFilePath.startsWith("file://")) {
        QUrl url(resolvedFilePath);
        if (url.isLocalFile()) {
            resolvedFilePath = url.toLocalFile();
        } else {
            resolvedFilePath = resolvedFilePath.mid(7);
        }
#ifdef _WIN32
        if (resolvedFilePath.startsWith("/") && resolvedFilePath.length() >= 3 && resolvedFilePath.at(2) == ':') {
            resolvedFilePath = resolvedFilePath.mid(1);
        }
#endif
    }

    QFileInfo fi(resolvedFilePath);
    if (!fi.exists() || fi.size() < 0) {
        emit transferFailed(transferIdStr, "File invalid");
        return;
    }

    auto store = m_store.lock();
    qint64 fileSize = fi.size();
    qint32 chunkSize = maxChunkSize;
    qint32 chunkCount = (fileSize + chunkSize - 1) / chunkSize;
    if (fileSize == 0) chunkCount = 0;

    QByteArray rootKey;
    QByteArray totalHash;
    QString spoolPath = m_spoolDir + "/" + transferIdStr + ".spool";
    if (!isContainedInSpoolDir(spoolPath, m_spoolDir)) {
        emit transferFailed(transferIdStr, "Invalid spool path");
        return;
    }
    bool reuseExistingSpool = false;

    if (store && !overrideTransferId.isEmpty()) {
        auto existingRes = store->getFileTransferById(transferIdStr);
        if (existingRes.success && existingRes.data.has_value() && existingRes.data->is_sender &&
            !existingRes.data->spool_path.isEmpty() && QFile::exists(existingRes.data->spool_path) &&
            existingRes.data->root_file_key.size() == 32) {
            rootKey = existingRes.data->root_file_key;
            totalHash = existingRes.data->file_hash;
            spoolPath = existingRes.data->spool_path;
            fileSize = existingRes.data->file_size;
            chunkSize = existingRes.data->chunk_size;
            chunkCount = existingRes.data->chunk_count;
            reuseExistingSpool = true;
        }
    }

    if (!reuseExistingSpool) {
        rootKey = m_crypto->RandomBytes(32);
        if (rootKey.isEmpty()) {
            emit transferFailed(transferIdStr, "Random key generation failed");
            return;
        }

        QFile src(resolvedFilePath);
        if (!src.open(QIODevice::ReadOnly)) {
            emit transferFailed(transferIdStr, "Failed to open source file");
            return;
        }

        QString senderDeviceId = m_settings->deviceId();
        QByteArray spoolKey = deriveSenderSpoolKey(rootKey, transferIdBin, senderDeviceId);

        QDir().mkpath(m_spoolDir);
        QFile spool(spoolPath);
        if (!spool.open(QIODevice::WriteOnly)) {
            emit transferFailed(transferIdStr, "Failed to open spool file");
            return;
        }

        QCryptographicHash fileHash(QCryptographicHash::Sha256);
        Crypto::AeadKey aSpoolKey = toAeadKey(spoolKey);

        for (qint32 i = 0; i < chunkCount; ++i) {
            QByteArray ptChunk = src.read(chunkSize);
            if (ptChunk.isEmpty() && fileSize != 0) break;

            fileHash.addData(ptChunk);
            Crypto::AeadNonce aNonce{generateNonce(i, 0)};
            auto encRes = m_crypto->AeadEncrypt(aSpoolKey, aNonce, ptChunk, transferIdBin);
            if (!encRes.success) {
                spool.close();
                spool.remove();
                emit transferFailed(transferIdStr, "Spool encryption failed");
                return;
            }
            if (writeData(spool, encRes.ciphertext.constData(), encRes.ciphertext.size(), QStringLiteral("sender_spool_ciphertext")) != encRes.ciphertext.size() ||
                writeData(spool, reinterpret_cast<const char*>(encRes.tag.data.data()), encRes.tag.data.size(), QStringLiteral("sender_spool_tag")) != encRes.tag.data.size()) {
                spool.close();
                spool.remove();
                emit transferFailed(transferIdStr, "Spool write failed");
                return;
            }
        }
        totalHash = fileHash.result();

        if (!spool.flush()) {
            spool.close();
            spool.remove();
            emit transferFailed(transferIdStr, "Spool flush failed");
            return;
        }
#ifdef Q_OS_LINUX
        if (fdatasync(spool.handle()) != 0) {
            spool.close();
            spool.remove();
            emit transferFailed(transferIdStr, "Spool durability failed");
            return;
        }
#endif
        spool.close();
    }

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
    tx.received_bitset = QByteArray(safeBitsetSize(chunkCount), '\0');

    if (store) store->saveFileTransfer(tx);
    m_transferFileNames[transferIdStr] = fi.fileName();

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

    m_activeUsers[transferIdStr] = recipientUser;
    const QString peerKey = transferIdStr + ":" + recipientDevice;
    m_inFlight.remove(peerKey);
    m_retryCounts.remove(peerKey);
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
    if (!isValidTransferId(tid)) {
        if (!tid.isEmpty()) emit transferFailed(tid, "Invalid transfer ID");
        return;
    }
    qint64 fileSize = obj["file_size"].toVariant().toLongLong();
    qint32 chunkCount = obj["chunk_count"].toInt();
    qint32 chunkSize = obj["chunk_size"].toInt();
    QByteArray hash = QByteArray::fromBase64(obj["total_hash"].toString().toLatin1());
    QByteArray rootKey = QByteArray::fromBase64(obj["root_file_key"].toString().toLatin1());

    if (!m_capabilities) {
        emit transferFailed(tid, "Capabilities unavailable");
        return;
    }
    auto state = m_capabilities->capabilityState();
    if (state == Storage::CapabilityState::UNAVAILABLE ||
        state == Storage::CapabilityState::INVALID ||
        state == Storage::CapabilityState::UNSUPPORTED ||
        state == Storage::CapabilityState::UNKNOWN) {
        emit transferFailed(tid, "Capabilities unavailable");
        return;
    }
    auto maxEnvOpt = m_capabilities->maxEnvelopeBytes();
    if (!maxEnvOpt.has_value() || maxEnvOpt.value() <= 42) {
        emit transferFailed(tid, "Capabilities unavailable");
        return;
    }
    qint32 maxChunkAllowed = maxEnvOpt.value() - 42;
    if (chunkSize > maxChunkAllowed) {
        emit transferFailed(tid, "Chunk size exceeds envelope limit");
        return;
    }

    qint64 safeChunkSize = chunkSize;
    if (fileSize < 0 || chunkCount < 0 || chunkSize <= 0 || (safeChunkSize > 0 && fileSize > (std::numeric_limits<qint64>::max() - safeChunkSize))) {
        emit transferFailed(tid, "Invalid transfer metadata");
        return;
    }
    if (chunkCount > 1 && chunkSize < MIN_CHUNK_SIZE_MULTI_CHUNK) {
        emit transferFailed(tid, "Chunk size too small");
        return;
    }
    qint64 expectedCount = (fileSize + safeChunkSize - 1) / safeChunkSize;
    if (fileSize == 0) expectedCount = 0;
    if (chunkCount != expectedCount) {
        emit transferFailed(tid, "Mismatched chunk count");
        return;
    }
    if (hash.size() != 32) {
        emit transferFailed(tid, "Invalid file hash");
        return;
    }
    if (rootKey.size() != 32) {
        emit transferFailed(tid, "Invalid transfer key");
        return;
    }

    qint64 required_spool_size = static_cast<qint64>(chunkCount) * (safeChunkSize + 16);
    if (fileSize > 1LL * 1024 * 1024 * 1024 * 1024 || required_spool_size < 0 || required_spool_size > 1LL * 1024 * 1024 * 1024 * 1024) {
        emit transferFailed(tid, "Transfer size exceeds limit");
        return;
    }
    if (chunkCount > MAX_CHUNK_COUNT) {
        emit transferFailed(tid, "Chunk count exceeds limit");
        return;
    }

    auto store = m_store.lock();
    if (store) {
        auto existingRes = store->getFileTransfer(tid, senderDevice);
        if (existingRes.success && existingRes.data.has_value() && existingRes.data->status == "completed" &&
            !existingRes.data->spool_path.isEmpty() && QFile::exists(existingRes.data->spool_path)) {
            emit transferCompleted(tid, existingRes.data->spool_path);
            return;
        }
    }

    QDir().mkpath(m_spoolDir);
    QString spoolPath = m_spoolDir + "/" + tid + ".recv.spool";
    if (!isContainedInSpoolDir(spoolPath, m_spoolDir)) {
        emit transferFailed(tid, "Invalid spool path");
        return;
    }
    QFile spool(spoolPath);
    if (spool.open(QIODevice::WriteOnly)) {
        if (!spool.resize(required_spool_size)) {
            spool.close();
            spool.remove();
            emit transferFailed(tid, "Failed to allocate spool");
            return;
        }
        spool.close();
    } else {
        emit transferFailed(tid, "Failed to create spool");
        return;
    }

    QByteArray bitset(safeBitsetSize(chunkCount), 0);

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

    if (store) store->saveFileTransfer(tx);
    m_activeUsers[tid] = senderUser;
    m_transferFileNames[tid] = obj["file_name"].toString();
    if (chunkCount == 0) {
        reconstructFile(tx);
    } else {
        emit transferProgress(tid, 0);
    }
}

void FileTransferManager::handleFileAck(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    if (!isValidTransferId(tid)) return;
    qint32 idx = obj["chunk_index"].toInt();
    
    auto store = m_store.lock();
    if (!store) return;
    auto txRes = store->getFileTransfer(tid, senderDevice);
    if (!txRes.success) return;
    auto tx = txRes.data.value();
    
    if (idx < 0 || idx >= tx.chunk_count) return;

    int byteIdx = idx / 8;
    int bitIdx = idx % 8;
    qsizetype reqBitset = safeBitsetSize(tx.chunk_count);
    if (tx.received_bitset.size() <= byteIdx) {
        int oldSize = tx.received_bitset.size();
        tx.received_bitset.resize(reqBitset);
        for(int i = oldSize; i < tx.received_bitset.size(); ++i) {
            tx.received_bitset[i] = '\0';
        }
    }

    if (byteIdx < tx.received_bitset.size()) {
        tx.received_bitset[byteIdx] = tx.received_bitset[byteIdx] | (1 << bitIdx);
        store->updateFileTransferBitset(tid, senderDevice, tx.received_bitset);
    }

    int ackedChunks = 0;
    for (int i = 0; i < tx.chunk_count; ++i) {
        int bIdx = i / 8;
        if (bIdx < tx.received_bitset.size() && (tx.received_bitset[bIdx] & (1 << (i % 8))) != 0) {
            ackedChunks++;
        }
    }
    int pct = (tx.chunk_count > 0) ? static_cast<int>((static_cast<qint64>(ackedChunks) * 100) / tx.chunk_count) : 100;
    emit transferProgress(tid, pct);
    
    processNextSend(tid, senderDevice, senderUser);
}

void FileTransferManager::handleFileNack(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    QString tid = obj["transfer_id"].toString();
    if (!isValidTransferId(tid)) return;

    auto store = m_store.lock();
    if (!store) return;
    auto txRes = store->getFileTransfer(tid, senderDevice);
    if (!txRes.success) {
        auto anyTxRes = store->getFileTransferById(tid);
        if (!anyTxRes.success || !anyTxRes.data.has_value() || !anyTxRes.data->is_sender) return;
        auto baseTx = anyTxRes.data.value();
        if (baseTx.spool_path.isEmpty() || !QFile::exists(baseTx.spool_path)) return;

        Storage::E2EEFileTransfer clonedTx = baseTx;
        clonedTx.peer_device_id = senderDevice;
        clonedTx.status = "active";
        clonedTx.received_bitset = QByteArray(safeBitsetSize(clonedTx.chunk_count), '\0');
        store->saveFileTransfer(clonedTx);

        QJsonObject fileStart;
        fileStart["type"] = "file_start";
        fileStart["transfer_id"] = tid;
        fileStart["file_name"] = m_transferFileNames.value(tid, QStringLiteral("attachment"));
        fileStart["file_size"] = clonedTx.file_size;
        fileStart["chunk_count"] = clonedTx.chunk_count;
        fileStart["chunk_size"] = clonedTx.chunk_size;
        fileStart["total_hash"] = QString::fromLatin1(clonedTx.file_hash.toBase64());
        fileStart["root_file_key"] = QString::fromLatin1(clonedTx.root_file_key.toBase64());
        if (m_sendChatMsgCb) {
            m_sendChatMsgCb(fileStart, senderUser, senderDevice);
        }
        m_activeUsers[tid] = senderUser;
        processNextSend(tid, senderDevice, senderUser);
        return;
    }
    auto tx = txRes.data.value();
    if (!tx.is_sender) return;

    if (tx.status != "active") {
        store->updateFileTransferStatus(tid, senderDevice, "active");
    }

    const QString peerKey = tid + ":" + senderDevice;
    if (obj.contains("chunk_index")) {
        int cIdx = obj["chunk_index"].toInt();
        m_inFlight[peerKey].remove(cIdx);
        m_inFlight[tid].remove(cIdx);
        m_retryCounts[peerKey].remove(cIdx);
        m_retryCounts[tid].remove(cIdx);
    }
    if (obj.contains("chunk_indexes")) {
        for (const auto& val : obj["chunk_indexes"].toArray()) {
            int cIdx = val.toInt();
            m_inFlight[peerKey].remove(cIdx);
            m_inFlight[tid].remove(cIdx);
            m_retryCounts[peerKey].remove(cIdx);
            m_retryCounts[tid].remove(cIdx);
        }
    }
    if (!obj.contains("chunk_index") && !obj.contains("chunk_indexes")) {
        m_inFlight[peerKey].clear();
        m_inFlight[tid].clear();
        m_retryCounts[peerKey].clear();
        m_retryCounts[tid].clear();
        QJsonObject fileStart;
        fileStart["type"] = "file_start";
        fileStart["transfer_id"] = tid;
        fileStart["file_name"] = m_transferFileNames.value(tid, QStringLiteral("attachment"));
        fileStart["file_size"] = tx.file_size;
        fileStart["chunk_count"] = tx.chunk_count;
        fileStart["chunk_size"] = tx.chunk_size;
        fileStart["total_hash"] = QString::fromLatin1(tx.file_hash.toBase64());
        fileStart["root_file_key"] = QString::fromLatin1(tx.root_file_key.toBase64());
        if (m_sendChatMsgCb) {
            m_sendChatMsgCb(fileStart, senderUser, senderDevice);
        }
    }
    m_activeUsers[tid] = senderUser;
    processNextSend(tid, senderDevice, senderUser);
}

void FileTransferManager::handleFileCancel(const QString& senderUser, const QString& senderDevice, const QJsonObject& obj) {
    Q_UNUSED(senderUser);
    QString tid = obj["transfer_id"].toString();
    if (!isValidTransferId(tid)) return;
    auto store = m_store.lock();
    if (store) {
        auto txRes = store->getFileTransfer(tid, senderDevice);
        if (txRes.success && txRes.data.has_value()) {
            const auto& tx = txRes.data.value();
            if (tx.status != "completed" && !tx.spool_path.isEmpty() &&
                isContainedInSpoolDir(tx.spool_path, m_spoolDir) && QFile::exists(tx.spool_path)) {
                QFile::remove(tx.spool_path);
            }
        }
        store->updateFileTransferStatus(tid, senderDevice, "cancelled");
    }
    QString recvSpool = m_spoolDir + "/" + tid + ".recv.spool";
    if (isContainedInSpoolDir(recvSpool, m_spoolDir) && QFile::exists(recvSpool)) {
        if (!store) {
            QFile::remove(recvSpool);
        } else {
            auto txRes = store->getFileTransferById(tid);
            if (!txRes.success || !txRes.data.has_value() || txRes.data->status != "completed") {
                QFile::remove(recvSpool);
            }
        }
    }
    const QString peerKey = tid + ":" + senderDevice;
    m_inFlight.remove(peerKey);
    m_inFlight.remove(tid);
    m_retryCounts.remove(peerKey);
    m_retryCounts.remove(tid);
    emit transferFailed(tid, "Transfer cancelled");
}

void FileTransferManager::cancelTransfer(const QString& transferId, const QString& peerDevice, const QString& reason) {
    if (!isValidTransferId(transferId)) return;
    auto store = m_store.lock();
    QString targetUser = m_activeUsers.value(transferId);
    if (!peerDevice.isEmpty()) {
        if (store) store->updateFileTransferStatus(transferId, peerDevice, "cancelled");
        const QString peerKey = transferId + ":" + peerDevice;
        m_inFlight.remove(peerKey);
        m_retryCounts.remove(peerKey);
        if (!targetUser.isEmpty() && m_sendChatMsgCb) {
            QJsonObject cancelObj;
            cancelObj["type"] = "file_cancel";
            cancelObj["transfer_id"] = transferId;
            cancelObj["reason"] = reason;
            m_sendChatMsgCb(cancelObj, targetUser, peerDevice);
        }
    } else if (store) {
        auto activeRes = store->getActiveFileTransfers();
        if (activeRes.success) {
            for (const auto& tx : activeRes.data.value()) {
                if (tx.transfer_id == transferId) {
                    store->updateFileTransferStatus(transferId, tx.peer_device_id, "cancelled");
                    const QString peerKey = transferId + ":" + tx.peer_device_id;
                    m_inFlight.remove(peerKey);
                    m_retryCounts.remove(peerKey);
                    if (!targetUser.isEmpty() && m_sendChatMsgCb) {
                        QJsonObject cancelObj;
                        cancelObj["type"] = "file_cancel";
                        cancelObj["transfer_id"] = transferId;
                        cancelObj["reason"] = reason;
                        m_sendChatMsgCb(cancelObj, targetUser, tx.peer_device_id);
                    }
                }
            }
        }
        auto singleRes = store->getFileTransferById(transferId);
        if (singleRes.success && singleRes.data.has_value() && singleRes.data->status == "active") {
            store->updateFileTransferStatus(transferId, singleRes.data->peer_device_id, "cancelled");
        }
    }
    m_inFlight.remove(transferId);
    m_retryCounts.remove(transferId);

    bool stillActive = false;
    if (store) {
        auto activeRes = store->getActiveFileTransfers();
        if (activeRes.success) {
            for (const auto& tx : activeRes.data.value()) {
                if (tx.transfer_id == transferId && tx.status != "cancelled") {
                    stillActive = true;
                    break;
                }
            }
        }
    }
    if (!stillActive) {
        if (store) {
            auto txRes = store->getFileTransferById(transferId);
            if (txRes.success && txRes.data.has_value()) {
                const auto& tx = txRes.data.value();
                if (tx.status != "completed") {
                    if (tx.is_sender) {
                        if (!tx.spool_path.isEmpty() && isContainedInSpoolDir(tx.spool_path, m_spoolDir) && QFile::exists(tx.spool_path)) {
                            QFile::remove(tx.spool_path);
                        }
                        QString sendSpool = m_spoolDir + "/" + transferId + ".spool";
                        if (isContainedInSpoolDir(sendSpool, m_spoolDir) && QFile::exists(sendSpool)) QFile::remove(sendSpool);
                    }
                }
            }
        }
    }
}

std::optional<Storage::E2EEFileTransfer> FileTransferManager::getTransferInfo(const QString& transferId) const {
    auto store = m_store.lock();
    if (!store || transferId.isEmpty() || !isValidTransferId(transferId)) return std::nullopt;
    auto res = store->getFileTransferById(transferId);
    if (res.success && res.data.has_value()) {
        return res.data.value();
    }
    return std::nullopt;
}

bool FileTransferManager::resumeIncomingTransfer(const QString& transferId, const QString& senderUser) {
    if (transferId.isEmpty() || !isValidTransferId(transferId)) return false;
    auto store = m_store.lock();
    QString peerUser = !senderUser.isEmpty() ? senderUser : m_activeUsers.value(transferId);
    if (!peerUser.isEmpty()) {
        m_activeUsers[transferId] = peerUser;
    }

    if (store) {
        auto txRes = store->getFileTransferById(transferId);
        if (txRes.success && txRes.data.has_value()) {
            auto tx = txRes.data.value();
            if (!tx.is_sender) {
                if (tx.status == "completed" && !tx.spool_path.isEmpty() && QFile::exists(tx.spool_path)) {
                    emit transferCompleted(transferId, tx.spool_path);
                    return true;
                }
                if (!tx.spool_path.isEmpty() && QFile::exists(tx.spool_path)) {
                    if (tx.status != "active") {
                        store->updateFileTransferStatus(transferId, tx.peer_device_id, "active");
                        tx.status = "active";
                    }
                    QJsonArray missing;
                    bool allReceived = true;
                    for (int i = 0; i < tx.chunk_count; ++i) {
                        int byteIdx = i / 8;
                        int bitIdx = i % 8;
                        bool hasChunk = (byteIdx < tx.received_bitset.size()) && ((tx.received_bitset[byteIdx] & (1 << bitIdx)) != 0);
                        if (!hasChunk) {
                            allReceived = false;
                            if (missing.size() < 5) {
                                missing.append(i);
                            }
                        }
                    }
                    if (allReceived && tx.chunk_count >= 0) {
                        reconstructFile(tx);
                        return true;
                    }
                    if (!peerUser.isEmpty() && m_sendChatMsgCb) {
                        QJsonObject nack;
                        nack["type"] = "file_nack";
                        nack["transfer_id"] = transferId;
                        nack["chunk_indexes"] = missing;
                        m_sendChatMsgCb(nack, peerUser, tx.peer_device_id);
                        return true;
                    }
                }
            }
        }
    }

    if (!peerUser.isEmpty() && m_sendChatMsgCb) {
        QJsonObject nack;
        nack["type"] = "file_nack";
        nack["transfer_id"] = transferId;
        m_sendChatMsgCb(nack, peerUser, QString());
        return true;
    }
    return false;
}

void FileTransferManager::onRetryTimeout() {
    auto store = m_store.lock();
    if (!store) return;

    auto txsRes = store->getActiveFileTransfers();
    if (!txsRes.success) return;
    for (const auto& tx : txsRes.data.value()) {
        if (!tx.is_sender) continue;
        QString recipientUser = m_activeUsers.value(tx.transfer_id);
        if (recipientUser.isEmpty()) continue;
        processNextSend(tx.transfer_id, tx.peer_device_id, recipientUser);
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
    const QString peerKey = transferId + ":" + peerDevice;
    auto& inFlight = m_inFlight[peerKey];
    
    qsizetype reqBitset = safeBitsetSize(tx.chunk_count);
    if (tx.received_bitset.size() < reqBitset) {
        int oldSize = tx.received_bitset.size();
        tx.received_bitset.resize(reqBitset);
        for(int i = oldSize; i < tx.received_bitset.size(); ++i) {
            tx.received_bitset[i] = '\0';
        }
    }

    for (auto it = inFlight.begin(); it != inFlight.end();) {
        int bIdx = it.key() / 8;
        if (bIdx < tx.received_bitset.size() && (tx.received_bitset[bIdx] & (1 << (it.key()%8))) != 0) {
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
        int bIdx = i / 8;
        if (bIdx >= tx.received_bitset.size() || (tx.received_bitset[bIdx] & (1 << (i%8))) == 0) {
            allAcked = false;
            if (inFlightCount < windowSize && !inFlight.contains(i)) {
                if (m_retryCounts[peerKey][i] > 5) {
                    store->updateFileTransferStatus(transferId, peerDevice, "failed");
                    m_inFlight.remove(peerKey);
                    bool otherDeviceActive = false;
                    bool anyCompleted = m_completedTransferIds.value(transferId, false);
                    auto activeRes = store->getActiveFileTransfers();
                    if (activeRes.success) {
                        for (const auto& otherTx : activeRes.data.value()) {
                            if (otherTx.transfer_id == transferId && otherTx.peer_device_id != peerDevice) {
                                if (otherTx.status == "active") {
                                    otherDeviceActive = true;
                                } else if (otherTx.status == "completed") {
                                    anyCompleted = true;
                                }
                            }
                        }
                    }
                    auto byIdRes = store->getFileTransferById(transferId);
                    if (byIdRes.success && byIdRes.data.has_value() && byIdRes.data->status == "completed") {
                        anyCompleted = true;
                    }
                    if (!otherDeviceActive && !anyCompleted) {
                        emit transferFailed(transferId, "Too many retries");
                    }
                    return;
                }
                m_retryCounts[peerKey][i]++;
                sendChunk(tx, i, recipientUser);
                inFlight[i] = now;
                inFlightCount++;
            }
        }
    }
    
    if (allAcked) {
        store->updateFileTransferStatus(transferId, peerDevice, "completed");
        m_completedTransferIds.insert(transferId, true);
        m_inFlight.remove(peerKey);
        m_retryCounts.remove(peerKey);
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

    QString senderDev = metadata.senderDeviceId.contains(':')
        ? metadata.senderDeviceId.section(':', 1)
        : metadata.senderDeviceId;
    auto txRes = store->getFileTransfer(tid, senderDev);
    if (!txRes.success && metadata.senderDeviceId.contains(':')) {
        txRes = store->getFileTransfer(tid, metadata.senderDeviceId);
    }
    if (!txRes.success) return VoidResult::fail("Transfer not found");

    auto tx = txRes.data.value();
    if (tx.is_sender) return VoidResult::fail("Sender cannot receive chunks");
    if (tx.status == "cancelled") return VoidResult::fail("Transfer not active");
    if (tx.chunk_count <= 0 || chunkEnv.chunkIndex >= static_cast<uint32_t>(tx.chunk_count)) return VoidResult::fail("Invalid chunk index");

    QString targetUser = metadata.senderUserId.isEmpty() ? m_activeUsers.value(tid) : metadata.senderUserId;
    if (targetUser.isEmpty() && metadata.senderDeviceId.contains(':')) {
        targetUser = metadata.senderDeviceId.section(':', 0, 0);
    }

    qint64 byteIdx = static_cast<qint64>(chunkEnv.chunkIndex) / 8;
    int bitIdx = static_cast<int>(chunkEnv.chunkIndex % 8);
    qsizetype reqBitset = safeBitsetSize(tx.chunk_count);
    if (tx.received_bitset.size() < reqBitset) {
        int oldSize = tx.received_bitset.size();
        tx.received_bitset.resize(reqBitset);
        for (int i = oldSize; i < tx.received_bitset.size(); ++i) {
            tx.received_bitset[i] = '\0';
        }
    }
    if (byteIdx >= tx.received_bitset.size()) {
        return VoidResult::fail("Bitset bounds violation");
    }
    if ((tx.received_bitset[byteIdx] & (1 << bitIdx)) != 0) {
        QJsonObject ack;
        ack["type"] = "file_ack";
        ack["transfer_id"] = tid;
        ack["chunk_index"] = static_cast<int>(chunkEnv.chunkIndex);
        m_sendChatMsgCb(ack, targetUser, tx.peer_device_id);
        return VoidResult::ok(std::monostate{});
    }
    if (tx.status != "active") return VoidResult::fail("Transfer not active");

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

    if (!isContainedInSpoolDir(tx.spool_path, m_spoolDir)) {
        return VoidResult::fail("Invalid spool path");
    }
    QFile spool(tx.spool_path);
    if (!spool.open(QIODevice::ReadWrite)) return VoidResult::fail("Failed to open spool");
    qint64 offset = static_cast<qint64>(chunkEnv.chunkIndex) * (static_cast<qint64>(tx.chunk_size) + 16);
    if (!spool.seek(offset)) return VoidResult::fail("Seek failed");
    
    if (writeData(spool, chunkEnv.ciphertext.constData(), chunkEnv.ciphertext.size(), QStringLiteral("receiver_spool_ciphertext")) != chunkEnv.ciphertext.size() ||
        writeData(spool, chunkEnv.tag.data.constData(), chunkEnv.tag.data.size(), QStringLiteral("receiver_spool_tag")) != chunkEnv.tag.data.size()) {
        spool.close();
        store->updateFileTransferStatus(tid, tx.peer_device_id, "failed");
        emit transferFailed(tid, "Spool write failed");
        return VoidResult::fail("Spool write failed");
    }
    if (!spool.flush()) {
        spool.close();
        store->updateFileTransferStatus(tid, tx.peer_device_id, "failed");
        emit transferFailed(tid, "Spool flush failed");
        return VoidResult::fail("Spool flush failed");
    }
#ifdef Q_OS_LINUX
    if (fdatasync(spool.handle()) != 0) {
        spool.close();
        store->updateFileTransferStatus(tid, tx.peer_device_id, "failed");
        emit transferFailed(tid, "Spool durability failed");
        return VoidResult::fail("Spool durability failed");
    }
#endif
    spool.close();

    if (byteIdx < tx.received_bitset.size()) {
        tx.received_bitset[byteIdx] = tx.received_bitset[byteIdx] | (1 << bitIdx);
    }
    auto updateRes = store->updateFileTransferBitset(tid, tx.peer_device_id, tx.received_bitset);
    if (!updateRes.success) {
        store->updateFileTransferStatus(tid, tx.peer_device_id, "failed");
        emit transferFailed(tid, "DB commit failed");
        return VoidResult::fail("DB commit failed");
    }

    QJsonObject ack;
    ack["type"] = "file_ack";
    ack["transfer_id"] = tid;
    ack["chunk_index"] = static_cast<int>(chunkEnv.chunkIndex);
    m_sendChatMsgCb(ack, targetUser, tx.peer_device_id);

    int receivedChunks = 0;
    bool done = true;
    for (int i = 0; i < tx.chunk_count; ++i) {
        int bIdx = i / 8;
        if (bIdx < tx.received_bitset.size() && (tx.received_bitset[bIdx] & (1 << (i%8))) != 0) {
            receivedChunks++;
        } else {
            done = false;
        }
    }
    int pct = (tx.chunk_count > 0) ? static_cast<int>((static_cast<qint64>(receivedChunks) * 100) / tx.chunk_count) : 100;
    emit transferProgress(tid, pct);
    if (done) reconstructFile(tx);

    return VoidResult::ok(std::monostate{});
}

void FileTransferManager::reconstructFile(const Storage::E2EEFileTransfer& tx) {
    bool isImageTransfer = false;
    if (m_isImageTransferCb) {
        isImageTransfer = m_isImageTransferCb(tx.transfer_id);
    }
    if (!isImageTransfer) {
        const QString fn = m_transferFileNames.value(tx.transfer_id).toLower();
        isImageTransfer = fn.endsWith(".jpg") || fn.endsWith(".jpeg") || fn.endsWith(".png") ||
                          fn.endsWith(".gif") || fn.endsWith(".webp") || fn.endsWith(".bmp");
    }

    QString sUrl = m_settings ? m_settings->serverUrl() : QString();
    QString sUser = m_settings ? m_settings->username() : QString();
    QString canonicalDir;
    if (!sUrl.isEmpty() && !sUser.isEmpty()) {
        canonicalDir = isImageTransfer
            ? NeoNect::Storage::StoragePathResolver::mediaImagesDirectory(sUrl, sUser)
            : NeoNect::Storage::StoragePathResolver::mediaAttachmentsDirectory(sUrl, sUser);
    } else {
        canonicalDir = m_spoolDir;
    }
    QDir().mkpath(canonicalDir);

    QString tmpOutPath = canonicalDir + "/" + tx.transfer_id + ".tmp." + QString::number(QRandomGenerator::global()->generate());
    QFile out(tmpOutPath);
    if (!out.open(QIODevice::WriteOnly)) {
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Failed to open output file");
        return;
    }
    
    QFile spool(tx.spool_path);
    if (!spool.open(QIODevice::ReadOnly)) {
        out.close();
        out.remove();
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Failed to read spool file");
        return;
    }

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
            auto store = m_store.lock();
            if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
            emit transferFailed(tx.transfer_id, "Chunk decryption failed during reconstruction");
            return;
        }
        if (writeData(out, decRes.value().constData(), decRes.value().size(), QStringLiteral("reconstruct_plaintext")) != decRes.value().size()) {
            out.close();
            out.remove();
            spool.close();
            auto store = m_store.lock();
            if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
            emit transferFailed(tx.transfer_id, "Output write failed");
            return;
        }
        fileHash.addData(decRes.value());
    }

    spool.close();

    if (!out.flush()) {
        out.close();
        out.remove();
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Output flush failed");
        return;
    }
#ifdef Q_OS_LINUX
    if (fdatasync(out.handle()) != 0) {
        out.close();
        out.remove();
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Output durability failed");
        return;
    }
#endif
    out.close();

    if (fileHash.result() != tx.file_hash) {
        QFile::remove(tmpOutPath);
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "File integrity check failed");
        return;
    }

    QString ext = "png";
    if (isImageTransfer) {
        QImageReader reader(tmpOutPath);
        QByteArray fmt = reader.format();
        if (!reader.canRead() || fmt.isEmpty()) {
            QFile::remove(tmpOutPath);
            auto store = m_store.lock();
            if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
            emit transferFailed(tx.transfer_id, "Invalid image format");
            return;
        }
        ext = QString::fromUtf8(fmt).toLower();
        if (ext.isEmpty()) ext = "png";
    }

    QString outPath;
    if (isImageTransfer) {
        outPath = canonicalDir + "/" + tx.transfer_id + "." + ext;
    } else {
        QString fn = m_transferFileNames.value(tx.transfer_id);
        outPath = canonicalDir + "/" + tx.transfer_id + (fn.isEmpty() ? ".final" : ("_" + QFileInfo(fn).fileName()));
    }

    if (!replaceFile(tmpOutPath, outPath)) {
        QFile::remove(tmpOutPath);
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Failed to finalize output file");
        return;
    }

    if (!syncDirectory(canonicalDir)) {
        auto store = m_store.lock();
        if (store) store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
        emit transferFailed(tx.transfer_id, "Directory synchronization failed");
        return;
    }

    auto store = m_store.lock();
    if (store) {
        Storage::E2EEFileTransfer updatedTx = tx;
        updatedTx.status = "completed";
        updatedTx.spool_path = outPath;
        auto saveRes = store->saveFileTransfer(updatedTx);
        if (!saveRes.success) {
            store->updateFileTransferStatus(tx.transfer_id, tx.peer_device_id, "failed");
            emit transferFailed(tx.transfer_id, "Failed to persist transfer completion");
            return;
        }
    } else {
        emit transferFailed(tx.transfer_id, "Store not available to complete transfer");
        return;
    }

    if (tx.spool_path != outPath && QFile::exists(tx.spool_path)) {
        QFile::remove(tx.spool_path);
    }

    emit transferCompleted(tx.transfer_id, outPath);
}

}
}
