/**
 * @file networkmanager.h
 * @brief Master network facade and state coordinator integrating all communication services for QML.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Serves as the primary communication facade between the QML declarative UI and the C++ service
 * layer. Orchestrates authentication (`AuthService`), device identity (`DeviceService`),
 * friend relationships (`FriendService`), and cryptographic relays (`RelayService`). Manages
 * active user sessions, TeamSpeak-style server bookmarks, auto-idle presence timers, and direct
 * conversation tabs.
 *
 * @par Design Patterns:
 * - <b>Facade Pattern</b>: Shields QML views from the distributed complexity of four underlying domain services.
 * - <b>Mediator Pattern</b>: Coordinates inter-service workflows (e.g. login -> device registration -> relay connection).
 * - <b>Observer Pattern</b>: Aggregates and rebroadcasts state change events and push notifications to QML.
 */

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"
#include "../crypto/icryptoservice.h"
#include "../services/authservice.h"
#include "../services/deviceservice.h"
#include "../services/relayservice.h"
#include "../services/friendservice.h"

/**
 * @class NetworkManager
 * @brief Master networking facade exposed to the QML user interface.
 */
class NetworkManager : public QObject {
    Q_OBJECT

    /** @brief Active server backend URL. */
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    /** @brief Active Bearer authentication token. */
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    /** @brief Authenticated local username. */
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY currentUsernameChanged)
    /** @brief Custom display name alias for local user. */
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    /** @brief Avatar image file URL or remote URI for local user. */
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    /** @brief True when an asynchronous network operation is pending. */
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    /** @brief True if transport connection (WebSocket/HTTP) is active. */
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    /** @brief List of confirmed friend usernames. */
    Q_PROPERTY(QStringList friends READ friends NOTIFY friendsChanged)
    /** @brief List of pending friend request usernames. */
    Q_PROPERTY(QStringList pendingRequests READ pendingRequests NOTIFY pendingRequestsChanged)
    /** @brief List of saved server bookmarks. */
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    /** @brief List of currently opened direct conversation tabs. */
    Q_PROPERTY(QVariantList openConversations READ openConversations NOTIFY openConversationsChanged)
    /** @brief Explicit user-selected presence preference (`"online"`, `"idle"`, `"dnd"`, `"offline"`). */
    Q_PROPERTY(QString userStatus READ userStatus WRITE setUserStatus NOTIFY userStatusChanged)
    /** @brief Effective presence status taking auto-idle timers into account. */
    Q_PROPERTY(QString effectiveStatus READ effectiveStatus NOTIFY effectiveStatusChanged)
    /** @brief True if user is currently marked as idle/AFK. */
    Q_PROPERTY(bool isIdle READ isIdle NOTIFY isIdleChanged)
    /** @brief True if stealth/invisible mode is enabled. */
    Q_PROPERTY(bool isInvisible READ isInvisible NOTIFY isInvisibleChanged)
    /** @brief True if Do-Not-Disturb is active. */
    Q_PROPERTY(bool isDnd READ isDnd NOTIFY isDndChanged)

public:
    /**
     * @brief Constructs the master network facade and wires inter-service signal pipelines.
     * @param transport Shared pointer to HTTP transport.
     * @param storage Shared pointer to settings repository.
     * @param cryptoService Shared pointer to crypto engine.
     * @param authService Shared pointer to authentication service.
     * @param deviceService Shared pointer to device identity service.
     * @param relayService Shared pointer to relay gateway service.
     * @param friendService Shared pointer to friend management service.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit NetworkManager(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport,
                            std::shared_ptr<NeoNect::Storage::ISettingsRepository> storage,
                            std::shared_ptr<NeoNect::Crypto::ICryptoService> cryptoService,
                            std::shared_ptr<NeoNect::Services::AuthService> authService,
                            std::shared_ptr<NeoNect::Services::DeviceService> deviceService,
                            std::shared_ptr<NeoNect::Services::RelayService> relayService,
                            std::shared_ptr<NeoNect::Services::FriendService> friendService,
                            QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~NetworkManager() override = default;

    /**
     * @brief Reconfigures the facade with a custom HTTP transport (e.g. for testing).
     * @param transport Injected custom transport implementation.
     */
    void initializeCustom(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport);

    /** @brief Returns active server base URL. */
    QString serverUrl() const;
    /** @brief Returns active session Bearer token. */
    QString token() const;
    /** @brief Returns authenticated local username. */
    QString currentUsername() const;
    /** @brief Returns user display name. */
    QString displayName() const;
    /** @brief Returns local avatar image URL. */
    QString avatarUrl() const;
    /** @brief Checks if a background request is in progress. */
    bool isLoading() const { return m_isLoading; }
    /** @brief Checks if connection to server is active. */
    bool isConnected() const;
    /** @brief Returns cached friends list. */
    QStringList friends() const;
    /** @brief Returns pending friend requests list. */
    QStringList pendingRequests() const;
    /** @brief Returns server bookmarks list. */
    QVariantList bookmarks() const;

    /**
     * @brief Updates the local user's display name alias.
     * @param displayName Custom display name string.
     */
    Q_INVOKABLE void setDisplayName(const QString &displayName);

    /**
     * @brief Queries custom display name alias for a specific peer.
     * @param username Target peer username.
     * @return Custom display name or empty string if default.
     */
    Q_INVOKABLE QString getDisplayName(const QString &username) const;

    /**
     * @brief Sets a custom display name alias for a specific peer.
     * @param username Target peer username.
     * @param displayName Custom alias.
     */
    Q_INVOKABLE void setPeerDisplayName(const QString &username, const QString &displayName);

    /**
     * @brief Updates user avatar image from a local file path or data URI.
     * @param filePathOrUrl Source image path.
     * @return True if avatar image was accepted and saved.
     */
    Q_INVOKABLE bool setAvatar(const QString &filePathOrUrl);

    /**
     * @brief Clears the user's custom avatar.
     */
    Q_INVOKABLE void clearAvatar();

    /**
     * @brief Retrieves cached avatar URL for a peer.
     * @param username Target peer username.
     * @return Avatar URL or fallback default.
     */
    Q_INVOKABLE QString getAvatarUrl(const QString &username) const;

    /**
     * @brief Caches avatar URL for a peer.
     * @param username Target peer username.
     * @param avatarUrl Avatar URL string.
     */
    Q_INVOKABLE void setPeerAvatarUrl(const QString &username, const QString &avatarUrl);

    /**
     * @brief Switches the active user profile namespace.
     * @param profileName Profile name.
     */
    Q_INVOKABLE void setProfile(const QString &profileName);

    /**
     * @brief Tests connectivity and validates compatibility of a remote server.
     * @param address Server address string.
     */
    Q_INVOKABLE void verifyServer(const QString &address);

    /**
     * @brief Checks if a desired username is available for registration.
     * @param username Queried username.
     */
    Q_INVOKABLE void checkUsernameAvailability(const QString &username);

    /**
     * @brief Submits a user registration request to the server.
     * @param username Chosen username.
     * @param password Account password.
     */
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);

    /**
     * @brief Authenticates user credentials with server and establishes session.
     * @param username Account username.
     * @param password Account password.
     */
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);

    /**
     * @brief Logs out current user, invalidates session token, and resets state.
     */
    Q_INVOKABLE void logoutUser();

    /**
     * @brief Alias for @ref logoutUser.
     */
    Q_INVOKABLE void logout() { logoutUser(); }

    // Bookmark Management (TeamSpeak style)
    /**
     * @brief Saves or updates a server quick-connect bookmark.
     * @param name Descriptive label.
     * @param serverUrl Server address.
     * @param username Auto-login username.
     * @param password Auto-login password.
     * @param id Optional UUID to update existing bookmark.
     */
    Q_INVOKABLE void saveBookmark(const QString &name, const QString &serverUrl, const QString &username, const QString &password, const QString &id = "");

    /**
     * @brief Deletes a server bookmark by ID.
     * @param id Bookmark UUID.
     */
    Q_INVOKABLE void deleteBookmark(const QString &id);

    /**
     * @brief Connects to and authenticates with a bookmarked server.
     * @param id Bookmark UUID.
     */
    Q_INVOKABLE void connectBookmark(const QString &id);

    /**
     * @brief Registers the local client device identity on the server.
     * @param deviceId Device UUID.
     * @param publicKey Base64 public key.
     */
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &publicKey);

    /**
     * @brief Fetches the public key associated with a specific device.
     * @param deviceId Device UUID.
     */
    Q_INVOKABLE void fetchDevicePublicKey(const QString &deviceId);

    /**
     * @brief Revokes a registered client device.
     * @param deviceId Device UUID.
     */
    Q_INVOKABLE void revokeDevice(const QString &deviceId);

    /**
     * @brief Queries public keys for all devices registered by a recipient.
     * @param username Recipient username.
     */
    Q_INVOKABLE void fetchRecipientKeys(const QString &username);

    /**
     * @brief Synchronizes user account profile information from server.
     */
    Q_INVOKABLE void fetchUserProfile();

    // E2EE Relay & Direct Chat
    /**
     * @brief Triggers immediate HTTP query for pending relay messages.
     */
    Q_INVOKABLE void pollPendingMessages();

    /**
     * @brief Sends an acknowledgment to prune a received relay message.
     * @param messageId Server relay message ID.
     */
    Q_INVOKABLE void acknowledgeMessage(qint64 messageId);

    // Friends & DM Management
    /**
     * @brief Sends a friend request to a target user.
     * @param username Target username.
     */
    Q_INVOKABLE void addFriend(const QString &username);

    /**
     * @brief Accepts an incoming friend request.
     * @param username Target username.
     */
    Q_INVOKABLE void acceptFriend(const QString &username);

    /**
     * @brief Rejects an incoming friend request.
     * @param username Target username.
     */
    Q_INVOKABLE void rejectFriend(const QString &username);

    /**
     * @brief Removes a user from friends list.
     * @param username Target username.
     */
    Q_INVOKABLE void removeFriend(const QString &username);

    /**
     * @brief Queries online status and last-seen activity for all friends.
     */
    Q_INVOKABLE void checkFriendsStatus();

    /**
     * @brief Queries online status for a specific user.
     * @param username Target username.
     */
    Q_INVOKABLE void checkUserStatus(const QString &username);

    /**
     * @brief Returns cached presence status string for a friend.
     * @param username Target username.
     */
    Q_INVOKABLE QString getFriendStatus(const QString &username) const;

    /**
     * @brief Returns map of all known friend statuses.
     */
    Q_INVOKABLE QVariantMap allFriendStatuses() const;

    // Presence & Status Management
    /** @brief Returns user status preference. */
    QString userStatus() const { return m_userStatusPreference; }
    /** @brief Calculates effective presence status factoring in auto-idle timers. */
    QString effectiveStatus() const;
    /** @brief Checks if user is auto-idle or manually AFK. */
    bool isIdle() const { return m_isAutoIdle || m_userStatusPreference == "afk" || m_userStatusPreference == "idle"; }
    /** @brief Checks if stealth mode is active. */
    bool isInvisible() const { return m_userStatusPreference == "offline"; }
    /** @brief Checks if DND mode is active. */
    bool isDnd() const { return m_userStatusPreference == "dnd"; }

    /**
     * @brief Updates user's explicit presence preference.
     * @param status Status keyword (`"online"`, `"idle"`, `"dnd"`, `"offline"`).
     */
    Q_INVOKABLE void setUserStatus(const QString &status);

    /**
     * @brief Resets auto-idle activity timer upon mouse or keyboard interaction.
     */
    Q_INVOKABLE void reportActivity();

    /**
     * @brief Configures idle inactivity threshold in milliseconds.
     * @param timeoutMs Duration before transitioning to auto-idle.
     */
    Q_INVOKABLE void setIdleTimeout(int timeoutMs);

    // Persistent & Activity-Sorted Direct Conversations
    /** @brief Returns list of active direct conversation tabs. */
    QVariantList openConversations() const;

    /**
     * @brief Opens a direct conversation tab with a peer, updating activity timestamp.
     * @param username Peer username.
     * @param activityTimestamp Optional custom timestamp (0 for current time).
     */
    Q_INVOKABLE void openDirectConversation(const QString &username, qint64 activityTimestamp = 0);

    /**
     * @brief Closes an open direct conversation tab.
     * @param username Peer username.
     */
    Q_INVOKABLE void closeDirectConversation(const QString &username);

    /**
     * @brief Updates the last-active timestamp of an open conversation to sort it to the top.
     * @param username Peer username.
     * @param activityTimestamp Optional custom timestamp.
     */
    Q_INVOKABLE void updateConversationActivity(const QString &username, qint64 activityTimestamp = 0);

    /**
     * @brief Queries the unread message count for a specific direct conversation.
     * @param username Peer username.
     * @return Unread count integer.
     */
    Q_INVOKABLE int unreadCount(const QString &username) const;

    /**
     * @brief Marks a conversation tab as read, resetting its unread count to zero.
     * @param username Peer username.
     */
    Q_INVOKABLE void markConversationAsRead(const QString &username);

    /**
     * @brief Increments unread counter for a conversation tab upon incoming message.
     * @param username Peer username.
     */
    Q_INVOKABLE void incrementUnreadCount(const QString &username);

signals:
    /** @brief Emitted when server URL property changes. */
    void serverUrlChanged();
    /** @brief Emitted when authentication token changes. */
    void tokenChanged();
    /** @brief Emitted when current authenticated username changes. */
    void currentUsernameChanged();
    /** @brief Emitted when local display name changes. */
    void displayNameChanged();
    /** @brief Emitted when a peer's display name alias is modified. */
    void peerDisplayNameUpdated(const QString &username, const QString &displayName);
    /** @brief Emitted when local user avatar changes. */
    void avatarUrlChanged();
    /** @brief Emitted when a peer's avatar image updates. */
    void peerAvatarUpdated(const QString &username, const QString &avatarUrl);
    /** @brief Emitted when loading state starts or finishes. */
    void isLoadingChanged();
    /** @brief Emitted when server connection state changes. */
    void isConnectedChanged();
    /** @brief Emitted when friends list changes. */
    void friendsChanged();
    /** @brief Emitted when pending requests list changes. */
    void pendingRequestsChanged();
    /** @brief Emitted when bookmarks list is modified. */
    void bookmarksChanged();
    /** @brief Emitted when open conversations tab list changes. */
    void openConversationsChanged();
    /** @brief Emitted when user status preference changes. */
    void userStatusChanged();
    /** @brief Emitted when effective status changes. */
    void effectiveStatusChanged();
    /** @brief Emitted when idle state toggles. */
    void isIdleChanged();
    /** @brief Emitted when stealth/invisible mode toggles. */
    void isInvisibleChanged(bool invisible);
    /** @brief Emitted when Do-Not-Disturb toggles. */
    void isDndChanged(bool dnd);

    /** @brief Emitted with server verification results. */
    void verificationResult(bool success, const QString &message);
    /** @brief Emitted with username availability results. */
    void availabilityResult(const QString &username, bool available, const QString &error);
    /** @brief Emitted with registration outcome. */
    void registrationResult(bool success, const QString &message);
    /** @brief Emitted with login outcome. */
    void loginResult(bool success, const QString &tokenOrError);
    /** @brief Emitted with device registration outcome. */
    void deviceRegistrationResult(bool success, const QString &message);
    /** @brief Emitted when device public key is fetched. */
    void deviceKeyFetched(const QString &deviceId, const QString &publicKey);
    /** @brief Emitted with device revocation outcome. */
    void deviceRevocationResult(bool success, const QString &message);
    /** @brief Emitted when recipient device public key bundles arrive. */
    void recipientKeysFetched(const QString &username, const QVariantList &devices);
    /** @brief Emitted when user profile details are fetched. */
    void userProfileFetched(bool success, const QString &username);

    /** @brief Emitted with add friend outcome. */
    void addFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with accept friend outcome. */
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with reject friend outcome. */
    void rejectFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with remove friend outcome. */
    void removeFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted when a friend's online status updates. */
    void friendStatusUpdated(const QString &username, const QString &status);
    /** @brief Emitted when an inbound relay message is received. */
    void incomingRelayMessageReceived(const QString &fromUsername, const QString &target, const QString &text, qint64 timestamp);

private slots:
    /** @brief Slot triggered when inactivity timer expires, transitioning to auto-idle. */
    void onIdleTimeout();

private:
    /** @brief Internal helper updating loading flag and emitting signal. */
    void setIsLoading(bool loading);
    /** @brief Connects signal/slot pipelines to underlying services. */
    void setupServiceSignals();
    /** @brief Automatically registers client device credentials after login. */
    void autoRegisterDevice();
    /** @brief Sorts open conversation tabs by last activity timestamp descending. */
    void sortOpenConversations();

    std::shared_ptr<NeoNect::Transport::IHttpTransport> m_transport;
    std::shared_ptr<NeoNect::Storage::ISettingsRepository> m_storage;
    std::shared_ptr<NeoNect::Crypto::ICryptoService> m_cryptoService;

    std::shared_ptr<NeoNect::Services::AuthService> m_authService;
    std::shared_ptr<NeoNect::Services::DeviceService> m_deviceService;
    std::shared_ptr<NeoNect::Services::RelayService> m_relayService;
    std::shared_ptr<NeoNect::Services::FriendService> m_friendService;

    bool m_isLoading{false};
    QString m_sessionToken;
    QString m_pendingBookmarkUsername;
    QString m_pendingBookmarkPassword;
    QVariantList m_openConversations;

    QString m_userStatusPreference{"online"};
    bool m_isAutoIdle{false};
    class QTimer *m_idleTimer{nullptr};
    int m_idleTimeoutMs{120000};
};
