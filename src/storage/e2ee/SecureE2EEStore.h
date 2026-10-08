#pragma once
#include "ISecureE2EEStore.h"
#include "IMasterKeyProvider.h"
#include <memory>
#include <mutex>

struct sqlite3;

namespace NeoNect {
namespace Storage {

class SecureE2EEStore : public ISecureE2EEStore {
public:
    explicit SecureE2EEStore(std::shared_ptr<IMasterKeyProvider> keyProvider);
    ~SecureE2EEStore() override;

    ServiceResult<std::monostate> initialize(const QString& dbPath, const QString& profileId) override;
    void close() override;
    ServiceResult<std::monostate> closeAndWipeDatabase() override;

    ServiceResult<std::monostate> saveIdentity(const E2EEIdentity& id) override;
    ServiceResult<E2EEIdentity> getIdentity() override;

    ServiceResult<std::monostate> saveSignedPreKey(const E2EESignedPreKey& spk) override;
    ServiceResult<E2EESignedPreKey> getSignedPreKey(qint64 key_id) override;
    ServiceResult<std::vector<E2EESignedPreKey>> getAllSignedPreKeys() override;

    ServiceResult<std::monostate> saveOneTimePreKeys(const std::vector<E2EEOneTimePreKey>& opks) override;
    ServiceResult<std::vector<E2EEOneTimePreKey>> getAvailableOneTimePreKeys() override;
    ServiceResult<std::monostate> consumeOneTimePreKeyAtomically(qint64 key_id) override;
    ServiceResult<E2EEOneTimePreKey> getOneTimePreKey(qint64 key_id) override;

    ServiceResult<std::monostate> saveSession(const E2EESession& session) override;
    ServiceResult<E2EESession> getSession(const QString& session_id) override;
    
    ServiceResult<std::monostate> updateSessionState(const SessionUpdateTx& tx) override;

    
    ServiceResult<std::monostate> saveFileTransfer(const E2EEFileTransfer& transfer) override;
    ServiceResult<E2EEFileTransfer> getFileTransfer(const QString& transfer_id, const QString& peer_device_id) override;
    ServiceResult<std::vector<E2EEFileTransfer>> getActiveFileTransfers() override;
    ServiceResult<std::monostate> updateFileTransferBitset(const QString& transfer_id, const QString& peer_device_id, const QByteArray& bitset) override;
    ServiceResult<std::monostate> updateFileTransferStatus(const QString& transfer_id, const QString& peer_device_id, const QString& status) override;

    ServiceResult<E2EESkippedKey> getSkippedKey(const QString& session_id, const QByteArray& remote_ratchet_public_key, qint64 message_number) override;

private:
    std::shared_ptr<IMasterKeyProvider> m_keyProvider;
    sqlite3* m_db{nullptr};
    std::mutex m_mutex;
    QString m_dbPath;

    ServiceResult<std::monostate> executeSql(const char* sql);
    ServiceResult<std::monostate> initializeSchema();
    bool checkSchemaVersion();
};

} // namespace Storage
} // namespace NeoNect
