// src/storage/settingsrepository.cpp
#include "settingsrepository.h"
#include "../common/constants.h"
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
    m_cachedDeviceId.clear();
    m_cachedPublicKey.clear();
}

QString SettingsRepository::profile() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_profile;
}

QString SettingsRepository::serverUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_SERVER_URL, Constants::DEFAULT_SERVER_URL).toString();
}

void SettingsRepository::setServerUrl(const QString &url) {
    std::lock_guard<std::mutex> lock(m_mutex);
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
    }
    m_cachedUsername = username;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_USERNAME, username);
}

QString SettingsRepository::getDeviceIdKey() const {
    QString user = m_cachedUsername.trimmed().toLower();
    if (user.isEmpty()) {
        QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        user = settings.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
    }
    if (user.isEmpty()) {
        return Constants::KEY_DEVICE_ID;
    }
    return QString("%1_%2").arg(Constants::KEY_DEVICE_ID, user);
}

QString SettingsRepository::getPublicKeyKey() const {
    QString user = m_cachedUsername.trimmed().toLower();
    if (user.isEmpty()) {
        QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        user = settings.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
    }
    if (user.isEmpty()) {
        return Constants::KEY_PUBLIC_KEY;
    }
    return QString("%1_%2").arg(Constants::KEY_PUBLIC_KEY, user);
}

QString SettingsRepository::deviceId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedDeviceId.isEmpty()) {
        return m_cachedDeviceId;
    }
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    m_cachedDeviceId = settings.value(getDeviceIdKey()).toString();
    return m_cachedDeviceId;
}

void SettingsRepository::setDeviceId(const QString &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedDeviceId = id;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(getDeviceIdKey(), id);
}

QString SettingsRepository::publicKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_cachedPublicKey.isEmpty()) {
        return m_cachedPublicKey;
    }
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    m_cachedPublicKey = settings.value(getPublicKeyKey()).toString();
    return m_cachedPublicKey;
}

void SettingsRepository::setPublicKey(const QString &key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedPublicKey = key;
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(getPublicKeyKey(), key);
}

QString SettingsRepository::getConversationsKey() const {
    QString user = m_cachedUsername.trimmed().toLower();
    if (user.isEmpty()) {
        QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        user = settings.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
    }
    if (user.isEmpty()) {
        return Constants::KEY_OPEN_CONVERSATIONS;
    }
    return QString("%1_%2").arg(Constants::KEY_OPEN_CONVERSATIONS, user);
}

QString SettingsRepository::getFriendsKey() const {
    QString user = m_cachedUsername.trimmed().toLower();
    if (user.isEmpty()) {
        QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        user = settings.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
    }
    if (user.isEmpty()) {
        return Constants::KEY_FRIENDS;
    }
    return QString("%1_%2").arg(Constants::KEY_FRIENDS, user);
}

QString SettingsRepository::getPendingRequestsKey() const {
    QString user = m_cachedUsername.trimmed().toLower();
    if (user.isEmpty()) {
        QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
        user = settings.value(Constants::KEY_USERNAME).toString().trimmed().toLower();
    }
    if (user.isEmpty()) {
        return Constants::KEY_PENDING_REQUESTS;
    }
    return QString("%1_%2").arg(Constants::KEY_PENDING_REQUESTS, user);
}

QStringList SettingsRepository::friends() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(getFriendsKey()).toStringList();
}

void SettingsRepository::setFriends(const QStringList &friends) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(getFriendsKey(), friends);
}

QStringList SettingsRepository::pendingRequests() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(getPendingRequestsKey()).toStringList();
}

void SettingsRepository::setPendingRequests(const QStringList &requests) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(getPendingRequestsKey(), requests);
}

void SettingsRepository::clearSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cachedAuthToken.clear();
    m_cachedUsername.clear();
    m_cachedDeviceId.clear();
    m_cachedPublicKey.clear();
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.remove(Constants::KEY_AUTH_TOKEN);
    settings.remove(Constants::KEY_USERNAME);
}

QVariantList SettingsRepository::bookmarks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    QString jsonStr = settings.value(Constants::KEY_BOOKMARKS).toString();
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
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    QJsonArray arr = QJsonArray::fromVariantList(bookmarks);
    QJsonDocument doc(arr);
    settings.setValue(Constants::KEY_BOOKMARKS, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
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
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    QString jsonStr = settings.value(getConversationsKey()).toString();
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
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    QJsonArray arr = QJsonArray::fromVariantList(conversations);
    QJsonDocument doc(arr);
    settings.setValue(getConversationsKey(), QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

} // namespace Storage
} // namespace NeoNect
