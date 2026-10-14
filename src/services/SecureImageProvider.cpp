#include <QVariant>
#include "SecureImageProvider.h"
#include <QImage>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QUuid>
#include <QCache>
#include <QMutex>
#include "../storage/StoragePathResolver.h"

namespace NeoNect {
namespace Services {

namespace {

QByteArray transferIdToBinary(const QString& tid) {
    return QUuid("{" + tid + "}").toRfc4122();
}

QByteArray deriveSenderSpoolKey(const std::shared_ptr<Crypto::ICryptoBackend>& crypto, const QByteArray& rootKey, const QByteArray& transferId, const QString& senderDeviceId) {
    QByteArray info = "sender_spool_" + senderDeviceId.toUtf8();
    auto res = crypto->HkdfSha256(rootKey, transferId, info, 32);
    return res.isEmpty() ? QByteArray() : res;
}

QByteArray deriveRecipientDeviceKey(const std::shared_ptr<Crypto::ICryptoBackend>& crypto, const QByteArray& rootKey, const QByteArray& transferId, const QString& recipientDeviceId) {
    QByteArray info = recipientDeviceId.toUtf8();
    auto res = crypto->HkdfSha256(rootKey, transferId, info, 32);
    return res.isEmpty() ? QByteArray() : res;
}

static Crypto::SecureBuffer toSecureBuffer(const QByteArray& arr) {
    Crypto::SecureBuffer sb(arr.size());
    if (arr.size() > 0) {
        memcpy(sb.data(), arr.constData(), arr.size());
    }
    return sb;
}

Crypto::AeadKey toAeadKey(const QByteArray& bin) {
    return Crypto::AeadKey{toSecureBuffer(bin)};
}

QByteArray generateNonce(uint32_t chunkIndex, uint8_t domain) {
    QByteArray n(12, '\0');
    n[0] = domain;
    n[8]  = (chunkIndex >> 24) & 0xFF;
    n[9]  = (chunkIndex >> 16) & 0xFF;
    n[10] = (chunkIndex >> 8)  & 0xFF;
    n[11] = chunkIndex & 0xFF;
    return n;
}

class SecureImageResponse : public QQuickImageResponse, public QRunnable {
public:
    SecureImageResponse(
        const QString &id,
        const QSize &requestedSize,
        std::weak_ptr<Storage::ISecureE2EEStore> store,
        std::shared_ptr<Crypto::ICryptoBackend> crypto,
        std::shared_ptr<Storage::ISettingsRepository> settings,
        SecureImageProvider *provider)
        : m_id(id), m_reqSize(requestedSize), m_store(store), m_crypto(crypto), m_settings(settings), m_provider(provider)
    {
        setAutoDelete(false);
    }

    QQuickTextureFactory *textureFactory() const override {
        if (m_image.isNull()) return nullptr; return QQuickTextureFactory::textureFactoryForImage(m_image);
    }


    void finishTask(const QString& err) {
        m_errorString = err;
        QMetaObject::invokeMethod(this, [this]() {
            setProperty("decodedImage", QVariant::fromValue(m_image));
            emit finished();
        }, Qt::QueuedConnection);
    }

    void run() override {
        QString cleanId = m_id.section('?', 0, 0).section('#', 0, 0).trimmed();
        if (cleanId.isEmpty()) { finishTask("error"); return; }

        if (m_provider) {
            QImage cached = m_provider->getCachedImage(cleanId);
            if (!cached.isNull()) {
                m_image = cached;
                finishTask("");
                return;
            }
        }

        QString sUrl = m_settings ? m_settings->serverUrl() : QString();
        QString sUser = m_settings ? m_settings->username() : QString();

        if (!sUrl.isEmpty() && !sUser.isEmpty()) {
            QString imagesDir = NeoNect::Storage::StoragePathResolver::mediaImagesDirectory(sUrl, sUser);
            QStringList exts = {"png", "jpg", "jpeg", "webp", "gif", "bmp"};
            for (const auto& ext : exts) {
                QString p = imagesDir + "/" + cleanId + "." + ext;
                if (QFile::exists(p)) {
                    QFile imgFile(p);
                    if (imgFile.open(QIODevice::ReadOnly)) {
                        QByteArray raw = imgFile.readAll();
                        if (m_image.loadFromData(raw)) {
                            if (m_provider) m_provider->setCachedImage(cleanId, m_image);
                            finishTask("");
                            return;
                        }
                    }
                }
            }
        }

        if (QFile::exists(cleanId)) {
            QFile directFile(cleanId);
            if (directFile.open(QIODevice::ReadOnly)) {
                QByteArray raw = directFile.readAll();
                if (m_image.loadFromData(raw)) {
                    if (m_provider) m_provider->setCachedImage(cleanId, m_image);
                    finishTask("");
                    return;
                }
            }
        }

        auto store = m_store.lock();
        if (!store) { finishTask("error"); return; }

        QString localDeviceId = m_settings ? m_settings->deviceId() : QString();
        auto txRes = store->getFileTransferById(cleanId);
        if (!txRes.success || !txRes.data.has_value()) { finishTask("error"); return; }
        auto tx = txRes.data.value();

        if (tx.file_size <= 0 || tx.chunk_size <= 0 || tx.chunk_count <= 0) { finishTask("error"); return; }
        if (!tx.is_sender) {
            qsizetype reqBitset = static_cast<qsizetype>((static_cast<qint64>(tx.chunk_count) + 7) / 8);
            if (tx.received_bitset.size() < reqBitset) { finishTask("error"); return; }
            for (int i = 0; i < tx.chunk_count; ++i) {
                int bIdx = i / 8;
                if (bIdx >= tx.received_bitset.size() || (tx.received_bitset[bIdx] & (1 << (i % 8))) == 0) {
                    finishTask("error"); return;
                }
            }
        }

        QFile spool(tx.spool_path);
        if (!spool.open(QIODevice::ReadOnly)) { finishTask("error"); return; }

        if (!tx.spool_path.endsWith(".spool")) {
            QByteArray rawData = spool.readAll();
            QCryptographicHash rawHash(QCryptographicHash::Sha256);
            rawHash.addData(rawData);
            if (rawHash.result() == tx.file_hash) {
                if (m_image.loadFromData(rawData)) {
                    if (m_provider) m_provider->setCachedImage(cleanId, m_image);
                    finishTask("");
                } else {
                    m_image = QImage();
                    finishTask("decode error");
                }
                return;
            }
            spool.seek(0);
        }

        QByteArray transferIdBin = transferIdToBinary(tx.transfer_id);
        QByteArray keyBytes = tx.is_sender
            ? deriveSenderSpoolKey(m_crypto, tx.root_file_key, transferIdBin, localDeviceId)
            : deriveRecipientDeviceKey(m_crypto, tx.root_file_key, transferIdBin, localDeviceId);
        uint8_t nonceDomain = tx.is_sender ? 0 : 1;
        Crypto::AeadKey aDeviceKey = toAeadKey(keyBytes);

        QByteArray imageData;
        imageData.reserve(static_cast<qsizetype>(tx.file_size));
        QCryptographicHash fileHash(QCryptographicHash::Sha256);

        for (int i = 0; i < tx.chunk_count; ++i) {
            qint64 offset = static_cast<qint64>(i) * (static_cast<qint64>(tx.chunk_size) + 16);
            if (!spool.seek(offset)) {
                finishTask("error"); return;
            }

            qint32 ptSize = tx.chunk_size;
            if (i == tx.chunk_count - 1) {
                qint32 rem = tx.file_size % tx.chunk_size;
                if (rem != 0) ptSize = rem;
            }

            if (spool.bytesAvailable() < ptSize + 16) {
                finishTask("error"); return;
            }

            QByteArray ct = spool.read(ptSize);
            Crypto::AeadTag tag{spool.read(16)};
            Crypto::AeadNonce aNonce{generateNonce(i, nonceDomain)};

            auto decRes = m_crypto->AeadDecrypt(aDeviceKey, aNonce, ct, tag, transferIdBin);
            if (!decRes.has_value()) {
                finishTask("error"); return;
            }
            imageData.append(decRes.value());
            fileHash.addData(decRes.value());
        }

        if (fileHash.result() != tx.file_hash) {
            finishTask("error"); return;
        }

        if (m_image.loadFromData(imageData)) {
            if (!sUrl.isEmpty() && !sUser.isEmpty()) {
                QString imagesDir = NeoNect::Storage::StoragePathResolver::mediaImagesDirectory(sUrl, sUser);
                QDir().mkpath(imagesDir);
                QString finalPath = imagesDir + "/" + cleanId + ".png";
                QString tmpFinal = finalPath + ".tmp." + QString::number(QRandomGenerator::global()->generate());
                if (m_image.save(tmpFinal, "PNG")) {
                    if (QFile::exists(finalPath)) QFile::remove(finalPath);
                    if (QFile::rename(tmpFinal, finalPath)) {
                        QString oldSpool = tx.spool_path;
                        tx.spool_path = finalPath;
                        store->saveFileTransfer(tx);
                        if (oldSpool != finalPath && QFile::exists(oldSpool)) {
                            QFile::remove(oldSpool);
                        }
                    } else {
                        QFile::remove(tmpFinal);
                    }
                }
            }
            if (m_provider) m_provider->setCachedImage(cleanId, m_image);
            finishTask("");
        } else {
            m_image = QImage();
            finishTask("decode error");
        }
    }

    QString m_id;
    QSize m_reqSize;
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
    std::shared_ptr<Crypto::ICryptoBackend> m_crypto;
    std::shared_ptr<Storage::ISettingsRepository> m_settings;
    SecureImageProvider *m_provider{nullptr};
    QImage m_image;
    QString m_errorString;
    QString errorString() const override { return m_errorString; }
};
}

SecureImageProvider::SecureImageProvider(
    std::weak_ptr<Storage::ISecureE2EEStore> store,
    std::shared_ptr<Crypto::ICryptoBackend> crypto,
    std::shared_ptr<Storage::ISettingsRepository> settings)
    : m_store(store), m_crypto(crypto), m_settings(settings)
{
}

QQuickImageResponse *SecureImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize) {
    auto *response = new SecureImageResponse(id, requestedSize, m_store, m_crypto, m_settings, this);
    m_pool.start(response);
    return response;
}

QString SecureImageProvider::makeCacheKey(const QString &id) const {
    if (!m_settings) return id;
    QString dev = m_settings->deviceId();
    QString user = m_settings->username();
    return dev + ":" + user + ":" + id;
}

QImage SecureImageProvider::getCachedImage(const QString &id) const {
    QMutexLocker locker(&m_cacheMutex);
    QImage *img = m_cache.object(makeCacheKey(id));
    return img ? *img : QImage();
}

void SecureImageProvider::setCachedImage(const QString &id, const QImage &image) {
    if (image.isNull()) return;
    QMutexLocker locker(&m_cacheMutex);
    if (m_cache.maxCost() == 0) {
        m_cache.setMaxCost(64);
    }
    m_cache.insert(makeCacheKey(id), new QImage(image));
}

void SecureImageProvider::clearCache() {
    QMutexLocker locker(&m_cacheMutex);
    m_cache.clear();
}

} // namespace Services
} // namespace NeoNect
