#pragma once
#include "IOSSecretStore.h"

namespace NeoNect {
namespace Storage {

class PlatformSecretStore : public IOSSecretStore {
public:
    explicit PlatformSecretStore(const QString& baseDir = QString());
    ~PlatformSecretStore() override;

    ServiceResult<QByteArray> readSecret(const QString& name) override;
    ServiceResult<std::monostate> writeSecret(const QString& name, const QByteArray& secret) override;
    ServiceResult<std::monostate> deleteSecret(const QString& name) override;

private:
    QString m_baseDir;
    QString getSecretFilePath(const QString& name) const;
};

} // namespace Storage
} // namespace NeoNect
