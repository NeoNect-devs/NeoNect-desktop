#pragma once
#include <QString>
#include <QByteArray>
#include "common/types.h"

namespace NeoNect {
namespace Storage {

class IOSSecretStore {
public:
    virtual ~IOSSecretStore() = default;
    
    virtual ServiceResult<QByteArray> readSecret(const QString& name) = 0;
    virtual ServiceResult<std::monostate> writeSecret(const QString& name, const QByteArray& secret) = 0;
    virtual ServiceResult<std::monostate> deleteSecret(const QString& name) = 0;
};

} // namespace Storage
} // namespace NeoNect
