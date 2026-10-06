#include "MasterKeyProvider.h"
#include <openssl/rand.h>

namespace NeoNect {
namespace Storage {

const QString MasterKeyProvider::kMasterKeyName = "e2ee_master_key";

MasterKeyProvider::MasterKeyProvider(
    std::shared_ptr<IOSSecretStore> secretStore,
    std::shared_ptr<IOSSecretStore> legacySecretStore,
    const QString& legacyProfileId)
    : m_secretStore(std::move(secretStore))
    , m_legacySecretStore(std::move(legacySecretStore))
    , m_legacyProfileId(legacyProfileId) {
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

    if (m_legacySecretStore && !m_legacyProfileId.isEmpty()) {
        const QString legacyKeyName = kMasterKeyName + "_" + m_legacyProfileId;
        auto legacyRead = m_legacySecretStore->readSecret(legacyKeyName);
        if (legacyRead.success) {
            if (legacyRead.data->size() != 32) {
                return ServiceResult<QByteArray>::fail("Legacy key is corrupted (invalid size)");
            }

            auto writeRes = m_secretStore->writeSecret(scopedKeyName, *legacyRead.data);
            if (!writeRes.success) {
                return ServiceResult<QByteArray>::fail("Failed to write legacy key to new store: " + writeRes.message);
            }

            auto readBackRes = m_secretStore->readSecret(scopedKeyName);
            if (!readBackRes.success || readBackRes.data->size() != 32 || *readBackRes.data != *legacyRead.data) {
                m_secretStore->deleteSecret(scopedKeyName);
                return ServiceResult<QByteArray>::fail("Failed to verify migrated key via read-back");
            }

            m_legacySecretStore->deleteSecret(legacyKeyName);
            return ServiceResult<QByteArray>::ok(*legacyRead.data);
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

    if (m_legacySecretStore && !m_legacyProfileId.isEmpty()) {
        const QString legacyKeyName = kMasterKeyName + "_" + m_legacyProfileId;
        m_legacySecretStore->deleteSecret(legacyKeyName);
    }

    if (!deleteResult.success) {
        return ServiceResult<std::monostate>::fail("Failed to delete master key: " + deleteResult.message);
    }
    return ServiceResult<std::monostate>::ok(std::monostate{});
}

} // namespace Storage
} // namespace NeoNect
