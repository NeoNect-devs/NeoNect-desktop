#include "StoragePathResolver.h"
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QCryptographicHash>

namespace NeoNect {
namespace Storage {

QString StoragePathResolver::appRoot() {
    // QStandardPaths::AppDataLocation usually incorporates the application name via QCoreApplication::applicationName().
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absolutePath();
}

QString StoragePathResolver::canonicalServerUrl(const QString& rawUrl) {
    QString trimmed = rawUrl.trimmed();
    if (trimmed.isEmpty()) return "";
    
    // Automatically prepend scheme if missing so QUrl parses the host correctly
    if (!trimmed.contains("://")) {
        trimmed = "https://" + trimmed;
    }
    
    QUrl url(trimmed);
    url.setScheme(url.scheme().toLower());
    url.setHost(url.host().toLower());
    url.setFragment(QString());
    
    if ((url.scheme() == "http" && url.port() == 80) ||
        (url.scheme() == "https" && url.port() == 443)) {
        url.setPort(-1); // Remove default ports
    }
    
    QString out = url.toString(QUrl::RemoveFragment);
    while (out.endsWith('/')) {
        out.chop(1);
    }
    return out;
}

QString StoragePathResolver::canonicalUsername(const QString& rawUsername) {
    return rawUsername.trimmed().toLower();
}

QString StoragePathResolver::canonicalConversationId(const QString& rawConversationId) {
    return rawConversationId.trimmed(); // Preserve case as existing conversationIds may rely on it
}

static QString sha256hex(const QString& input) {
    return QString(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256).toHex()).toLower();
}

QString StoragePathResolver::serverKey(const QString& serverUrl) {
    return sha256hex(canonicalServerUrl(serverUrl));
}

QString StoragePathResolver::accountKey(const QString& username) {
    return sha256hex(canonicalUsername(username));
}

QString StoragePathResolver::chatKey(const QString& conversationId) {
    return sha256hex(canonicalConversationId(conversationId));
}

QString StoragePathResolver::serverRoot(const QString& serverUrl) {
    return QDir(appRoot()).filePath(QString("servers/%1").arg(serverKey(serverUrl)));
}

QString StoragePathResolver::accountRoot(const QString& serverUrl, const QString& username) {
    return QDir(serverRoot(serverUrl)).filePath(QString("users/%1").arg(accountKey(username)));
}

QString StoragePathResolver::accountDirectory(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("account");
}

QString StoragePathResolver::e2eeDirectory(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("e2ee");
}

QString StoragePathResolver::settingsDirectory(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("settings");
}

QString StoragePathResolver::cacheDirectory(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("cache");
}

QString StoragePathResolver::chatsDirectory(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("chats");
}

QString StoragePathResolver::chatDirectory(const QString& serverUrl, const QString& username, const QString& conversationId) {
    return QDir(chatsDirectory(serverUrl, username)).filePath(chatKey(conversationId));
}

QString StoragePathResolver::chatAttachmentsDirectory(const QString& serverUrl, const QString& username, const QString& conversationId) {
    return QDir(chatDirectory(serverUrl, username, conversationId)).filePath("attachments");
}

QString StoragePathResolver::messageDbPath(const QString& serverUrl, const QString& username) {
    return QDir(accountRoot(serverUrl, username)).filePath("messages.db");
}

QString StoragePathResolver::e2eeDbPath(const QString& serverUrl, const QString& username) {
    return QDir(e2eeDirectory(serverUrl, username)).filePath("e2ee.db");
}

QString StoragePathResolver::accountMetadataPath(const QString& serverUrl, const QString& username) {
    return QDir(accountDirectory(serverUrl, username)).filePath("profile.json");
}

QString StoragePathResolver::chatMetadataPath(const QString& serverUrl, const QString& username, const QString& conversationId) {
    return QDir(chatDirectory(serverUrl, username, conversationId)).filePath("chat.json");
}

} // namespace Storage
} // namespace NeoNect
