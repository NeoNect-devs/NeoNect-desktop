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

    void loadFriends(); // will now fetch from backend API
    void addFriend(const QString &username);
    void acceptFriend(const QString &username);
    void rejectFriend(const QString &username);
    void removeFriend(const QString &username);

    void checkFriendsStatus();
    void updateLastSeen(const QString &username);

signals:
    void friendsListChanged(const QStringList &friends);
    void pendingRequestsChanged(const QStringList &requests);
    void addFriendResult(bool success, const QString &message, const QString &username);
    void acceptFriendResult(bool success, const QString &message, const QString &username);
    void rejectFriendResult(bool success, const QString &message, const QString &username);
    void removeFriendResult(bool success, const QString &message, const QString &username);
    void friendStatusUpdated(const QString &username, const QString &status);

private:
    QTimer *m_heartbeatTimer = nullptr;
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;

    QStringList m_cachedFriends;
    QStringList m_cachedPending;

    mutable std::mutex m_presenceMutex;
    QMap<QString, QDateTime> m_lastSeen;
};

} // namespace Services
} // namespace NeoNect
