// src/services/friendservice.h
#pragma once
#include <QObject>
#include <QStringList>
#include <QDateTime>
#include <QMap>
#include <memory>
#include <mutex>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"

namespace Avila {
namespace Services {

class FriendService : public QObject {
    Q_OBJECT
public:
    explicit FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           QObject *parent = nullptr);
    ~FriendService() override = default;

    QStringList friends() const;
    void loadFriends();
    void addFriend(const QString &username);
    void checkFriendsStatus();
    void updateLastSeen(const QString &username);

signals:
    void friendsListChanged(const QStringList &friends);
    void addFriendResult(bool success, const QString &message, const QString &username);
    void friendStatusUpdated(const QString &username, const QString &status);

private:
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;

    mutable std::mutex m_presenceMutex;
    QMap<QString, QDateTime> m_lastSeen;
};

} // namespace Services
} // namespace Avila
