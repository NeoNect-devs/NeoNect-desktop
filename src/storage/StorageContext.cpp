#include "StorageContext.h"
#include <QStandardPaths>
#include <QDir>
#include <iostream>

namespace NeoNect {
namespace Storage {

namespace {
class StorageContextMockSecretStore : public IOSSecretStore {
public:
    ServiceResult<std::monostate> writeSecret(const QString&, const QByteArray&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
    ServiceResult<QByteArray> readSecret(const QString&) override { return ServiceResult<QByteArray>::ok(QByteArray(32, 'M')); }
    ServiceResult<std::monostate> deleteSecret(const QString&) override { return ServiceResult<std::monostate>::ok(std::monostate{}); }
};
}

StorageContext::StorageContext(const QString& serverUrl, const QString& username, bool isMockMode, const QString& profile)
    : m_serverUrl(serverUrl), m_username(username)
{
    if (m_username.isEmpty()) {
        std::cerr << "CRITICAL: StorageContext created with empty username (unauthenticated state)." << std::endl;
        throw std::invalid_argument("StorageContext requires an authenticated username");
    }
    
    // Resolve Canonical Paths
    QString accountDir = StoragePathResolver::accountRoot(serverUrl, username);
    QDir().mkpath(accountDir);
    
    QString e2eeDir = StoragePathResolver::e2eeDirectory(serverUrl, username);
    QDir().mkpath(e2eeDir);
    
    // Instantiate E2EE Core
    if (isMockMode) {
        m_secretStore = std::make_shared<StorageContextMockSecretStore>();
    } else {
        m_secretStore = std::make_shared<PlatformSecretStore>();
    }
    m_keyProvider = std::make_shared<MasterKeyProvider>(m_secretStore);
    m_secureStore = std::make_shared<SecureE2EEStore>(m_keyProvider);
    
    QString secureDbPath = StoragePathResolver::e2eeDbPath(serverUrl, username);
    QString keyProfileId = profile.isEmpty() ? username : QString("%1_%2").arg(profile, username);
    m_secureStore->initialize(secureDbPath, keyProfileId);
    
    // Instantiate Message Storage
    QString msgDbPath = StoragePathResolver::messageDbPath(serverUrl, username);
    m_messageStorage = std::make_shared<Core::Messaging::SqliteMessageStorage>(msgDbPath);
    
    // Instantiate Message Queue using the SAME path
    m_messageQueue = std::make_shared<Core::Messaging::MessageQueue>(msgDbPath);
}

StorageContext::~StorageContext() {
    // RAII will tear down queue, then storage, then e2ee
    // Explicit wipes/closes to ensure handles are freed before directory manipulation
    if (m_secureStore) {
        m_secureStore->close();
    }
}

} // namespace Storage
} // namespace NeoNect
