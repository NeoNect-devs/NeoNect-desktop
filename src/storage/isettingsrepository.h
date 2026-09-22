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
 *
 * @par Concurrency & Invariant Constraints:
 * - <b>Thread Safety</b>: Implementations must guarantee reentrant and safe concurrent calls from
 *   GUI main thread, background network workers, and transport threads.
 * - <b>Atomicity</b>: Profile transitions must atomically flush caches before switching context.
 * - <b>Cryptographic Security</b>: Sensitive credentials (session tokens, passwords) must be
 *   encrypted at rest using OS keystore or local AES-256-GCM cipher with machine-bound keys.
 */

#pragma once
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace NeoNect {

/**
 * @namespace NeoNect::Storage
 * @brief Persistent storage layer, repository abstractions, and relational database managers.
 * @details Manages encrypted QSettings profile stores, asynchronous SQLite WAL history databases,
 * and user configuration persistence.
 */
namespace Storage {

/**
 * @class ISettingsRepository
 * @brief Abstract contract for persistent configuration and credential storage.
 *
 * @par Preconditions and Operational Bounds:
 * - Profile identifiers must not exceed 64 characters and must not contain illegal path characters.
 * - URLs must conform to RFC 3986 with maximum length of 2048 characters.
 * - Session tokens must adhere to compact JWT format with maximum length of 4096 bytes.
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
     * @pre `profileName` must not contain path traversal characters (`..`, `/`, `\`) or reserved symbols.
     * @pre Length of `profileName` must be between 1 and 64 characters.
     * @post The repository namespace switches to the given profile, and any profile-scoped memory
     *       caches are flushed and re-synchronized from disk.
     */
    virtual void setProfile(const QString &profileName) = 0;

    /**
     * @brief Returns the name of the currently active profile.
     * @return Active profile identifier string (defaults to `"default"` if unset).
     * @post Return value is non-empty.
     */
    virtual QString profile() const = 0;

    /**
     * @brief Retrieves the default or currently configured server URL.
     * @return Canonical server base URL string.
     */
    virtual QString serverUrl() const = 0;

    /**
     * @brief Stores the base URL of the remote relay/auth server.
     * @param url Base URL (e.g. `"https://api.neonect.chat"`).
     * @pre `url` must follow RFC 3986 URI syntax with scheme `http://`, `https://`, `ws://`, or `wss://`.
     * @pre Max length is 2048 characters.
     * @post Persistent storage is updated and cached in memory.
     */
    virtual void setServerUrl(const QString &url) = 0;

    /**
     * @brief Retrieves the active JWT or bearer session token.
     * @return Decrypted session token string, or empty string if unauthenticated.
     */
    virtual QString authToken() const = 0;

    /**
     * @brief Stores the bearer session token received upon successful authentication.
     * @param token Authentication token string.
     * @pre `token` must be a compact base64url JWT or empty string upon logout. Max length 4096 bytes.
     * @post Token is encrypted at rest using machine-bound AES-256-GCM before saving to disk.
     */
    virtual void setAuthToken(const QString &token) = 0;

    /**
     * @brief Retrieves the authenticated local username.
     * @return Username string or empty if not logged in.
     */
    virtual QString username() const = 0;

    /**
     * @brief Stores the authenticated local username.
     * @param username Unique username string.
     * @pre `username` must be between 3 and 32 characters, matching alphanumeric/dash/underscore charset.
     * @post Value is written to profile settings and cached.
     */
    virtual void setUsername(const QString &username) = 0;

    /**
     * @brief Retrieves the persistent client device ID (UUID v4).
     * @return 36-character canonical RFC 4122 UUID string.
     * @post Return value is non-empty and stable across reboots.
     */
    virtual QString deviceId() const = 0;

    /**
     * @brief Stores the unique client device identifier.
     * @param id Device UUID.
     * @pre `id` must be a valid RFC 4122 UUID string (36 chars, 8-4-4-4-12 format).
     * @post Value is committed to persistent storage.
     */
    virtual void setDeviceId(const QString &id) = 0;

    /**
     * @brief Retrieves the base64-encoded public key for this device.
     * @return Base64-encoded cryptographic public key.
     */
    virtual QString publicKey() const = 0;

    /**
     * @brief Stores the base64-encoded public key for this device.
     * @param key Public key string.
     * @pre `key` must be a valid base64-encoded public key representation (max 1024 chars).
     * @post Key is stored under current profile namespace.
     */
    virtual void setPublicKey(const QString &key) = 0;

    /**
     * @brief Retrieves the cached list of approved friend usernames.
     * @return List of unique friend usernames.
     * @par Invariant:
     * - Contains no duplicate usernames.
     */
    virtual QStringList friends() const = 0;

    /**
     * @brief Persists the list of friend usernames.
     * @param friends List of usernames.
     * @pre Total count must not exceed 10,000 entries.
     * @post Friend list is persisted and cached.
     */
    virtual void setFriends(const QStringList &friends) = 0;

    /**
     * @brief Retrieves the pending incoming friend request usernames.
     * @return List of usernames with pending requests.
     */
    virtual QStringList pendingRequests() const = 0;

    /**
     * @brief Persists the pending friend requests.
     * @param requests List of usernames.
     * @pre Total count must not exceed 5,000 entries.
     * @post Pending requests list is committed.
     */
    virtual void setPendingRequests(const QStringList &requests) = 0;

    /**
     * @brief Retrieves the saved TeamSpeak-style server bookmarks.
     * @return List of bookmark property maps.
     */
    virtual QVariantList bookmarks() const = 0;

    /**
     * @brief Overwrites all server bookmarks.
     * @param bookmarks List of bookmark property maps.
     * @pre Each map entry in `bookmarks` must contain at least `"id"`, `"name"`, and `"serverUrl"`.
     * @post Existing bookmarks are replaced with new list.
     */
    virtual void setBookmarks(const QVariantList &bookmarks) = 0;

    /**
     * @brief Adds a new server bookmark to persistent storage.
     * @param bookmark Map containing bookmark details (`name`, `serverUrl`, `username`, `password`, `id`).
     * @pre `bookmark` must contain a non-empty `"id"` string and valid `"serverUrl"`.
     * @post Total bookmark count increases by 1.
     */
    virtual void addBookmark(const QVariantMap &bookmark) = 0;

    /**
     * @brief Updates an existing server bookmark.
     * @param bookmark Modified bookmark map matching on `id`.
     * @pre `bookmark` must contain an `"id"` matching an existing bookmark record.
     * @post The matching bookmark entry is updated in place.
     */
    virtual void updateBookmark(const QVariantMap &bookmark) = 0;

    /**
     * @brief Removes a server bookmark by its unique identifier.
     * @param id Unique bookmark UUID.
     * @pre `id` must be non-empty.
     * @post If matching bookmark exists, it is removed; otherwise no-op.
     */
    virtual void removeBookmark(const QString &id) = 0;

    /**
     * @brief Retrieves the list of actively opened conversation channels/DMs.
     * @return List of conversation descriptors for workspace restoration.
     */
    virtual QVariantList openConversations() const = 0;

    /**
     * @brief Persists the list of open conversation channels for session restoration.
     * @param conversations List of conversation descriptor maps.
     * @pre Total open conversations count <= 50.
     * @post Descriptors are saved to profile state.
     */
    virtual void setOpenConversations(const QVariantList &conversations) = 0;

    /**
     * @brief Retrieves the user's custom display name alias.
     * @return Display name or empty if using username.
     */
    virtual QString displayName() const = 0;

    /**
     * @brief Sets the user's custom display name alias.
     * @param displayName Nickname or full name.
     * @pre Max length is 64 characters.
     * @post Persisted to active profile.
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
     * @pre `username` must be non-empty, `displayName` max length 64 chars.
     * @post Stored in peer metadata dictionary.
     */
    virtual void setPeerDisplayName(const QString &username, const QString &displayName) = 0;

    /**
     * @brief Retrieves the local user's avatar image URL or file path.
     * @return URI or local filesystem path.
     */
    virtual QString avatarUrl() const = 0;

    /**
     * @brief Sets the local user's avatar URL or file path.
     * @param url Avatar file or remote URL.
     * @pre Max length 2048 chars.
     * @post Stored in profile state.
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
     * @pre `username` must be non-empty, `url` max length 2048 chars.
     * @post Stored in peer avatar dictionary.
     */
    virtual void setPeerAvatarUrl(const QString &username, const QString &url) = 0;

    /**
     * @brief Clears active session tokens, usernames, and transient state upon logout.
     * @post `authToken()` is empty string.
     * @post Local cache is purged.
     * @post Permanent settings (device ID, bookmarks, server URLs) remain intact.
     */
    virtual void clearSession() = 0;
};

} // namespace Storage
} // namespace NeoNect
