#pragma once
#include "IMasterKeyProvider.h"
#include "IOSSecretStore.h"
#include <memory>
#include <QString>

namespace NeoNect {
namespace Storage {

class MasterKeyProvider : public IMasterKeyProvider {
public:
    explicit MasterKeyProvider(std::shared_ptr<IOSSecretStore> secretStore);
    ~MasterKeyProvider() override;

    ServiceResult<QByteArray> loadOrCreate() override;

private:
    std::shared_ptr<IOSSecretStore> m_secretStore;
    static const QString kMasterKeyName;
};

} // namespace Storage
} // namespace NeoNect
