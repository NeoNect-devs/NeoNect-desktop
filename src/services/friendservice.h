/**
 * @file friendservice.h
 * @brief Service layer coordinator managing friendships, friend requests, presence tracking, and heartbeats.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Manages social relationships and real-time presence. Coordinates with the server REST API for
 * friend management while also synthesizing peer-to-peer signaling packets over the E2EE relay
 * pipeline to notify remote contacts of status updates, friend invitations, and acceptances in real-time.
 *
 * @par Design Patterns:
 * - <b>Service Layer Pattern</b>: Encapsulates social workflows, contacts synchronization, and presence logic.
 * - <b>Heartbeat Pattern</b>: Periodic timer polls peer presence and updates last-seen timestamps.
 * - <b>Mediator / Inter-Service Collaboration</b>: Interacts with `RelayService` via domain packets for live notifications.
 * - <b>Thread-Safe Monitor</b>: Mutex synchronization guards access to presence and status maps.
 */

#pragma once
#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <cstdint>
#include <memory>
#include <mutex>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"
#include "../domain/message.h"

namespace NeoNect {
namespace Services {

/**
 * @class FriendService
 * @brief Manages friends lists, inbound/outbound friend requests, and real-time presence telemetry.
 */
class FriendService : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the friend service.
     * @param transport Shared pointer to HTTP transport abstraction.
     * @param storage Shared pointer to settings repository.
     * @param parent Optional parent QObject for Qt tree ownership.
     */
    explicit FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           QObject *parent = nullptr);

    /**
     * @brief Destructor. Stops active heartbeat timers.
     */
    ~FriendService() override = default;

    /**
     * @brief Starts the periodic presence heartbeat and friend synchronization timers.
     */
    void startHeartbeat();

    /**
     * @brief Stops the periodic presence heartbeat and friend synchronization timers and invalidates in-flight callbacks.
     */
    void stopHeartbeat();

    /**
     * @brief Invalidates any in-flight asynchronous callbacks for the current user session.
     */
    void invalidateSession();

    /**
     * @brief Retrieves the cached list of confirmed friend usernames.
     */
    QStringList friends() const;

    /**
     * @brief Retrieves the cached list of pending incoming friend request usernames.
     */
    QStringList pendingRequests() const;

    /**
     * @brief Retrieves the cached list of pending outgoing friend request usernames.
     */
    QStringList outgoingRequests() const;

    /**
     * @brief Loads confirmed friends and pending friend requests from the backend API.
     */
    void loadFriends();

    /**
     * @brief Loads incoming and outgoing pending friend requests from GET /api/v1/friends/requests.
     */
    void loadFriendRequests();

    /**
     * @brief Sends a friend request to a target user via POST /api/v1/friends.
     * @param username Target username.
     */
    void addFriend(const QString &username);

    /**
     * @brief Accepts an incoming friend request from a target user via POST /api/v1/friends/requests/accept.
     * @param username Target username.
     */
    void acceptFriend(const QString &username);

    /**
     * @brief Declines an incoming friend request from a target user via POST /api/v1/friends/requests/decline.
     * @param username Target username.
     */
    void rejectFriend(const QString &username);

    /**
     * @brief Cancels an outgoing friend request to a target user via DELETE /api/v1/friends/requests.
     * @param username Target username.
     */
    void cancelFriendRequest(const QString &username);

    /**
     * @brief Removes an accepted friend via DELETE /api/v1/friends.
     * @param username Target username.
     */
    void removeFriend(const QString &username);

    /**
     * @brief Queries online status and last-seen activity for all confirmed friends.
     */
    void checkFriendsStatus();

    /**
     * @brief Queries online status for a specific user.
     * @param username Target username.
     */
    void checkUserStatus(const QString &username);

    /**
     * @brief Updates the local last-seen timestamp for a peer.
     * @param username Target username.
     */
    void updateLastSeen(const QString &username);

    /**
     * @brief Sets the reported presence status for a peer (e.g. `"online"`, `"idle"`, `"offline"`).
     * @param username Target username.
     * @param status Status keyword.
     */
    void setPeerStatus(const QString &username, const QString &status);

    /**
     * @brief Retrieves the presence status keyword for a peer.
     * @param username Target username.
     * @return Status string.
     */
    QString getPeerStatus(const QString &username) const;

    /**
     * @brief Retrieves a map of all known peer presence statuses.
     * @return Map where keys are usernames and values are status strings.
     */
    QVariantMap allPeerStatuses() const;

public slots:
    /**
     * @brief Handles incoming domain packets parsed by RelayService representing friend events.
     * @param msg Inbound domain message entity.
     */
    void handleIncomingFriendPacket(const NeoNect::Domain::Message &msg);

    /**
     * @brief Slot notified by RelayService when a friend signaling packet delivery succeeds or fails.
     * @param targetUser Recipient username.
     * @param messageId Unique packet ID.
     * @param success True if packet delivered.
     * @param errorMessage Error description if failed.
     */
    void handleTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage);

signals:
    /** @brief Emitted when the friends list changes. */
    void friendsListChanged(const QStringList &friends);
    /** @brief Emitted when incoming pending requests list changes. */
    void pendingRequestsChanged(const QStringList &requests);
    /** @brief Emitted when outgoing pending requests list changes. */
    void outgoingRequestsChanged(const QStringList &requests);
    /** @brief Emitted with the result of an add friend operation. */
    void addFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of an accept friend operation. */
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of a reject/decline friend operation. */
    void rejectFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of canceling an outgoing friend request. */
    void cancelFriendRequestResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of a remove friend operation. */
    void removeFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted when a peer's presence status changes. */
    void friendStatusUpdated(const QString &username, const QString &status);

    /** @brief Emitted to request RelayService to transmit a signaling packet across the network. */
    void requestSendDomainMessage(const NeoNect::Domain::Message &msg);
    /** @brief Emitted when an incoming friend request is received from a peer. */
    void friendRequestReceived(const QString &username);
    /** @brief Emitted when a peer accepts our friend request. */
    void friendAccepted(const QString &username);
    /** @brief Emitted when a peer rejects our friend request. */
    void friendRejected(const QString &username);

private:
    bool ensureAuthToken();
    void syncSessionContext();
    void fetchFriends();
    void requestFriendsRefresh();
    void requestFriendRequestsRefresh();
    bool hasInFlightMutationForUser(const QString &lowerUsername) const;

    /** @brief Timer triggering periodic presence refresh. */
    QTimer *m_heartbeatTimer = nullptr;
    /** @brief Timer triggering periodic friend and request list synchronization. */
    QTimer *m_syncTimer = nullptr;
    /** @brief HTTP transport layer. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    /** @brief Settings repository. */
    std::shared_ptr<Storage::ISettingsRepository> m_storage;

    /** @brief In-memory cache of confirmed friend usernames. */
    QStringList m_cachedFriends;
    /** @brief In-memory cache of incoming pending request usernames. */
    QStringList m_cachedPending;
    /** @brief In-memory cache of outgoing pending request usernames. */
    QStringList m_cachedOutgoing;
    /** @brief Tracked outgoing request targets in current session to detect acceptance upon sync. */
    QSet<QString> m_knownOutgoing;
    /** @brief Pending friend requests mapped to message IDs (legacy compatibility). */
    QMap<QString, QString> m_pendingFriendRequests;

    /** @brief Monotonic session generation counter to discard stale async callbacks across user switches/logouts. */
    uint64_t m_sessionGeneration = 0;
    /** @brief Active session username associated with current generation. */
    QString m_sessionUsername;
    /** @brief Guard preventing overlapping GET /api/v1/friends requests. */
    bool m_loadingFriends = false;
    /** @brief Guard preventing overlapping GET /api/v1/friends/requests requests. */
    bool m_loadingRequests = false;
    /** @brief Indicates a mutation completed while GET /api/v1/friends was in flight and requires a fresh fetch. */
    bool m_friendsRefreshPending = false;
    /** @brief Indicates a mutation completed while GET /api/v1/friends/requests was in flight and requires a fresh fetch. */
    bool m_requestsRefreshPending = false;
    /** @brief Tracks whether the latest GET /api/v1/friends completed with valid data in the current session. */
    bool m_lastFriendsFetchSucceeded = false;
    /** @brief Set of operation-scoped keys (e.g. "add:bob") with an active in-flight mutation request. */
    QSet<QString> m_inFlightMutations;

    /** @brief Mutex protecting peer presence maps. */
    mutable std::mutex m_presenceMutex;
    /** @brief Last seen timestamps per peer. */
    QMap<QString, QDateTime> m_lastSeen;
    /** @brief Reported presence status per peer. */
    QMap<QString, QString> m_peerStatuses;
};

} // namespace Services
} // namespace NeoNect
