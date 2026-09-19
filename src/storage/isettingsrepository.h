// src/storage/isettingsrepository.h
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace NeoNect {
namespace Storage {

class ISettingsRepository {
public:
    virtual ~ISettingsRepository() = default;

    virtual void setProfile(const QString &profileName) = 0;
    virtual QString profile() const = 0;

    virtual QString serverUrl() const = 0;
    virtual void setServerUrl(const QString &url) = 0;

    virtual QString authToken() const = 0;
    virtual void setAuthToken(const QString &token) = 0;

    virtual QString username() const = 0;
    virtual void setUsername(const QString &username) = 0;

    virtual QString deviceId() const = 0;
    virtual void setDeviceId(const QString &id) = 0;

    virtual QString publicKey() const = 0;
    virtual void setPublicKey(const QString &key) = 0;

    virtual QStringList friends() const = 0;
    virtual void setFriends(const QStringList &friends) = 0;

    virtual QStringList pendingRequests() const = 0;
    virtual void setPendingRequests(const QStringList &requests) = 0;

    virtual QVariantList bookmarks() const = 0;
    virtual void setBookmarks(const QVariantList &bookmarks) = 0;
    virtual void addBookmark(const QVariantMap &bookmark) = 0;
    virtual void updateBookmark(const QVariantMap &bookmark) = 0;
    virtual void removeBookmark(const QString &id) = 0;

    virtual void clearSession() = 0;
};

} // namespace Storage
} // namespace NeoNect
