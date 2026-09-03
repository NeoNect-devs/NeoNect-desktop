// src/storage/settingsrepository.cpp
#include "settingsrepository.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>

namespace NeoNect {
namespace Storage {

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
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_AUTH_TOKEN).toString();
}

void SettingsRepository::setAuthToken(const QString &token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_AUTH_TOKEN, token);
}

QString SettingsRepository::username() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_USERNAME).toString();
}

void SettingsRepository::setUsername(const QString &username) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_USERNAME, username);
}

QString SettingsRepository::deviceId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_DEVICE_ID).toString();
}

void SettingsRepository::setDeviceId(const QString &id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_DEVICE_ID, id);
}

QString SettingsRepository::publicKey() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_PUBLIC_KEY).toString();
}

void SettingsRepository::setPublicKey(const QString &key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_PUBLIC_KEY, key);
}

QStringList SettingsRepository::friends() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    return settings.value(Constants::KEY_FRIENDS).toStringList();
}

void SettingsRepository::setFriends(const QStringList &friends) {
    std::lock_guard<std::mutex> lock(m_mutex);
    QSettings settings(Constants::SETTINGS_ROOT_GROUP, getGroupName());
    settings.setValue(Constants::KEY_FRIENDS, friends);
}

void SettingsRepository::clearSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
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

} // namespace Storage
} // namespace NeoNect
