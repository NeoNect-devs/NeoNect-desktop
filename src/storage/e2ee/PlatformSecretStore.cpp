#include "PlatformSecretStore.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#elif defined(HAS_LIBSECRET)
#undef signals
#include <libsecret/secret.h>
#define signals Q_SIGNALS

static const SecretSchema* get_neonect_schema() {
    static const SecretSchema schema = {
        "org.neonect.NeoNect",
        SECRET_SCHEMA_NONE,
        {
            { "user", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "name", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
        }
    };
    return &schema;
}
#endif

namespace NeoNect {
namespace Storage {

PlatformSecretStore::PlatformSecretStore(const QString& baseDir)
    : m_baseDir(baseDir) {}
PlatformSecretStore::~PlatformSecretStore() = default;

QString PlatformSecretStore::getSecretFilePath(const QString& name) const {
    QString appDataDir = m_baseDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : m_baseDir;
    QDir dir(appDataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    // Hash the name so we don't store arbitrary strings as filenames directly
    QByteArray hash = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256);
    return dir.absoluteFilePath(hash.toHex() + ".sec");
}

ServiceResult<QByteArray> PlatformSecretStore::readSecret(const QString& name) {
#ifdef Q_OS_WIN
    QString filePath = getSecretFilePath(name);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ServiceResult<QByteArray>::fail("Secret file not found or unreadable");
    }
    QByteArray encryptedData = file.readAll();
    file.close();

    DATA_BLOB dataIn;
    dataIn.pbData = reinterpret_cast<BYTE*>(encryptedData.data());
    dataIn.cbData = static_cast<DWORD>(encryptedData.size());

    DATA_BLOB dataOut;
    if (CryptUnprotectData(&dataIn, nullptr, nullptr, nullptr, nullptr, 0, &dataOut)) {
        QByteArray decrypted(reinterpret_cast<const char*>(dataOut.pbData), dataOut.cbData);
        SecureZeroMemory(dataOut.pbData, dataOut.cbData);
        LocalFree(dataOut.pbData);
        return ServiceResult<QByteArray>::ok(decrypted);
    }
    return ServiceResult<QByteArray>::fail("Failed to unprotect secret data");
#elif defined(HAS_LIBSECRET)
    GError* error = nullptr;
    gchar* password = secret_password_lookup_sync(
        get_neonect_schema(),
        nullptr,
        &error,
        "user", "neonect",
        "name", name.toUtf8().constData(),
        nullptr);

    if (error != nullptr) {
        QString errMsg = QString("Failed to lookup secret: %1").arg(error->message);
        g_error_free(error);
        return ServiceResult<QByteArray>::fail(errMsg);
    }

    if (!password) {
        return ServiceResult<QByteArray>::fail("Secret not found");
    }

    QByteArray decrypted = QByteArray::fromHex(password);
    secret_password_free(password);
    return ServiceResult<QByteArray>::ok(decrypted);
#else
    Q_UNUSED(name);
    return ServiceResult<QByteArray>::fail("Secure OS secret storage is not available in this build.");
#endif
}

ServiceResult<std::monostate> PlatformSecretStore::writeSecret(const QString& name, const QByteArray& secret) {
#ifdef Q_OS_WIN
    DATA_BLOB dataIn;
    dataIn.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(secret.data()));
    dataIn.cbData = static_cast<DWORD>(secret.size());

    DATA_BLOB dataOut;
    if (CryptProtectData(&dataIn, L"NeoNect_Secret", nullptr, nullptr, nullptr, 0, &dataOut)) {
        QString filePath = getSecretFilePath(name);
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reinterpret_cast<const char*>(dataOut.pbData), dataOut.cbData);
            file.close();
            LocalFree(dataOut.pbData);
            return ServiceResult<std::monostate>::ok({});
        }
        LocalFree(dataOut.pbData);
        return ServiceResult<std::monostate>::fail("Failed to write protected data to file");
    }
    return ServiceResult<std::monostate>::fail("Failed to protect secret data");
#elif defined(HAS_LIBSECRET)
    GError* error = nullptr;
    QByteArray hexSecret = secret.toHex();
    gboolean success = secret_password_store_sync(
        get_neonect_schema(),
        SECRET_COLLECTION_DEFAULT,
        "NeoNect Master Key",
        hexSecret.constData(),
        nullptr,
        &error,
        "user", "neonect",
        "name", name.toUtf8().constData(),
        nullptr);

    if (error != nullptr) {
        QString errMsg = QString("Failed to write secret: %1").arg(error->message);
        g_error_free(error);
        return ServiceResult<std::monostate>::fail(errMsg);
    }
    if (!success) {
        return ServiceResult<std::monostate>::fail("Failed to store secret in libsecret");
    }
    return ServiceResult<std::monostate>::ok({});
#else
    Q_UNUSED(name);
    Q_UNUSED(secret);
    return ServiceResult<std::monostate>::fail("Secure OS secret storage is not available in this build.");
#endif
}

ServiceResult<std::monostate> PlatformSecretStore::deleteSecret(const QString& name) {
#ifdef Q_OS_WIN
    QString filePath = getSecretFilePath(name);
    if (QFile::exists(filePath)) {
        if (QFile::remove(filePath)) {
            return ServiceResult<std::monostate>::ok({});
        }
        return ServiceResult<std::monostate>::fail("Failed to delete secret file");
    }
    return ServiceResult<std::monostate>::ok({});
#elif defined(HAS_LIBSECRET)
    GError* error = nullptr;
    secret_password_clear_sync(
        get_neonect_schema(),
        nullptr,
        &error,
        "user", "neonect",
        "name", name.toUtf8().constData(),
        nullptr);

    if (error != nullptr) {
        QString errMsg = QString("Failed to delete secret: %1").arg(error->message);
        g_error_free(error);
        return ServiceResult<std::monostate>::fail(errMsg);
    }
    return ServiceResult<std::monostate>::ok({});
#else
    Q_UNUSED(name);
    return ServiceResult<std::monostate>::fail("Secure OS secret storage is not available in this build.");
#endif
}

} // namespace Storage
} // namespace NeoNect
