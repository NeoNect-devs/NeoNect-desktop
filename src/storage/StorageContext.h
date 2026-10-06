#pragma once

#include <QString>
#include <memory>
#include <QDir>
#include "StoragePathResolver.h"
#include "e2ee/SecureE2EEStore.h"
#include "e2ee/MasterKeyProvider.h"
#include "e2ee/PlatformSecretStore.h"
#include "e2ee/IOSSecretStore.h"
#include "../core/messaging/MessageStorage.h"
#include "../core/messaging/OfflineQueue.h"

namespace NeoNect {
namespace Storage {

class StorageContext {
public:
    /**
     * @brief Creates an authenticated user-scoped storage context.
     * @param serverUrl Canonical server URL
     * @param username Authenticated canonical username
     * @param isMockMode True if running in mock/test mode
     * @param profile Client profile (optional)
     *
     * Pre-auth rule: MUST NOT be called if username is empty.
     */
    StorageContext(const QString& serverUrl, const QString& username, bool isMockMode, const QString& profile = QString());
    ~StorageContext();

    StorageContext(const StorageContext&) = delete;
    StorageContext& operator=(const StorageContext&) = delete;

    std::weak_ptr<SecureE2EEStore> secureStore() const { return m_secureStore; }
    std::weak_ptr<Core::Messaging::IMessageStorage> messageStorage() const { return m_messageStorage; }
    std::weak_ptr<Core::Messaging::MessageQueue> messageQueue() const { return m_messageQueue; }
    QString serverUrl() const { return m_serverUrl; }
    QString username() const { return m_username; }

private:
    QString m_serverUrl;
    QString m_username;
    
    std::shared_ptr<IOSSecretStore> m_secretStore;
    std::shared_ptr<MasterKeyProvider> m_keyProvider;
    std::shared_ptr<SecureE2EEStore> m_secureStore;
    std::shared_ptr<Core::Messaging::IMessageStorage> m_messageStorage;
    std::shared_ptr<Core::Messaging::MessageQueue> m_messageQueue;
};

} // namespace Storage
} // namespace NeoNect
