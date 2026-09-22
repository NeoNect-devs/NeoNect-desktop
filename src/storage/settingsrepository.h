/**
 * @file settingsrepository.h
 * @brief Thread-safe QSettings-backed concrete implementation of ISettingsRepository.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Manages client settings, session keys, authentication credentials, and user preferences.
 * Implements a read-through in-memory cache protected by a mutual exclusion lock (`std::mutex`)
 * to prevent repeated disk I/O while maintaining thread safety across GUI and worker threads.
 *
 * @par Design Patterns:
 * - <b>Concrete Repository</b>: Concrete implementation of `ISettingsRepository` via `QSettings`.
 * - <b>Cache-Aside / Read-Through Cache</b>: Frequently accessed fields (auth token, username, device ID) are cached in memory.
 * - <b>Thread-Safe Monitor</b>: Mutex synchronization guarantees safe concurrent access from any thread.
 */

#pragma once
#include "isettingsrepository.h"
#include <QSettings>
#include <memory>
#include <mutex>

namespace NeoNect {
namespace Storage {

/**
 * @class SettingsRepository
 * @brief Persistent configuration store with profile namespaces and in-memory caching.
 */
class SettingsRepository : public ISettingsRepository {
public:
    /**
     * @brief Constructs the settings repository for a designated profile namespace.
     * @param profileName Optional profile name (e.g. `"default"`).
     */
    explicit SettingsRepository(const QString &profileName = QString());

    /**
     * @brief Virtual destructor ensuring clean teardown.
     */
    ~SettingsRepository() override = default;

    /**
     * @brief Switches the active profile namespace and flushes in-memory cache.
     * @param profileName New profile name.
     */
    void setProfile(const QString &profileName) override;

    /**
     * @brief Retrieves the active profile name.
     */
    QString profile() const override;

    /**
     * @brief Retrieves the persisted backend server address.
     */
    QString serverUrl() const override;

    /**
     * @brief Stores the backend server address.
     * @param url Server URL.
     */
    void setServerUrl(const QString &url) override;

    /**
     * @brief Retrieves the active session token, returning cached value if valid.
     */
    QString authToken() const override;

    /**
     * @brief Persists and caches the active auth token.
     * @param token Authentication token string.
     */
    void setAuthToken(const QString &token) override;

    /**
     * @brief Retrieves the active local username.
     */
    QString username() const override;

    /**
     * @brief Persists and caches the local username.
     * @param username Username string.
     */
    void setUsername(const QString &username) override;

    /**
     * @brief Retrieves the client device UUID, generating one if not yet stored.
     */
    QString deviceId() const override;

    /**
     * @brief Persists and caches the client device UUID.
     * @param id Device UUID.
     */
    void setDeviceId(const QString &id) override;

    /**
     * @brief Retrieves the base64-encoded public key.
     */
    QString publicKey() const override;

    /**
     * @brief Persists and caches the public key.
     * @param key Base64 public key.
     */
    void setPublicKey(const QString &key) override;

    /**
     * @brief Retrieves the persisted list of friend usernames.
     */
    QStringList friends() const override;

    /**
     * @brief Persists the list of friend usernames.
     * @param friends Friend usernames.
     */
    void setFriends(const QStringList &friends) override;

    /**
     * @brief Retrieves the pending incoming friend request usernames.
     */
    QStringList pendingRequests() const override;

    /**
     * @brief Persists incoming friend request usernames.
     * @param requests Request usernames.
     */
    void setPendingRequests(const QStringList &requests) override;

    /**
     * @brief Retrieves saved server bookmarks.
     */
    QVariantList bookmarks() const override;

    /**
     * @brief Overwrites server bookmarks.
     * @param bookmarks List of bookmark maps.
     */
    void setBookmarks(const QVariantList &bookmarks) override;

    /**
     * @brief Appends a new bookmark to persistent storage.
     * @param bookmark Map containing bookmark data.
     */
    void addBookmark(const QVariantMap &bookmark) override;

    /**
     * @brief Updates an existing bookmark matching `id`.
     * @param bookmark Modified bookmark map.
     */
    void updateBookmark(const QVariantMap &bookmark) override;

    /**
     * @brief Deletes a bookmark by UUID.
     * @param id Bookmark UUID.
     */
    void removeBookmark(const QString &id) override;

    /**
     * @brief Retrieves the list of actively opened conversation channels/DMs.
     */
    QVariantList openConversations() const override;

    /**
     * @brief Persists open conversation channels.
     * @param conversations Conversation descriptor list.
     */
    void setOpenConversations(const QVariantList &conversations) override;

    /**
     * @brief Retrieves the local display name.
     */
    QString displayName() const override;

    /**
     * @brief Persists and caches the local display name.
     * @param displayName Nickname or display alias.
     */
    void setDisplayName(const QString &displayName) override;

    /**
     * @brief Retrieves a custom display name override for a peer.
     * @param username Peer username.
     */
    QString peerDisplayName(const QString &username) const override;

    /**
     * @brief Stores a custom display name override for a peer.
     * @param username Peer username.
     * @param displayName Custom alias.
     */
    void setPeerDisplayName(const QString &username, const QString &displayName) override;

    /**
     * @brief Retrieves the local user's avatar URL.
     */
    QString avatarUrl() const override;

    /**
     * @brief Persists and caches the local user's avatar URL.
     * @param url Avatar file path or URL.
     */
    void setAvatarUrl(const QString &url) override;

    /**
     * @brief Retrieves the cached avatar URL for a peer.
     * @param username Peer username.
     */
    QString peerAvatarUrl(const QString &username) const override;

    /**
     * @brief Persists the cached avatar URL for a peer.
     * @param username Peer username.
     * @param url Avatar URL.
     */
    void setPeerAvatarUrl(const QString &username, const QString &url) override;

    /**
     * @brief Clears active session tokens, credentials, and in-memory caches upon logout.
     */
    void clearSession() override;

private:
    /** @brief Computes the QSettings group key based on the active profile. */
    QString getGroupName() const;
    /** @brief Returns the settings key for open conversations. */
    QString getConversationsKey() const;
    /** @brief Returns the settings key for friends list. */
    QString getFriendsKey() const;
    /** @brief Returns the settings key for pending friend requests. */
    QString getPendingRequestsKey() const;
    /** @brief Returns the settings key for device ID. */
    QString getDeviceIdKey() const;
    /** @brief Returns the settings key for public key. */
    QString getPublicKeyKey() const;
    /** @brief Returns the settings key for user display name. */
    QString getDisplayNameKey() const;
    /** @brief Returns the settings key for peer display names map. */
    QString getPeerDisplayNamesKey() const;
    /** @brief Returns the settings key for user avatar URL. */
    QString getAvatarUrlKey() const;
    /** @brief Returns the settings key for peer avatar URLs map. */
    QString getPeerAvatarsKey() const;

    /** @brief Mutex protecting concurrent access and cache updates. */
    mutable std::mutex m_mutex;
    /** @brief Active user profile namespace. */
    QString m_profile;
    /** @brief Read-through cached authentication token. */
    mutable QString m_cachedAuthToken;
    /** @brief Read-through cached username. */
    mutable QString m_cachedUsername;
    /** @brief Read-through cached device ID. */
    mutable QString m_cachedDeviceId;
    /** @brief Read-through cached public key. */
    mutable QString m_cachedPublicKey;
    /** @brief Read-through cached display name. */
    mutable QString m_cachedDisplayName;
    /** @brief Read-through cached avatar URL. */
    mutable QString m_cachedAvatarUrl;
};

} // namespace Storage
} // namespace NeoNect
