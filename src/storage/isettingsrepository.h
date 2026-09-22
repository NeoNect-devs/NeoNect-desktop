/**
 * @file isettingsrepository.h
 * @brief Abstract repository interface for client configurations, profiles, and encrypted credentials.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Defines the contract for persistence of client state, authentication sessions, device keys,
 * peer metadata, server bookmarks, and open conversation tabs. Implementations provide thread-safe
 * access and profile-isolated namespaces.
 *
 * @par Design Pattern:
 * - <b>Repository Pattern</b>: Mediates between the domain/service layers and the data mapping layer.
 * - <b>Interface Segregation</b>: Clear separation of configuration read/write capabilities.
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace NeoNect {
namespace Storage {

/**
 * @class ISettingsRepository
 * @brief Abstract contract for persistent configuration and credential storage.
 */
class ISettingsRepository {
public:
    /**
     * @brief Virtual destructor for clean polymorphic disposal.
     */
    virtual ~ISettingsRepository() = default;

    /**
     * @brief Switches the active user profile namespace.
     * @param profileName Profile identifier (e.g. `"default"`, `"work"`, or a specific username).
     */
    virtual void setProfile(const QString &profileName) = 0;

    /**
     * @brief Returns the name of the currently active profile.
     */
    virtual QString profile() const = 0;

    /**
     * @brief Retrieves the default or currently configured server URL.
     */
    virtual QString serverUrl() const = 0;

    /**
     * @brief Stores the base URL of the remote relay/auth server.
     * @param url Base URL (e.g. `"https://api.neonect.chat"`).
     */
    virtual void setServerUrl(const QString &url) = 0;

    /**
     * @brief Retrieves the active JWT or bearer session token.
     */
    virtual QString authToken() const = 0;

    /**
     * @brief Stores the bearer session token received upon successful authentication.
     * @param token Authentication token string.
     */
    virtual void setAuthToken(const QString &token) = 0;

    /**
     * @brief Retrieves the authenticated local username.
     */
    virtual QString username() const = 0;

    /**
     * @brief Stores the authenticated local username.
     * @param username Unique username string.
     */
    virtual void setUsername(const QString &username) = 0;

    /**
     * @brief Retrieves the persistent client device ID (UUID v4).
     */
    virtual QString deviceId() const = 0;

    /**
     * @brief Stores the unique client device identifier.
     * @param id Device UUID.
     */
    virtual void setDeviceId(const QString &id) = 0;

    /**
     * @brief Retrieves the base64-encoded public key for this device.
     */
    virtual QString publicKey() const = 0;

    /**
     * @brief Stores the base64-encoded public key for this device.
     * @param key Public key string.
     */
    virtual void setPublicKey(const QString &key) = 0;

    /**
     * @brief Retrieves the cached list of approved friend usernames.
     */
    virtual QStringList friends() const = 0;

    /**
     * @brief Persists the list of friend usernames.
     * @param friends List of usernames.
     */
    virtual void setFriends(const QStringList &friends) = 0;

    /**
     * @brief Retrieves the pending incoming friend request usernames.
     */
    virtual QStringList pendingRequests() const = 0;

    /**
     * @brief Persists the pending friend requests.
     * @param requests List of usernames.
     */
    virtual void setPendingRequests(const QStringList &requests) = 0;

    /**
     * @brief Retrieves the saved TeamSpeak-style server bookmarks.
     */
    virtual QVariantList bookmarks() const = 0;

    /**
     * @brief Overwrites all server bookmarks.
     * @param bookmarks List of bookmark property maps.
     */
    virtual void setBookmarks(const QVariantList &bookmarks) = 0;

    /**
     * @brief Adds a new server bookmark to persistent storage.
     * @param bookmark Map containing bookmark details (`name`, `serverUrl`, `username`, `password`, `id`).
     */
    virtual void addBookmark(const QVariantMap &bookmark) = 0;

    /**
     * @brief Updates an existing server bookmark.
     * @param bookmark Modified bookmark map matching on `id`.
     */
    virtual void updateBookmark(const QVariantMap &bookmark) = 0;

    /**
     * @brief Removes a server bookmark by its unique identifier.
     * @param id Unique bookmark UUID.
     */
    virtual void removeBookmark(const QString &id) = 0;

    /**
     * @brief Retrieves the list of actively opened conversation channels/DMs.
     */
    virtual QVariantList openConversations() const = 0;

    /**
     * @brief Persists the list of open conversation channels for session restoration.
     * @param conversations List of conversation descriptor maps.
     */
    virtual void setOpenConversations(const QVariantList &conversations) = 0;

    /**
     * @brief Retrieves the user's custom display name alias.
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Sets the user's custom display name alias.
     * @param displayName Nickname or full name.
     */
    virtual void setDisplayName(const QString &displayName) = 0;

    /**
     * @brief Retrieves a custom display name alias set for a specific peer.
     * @param username Target peer's username.
     * @return Custom alias or empty string if not overridden.
     */
    virtual QString peerDisplayName(const QString &username) const = 0;

    /**
     * @brief Sets a custom display name alias for a specific peer.
     * @param username Target peer's username.
     * @param displayName Custom alias.
     */
    virtual void setPeerDisplayName(const QString &username, const QString &displayName) = 0;

    /**
     * @brief Retrieves the local user's avatar image URL or file path.
     */
    virtual QString avatarUrl() const = 0;

    /**
     * @brief Sets the local user's avatar URL or file path.
     * @param url Avatar file or remote URL.
     */
    virtual void setAvatarUrl(const QString &url) = 0;

    /**
     * @brief Retrieves the cached avatar URL for a specific peer.
     * @param username Peer username.
     * @return Avatar URL or empty string if none cached.
     */
    virtual QString peerAvatarUrl(const QString &username) const = 0;

    /**
     * @brief Caches the avatar URL for a specific peer.
     * @param username Peer username.
     * @param url Avatar URL string.
     */
    virtual void setPeerAvatarUrl(const QString &username, const QString &url) = 0;

    /**
     * @brief Clears active session tokens, usernames, and transient state upon logout.
     */
    virtual void clearSession() = 0;
};

} // namespace Storage
} // namespace NeoNect
