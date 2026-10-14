#pragma once
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <QRunnable>
#include <QCache>
#include <QMutex>
#include <memory>
#include "../storage/e2ee/ISecureE2EEStore.h"
#include "../crypto/ICryptoBackend.h"
#include "../storage/isettingsrepository.h"

namespace NeoNect {
namespace Services {

class SecureImageProvider : public QQuickAsyncImageProvider {
public:
    SecureImageProvider(
        std::weak_ptr<Storage::ISecureE2EEStore> store,
        std::shared_ptr<Crypto::ICryptoBackend> crypto,
        std::shared_ptr<Storage::ISettingsRepository> settings);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    void setStore(std::weak_ptr<Storage::ISecureE2EEStore> store) {
        m_store = store;
        clearCache();
    }

    QImage getCachedImage(const QString &id) const;
    void setCachedImage(const QString &id, const QImage &image);
    void clearCache();
    QString makeCacheKey(const QString &id) const;

private:
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
    std::shared_ptr<Crypto::ICryptoBackend> m_crypto;
    std::shared_ptr<Storage::ISettingsRepository> m_settings;
    QThreadPool m_pool;
    mutable QMutex m_cacheMutex;
    QCache<QString, QImage> m_cache;
};

} // namespace Services
} // namespace NeoNect
