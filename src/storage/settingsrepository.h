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
 *
 * @par Concurrency Constraints:
 * - All public member methods synchronize internally via `std::lock_guard<std::mutex> lock(m_mutex)`.
 * - Callers may invoke any member function concurrently across arbitrary QThreads without external locking.
 * - Cache mutation and `QSettings` disk writes occur within the same critical section to prevent dirty reads.
 *
 * @par Security Invariants:
 * - The session auth token and sensitive private configurations are encrypted before writing to `QSettings`.
 * - In-memory cache is immediately invalidated upon `setProfile()` or `clearSession()`.
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
 *
 * @par Concurrency:
 * Safe for concurrent invocation across multiple threads. All methods acquire `m_mutex`.
 */
class SettingsRepository : public ISettingsRepository {
public:
    /**
     * @brief Constructs the settings repository for a designated profile namespace.
     * @param profileName Optional profile name (e.g. `"default"`). Defaults to empty, which maps to `"default"`.
     * @post Internal cache is initialized to empty state.
     */
    explicit SettingsRepository(const QString &profileName = QString());

    /**
     * @brief Virtual destructor ensuring clean teardown.
     */
    ~SettingsRepository() override = default;

    /**
     * @brief Switches the active profile namespace and flushes in-memory cache.
     * @param profileName New profile name.
     * @pre `profileName` must not contain `/`, `\`, or path traversal sequences. Length <= 64 chars.
     * @post In-memory cache strings are cleared. Subsequent requests read from the new profile section.
     */
    void setProfile(const QString &profileName) override;

    /**
     * @brief Retrieves the active profile name.
     * @return Active profile name (e.g. `"default"`).
     */
    QString profile() const override;

    /**
     * @brief Retrieves the persisted backend server address.
     * @return Server URL or default fallback if unset.
     */
    QString serverUrl() const override;

    /**
     * @brief Stores the backend server address.
     * @param url Server URL.
     * @pre `url` must follow RFC 3986 with scheme `http`, `https`, `ws`, or `wss`. Max length 2048 chars.
     * @post Written to persistent storage and flushed.
     */
    void setServerUrl(const QString &url) override;

    /**
     * @brief Retrieves the active session token, returning cached value if valid.
     * @return Decrypted JWT token string or empty string.
     * @post Reads through in-memory cache if populated; otherwise decrypts from disk.
     */
    QString authToken() const override;

    /**
     * @brief Persists and caches the active auth token.
     * @param token Authentication token string.
     * @pre `token` must be valid compact JWT or empty. Max length 4096 bytes.
     * @post Encrypted via AES-256-GCM machine key, saved to disk, and updated in `m_cachedAuthToken`.
     */
    void setAuthToken(const QString &token) override;

    /**
     * @brief Retrieves the active local username.
     * @return Username string or empty if unauthenticated.
     */
    QString username() const override;

    /**
     * @brief Persists and caches the local username.
     * @param username Username string.
     * @pre `username` length between 3 and 32 characters.
     * @post Persisted to settings and updated in `m_cachedUsername`.
     */
    void setUsername(const QString &username) override;

    /**
     * @brief Retrieves the client device UUID, generating one if not yet stored.
     * @return 36-character RFC 4122 UUID v4.
     * @post Generated once and permanently persisted; never changes across invocations.
     */
    QString deviceId() const override;

    /**
     * @brief Persists and caches the client device UUID.
     * @param id Device UUID.
     * @pre Valid RFC 4122 UUID string.
     * @post Updated in persistent storage and `m_cachedDeviceId`.
     */
    void setDeviceId(const QString &id) override;

    /**
     * @brief Retrieves the base64-encoded public key.
     * @return Base64 public key string.
     */
    QString publicKey() const override;

    /**
     * @brief Persists and caches the public key.
     * @param key Base64 public key.
     * @pre Max length 1024 characters.
     * @post Updated in disk storage and `m_cachedPublicKey`.
     */
    void setPublicKey(const QString &key) override;

    /**
     * @brief Retrieves the persisted list of friend usernames.
     * @return List of usernames.
     */
    QStringList friends() const override;

    /**
     * @brief Persists the list of friend usernames.
     * @param friends Friend usernames.
     * @pre Maximum 10,000 friends.
     * @post Written to disk.
     */
    void setFriends(const QStringList &friends) override;

    /**
     * @brief Retrieves the pending incoming friend request usernames.
     * @return List of request usernames.
     */
    QStringList pendingRequests() const override;

    /**
     * @brief Persists incoming friend request usernames.
     * @param requests Request usernames.
     * @pre Maximum 5,000 requests.
     * @post Written to disk.
     */
    void setPendingRequests(const QStringList &requests) override;

    /**
     * @brief Retrieves saved server bookmarks.
     * @return List of bookmark variant maps.
     */
    QVariantList bookmarks() const override;

    /**
     * @brief Overwrites server bookmarks.
     * @param bookmarks List of bookmark maps.
     * @pre Maximum 1,000 bookmarks.
     * @post Overwrites existing bookmark array on disk.
     */
    void setBookmarks(const QVariantList &bookmarks) override;

    /**
     * @brief Appends a new bookmark to persistent storage.
     * @param bookmark Map containing bookmark data (`id`, `name`, `serverUrl`).
     * @pre `bookmark` must have non-empty `"id"`.
     * @post Appended to stored bookmark list.
     */
    void addBookmark(const QVariantMap &bookmark) override;

    /**
     * @brief Updates an existing bookmark matching `id`.
     * @param bookmark Modified bookmark map.
     * @pre `bookmark` must contain existing `"id"`.
     * @post Matching entry updated in place.
     */
    void updateBookmark(const QVariantMap &bookmark) override;

    /**
     * @brief Deletes a bookmark by UUID.
     * @param id Bookmark UUID.
     * @pre `id` is non-empty.
     * @post If entry exists, it is removed; otherwise no-op.
     */
    void removeBookmark(const QString &id) override;

    /**
     * @brief Retrieves the list of actively opened conversation channels/DMs.
     * @return List of conversation descriptors.
     */
    QVariantList openConversations() const override;

    /**
     * @brief Persists open conversation channels.
     * @param conversations Conversation descriptor list.
     * @pre Maximum 50 open conversations.
     * @post Written to profile settings.
     */
    void setOpenConversations(const QVariantList &conversations) override;

    /**
     * @brief Retrieves the local display name.
     * @return Display alias or empty string.
     */
    QString displayName() const override;

    /**
     * @brief Persists and caches the local display name.
     * @param displayName Nickname or display alias.
     * @pre Maximum length 64 characters.
     * @post Written to settings and `m_cachedDisplayName`.
     */
    void setDisplayName(const QString &displayName) override;

    /**
     * @brief Retrieves a custom display name override for a peer.
     * @param username Peer username.
     * @return Custom alias or empty string.
     */
    QString peerDisplayName(const QString &username) const override;

    /**
     * @brief Stores a custom display name override for a peer.
     * @param username Peer username.
     * @param displayName Custom alias.
     * @pre `username` is non-empty, `displayName` max 64 characters.
     * @post Saved in peer display name dictionary.
     */
    void setPeerDisplayName(const QString &username, const QString &displayName) override;

    /**
     * @brief Retrieves the local user's avatar URL.
     * @return URI or local filepath.
     */
    QString avatarUrl() const override;

    /**
     * @brief Persists and caches the local user's avatar URL.
     * @param url Avatar file path or URL.
     * @pre Maximum length 2048 characters.
     * @post Saved in settings and `m_cachedAvatarUrl`.
     */
    void setAvatarUrl(const QString &url) override;

    /**
     * @brief Retrieves the cached avatar URL for a peer.
     * @param username Peer username.
     * @return Avatar URL or empty string.
     */
    QString peerAvatarUrl(const QString &username) const override;

    /**
     * @brief Persists the cached avatar URL for a peer.
     * @param username Peer username.
     * @param url Avatar URL.
     * @pre `username` non-empty, `url` max 2048 characters.
     * @post Saved in peer avatar dictionary.
     */
    void setPeerAvatarUrl(const QString &username, const QString &url) override;

    /**
     * @brief Clears active session tokens, credentials, and in-memory caches upon logout.
     * @post `m_cachedAuthToken` is empty, `m_cachedUsername` is empty, persistent auth token is removed.
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

    /** @brief Mutex protecting concurrent access and cache updates across threads. */
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
