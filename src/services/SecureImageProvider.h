#pragma once
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <QRunnable>
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
    void setStore(std::weak_ptr<Storage::ISecureE2EEStore> store) { m_store = store; }

private:
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
    std::shared_ptr<Crypto::ICryptoBackend> m_crypto;
    std::shared_ptr<Storage::ISettingsRepository> m_settings;
    QThreadPool m_pool;
};

} // namespace Services
} // namespace NeoNect
