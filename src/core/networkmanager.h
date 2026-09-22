// src/core/networkmanager.h
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

class NetworkManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY currentUsernameChanged)
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QStringList friends READ friends NOTIFY friendsChanged)
    Q_PROPERTY(QStringList pendingRequests READ pendingRequests NOTIFY pendingRequestsChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList openConversations READ openConversations NOTIFY openConversationsChanged)
    Q_PROPERTY(QString userStatus READ userStatus WRITE setUserStatus NOTIFY userStatusChanged)
    Q_PROPERTY(QString effectiveStatus READ effectiveStatus NOTIFY effectiveStatusChanged)
    Q_PROPERTY(bool isIdle READ isIdle NOTIFY isIdleChanged)
    Q_PROPERTY(bool isInvisible READ isInvisible NOTIFY isInvisibleChanged)
    Q_PROPERTY(bool isDnd READ isDnd NOTIFY isDndChanged)

public:
    explicit NetworkManager(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport,
                            std::shared_ptr<NeoNect::Storage::ISettingsRepository> storage,
                            std::shared_ptr<NeoNect::Crypto::ICryptoService> cryptoService,
                            std::shared_ptr<NeoNect::Services::AuthService> authService,
                            std::shared_ptr<NeoNect::Services::DeviceService> deviceService,
                            std::shared_ptr<NeoNect::Services::RelayService> relayService,
                            std::shared_ptr<NeoNect::Services::FriendService> friendService,
                            QObject *parent = nullptr);
    ~NetworkManager() override = default;

    void initializeCustom(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport);

    QString serverUrl() const;
    QString token() const;
    QString currentUsername() const;
    QString displayName() const;
    QString avatarUrl() const;
    bool isLoading() const { return m_isLoading; }
    bool isConnected() const;
    QStringList friends() const;
    QStringList pendingRequests() const;
    QVariantList bookmarks() const;

    Q_INVOKABLE void setDisplayName(const QString &displayName);
    Q_INVOKABLE QString getDisplayName(const QString &username) const;
    Q_INVOKABLE void setPeerDisplayName(const QString &username, const QString &displayName);

    Q_INVOKABLE bool setAvatar(const QString &filePathOrUrl);
    Q_INVOKABLE void clearAvatar();
    Q_INVOKABLE QString getAvatarUrl(const QString &username) const;
    Q_INVOKABLE void setPeerAvatarUrl(const QString &username, const QString &avatarUrl);

    Q_INVOKABLE void setProfile(const QString &profileName);
    Q_INVOKABLE void verifyServer(const QString &address);

    Q_INVOKABLE void checkUsernameAvailability(const QString &username);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);
    Q_INVOKABLE void logoutUser();
    Q_INVOKABLE void logout() { logoutUser(); }

    // Bookmark Management (TeamSpeak style)
    Q_INVOKABLE void saveBookmark(const QString &name, const QString &serverUrl, const QString &username, const QString &password, const QString &id = "");
    Q_INVOKABLE void deleteBookmark(const QString &id);
    Q_INVOKABLE void connectBookmark(const QString &id);

    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &publicKey);
    Q_INVOKABLE void fetchDevicePublicKey(const QString &deviceId);
    Q_INVOKABLE void revokeDevice(const QString &deviceId);
    Q_INVOKABLE void fetchRecipientKeys(const QString &username);
    Q_INVOKABLE void fetchUserProfile();

    // E2EE Relay & Direct Chat
    Q_INVOKABLE void pollPendingMessages();
    Q_INVOKABLE void acknowledgeMessage(qint64 messageId);

    // Friends & DM Management
    Q_INVOKABLE void addFriend(const QString &username);
    Q_INVOKABLE void acceptFriend(const QString &username);
    Q_INVOKABLE void rejectFriend(const QString &username);
    Q_INVOKABLE void removeFriend(const QString &username);
    Q_INVOKABLE void checkFriendsStatus();
    Q_INVOKABLE void checkUserStatus(const QString &username);
    Q_INVOKABLE QString getFriendStatus(const QString &username) const;
    Q_INVOKABLE QVariantMap allFriendStatuses() const;

    // Presence & Status Management
    QString userStatus() const { return m_userStatusPreference; }
    QString effectiveStatus() const;
    bool isIdle() const { return m_isAutoIdle || m_userStatusPreference == "afk" || m_userStatusPreference == "idle"; }
    bool isInvisible() const { return m_userStatusPreference == "offline"; }
    bool isDnd() const { return m_userStatusPreference == "dnd"; }
    Q_INVOKABLE void setUserStatus(const QString &status);
    Q_INVOKABLE void reportActivity();
    Q_INVOKABLE void setIdleTimeout(int timeoutMs);

    // Persistent & Activity-Sorted Direct Conversations
    QVariantList openConversations() const;
    Q_INVOKABLE void openDirectConversation(const QString &username, qint64 activityTimestamp = 0);
    Q_INVOKABLE void closeDirectConversation(const QString &username);
    Q_INVOKABLE void updateConversationActivity(const QString &username, qint64 activityTimestamp = 0);
    Q_INVOKABLE int unreadCount(const QString &username) const;
    Q_INVOKABLE void markConversationAsRead(const QString &username);
    Q_INVOKABLE void incrementUnreadCount(const QString &username);

signals:
    void serverUrlChanged();
    void tokenChanged();
    void currentUsernameChanged();
    void displayNameChanged();
    void peerDisplayNameUpdated(const QString &username, const QString &displayName);
    void avatarUrlChanged();
    void peerAvatarUpdated(const QString &username, const QString &avatarUrl);
    void isLoadingChanged();
    void isConnectedChanged();
    void friendsChanged();
    void pendingRequestsChanged();
    void bookmarksChanged();
    void openConversationsChanged();
    void userStatusChanged();
    void effectiveStatusChanged();
    void isIdleChanged();
    void isInvisibleChanged(bool invisible);
    void isDndChanged(bool dnd);

    void verificationResult(bool success, const QString &message);
    void availabilityResult(const QString &username, bool available, const QString &error);
    void registrationResult(bool success, const QString &message);
    void loginResult(bool success, const QString &tokenOrError);
    void deviceRegistrationResult(bool success, const QString &message);
    void deviceKeyFetched(const QString &deviceId, const QString &publicKey);
    void deviceRevocationResult(bool success, const QString &message);
    void recipientKeysFetched(const QString &username, const QVariantList &devices);
    void userProfileFetched(bool success, const QString &username);

    void addFriendResult(bool success, const QString &message, const QString &username);
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    void rejectFriendResult(bool success, const QString &message, const QString &username);
    void removeFriendResult(bool success, const QString &message, const QString &username);
    void friendStatusUpdated(const QString &username, const QString &status);
    void incomingRelayMessageReceived(const QString &fromUsername, const QString &target, const QString &text, qint64 timestamp);

private slots:
    void onIdleTimeout();

private:
    void setIsLoading(bool loading);
    void setupServiceSignals();
    void autoRegisterDevice();
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
