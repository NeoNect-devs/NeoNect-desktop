// src/services/friendservice.h
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

class FriendService : public QObject {
    Q_OBJECT
public:
    void startHeartbeat();
    void stopHeartbeat();
    explicit FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           QObject *parent = nullptr);
    ~FriendService() override = default;

    QStringList friends() const;
    QStringList pendingRequests() const;

    void loadFriends();
    void addFriend(const QString &username);
    void acceptFriend(const QString &username);
    void rejectFriend(const QString &username);
    void removeFriend(const QString &username);

    void checkFriendsStatus();
    void updateLastSeen(const QString &username);

public slots:
    void handleIncomingFriendPacket(const NeoNect::Domain::Message &msg);
    void handleTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage);

signals:
    void friendsListChanged(const QStringList &friends);
    void pendingRequestsChanged(const QStringList &requests);
    void addFriendResult(bool success, const QString &message, const QString &username);
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    void rejectFriendResult(bool success, const QString &message, const QString &username);
    void removeFriendResult(bool success, const QString &message, const QString &username);
    void friendStatusUpdated(const QString &username, const QString &status);

    void requestSendDomainMessage(const NeoNect::Domain::Message &msg);
    void friendRequestReceived(const QString &username);
    void friendAccepted(const QString &username);

private:
    QTimer *m_heartbeatTimer = nullptr;
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;

    QStringList m_cachedFriends;
    QStringList m_cachedPending;
    QMap<QString, QString> m_pendingFriendRequests;

    mutable std::mutex m_presenceMutex;
    QMap<QString, QDateTime> m_lastSeen;
};

} // namespace Services
} // namespace NeoNect
