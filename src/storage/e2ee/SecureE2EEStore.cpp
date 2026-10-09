#include "SecureE2EEStore.h"
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include "../../../third_party/sqlcipher/sqlite3.h"

namespace NeoNect {
namespace Storage {

SecureE2EEStore::SecureE2EEStore(std::shared_ptr<IMasterKeyProvider> keyProvider)
    : m_keyProvider(std::move(keyProvider)) {}

SecureE2EEStore::~SecureE2EEStore() {
    close();
}

ServiceResult<std::monostate> SecureE2EEStore::executeSql(const char* sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        QString err = QString::fromUtf8(errMsg ? errMsg : "Unknown SQL error");
        sqlite3_free(errMsg);
        return NeoNect::ServiceResult<std::monostate>::fail(err);
    }
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<std::monostate> SecureE2EEStore::initialize(const QString& dbPath, const QString& profileId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_dbPath = dbPath;

    auto keyResult = m_keyProvider->loadOrCreate(profileId);
    if (!keyResult.success) {
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to obtain master key: " + keyResult.message);
    }
    QByteArray masterKey = *keyResult.data;

    bool isNewDb = !QFile::exists(dbPath);

    if (sqlite3_open_v2(dbPath.toUtf8().constData(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        masterKey.fill(0);
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to open encrypted database file");
    }

    if (sqlite3_key(m_db, masterKey.constData(), masterKey.size()) != SQLITE_OK) {
        masterKey.fill(0);
        close();
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to set master key");
    }
    masterKey.fill(0);

    sqlite3_busy_timeout(m_db, 5000);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, "SELECT count(*) FROM sqlite_master;", nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        QString err = QString::fromUtf8(errMsg ? errMsg : "Authentication failed");
        sqlite3_free(errMsg);
        close();
        return NeoNect::ServiceResult<std::monostate>::fail("Incorrect master key or corrupted database: " + err);
    }

    executeSql("PRAGMA journal_mode = WAL;");
    executeSql("PRAGMA synchronous = NORMAL;");
    executeSql("PRAGMA foreign_keys = ON;");

    if (isNewDb || !checkSchemaVersion()) {
        auto schemaResult = initializeSchema();
        if (!schemaResult.success) {
            close();
            return schemaResult;
        }
    } else {
        // Apply migrations for existing databases to support effective capability snapshots
        executeSql("ALTER TABLE file_transfers ADD COLUMN max_envelope_bytes INTEGER NOT NULL DEFAULT 0;");
        executeSql("ALTER TABLE file_transfers ADD COLUMN max_http_body_bytes INTEGER NOT NULL DEFAULT 0;");
    }

    return NeoNect::ServiceResult<std::monostate>::ok({});
}

void SecureE2EEStore::close() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

ServiceResult<std::monostate> SecureE2EEStore::closeAndWipeDatabase() {
    std::lock_guard<std::mutex> lock(m_mutex);
    close();
    
    if (!m_dbPath.isEmpty() && QFile::exists(m_dbPath)) {
        if (!QFile::remove(m_dbPath)) {
            return NeoNect::ServiceResult<std::monostate>::fail("Failed to remove database file: " + m_dbPath);
        }
    }
    
    // Also remove WAL and SHM files if they exist
    if (!m_dbPath.isEmpty()) {
        QString walPath = m_dbPath + "-wal";
        QString shmPath = m_dbPath + "-shm";
        QFile::remove(walPath);
        QFile::remove(shmPath);
    }
    
    m_dbPath.clear();
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

bool SecureE2EEStore::checkSchemaVersion() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT version FROM schema_version LIMIT 1", -1, &stmt, nullptr) == SQLITE_OK) {
        bool hasSchema = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        return hasSchema;
    }
    return false;
}

ServiceResult<std::monostate> SecureE2EEStore::initializeSchema() {
    const char* schemaSql = R"(
        BEGIN EXCLUSIVE TRANSACTION;
        CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY);
        INSERT OR IGNORE INTO schema_version (version) VALUES (1);

        CREATE TABLE IF NOT EXISTS identity (
            identity_id INTEGER PRIMARY KEY,
            public_key BLOB NOT NULL,
            private_key BLOB NOT NULL,
            created_at INTEGER NOT NULL,
            version INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS signed_prekeys (
            key_id INTEGER PRIMARY KEY,
            public_key BLOB NOT NULL,
            private_key BLOB NOT NULL,
            signature BLOB NOT NULL,
            created_at INTEGER NOT NULL,
            status INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS one_time_prekeys (
            key_id INTEGER PRIMARY KEY,
            public_key BLOB NOT NULL,
            private_key BLOB NOT NULL,
            state INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            consumed_at INTEGER
        );

        
        CREATE TABLE IF NOT EXISTS file_transfers (
            transfer_id TEXT NOT NULL,
            peer_device_id TEXT NOT NULL,
            is_sender INTEGER NOT NULL,
            file_size INTEGER NOT NULL,
            chunk_size INTEGER NOT NULL,
            max_envelope_bytes INTEGER NOT NULL DEFAULT 0,
            max_http_body_bytes INTEGER NOT NULL DEFAULT 0,
            chunk_count INTEGER NOT NULL,
            file_hash BLOB NOT NULL,
            spool_path TEXT NOT NULL,
            root_file_key BLOB NOT NULL,
            received_bitset BLOB,
            status TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            PRIMARY KEY (transfer_id, peer_device_id)
        );

        CREATE TABLE IF NOT EXISTS ratchet_sessions (
            session_id TEXT PRIMARY KEY,
            remote_identity_key BLOB NOT NULL,
            local_identity_id INTEGER NOT NULL,
            DHs BLOB NOT NULL,
            DHr BLOB NOT NULL,
            RK BLOB NOT NULL,
            CKs BLOB NOT NULL,
            CKr BLOB NOT NULL,
            Ns INTEGER NOT NULL,
            Nr INTEGER NOT NULL,
            PN INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            version INTEGER NOT NULL
        );

        CREATE TABLE IF NOT EXISTS skipped_message_keys (
            session_id TEXT NOT NULL,
            remote_ratchet_public_key BLOB NOT NULL,
            message_number INTEGER NOT NULL,
            message_key BLOB NOT NULL,
            created_at INTEGER NOT NULL,
            PRIMARY KEY (session_id, remote_ratchet_public_key, message_number)
        );
        COMMIT;
    )";
    return executeSql(schemaSql);
}

ServiceResult<std::monostate> SecureE2EEStore::saveIdentity(const E2EEIdentity& id) {
    if (id.public_key.size() != 32 || id.private_key.size() != 32) {
        return NeoNect::ServiceResult<std::monostate>::fail("Invalid identity key length. Must be exactly 32 bytes.");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO identity (identity_id, public_key, private_key, created_at, version) VALUES (?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, id.identity_id);
    sqlite3_bind_blob(stmt, 2, id.public_key.constData(), id.public_key.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, id.private_key.constData(), id.private_key.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, id.created_at);
    sqlite3_bind_int64(stmt, 5, id.version);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
    }

    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<E2EEIdentity> SecureE2EEStore::getIdentity() {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT identity_id, public_key, private_key, created_at, version FROM identity ORDER BY version DESC LIMIT 1";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<E2EEIdentity>::fail("Failed to prepare statement");
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EEIdentity id;
        id.identity_id = sqlite3_column_int64(stmt, 0);
        id.public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        id.private_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2)), sqlite3_column_bytes(stmt, 2));
        id.created_at = sqlite3_column_int64(stmt, 3);
        id.version = sqlite3_column_int64(stmt, 4);
        sqlite3_finalize(stmt);
        return ServiceResult<E2EEIdentity>::ok(id);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<E2EEIdentity>::fail("Identity not found");
}

ServiceResult<std::monostate> SecureE2EEStore::saveSignedPreKey(const E2EESignedPreKey& spk) {
    if (spk.public_key.size() != 32 || spk.private_key.size() != 32 || spk.signature.size() != 64) {
        return NeoNect::ServiceResult<std::monostate>::fail("Invalid SignedPreKey length");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO signed_prekeys (key_id, public_key, private_key, signature, created_at, status) VALUES (?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, spk.key_id);
    sqlite3_bind_blob(stmt, 2, spk.public_key.constData(), spk.public_key.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, spk.private_key.constData(), spk.private_key.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 4, spk.signature.constData(), spk.signature.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, spk.created_at);
    sqlite3_bind_int(stmt, 6, spk.status);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
    }

    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<E2EESignedPreKey> SecureE2EEStore::getSignedPreKey(qint64 key_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT key_id, public_key, private_key, signature, created_at, status FROM signed_prekeys WHERE key_id = ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<E2EESignedPreKey>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, key_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EESignedPreKey spk;
        spk.key_id = sqlite3_column_int64(stmt, 0);
        spk.public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        spk.private_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2)), sqlite3_column_bytes(stmt, 2));
        spk.signature = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 3)), sqlite3_column_bytes(stmt, 3));
        spk.created_at = sqlite3_column_int64(stmt, 4);
        spk.status = sqlite3_column_int(stmt, 5);
        sqlite3_finalize(stmt);
        return ServiceResult<E2EESignedPreKey>::ok(spk);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<E2EESignedPreKey>::fail("SignedPreKey not found");
}

ServiceResult<std::vector<E2EESignedPreKey>> SecureE2EEStore::getAllSignedPreKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT key_id, public_key, private_key, signature, created_at, status FROM signed_prekeys";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<std::vector<E2EESignedPreKey>>::fail("Failed to prepare statement");
    }

    std::vector<E2EESignedPreKey> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EESignedPreKey spk;
        spk.key_id = sqlite3_column_int64(stmt, 0);
        spk.public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        spk.private_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2)), sqlite3_column_bytes(stmt, 2));
        spk.signature = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 3)), sqlite3_column_bytes(stmt, 3));
        spk.created_at = sqlite3_column_int64(stmt, 4);
        spk.status = sqlite3_column_int(stmt, 5);
        result.push_back(spk);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<std::vector<E2EESignedPreKey>>::ok(result);
}

ServiceResult<std::monostate> SecureE2EEStore::saveOneTimePreKeys(const std::vector<E2EEOneTimePreKey>& opks) {
    for (const auto& opk : opks) {
        if (opk.public_key.size() != 32 || opk.private_key.size() != 32) {
            return NeoNect::ServiceResult<std::monostate>::fail("Invalid OneTimePreKey length");
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    executeSql("BEGIN TRANSACTION;");
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO one_time_prekeys (key_id, public_key, private_key, state, created_at, consumed_at) VALUES (?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        executeSql("ROLLBACK;");
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare statement");
    }

    for (const auto& opk : opks) {
        sqlite3_bind_int64(stmt, 1, opk.key_id);
        sqlite3_bind_blob(stmt, 2, opk.public_key.constData(), opk.public_key.size(), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 3, opk.private_key.constData(), opk.private_key.size(), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, static_cast<int>(opk.state));
        sqlite3_bind_int64(stmt, 5, opk.created_at);
        if (opk.state == OPKState::CONSUMED) {
            sqlite3_bind_int64(stmt, 6, opk.consumed_at);
        } else {
            sqlite3_bind_null(stmt, 6);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            executeSql("ROLLBACK;");
            return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    executeSql("COMMIT;");
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<std::vector<E2EEOneTimePreKey>> SecureE2EEStore::getAvailableOneTimePreKeys() {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT key_id, public_key, private_key, state, created_at, consumed_at FROM one_time_prekeys WHERE state = ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<std::vector<E2EEOneTimePreKey>>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(OPKState::AVAILABLE));

    std::vector<E2EEOneTimePreKey> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EEOneTimePreKey opk;
        opk.key_id = sqlite3_column_int64(stmt, 0);
        opk.public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        opk.private_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2)), sqlite3_column_bytes(stmt, 2));
        opk.state = static_cast<OPKState>(sqlite3_column_int(stmt, 3));
        opk.created_at = sqlite3_column_int64(stmt, 4);
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            opk.consumed_at = sqlite3_column_int64(stmt, 5);
        } else {
            opk.consumed_at = 0;
        }
        result.push_back(opk);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<std::vector<E2EEOneTimePreKey>>::ok(result);
}

ServiceResult<std::monostate> SecureE2EEStore::consumeOneTimePreKeyAtomically(qint64 key_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE one_time_prekeys SET state = ?, consumed_at = ? WHERE key_id = ? AND state = ?";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int(stmt, 1, static_cast<int>(OPKState::CONSUMED));
    sqlite3_bind_int64(stmt, 2, QDateTime::currentMSecsSinceEpoch());
    sqlite3_bind_int64(stmt, 3, key_id);
    sqlite3_bind_int(stmt, 4, static_cast<int>(OPKState::AVAILABLE));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
    }
    
    int changes = sqlite3_changes(m_db);
    sqlite3_finalize(stmt);

    if (changes > 0) {
        return NeoNect::ServiceResult<std::monostate>::ok({});
    } else {
        return NeoNect::ServiceResult<std::monostate>::fail("Key not found or already consumed");
    }
}

ServiceResult<E2EEOneTimePreKey> SecureE2EEStore::getOneTimePreKey(qint64 key_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT key_id, public_key, private_key, state, created_at, consumed_at FROM one_time_prekeys WHERE key_id = ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<E2EEOneTimePreKey>::fail("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, key_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EEOneTimePreKey opk;
        opk.key_id = sqlite3_column_int64(stmt, 0);
        opk.public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        opk.private_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 2)), sqlite3_column_bytes(stmt, 2));
        opk.state = static_cast<OPKState>(sqlite3_column_int(stmt, 3));
        opk.created_at = sqlite3_column_int64(stmt, 4);
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            opk.consumed_at = sqlite3_column_int64(stmt, 5);
        } else {
            opk.consumed_at = 0;
        }
        sqlite3_finalize(stmt);
        return ServiceResult<E2EEOneTimePreKey>::ok(opk);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<E2EEOneTimePreKey>::fail("OneTimePreKey not found");
}

ServiceResult<std::monostate> SecureE2EEStore::saveSession(const E2EESession& session) {
    if (session.remote_identity_key.size() != 32 || (session.DHs.size() != 32 && session.DHs.size() != 64 && session.DHs.size() != 0) || (session.DHr.size() != 32 && session.DHr.size() != 0) ||
        session.RK.size() != 32 || (session.CKs.size() != 32 && session.CKs.size() != 0) || (session.CKr.size() != 32 && session.CKr.size() != 0)) {
        return NeoNect::ServiceResult<std::monostate>::fail("Invalid session key length");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO ratchet_sessions (session_id, remote_identity_key, local_identity_id, DHs, DHr, RK, CKs, CKr, Ns, Nr, PN, created_at, updated_at, version) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, session.session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, session.remote_identity_key.constData(), session.remote_identity_key.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, session.local_identity_id);
    sqlite3_bind_blob(stmt, 4, session.DHs.constData(), session.DHs.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 5, session.DHr.constData(), session.DHr.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 6, session.RK.constData(), session.RK.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 7, session.CKs.constData(), session.CKs.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 8, session.CKr.constData(), session.CKr.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 9, session.Ns);
    sqlite3_bind_int64(stmt, 10, session.Nr);
    sqlite3_bind_int64(stmt, 11, session.PN);
    sqlite3_bind_int64(stmt, 12, session.created_at);
    sqlite3_bind_int64(stmt, 13, session.updated_at);
    sqlite3_bind_int64(stmt, 14, session.version);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute statement");
    }

    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<E2EESession> SecureE2EEStore::getSession(const QString& session_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT session_id, remote_identity_key, local_identity_id, DHs, DHr, RK, CKs, CKr, Ns, Nr, PN, created_at, updated_at, version FROM ratchet_sessions WHERE session_id = ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<E2EESession>::fail("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EESession session;
        session.session_id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        session.remote_identity_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        session.local_identity_id = sqlite3_column_int64(stmt, 2);
        session.DHs = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 3)), sqlite3_column_bytes(stmt, 3));
        session.DHr = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 4)), sqlite3_column_bytes(stmt, 4));
        session.RK = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 5)), sqlite3_column_bytes(stmt, 5));
        session.CKs = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 6)), sqlite3_column_bytes(stmt, 6));
        session.CKr = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 7)), sqlite3_column_bytes(stmt, 7));
        session.Ns = sqlite3_column_int64(stmt, 8);
        session.Nr = sqlite3_column_int64(stmt, 9);
        session.PN = sqlite3_column_int64(stmt, 10);
        session.created_at = sqlite3_column_int64(stmt, 11);
        session.updated_at = sqlite3_column_int64(stmt, 12);
        session.version = sqlite3_column_int64(stmt, 13);
        sqlite3_finalize(stmt);
        return ServiceResult<E2EESession>::ok(session);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<E2EESession>::fail("Session not found");
}

ServiceResult<std::monostate> SecureE2EEStore::updateSessionState(const SessionUpdateTx& tx) {
    if (tx.session.remote_identity_key.size() != 32 || (tx.session.DHs.size() != 32 && tx.session.DHs.size() != 64 && tx.session.DHs.size() != 0) || (tx.session.DHr.size() != 32 && tx.session.DHr.size() != 0) ||
        tx.session.RK.size() != 32 || (tx.session.CKs.size() != 32 && tx.session.CKs.size() != 0) || (tx.session.CKr.size() != 32 && tx.session.CKr.size() != 0)) {
        return NeoNect::ServiceResult<std::monostate>::fail("Invalid session key length");
    }
    for (const auto& key : tx.new_skipped_keys) {
        if (key.remote_ratchet_public_key.size() != 32 || key.message_key.size() != 32) {
            return NeoNect::ServiceResult<std::monostate>::fail("Invalid skipped key length");
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    
    executeSql("BEGIN TRANSACTION;");

    // Update Session
    sqlite3_stmt* stmtSession = nullptr;
    const char* sqlSession = "INSERT OR REPLACE INTO ratchet_sessions (session_id, remote_identity_key, local_identity_id, DHs, DHr, RK, CKs, CKr, Ns, Nr, PN, created_at, updated_at, version) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(m_db, sqlSession, -1, &stmtSession, nullptr) != SQLITE_OK) {
        executeSql("ROLLBACK;");
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare session statement");
    }

    sqlite3_bind_text(stmtSession, 1, tx.session.session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmtSession, 2, tx.session.remote_identity_key.constData(), tx.session.remote_identity_key.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmtSession, 3, tx.session.local_identity_id);
    sqlite3_bind_blob(stmtSession, 4, tx.session.DHs.constData(), tx.session.DHs.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmtSession, 5, tx.session.DHr.constData(), tx.session.DHr.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmtSession, 6, tx.session.RK.constData(), tx.session.RK.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmtSession, 7, tx.session.CKs.constData(), tx.session.CKs.size(), SQLITE_STATIC);
    sqlite3_bind_blob(stmtSession, 8, tx.session.CKr.constData(), tx.session.CKr.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmtSession, 9, tx.session.Ns);
    sqlite3_bind_int64(stmtSession, 10, tx.session.Nr);
    sqlite3_bind_int64(stmtSession, 11, tx.session.PN);
    sqlite3_bind_int64(stmtSession, 12, tx.session.created_at);
    sqlite3_bind_int64(stmtSession, 13, tx.session.updated_at);
    sqlite3_bind_int64(stmtSession, 14, tx.session.version);

    if (sqlite3_step(stmtSession) != SQLITE_DONE) {
        sqlite3_finalize(stmtSession);
        executeSql("ROLLBACK;");
        return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute session update");
    }
    sqlite3_finalize(stmtSession);

    // New Skipped Keys
    if (!tx.new_skipped_keys.empty()) {
        sqlite3_stmt* stmtNewKey = nullptr;
        const char* sqlNewKey = "INSERT INTO skipped_message_keys (session_id, remote_ratchet_public_key, message_number, message_key, created_at) VALUES (?, ?, ?, ?, ?)";
        
        if (sqlite3_prepare_v2(m_db, sqlNewKey, -1, &stmtNewKey, nullptr) != SQLITE_OK) {
            executeSql("ROLLBACK;");
            return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare new key statement");
        }

        for (const auto& key : tx.new_skipped_keys) {
            sqlite3_bind_text(stmtNewKey, 1, key.session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_blob(stmtNewKey, 2, key.remote_ratchet_public_key.constData(), key.remote_ratchet_public_key.size(), SQLITE_STATIC);
            sqlite3_bind_int64(stmtNewKey, 3, key.message_number);
            sqlite3_bind_blob(stmtNewKey, 4, key.message_key.constData(), key.message_key.size(), SQLITE_STATIC);
            sqlite3_bind_int64(stmtNewKey, 5, key.created_at);

            if (sqlite3_step(stmtNewKey) != SQLITE_DONE) {
                sqlite3_finalize(stmtNewKey);
                executeSql("ROLLBACK;");
                return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute new key insertion");
            }
            sqlite3_reset(stmtNewKey);
        }
        sqlite3_finalize(stmtNewKey);
    }

    // Deleted Skipped Keys
    if (!tx.deleted_skipped_keys.empty()) {
        sqlite3_stmt* stmtDelKey = nullptr;
        const char* sqlDelKey = "DELETE FROM skipped_message_keys WHERE session_id = ? AND remote_ratchet_public_key = ? AND message_number = ?";
        
        if (sqlite3_prepare_v2(m_db, sqlDelKey, -1, &stmtDelKey, nullptr) != SQLITE_OK) {
            executeSql("ROLLBACK;");
            return NeoNect::ServiceResult<std::monostate>::fail("Failed to prepare delete key statement");
        }

        for (const auto& key : tx.deleted_skipped_keys) {
            sqlite3_bind_text(stmtDelKey, 1, key.session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_blob(stmtDelKey, 2, key.remote_ratchet_public_key.constData(), key.remote_ratchet_public_key.size(), SQLITE_STATIC);
            sqlite3_bind_int64(stmtDelKey, 3, key.message_number);

            if (sqlite3_step(stmtDelKey) != SQLITE_DONE) {
                sqlite3_finalize(stmtDelKey);
                executeSql("ROLLBACK;");
                return NeoNect::ServiceResult<std::monostate>::fail("Failed to execute key deletion");
            }
            sqlite3_reset(stmtDelKey);
        }
        sqlite3_finalize(stmtDelKey);
    }

    executeSql("COMMIT;");
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

ServiceResult<E2EESkippedKey> SecureE2EEStore::getSkippedKey(const QString& session_id, const QByteArray& remote_ratchet_public_key, qint64 message_number) {
    std::lock_guard<std::mutex> lock(m_mutex);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT session_id, remote_ratchet_public_key, message_number, message_key, created_at FROM skipped_message_keys WHERE session_id = ? AND remote_ratchet_public_key = ? AND message_number = ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return ServiceResult<E2EESkippedKey>::fail("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, session_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, remote_ratchet_public_key.constData(), remote_ratchet_public_key.size(), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, message_number);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        E2EESkippedKey key;
        key.session_id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        key.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 1)), sqlite3_column_bytes(stmt, 1));
        key.message_number = sqlite3_column_int64(stmt, 2);
        key.message_key = QByteArray(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 3)), sqlite3_column_bytes(stmt, 3));
        key.created_at = sqlite3_column_int64(stmt, 4);
        sqlite3_finalize(stmt);
        return ServiceResult<E2EESkippedKey>::ok(key);
    }

    sqlite3_finalize(stmt);
    return ServiceResult<E2EESkippedKey>::fail("Skipped key not found");
}

} // namespace Storage
} // namespace NeoNect


NeoNect::ServiceResult<std::monostate> NeoNect::Storage::SecureE2EEStore::saveFileTransfer(const NeoNect::Storage::E2EEFileTransfer& transfer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<std::monostate>::fail("Database not open");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO file_transfers (transfer_id, peer_device_id, is_sender, file_size, chunk_size, max_envelope_bytes, max_http_body_bytes, chunk_count, file_hash, spool_path, root_file_key, received_bitset, status, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail(sqlite3_errmsg(m_db));
    }

    sqlite3_bind_text(stmt, 1, transfer.transfer_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, transfer.peer_device_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, transfer.is_sender);
    sqlite3_bind_int64(stmt, 4, transfer.file_size);
    sqlite3_bind_int(stmt, 5, transfer.chunk_size);
    sqlite3_bind_int(stmt, 6, transfer.max_envelope_bytes);
    sqlite3_bind_int(stmt, 7, transfer.max_http_body_bytes);
    sqlite3_bind_int(stmt, 8, transfer.chunk_count);
    sqlite3_bind_blob(stmt, 9, transfer.file_hash.constData(), transfer.file_hash.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, transfer.spool_path.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 11, transfer.root_file_key.constData(), transfer.root_file_key.size(), SQLITE_TRANSIENT);
    if (transfer.received_bitset.isEmpty()) {
        sqlite3_bind_null(stmt, 12);
    } else {
        sqlite3_bind_blob(stmt, 12, transfer.received_bitset.constData(), transfer.received_bitset.size(), SQLITE_TRANSIENT);
    }
    sqlite3_bind_text(stmt, 13, transfer.status.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 14, transfer.created_at);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return NeoNect::ServiceResult<std::monostate>::fail("Failed to save file transfer");
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer> NeoNect::Storage::SecureE2EEStore::getFileTransfer(const QString& transfer_id, const QString& peer_device_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Database not open");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT is_sender, file_size, chunk_size, max_envelope_bytes, max_http_body_bytes, chunk_count, file_hash, spool_path, root_file_key, received_bitset, status, created_at FROM file_transfers WHERE transfer_id = ? AND peer_device_id = ?";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail(sqlite3_errmsg(m_db));
    }

    sqlite3_bind_text(stmt, 1, transfer_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, peer_device_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        NeoNect::Storage::E2EEFileTransfer transfer;
        transfer.transfer_id = transfer_id;
        transfer.peer_device_id = peer_device_id;
        transfer.is_sender = sqlite3_column_int(stmt, 0);
        transfer.file_size = sqlite3_column_int64(stmt, 1);
        transfer.chunk_size = sqlite3_column_int(stmt, 2);
        transfer.max_envelope_bytes = sqlite3_column_int(stmt, 3);
        transfer.max_http_body_bytes = sqlite3_column_int(stmt, 4);
        transfer.chunk_count = sqlite3_column_int(stmt, 5);
        
        const void* hash_ptr = sqlite3_column_blob(stmt, 6);
        int hash_len = sqlite3_column_bytes(stmt, 6);
        if (hash_ptr && hash_len > 0) transfer.file_hash = QByteArray(static_cast<const char*>(hash_ptr), hash_len);

        transfer.spool_path = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7)));

        const void* key_ptr = sqlite3_column_blob(stmt, 8);
        int key_len = sqlite3_column_bytes(stmt, 8);
        if (key_ptr && key_len > 0) transfer.root_file_key = QByteArray(static_cast<const char*>(key_ptr), key_len);

        const void* bitset_ptr = sqlite3_column_blob(stmt, 9);
        int bitset_len = sqlite3_column_bytes(stmt, 9);
        if (bitset_ptr && bitset_len > 0) transfer.received_bitset = QByteArray(static_cast<const char*>(bitset_ptr), bitset_len);

        transfer.status = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));
        transfer.created_at = sqlite3_column_int64(stmt, 11);
        
        if (transfer.is_sender && (transfer.max_envelope_bytes <= 0 || transfer.max_http_body_bytes <= 0 || transfer.chunk_size <= 0)) {
            sqlite3_finalize(stmt);
            return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Missing or invalid capability snapshot");
        }
        
        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::ok(std::move(transfer));
    }

    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("File transfer not found");
}

NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>> NeoNect::Storage::SecureE2EEStore::getActiveFileTransfers() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>>::fail("Database not open");

    std::vector<NeoNect::Storage::E2EEFileTransfer> transfers;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT transfer_id, peer_device_id, is_sender, file_size, chunk_size, max_envelope_bytes, max_http_body_bytes, chunk_count, file_hash, spool_path, root_file_key, received_bitset, status, created_at FROM file_transfers WHERE status != 'completed' AND status != 'cancelled'";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>>::fail(sqlite3_errmsg(m_db));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NeoNect::Storage::E2EEFileTransfer transfer;
        transfer.transfer_id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        transfer.peer_device_id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        transfer.is_sender = sqlite3_column_int(stmt, 2);
        transfer.file_size = sqlite3_column_int64(stmt, 3);
        transfer.chunk_size = sqlite3_column_int(stmt, 4);
        transfer.max_envelope_bytes = sqlite3_column_int(stmt, 5);
        transfer.max_http_body_bytes = sqlite3_column_int(stmt, 6);
        transfer.chunk_count = sqlite3_column_int(stmt, 7);
        
        const void* hash_ptr = sqlite3_column_blob(stmt, 8);
        int hash_len = sqlite3_column_bytes(stmt, 8);
        if (hash_ptr && hash_len > 0) transfer.file_hash = QByteArray(static_cast<const char*>(hash_ptr), hash_len);

        transfer.spool_path = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));

        const void* key_ptr = sqlite3_column_blob(stmt, 10);
        int key_len = sqlite3_column_bytes(stmt, 10);
        if (key_ptr && key_len > 0) transfer.root_file_key = QByteArray(static_cast<const char*>(key_ptr), key_len);

        const void* bitset_ptr = sqlite3_column_blob(stmt, 11);
        int bitset_len = sqlite3_column_bytes(stmt, 11);
        if (bitset_ptr && bitset_len > 0) transfer.received_bitset = QByteArray(static_cast<const char*>(bitset_ptr), bitset_len);

        transfer.status = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12)));
        transfer.created_at = sqlite3_column_int64(stmt, 13);
        
        if (transfer.is_sender && (transfer.max_envelope_bytes <= 0 || transfer.max_http_body_bytes <= 0 || transfer.chunk_size <= 0)) {
            continue; // Skip invalid persisted transfers
        }
        
        transfers.push_back(std::move(transfer));
    }

    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<std::vector<NeoNect::Storage::E2EEFileTransfer>>::ok(std::move(transfers));
}

NeoNect::ServiceResult<std::monostate> NeoNect::Storage::SecureE2EEStore::updateFileTransferBitset(const QString& transfer_id, const QString& peer_device_id, const QByteArray& bitset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<std::monostate>::fail("Database not open");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE file_transfers SET received_bitset = ? WHERE transfer_id = ? AND peer_device_id = ?";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail(sqlite3_errmsg(m_db));
    }

    sqlite3_bind_blob(stmt, 1, bitset.constData(), bitset.size(), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, transfer_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, peer_device_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return NeoNect::ServiceResult<std::monostate>::fail("Failed to update bitset");
    return NeoNect::ServiceResult<std::monostate>::ok({});
}

NeoNect::ServiceResult<std::monostate> NeoNect::Storage::SecureE2EEStore::updateFileTransferStatus(const QString& transfer_id, const QString& peer_device_id, const QString& status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<std::monostate>::fail("Database not open");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE file_transfers SET status = ? WHERE transfer_id = ? AND peer_device_id = ?";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<std::monostate>::fail(sqlite3_errmsg(m_db));
    }

    sqlite3_bind_text(stmt, 1, status.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, transfer_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, peer_device_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return NeoNect::ServiceResult<std::monostate>::fail("Failed to update status");
    return NeoNect::ServiceResult<std::monostate>::ok({});
}


NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer> NeoNect::Storage::SecureE2EEStore::getFileTransferById(const QString& transfer_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Database not open");

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT peer_device_id, is_sender, file_size, chunk_size, max_envelope_bytes, max_http_body_bytes, chunk_count, file_hash, spool_path, root_file_key, received_bitset, status, created_at FROM file_transfers WHERE transfer_id = ? LIMIT 1";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail(sqlite3_errmsg(m_db));
    }

    sqlite3_bind_text(stmt, 1, transfer_id.toUtf8().constData(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        NeoNect::Storage::E2EEFileTransfer transfer;
        transfer.transfer_id = transfer_id;
        transfer.peer_device_id = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        transfer.is_sender = sqlite3_column_int(stmt, 1);
        transfer.file_size = sqlite3_column_int64(stmt, 2);
        transfer.chunk_size = sqlite3_column_int(stmt, 3);
        transfer.max_envelope_bytes = sqlite3_column_int(stmt, 4);
        transfer.max_http_body_bytes = sqlite3_column_int(stmt, 5);
        transfer.chunk_count = sqlite3_column_int(stmt, 6);

        const void* hash_ptr = sqlite3_column_blob(stmt, 7);
        int hash_len = sqlite3_column_bytes(stmt, 7);
        if (hash_ptr && hash_len > 0) transfer.file_hash = QByteArray(static_cast<const char*>(hash_ptr), hash_len);

        transfer.spool_path = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));

        const void* key_ptr = sqlite3_column_blob(stmt, 9);
        int key_len = sqlite3_column_bytes(stmt, 9);
        if (key_ptr && key_len > 0) transfer.root_file_key = QByteArray(static_cast<const char*>(key_ptr), key_len);

        const void* bitset_ptr = sqlite3_column_blob(stmt, 10);
        int bitset_len = sqlite3_column_bytes(stmt, 10);
        if (bitset_ptr && bitset_len > 0) transfer.received_bitset = QByteArray(static_cast<const char*>(bitset_ptr), bitset_len);

        transfer.status = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11)));
        transfer.created_at = sqlite3_column_int64(stmt, 12);

        sqlite3_finalize(stmt);
        return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::ok(transfer);
    }
    sqlite3_finalize(stmt);
    return NeoNect::ServiceResult<NeoNect::Storage::E2EEFileTransfer>::fail("Transfer not found");
}
