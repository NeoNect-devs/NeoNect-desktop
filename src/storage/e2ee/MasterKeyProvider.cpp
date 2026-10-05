#include "MasterKeyProvider.h"
#include <openssl/rand.h>

namespace NeoNect {
namespace Storage {

const QString MasterKeyProvider::kMasterKeyName = "e2ee_master_key";

MasterKeyProvider::MasterKeyProvider(std::shared_ptr<IOSSecretStore> secretStore)
    : m_secretStore(std::move(secretStore)) {
}

MasterKeyProvider::~MasterKeyProvider() = default;

ServiceResult<QByteArray> MasterKeyProvider::loadOrCreate(const QString& profileId) {
    const QString scopedKeyName = kMasterKeyName + "_" + profileId;

    auto readResult = m_secretStore->readSecret(scopedKeyName);
    if (readResult.success) {
        if (readResult.data->size() == 32) {
            return ServiceResult<QByteArray>::ok(*readResult.data);
        } else {
            // Invalid size, ignore and replace
            m_secretStore->deleteSecret(scopedKeyName);
        }
    }

    QByteArray newKey;
    newKey.resize(32);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(newKey.data()), 32) != 1) {
        return ServiceResult<QByteArray>::fail("Failed to generate secure random master key");
    }

    auto writeResult = m_secretStore->writeSecret(scopedKeyName, newKey);
    if (!writeResult.success) {
        // Zeroize memory
        newKey.fill(0);
        return ServiceResult<QByteArray>::fail("Failed to persist new master key: " + writeResult.message);
    }

    return ServiceResult<QByteArray>::ok(newKey);
}

ServiceResult<std::monostate> MasterKeyProvider::remove(const QString& profileId) {
    const QString scopedKeyName = kMasterKeyName + "_" + profileId;
    auto deleteResult = m_secretStore->deleteSecret(scopedKeyName);
    if (!deleteResult.success) {
        return ServiceResult<std::monostate>::fail("Failed to delete master key: " + deleteResult.message);
    }
    return ServiceResult<std::monostate>::ok(std::monostate{});
}

} // namespace Storage
} // namespace NeoNect
