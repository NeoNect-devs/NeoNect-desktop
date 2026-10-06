#include <QRandomGenerator>
// src/storage/settingsrepository.cpp
#include "settingsrepository.h"
#include "StoragePathResolver.h"
#include "SettingsMigrationOrchestrator.h"
#include "../common/constants.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QSysInfo>
#include <QCryptographicHash>
#include "../crypto/cryptoservice.h"

namespace NeoNect {
namespace Storage {

namespace {

QByteArray getMachineKey() {
    QByteArray machineId = QSysInfo::machineUniqueId();
    if (machineId.isEmpty()) machineId = "fallback-machine-id-12345";
    return QCryptographicHash::hash(machineId, QCryptographicHash::Sha256);
}

Crypto::CryptoService& getLocalCrypto() {
    static Crypto::CryptoService svc;
    static bool initialized = false;
    if (!initialized) {
        svc.setMasterKey(getMachineKey());
        initialized = true;
    }
    return svc;
}

QString encryptString(const QString& str) {
    if (str.isEmpty()) return str;
    auto enc = getLocalCrypto().encryptAesGcm(str.toUtf8());
    if (enc.success) {
        QJsonObject obj;
        obj["ct"] = QString::fromLatin1(enc.cipherWithTag.toBase64());
        obj["iv"] = QString::fromLatin1(enc.nonce.toBase64());
        return QString::fromLatin1(QJsonDocument(obj).toJson(QJsonDocument::Compact).toBase64());
    }
    return str;
}

QString decryptString(const QString& str) {
    if (str.isEmpty()) return str;
    QByteArray jsonBytes = QByteArray::fromBase64(str.toLatin1());
    auto doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject()) return str; // Fallback to plain if not encrypted (migration)
    QByteArray ct = QByteArray::fromBase64(doc.object().value("ct").toString().toLatin1());
    QByteArray iv = QByteArray::fromBase64(doc.object().value("iv").toString().toLatin1());
    if (ct.isEmpty() || iv.isEmpty()) return str;
    QByteArray dec = getLocalCrypto().decryptAesGcm(ct, iv);
    if (!dec.isEmpty()) return QString::fromUtf8(dec);
    return str;
}

} // namespace


SettingsRepository::SettingsRepository(const QString &profileName)
    : m_profile(profileName.trimmed()) {
}


std::unique_ptr<QSettings> SettingsRepository::getIsolatedSettings() const {
    QString user = m_cachedUsername.trimmed().toLower();
    QString server = m_cachedServerUrl.trimmed();

    if (user.isEmpty() || server.isEmpty()) {
        QSettings global(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        if (user.isEmpty()) user = global.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
        if (server.isEmpty()) server = global.value(Constants::KEY_SERVER_URL, Constants::DEFAULT_SERVER_URL).toString().trimmed();
    }

    if (user.isEmpty() || server.isEmpty()) {
        return nullptr;
    }

    QString dir = StoragePathResolver::settingsDirectory(server, user);
    QDir().mkpath(dir);
    QString path = QDir(dir).filePath("settings.ini");

    auto isolated = std::make_unique<QSettings>(path, QSettings::IniFormat);
    SettingsMigrationOrchestrator::migrateSettingsIfNecessary(m_profile, server, user, *isolated);
    return isolated;
}

QString SettingsRepository::getGroupName() const {
    if (m_profile.isEmpty()) {
        return Constants::DEFAULT_PROFILE_GROUP;
    }
    return QString("%1_%2").arg(Constants::DEFAULT_PROFILE_GROUP, m_profile);
}

void SettingsRepository::setProfile(const QString &profileName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profile = profileName.trimmed();
    m_cachedAuthToken.clear();
    m_cachedUsername.clear();
    m_cachedServerUrl.clear();
    m_cachedServerUrl.clear();
    m_cachedDeviceId.clear();
    m_cachedPublicKey.clear();
}

QString SettingsRepository::profile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile;
}

QString SettingsRepository::serverUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedServerUrl.isEmpty()) {
        return m_cachedServerUrl;
    }
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    m_cachedServerUrl = settings.value(Constants::KEY_SERVER_URL, Constants::DEFAULT_SERVER_URL).toString();
    return m_cachedServerUrl;
}

void SettingsRepository::setServerUrl(const QString &url) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedServerUrl = url;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_SERVER_URL, url);
}

QString SettingsRepository::authToken() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedAuthToken.isEmpty()) {
        return m_cachedAuthToken;
    }
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    m_cachedAuthToken = decryptString(settings.value(Constants::KEY_AUTH_TOKEN).toString());
    return m_cachedAuthToken;
}

void SettingsRepository::setAuthToken(const QString &token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedAuthToken = token;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_AUTH_TOKEN, encryptString(token));
}

QString SettingsRepository::username() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedUsername.isEmpty()) {
        return m_cachedUsername;
    }
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    m_cachedUsername = settings.value(Constants::KEY_USERNAME).toString();
    return m_cachedUsername;
}

void SettingsRepository::setUsername(const QString &username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_cachedUsername != username) {
        m_cachedDeviceId.clear();
        m_cachedPublicKey.clear();
        m_cachedDisplayName.clear();
        m_cachedAvatarUrl.clear();
    }
    m_cachedUsername = username;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_USERNAME, username);
}

QString SettingsRepository::deviceId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedDeviceId.isEmpty()) {
        return m_cachedDeviceId;
    }
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    m_cachedDeviceId = settings->value(Constants::KEY_DEVICE_ID).toString();
    return m_cachedDeviceId;
}

void SettingsRepository::setDeviceId(const QString &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedDeviceId = id;
    auto settings = getIsolatedSettings();
    if (!settings) return;
    settings->setValue(Constants::KEY_DEVICE_ID, id);
}

QString SettingsRepository::publicKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedPublicKey.isEmpty()) {
        return m_cachedPublicKey;
    }
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    m_cachedPublicKey = settings->value(Constants::KEY_PUBLIC_KEY).toString();
    return m_cachedPublicKey;
}

void SettingsRepository::setPublicKey(const QString &key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedPublicKey = key;
    auto settings = getIsolatedSettings();
    if (!settings) return;
    settings->setValue(Constants::KEY_PUBLIC_KEY, key);
}

QStringList SettingsRepository::friends() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QStringList();
    return settings->value(Constants::KEY_FRIENDS).toStringList();
}

void SettingsRepository::setFriends(const QStringList &friends) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    settings->setValue(Constants::KEY_FRIENDS, friends);
}

QStringList SettingsRepository::pendingRequests() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QStringList();
    return settings->value(Constants::KEY_PENDING_REQUESTS).toStringList();
}

void SettingsRepository::setPendingRequests(const QStringList &requests) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    settings->setValue(Constants::KEY_PENDING_REQUESTS, requests);
}

void SettingsRepository::clearSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedAuthToken.clear();
    m_cachedUsername.clear();
    m_cachedServerUrl.clear();
    m_cachedServerUrl.clear();
    m_cachedDeviceId.clear();
    m_cachedPublicKey.clear();
    m_cachedDisplayName.clear();
    m_cachedAvatarUrl.clear();
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.remove(Constants::KEY_AUTH_TOKEN);
    settings.remove(Constants::KEY_USERNAME);
}

QString SettingsRepository::displayName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedDisplayName.isEmpty()) {
        return m_cachedDisplayName;
    }
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    m_cachedDisplayName = settings->value(Constants::KEY_DISPLAY_NAME).toString();
    return m_cachedDisplayName;
}

void SettingsRepository::setDisplayName(const QString &displayName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedDisplayName = displayName;
    auto settings = getIsolatedSettings();
    if (!settings) return;
    settings->setValue(Constants::KEY_DISPLAY_NAME, displayName);
}

QString SettingsRepository::peerDisplayName(const QString &username) const {
    QString cleanUser = username.trimmed().toLower();
    if (cleanUser.isEmpty()) return QString();
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    QString jsonStr = settings->value(Constants::KEY_PEER_DISPLAY_NAMES).toString();
    if (jsonStr.trimmed().isEmpty()) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return QString();
    return doc.object().value(cleanUser).toString();
}

void SettingsRepository::setPeerDisplayName(const QString &username, const QString &displayName) {
    QString cleanUser = username.trimmed().toLower();
    if (cleanUser.isEmpty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    QString jsonStr = settings->value(Constants::KEY_PEER_DISPLAY_NAMES).toString();
    QJsonObject mapObj;
    if (!jsonStr.trimmed().isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isObject()) mapObj = doc.object();
    }
    if (displayName.trimmed().isEmpty()) {
        mapObj.remove(cleanUser);
    } else {
        mapObj[cleanUser] = displayName.trimmed();
    }
    settings->setValue(Constants::KEY_PEER_DISPLAY_NAMES, QString::fromUtf8(QJsonDocument(mapObj).toJson(QJsonDocument::Compact)));
}

QString SettingsRepository::avatarUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedAvatarUrl.isEmpty()) {
        return m_cachedAvatarUrl;
    }
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    m_cachedAvatarUrl = settings->value(Constants::KEY_AVATAR_URL).toString();
    return m_cachedAvatarUrl;
}

void SettingsRepository::setAvatarUrl(const QString &url) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedAvatarUrl = url;
    auto settings = getIsolatedSettings();
    if (!settings) return;
    if (url.trimmed().isEmpty()) {
        settings->remove(Constants::KEY_AVATAR_URL);
    } else {
        settings->setValue(Constants::KEY_AVATAR_URL, url);
    }
}

QString SettingsRepository::peerAvatarUrl(const QString &username) const {
    QString cleanUser = username.trimmed().toLower();
    if (cleanUser.isEmpty()) return QString();
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QString();
    QString jsonStr = settings->value(Constants::KEY_PEER_AVATARS).toString();
    if (jsonStr.trimmed().isEmpty()) return QString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) return QString();
    return doc.object().value(cleanUser).toString();
}

void SettingsRepository::setPeerAvatarUrl(const QString &username, const QString &url) {
    QString cleanUser = username.trimmed().toLower();
    if (cleanUser.isEmpty()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    QString jsonStr = settings->value(Constants::KEY_PEER_AVATARS).toString();
    QJsonObject mapObj;
    if (!jsonStr.trimmed().isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isObject()) mapObj = doc.object();
    }
    if (url.trimmed().isEmpty()) {
        mapObj.remove(cleanUser);
    } else {
        mapObj[cleanUser] = url.trimmed();
    }
    settings->setValue(Constants::KEY_PEER_AVATARS, QString::fromUtf8(QJsonDocument(mapObj).toJson(QJsonDocument::Compact)));
}

QVariantList SettingsRepository::bookmarks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QVariantList();
    QString jsonStr = settings->value(Constants::KEY_BOOKMARKS).toString();
    if (jsonStr.trimmed().isEmpty()) {
        return QVariantList();
    }
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) {
        return QVariantList();
    }
    return doc.array().toVariantList();
}

void SettingsRepository::setBookmarks(const QVariantList &bookmarks) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    QJsonArray arr = QJsonArray::fromVariantList(bookmarks);
    QJsonDocument doc(arr);
    settings->setValue(Constants::KEY_BOOKMARKS, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

void SettingsRepository::addBookmark(const QVariantMap &bookmark) {
    QVariantList list = bookmarks();
    QVariantMap newBm = bookmark;
    if (!newBm.contains("id") || newBm.value("id").toString().trimmed().isEmpty()) {
        newBm["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    if (!newBm.contains("createdAt")) {
        newBm["createdAt"] = QDateTime::currentMSecsSinceEpoch();
    }
    list.append(newBm);
    setBookmarks(list);
}

void SettingsRepository::updateBookmark(const QVariantMap &bookmark) {
    QString id = bookmark.value("id").toString();
    if (id.isEmpty()) return;

    QVariantList list = bookmarks();
    bool found = false;
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap bm = list.at(i).toMap();
        if (bm.value("id").toString() == id) {
            for (auto it = bookmark.begin(); it != bookmark.end(); ++it) {
                bm[it.key()] = it.value();
            }
            list[i] = bm;
            found = true;
            break;
        }
    }
    if (found) {
        setBookmarks(list);
    }
}

void SettingsRepository::removeBookmark(const QString &id) {
    if (id.isEmpty()) return;
    QVariantList list = bookmarks();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).toMap().value("id").toString() == id) {
            list.removeAt(i);
            setBookmarks(list);
            break;
        }
    }
}

QVariantList SettingsRepository::openConversations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return QVariantList();
    QString jsonStr = settings->value(Constants::KEY_OPEN_CONVERSATIONS).toString();
    if (jsonStr.trimmed().isEmpty()) {
        return QVariantList();
    }
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray()) {
        return QVariantList();
    }
    return doc.array().toVariantList();
}

void SettingsRepository::setOpenConversations(const QVariantList &conversations) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto settings = getIsolatedSettings();
    if (!settings) return;
    QJsonArray arr = QJsonArray::fromVariantList(conversations);
    QJsonDocument doc(arr);
    settings->setValue(Constants::KEY_OPEN_CONVERSATIONS, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

} // namespace Storage
} // namespace NeoNect
