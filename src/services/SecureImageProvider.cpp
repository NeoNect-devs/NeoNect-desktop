#include <QVariant>
#include "SecureImageProvider.h"
#include <QImage>
#include <QFile>
#include <QCryptographicHash>
#include <QUuid>

namespace NeoNect {
namespace Services {

namespace {

QByteArray transferIdToBinary(const QString& tid) {
    return QUuid("{" + tid + "}").toRfc4122();
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
        std::shared_ptr<Storage::ISettingsRepository> settings)
        : m_id(id), m_reqSize(requestedSize), m_store(store), m_crypto(crypto), m_settings(settings)
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
        auto store = m_store.lock();
        if (!store) { finishTask("error"); return; }

        QString localDeviceId = m_settings->deviceId();
        // Since we are the recipient, the sender passed our device ID.
                auto txRes = store->getFileTransferById(m_id);
        if (!txRes.success) { finishTask("error"); return; }
        auto tx = txRes.data.value();

        if (tx.status != "completed") { finishTask("error"); return; }
        if (tx.file_size < 0 || tx.chunk_size <= 0 || tx.chunk_count <= 0) { finishTask("error"); return; }
        if (tx.received_bitset.size() < (tx.chunk_count + 7) / 8) { finishTask("error"); return; }

        for (int i = 0; i < tx.chunk_count; ++i) {
            if ((tx.received_bitset[i / 8] & (1 << (i % 8))) == 0) {
                finishTask("error"); return;
            }
        }

        QFile spool(tx.spool_path);
        if (!spool.open(QIODevice::ReadOnly)) { finishTask("error"); return; }

        QByteArray deviceKey = deriveRecipientDeviceKey(m_crypto, tx.root_file_key, transferIdToBinary(tx.transfer_id), localDeviceId);
        Crypto::AeadKey aDeviceKey = toAeadKey(deviceKey);

        QByteArray imageData;
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
            Crypto::AeadNonce aNonce{generateNonce(i, 1)};

            auto decRes = m_crypto->AeadDecrypt(aDeviceKey, aNonce, ct, tag, transferIdToBinary(tx.transfer_id));
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
    auto *response = new SecureImageResponse(id, requestedSize, m_store, m_crypto, m_settings);
    m_pool.start(response);
    return response;
}

} // namespace Services
} // namespace NeoNect
