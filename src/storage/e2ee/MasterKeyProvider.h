#pragma once
#include "IMasterKeyProvider.h"
#include "IOSSecretStore.h"
#include <memory>
#include <QString>

namespace NeoNect {
namespace Storage {

class MasterKeyProvider : public IMasterKeyProvider {
public:
    explicit MasterKeyProvider(
        std::shared_ptr<IOSSecretStore> secretStore,
        std::shared_ptr<IOSSecretStore> legacySecretStore = nullptr,
        const QString& legacyProfileId = QString());
    ~MasterKeyProvider() override;

    ServiceResult<QByteArray> loadOrCreate(const QString& profileId) override;
    ServiceResult<std::monostate> remove(const QString& profileId) override;

private:
    std::shared_ptr<IOSSecretStore> m_secretStore;
    std::shared_ptr<IOSSecretStore> m_legacySecretStore;
    QString m_legacyProfileId;
    static const QString kMasterKeyName;
};

} // namespace Storage
} // namespace NeoNect
