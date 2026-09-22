// src/storage/settingsrepository.h
#pragma once
#include "isettingsrepository.h"
#include <QSettings>
#include <memory>
#include <mutex>

namespace NeoNect {
namespace Storage {

class SettingsRepository : public ISettingsRepository {
public:
    explicit SettingsRepository(const QString &profileName = QString());
    ~SettingsRepository() override = default;

    void setProfile(const QString &profileName) override;
    QString profile() const override;

    QString serverUrl() const override;
    void setServerUrl(const QString &url) override;

    QString authToken() const override;
    void setAuthToken(const QString &token) override;

    QString username() const override;
    void setUsername(const QString &username) override;

    QString deviceId() const override;
    void setDeviceId(const QString &id) override;

    QString publicKey() const override;
    void setPublicKey(const QString &key) override;

    QStringList friends() const override;
    void setFriends(const QStringList &friends) override;

    QStringList pendingRequests() const override;
    void setPendingRequests(const QStringList &requests) override;

    QVariantList bookmarks() const override;
    void setBookmarks(const QVariantList &bookmarks) override;
    void addBookmark(const QVariantMap &bookmark) override;
    void updateBookmark(const QVariantMap &bookmark) override;
    void removeBookmark(const QString &id) override;

    QVariantList openConversations() const override;
    void setOpenConversations(const QVariantList &conversations) override;

    QString displayName() const override;
    void setDisplayName(const QString &displayName) override;

    QString peerDisplayName(const QString &username) const override;
    void setPeerDisplayName(const QString &username, const QString &displayName) override;

    QString avatarUrl() const override;
    void setAvatarUrl(const QString &url) override;

    QString peerAvatarUrl(const QString &username) const override;
    void setPeerAvatarUrl(const QString &username, const QString &url) override;

    void clearSession() override;

private:
    QString getGroupName() const;
    QString getConversationsKey() const;
    QString getFriendsKey() const;
    QString getPendingRequestsKey() const;
    QString getDeviceIdKey() const;
    QString getPublicKeyKey() const;
    QString getDisplayNameKey() const;
    QString getPeerDisplayNamesKey() const;
    QString getAvatarUrlKey() const;
    QString getPeerAvatarsKey() const;

    mutable std::mutex m_mutex;
    QString m_profile;
    mutable QString m_cachedAuthToken;
    mutable QString m_cachedUsername;
    mutable QString m_cachedDeviceId;
    mutable QString m_cachedPublicKey;
    mutable QString m_cachedDisplayName;
    mutable QString m_cachedAvatarUrl;
};

} // namespace Storage
} // namespace NeoNect
