#pragma once
#include <QByteArray>
#include "common/types.h"

namespace NeoNect {
namespace Storage {

class IMasterKeyProvider {
public:
    virtual ~IMasterKeyProvider() = default;

    virtual ServiceResult<QByteArray> loadOrCreate() = 0;
};

} // namespace Storage
} // namespace NeoNect
