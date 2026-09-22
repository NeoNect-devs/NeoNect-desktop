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
     * @brief Starts the periodic presence heartbeat timer.
     */
    void startHeartbeat();

    /**
     * @brief Stops the periodic presence heartbeat timer.
     */
    void stopHeartbeat();

    /**
     * @brief Retrieves the cached list of confirmed friend usernames.
     */
    QStringList friends() const;

    /**
     * @brief Retrieves the cached list of pending friend request usernames.
     */
    QStringList pendingRequests() const;

    /**
     * @brief Loads friends and pending requests from the backend API.
     */
    void loadFriends();

    /**
     * @brief Sends a friend request to a target user via API and relays an E2EE notification packet.
     * @param username Target username.
     */
    void addFriend(const QString &username);

    /**
     * @brief Accepts an incoming friend request from a target user.
     * @param username Target username.
     */
    void acceptFriend(const QString &username);

    /**
     * @brief Rejects an incoming friend request from a target user.
     * @param username Target username.
     */
    void rejectFriend(const QString &username);

    /**
     * @brief Removes a user from the friends list.
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
    /** @brief Emitted when pending requests list changes. */
    void pendingRequestsChanged(const QStringList &requests);
    /** @brief Emitted with the result of an add friend operation. */
    void addFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of an accept friend operation. */
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    /** @brief Emitted with the result of a reject friend operation. */
    void rejectFriendResult(bool success, const QString &message, const QString &username);
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
    /** @brief Timer triggering periodic presence refresh. */
    QTimer *m_heartbeatTimer = nullptr;
    /** @brief HTTP transport layer. */
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    /** @brief Settings repository. */
    std::shared_ptr<Storage::ISettingsRepository> m_storage;

    /** @brief In-memory cache of confirmed friend usernames. */
    QStringList m_cachedFriends;
    /** @brief In-memory cache of pending request usernames. */
    QStringList m_cachedPending;
    /** @brief Pending friend requests mapped to message IDs. */
    QMap<QString, QString> m_pendingFriendRequests;

    /** @brief Mutex protecting peer presence maps. */
    mutable std::mutex m_presenceMutex;
    /** @brief Last seen timestamps per peer. */
    QMap<QString, QDateTime> m_lastSeen;
    /** @brief Reported presence status per peer. */
    QMap<QString, QString> m_peerStatuses;
};

} // namespace Services
} // namespace NeoNect
