#pragma once
#include "IOSSecretStore.h"

namespace NeoNect {
namespace Storage {

class PlatformSecretStore : public IOSSecretStore {
public:
    PlatformSecretStore();
    ~PlatformSecretStore() override;

    ServiceResult<QByteArray> readSecret(const QString& name) override;
    ServiceResult<std::monostate> writeSecret(const QString& name, const QByteArray& secret) override;
    ServiceResult<std::monostate> deleteSecret(const QString& name) override;

private:
    QString getSecretFilePath(const QString& name) const;
};

} // namespace Storage
} // namespace NeoNect
