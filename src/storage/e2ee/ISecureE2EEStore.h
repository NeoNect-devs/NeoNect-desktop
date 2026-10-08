#pragma once
#include <QString>
#include <QByteArray>
#include <vector>
#include <optional>
#include "common/types.h"

namespace NeoNect {
namespace Storage {

enum class OPKState {
    AVAILABLE = 0,
    CONSUMED = 1,
    RETIRED = 2
};

struct E2EEIdentity {
    qint64 identity_id;
    QByteArray public_key;
    QByteArray private_key;
    qint64 created_at;
    qint64 version;
};

struct E2EESignedPreKey {
    qint64 key_id;
    QByteArray public_key;
    QByteArray private_key;
    QByteArray signature;
    qint64 created_at;
    qint32 status;
};

struct E2EEOneTimePreKey {
    qint64 key_id;
    QByteArray public_key;
    QByteArray private_key;
    OPKState state;
    qint64 created_at;
    qint64 consumed_at;
};


struct E2EEFileTransfer {
    QString transfer_id;
    QString peer_device_id;
    int is_sender;
    qint64 file_size;
    qint32 chunk_size;
    qint32 max_envelope_bytes;
    qint32 max_http_body_bytes;
    qint32 chunk_count;
    QByteArray file_hash;
    QString spool_path;
    QByteArray root_file_key;
    QByteArray received_bitset;
    QString status;
    qint64 created_at;
};

struct E2EESession {
    QString session_id;
    QByteArray remote_identity_key;
    qint64 local_identity_id;
    QByteArray DHs;
    QByteArray DHr;
    QByteArray RK;
    QByteArray CKs;
    QByteArray CKr;
    qint64 Ns;
    qint64 Nr;
    qint64 PN;
    qint64 created_at;
    qint64 updated_at;
    qint64 version;
};

struct E2EESkippedKey {
    QString session_id;
    QByteArray remote_ratchet_public_key;
    qint64 message_number;
    QByteArray message_key;
    qint64 created_at;
};

struct SessionUpdateTx {
    E2EESession session;
    std::vector<E2EESkippedKey> new_skipped_keys;
    std::vector<E2EESkippedKey> deleted_skipped_keys;
};

class ISecureE2EEStore {
public:
    virtual ~ISecureE2EEStore() = default;

    virtual ServiceResult<std::monostate> initialize(const QString& dbPath, const QString& profileId) = 0;
    virtual void close() = 0;
    virtual ServiceResult<std::monostate> closeAndWipeDatabase() = 0;

    virtual ServiceResult<std::monostate> saveIdentity(const E2EEIdentity& id) = 0;
    virtual ServiceResult<E2EEIdentity> getIdentity() = 0;

    virtual ServiceResult<std::monostate> saveSignedPreKey(const E2EESignedPreKey& spk) = 0;
    virtual ServiceResult<E2EESignedPreKey> getSignedPreKey(qint64 key_id) = 0;
    virtual ServiceResult<std::vector<E2EESignedPreKey>> getAllSignedPreKeys() = 0;

    virtual ServiceResult<std::monostate> saveOneTimePreKeys(const std::vector<E2EEOneTimePreKey>& opks) = 0;
    virtual ServiceResult<std::vector<E2EEOneTimePreKey>> getAvailableOneTimePreKeys() = 0;
    virtual ServiceResult<std::monostate> consumeOneTimePreKeyAtomically(qint64 key_id) = 0;
    virtual ServiceResult<E2EEOneTimePreKey> getOneTimePreKey(qint64 key_id) = 0;

    virtual ServiceResult<std::monostate> saveSession(const E2EESession& session) = 0;
    virtual ServiceResult<E2EESession> getSession(const QString& session_id) = 0;
    
    virtual ServiceResult<std::monostate> updateSessionState(const SessionUpdateTx& tx) = 0;

    
    virtual ServiceResult<std::monostate> saveFileTransfer(const E2EEFileTransfer& transfer) = 0;
    virtual ServiceResult<E2EEFileTransfer> getFileTransfer(const QString& transfer_id, const QString& peer_device_id) = 0;
    virtual ServiceResult<std::vector<E2EEFileTransfer>> getActiveFileTransfers() = 0;
    virtual ServiceResult<std::monostate> updateFileTransferBitset(const QString& transfer_id, const QString& peer_device_id, const QByteArray& bitset) = 0;
    virtual ServiceResult<std::monostate> updateFileTransferStatus(const QString& transfer_id, const QString& peer_device_id, const QString& status) = 0;

    virtual ServiceResult<E2EESkippedKey> getSkippedKey(const QString& session_id, const QByteArray& remote_ratchet_public_key, qint64 message_number) = 0;
};

} // namespace Storage
} // namespace NeoNect
