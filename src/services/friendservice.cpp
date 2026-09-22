#include "friendservice.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include "../common/constants.h"

namespace NeoNect {
namespace Services {

FriendService::FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Storage::ISettingsRepository> storage,
                             QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage))
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(6000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &FriendService::checkFriendsStatus);

    m_cachedFriends = m_storage->friends();
    m_cachedPending = m_storage->pendingRequests();
}

QStringList FriendService::friends() const {
    return m_cachedFriends;
}

QStringList FriendService::pendingRequests() const {
    return m_cachedPending;
}

void FriendService::loadFriends() {
    m_cachedFriends = m_storage->friends();
    m_cachedPending = m_storage->pendingRequests();
    emit friendsListChanged(m_cachedFriends);
    emit pendingRequestsChanged(m_cachedPending);

    bool hasAuth = (m_transport && !m_transport->authToken().trimmed().isEmpty()) ||
                   (m_storage && !m_storage->authToken().trimmed().isEmpty());
    if (hasAuth && m_transport) {
        m_transport->get(Constants::EP_FRIENDS, {}, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
            Q_UNUSED(errStr);
            if (error == QNetworkReply::NoError && statusCode == 200) {
                auto doc = QJsonDocument::fromJson(data);
                if (doc.isObject() && doc.object().contains("friends")) {
                    QJsonArray arr = doc.object().value("friends").toArray();
                    QStringList serverFriends;
                    for (const auto &v : arr) {
                        QString f = v.toString().trimmed();
                        if (!f.isEmpty() && !serverFriends.contains(f, Qt::CaseInsensitive)) {
                            serverFriends.append(f);
                        }
                    }
                    if (serverFriends != m_cachedFriends) {
                        m_cachedFriends = serverFriends;
                        m_storage->setFriends(m_cachedFriends);
                        emit friendsListChanged(m_cachedFriends);
                        checkFriendsStatus();
                    }
                }
            }
        });
    }
}

void FriendService::setPeerStatus(const QString &username, const QString &status) {
    QString target = username.trimmed().toLower();
    if (target.isEmpty()) return;
    QString st = status.trimmed().toLower();
    if (st == "idle") st = "afk";

    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_peerStatuses[target] = st;
        if (st != "offline") {
            m_lastSeen[target] = QDateTime::currentDateTime();
        }
    }
    emit friendStatusUpdated(target, st);
}

void FriendService::updateLastSeen(const QString &username) {
    QString target = username.trimmed().toLower();
    if (target.isEmpty()) return;

    QString currentSt = "online";
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_lastSeen[target] = QDateTime::currentDateTime();
        if (m_peerStatuses.contains(target) && m_peerStatuses[target] != "offline") {
            currentSt = m_peerStatuses[target];
        } else {
            m_peerStatuses[target] = "online";
        }
    }
    emit friendStatusUpdated(target, currentSt);
}

void FriendService::checkFriendsStatus() {
    QStringList friendList = m_cachedFriends;
    if (friendList.isEmpty()) {
        return;
    }

    for (const QString &friendUser : friendList) {
        checkUserStatus(friendUser);
    }
}

void FriendService::checkUserStatus(const QString &username) {
    QString u = username.trimmed().toLower();
    if (u.isEmpty()) return;

    QMap<QString, QString> params;
    params.insert("u", u);

    m_transport->get(Constants::EP_PRESENCE, params, this, [this, u](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (error == QNetworkReply::NoError && statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("online")) {
                bool isOnline = doc.object().value("online").toBool();
                if (isOnline) {
                    QString st = "online";
                    {
                        std::lock_guard<std::mutex> lock(m_presenceMutex);
                        if (m_peerStatuses.contains(u) && m_peerStatuses[u] != "offline") {
                            st = m_peerStatuses[u];
                        } else {
                            m_peerStatuses[u] = "online";
                        }
                    }
                    emit friendStatusUpdated(u, st);
                } else {
                    {
                        std::lock_guard<std::mutex> lock(m_presenceMutex);
                        m_peerStatuses[u] = "offline";
                    }
                    emit friendStatusUpdated(u, "offline");
                }
                return;
            }
        }
        // If query failed or returned invalid response, reflect offline status
        {
            std::lock_guard<std::mutex> lock(m_presenceMutex);
            m_peerStatuses[u] = "offline";
        }
        emit friendStatusUpdated(u, "offline");
    });
}

void FriendService::addFriend(const QString &username) {
    QString target = username.trimmed();
    if (target.isEmpty()) return;

    QString myUsername = m_storage->username().trimmed();
    if (!myUsername.isEmpty() && target.compare(myUsername, Qt::CaseInsensitive) == 0) {
        emit addFriendResult(false, "You cannot add yourself as a friend.", target);
        return;
    }

    for (const QString &f : m_cachedFriends) {
        if (f.compare(target, Qt::CaseInsensitive) == 0) {
            emit addFriendResult(false, target + " is already your friend.", target);
            return;
        }
    }

    for (const QString &p : m_cachedPending) {
        if (p.compare(target, Qt::CaseInsensitive) == 0) {
            acceptFriend(target);
            emit addFriendResult(true, "Accepted incoming request from " + target, target);
            return;
        }
    }

    QMap<QString, QString> params;
    params.insert("u", target);

    m_transport->get(Constants::EP_USERS_AVAILABILITY, params, this, [this, target](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (statusCode == 401) {
            emit addFriendResult(false, "Authentication required. Please log in again.", target);
            return;
        }
        if (statusCode == 429) {
            emit addFriendResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }
        if (error != QNetworkReply::NoError || statusCode != 200) {
            emit addFriendResult(false, "Failed to communicate with server.", target);
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit addFriendResult(false, "Invalid server response.", target);
            return;
        }

        bool available = doc.object().value("available").toBool();
        if (available) {
            // Username is available, meaning no user with this username exists
            emit addFriendResult(false, "User '@" + target + "' does not exist on the network.", target);
            return;
        }

        // User exists! Send friend request packet via Relay so recipient gets real-time notification
        Domain::Message msg;
        msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg.conversationId = "dms:" + target;
        msg.senderId = m_storage->username();
        msg.type = "friend_request";
        msg.text = "Friend request";
        msg.timestamp = QDateTime::currentSecsSinceEpoch();

        m_pendingFriendRequests.insert(msg.id, target);
        emit requestSendDomainMessage(msg);
        emit addFriendResult(true, "Friend request sent to @" + target, target);
    });
}

void FriendService::handleTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage) {
    if (m_pendingFriendRequests.contains(messageId)) {
        QString target = m_pendingFriendRequests.take(messageId);
        if (success) {
            emit addFriendResult(true, "Friend request delivered to @" + target, target);
        } else {
            QString err = errorMessage.isEmpty() ? "Recipient is not reachable or offline" : errorMessage;
            emit addFriendResult(false, "Failed to deliver request to @" + target + ": " + err, target);
        }
    }
}

void FriendService::handleIncomingFriendPacket(const NeoNect::Domain::Message &msg) {
    QString sender = msg.senderId.trimmed();
    qDebug() << "[FriendService] handleIncomingFriendPacket received from:" << sender << "type:" << msg.type << "text:" << msg.text;
    if (sender.isEmpty()) return;

    QString myUsername = m_storage->username().trimmed();
    if (!myUsername.isEmpty() && sender.compare(myUsername, Qt::CaseInsensitive) == 0) {
        qDebug() << "[FriendService] Ignoring packet from self:" << sender;
        return;
    }

    if (msg.type == "friend_request") {
        for (const QString &f : m_cachedFriends) {
            if (f.compare(sender, Qt::CaseInsensitive) == 0) {
                qDebug() << "[FriendService]" << sender << "is already a friend.";
                return;
            }
        }

        bool alreadyPending = false;
        for (const QString &p : m_cachedPending) {
            if (p.compare(sender, Qt::CaseInsensitive) == 0) {
                alreadyPending = true;
                break;
            }
        }

        if (!alreadyPending) {
            m_cachedPending.append(sender);
            m_storage->setPendingRequests(m_cachedPending);
            emit pendingRequestsChanged(m_cachedPending);
            qDebug() << "[FriendService] Emitting friendRequestReceived for:" << sender;
            emit friendRequestReceived(sender);
        } else {
            qDebug() << "[FriendService]" << sender << "is already in pending requests.";
        }
    } else if (msg.type == "friend_accept") {
        for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
            if (m_cachedPending[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedPending.removeAt(i);
            }
        }
        m_storage->setPendingRequests(m_cachedPending);
        emit pendingRequestsChanged(m_cachedPending);

        bool alreadyFriend = false;
        for (const QString &f : m_cachedFriends) {
            if (f.compare(sender, Qt::CaseInsensitive) == 0) {
                alreadyFriend = true;
                break;
            }
        }

        if (!alreadyFriend) {
            m_cachedFriends.append(sender);
            m_storage->setFriends(m_cachedFriends);
            emit friendsListChanged(m_cachedFriends);
            qDebug() << "[FriendService] Emitting friendAccepted for:" << sender;
            emit friendAccepted(sender);
        }
    } else if (msg.type == "friend_reject") {
        for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
            if (m_cachedPending[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedPending.removeAt(i);
            }
        }
        m_storage->setPendingRequests(m_cachedPending);
        emit pendingRequestsChanged(m_cachedPending);

        bool changedFriends = false;
        for (int i = m_cachedFriends.size() - 1; i >= 0; --i) {
            if (m_cachedFriends[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedFriends.removeAt(i);
                changedFriends = true;
            }
        }
        if (changedFriends) {
            m_storage->setFriends(m_cachedFriends);
            emit friendsListChanged(m_cachedFriends);
        }

        qDebug() << "[FriendService] Emitting friendRejected for:" << sender;
        emit friendRejected(sender);
    }
}

void FriendService::acceptFriend(const QString &username) {
    QString target = username.trimmed();
    if (target.isEmpty()) return;

    // Register friendship on backend via POST /api/v1/friends
    QJsonObject payload;
    payload["username"] = target;
    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_transport->post(Constants::EP_FRIENDS, postData, this, [target](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(data);
        Q_UNUSED(error);
        Q_UNUSED(errStr);
        qDebug() << "[FriendService] acceptFriend backend status:" << statusCode << "for:" << target;
    });

    for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
        if (m_cachedPending[i].compare(target, Qt::CaseInsensitive) == 0) {
            m_cachedPending.removeAt(i);
        }
    }
    m_storage->setPendingRequests(m_cachedPending);
    emit pendingRequestsChanged(m_cachedPending);

    bool alreadyFriend = false;
    for (const QString &f : m_cachedFriends) {
        if (f.compare(target, Qt::CaseInsensitive) == 0) {
            alreadyFriend = true;
            break;
        }
    }

    if (!alreadyFriend) {
        m_cachedFriends.append(target);
        m_storage->setFriends(m_cachedFriends);
        emit friendsListChanged(m_cachedFriends);
    }

    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = "dms:" + target;
    msg.senderId = m_storage->username();
    msg.type = "friend_accept";
    msg.text = "Accepted friend request";
    msg.timestamp = QDateTime::currentSecsSinceEpoch();

    emit requestSendDomainMessage(msg);
    emit acceptFriendResult(true, "Accepted " + target, target);
}

void FriendService::rejectFriend(const QString &username) {
    QString target = username.trimmed();
    if (target.isEmpty()) return;

    for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
        if (m_cachedPending[i].compare(target, Qt::CaseInsensitive) == 0) {
            m_cachedPending.removeAt(i);
        }
    }
    m_storage->setPendingRequests(m_cachedPending);
    emit pendingRequestsChanged(m_cachedPending);

    bool changedFriends = false;
    for (int i = m_cachedFriends.size() - 1; i >= 0; --i) {
        if (m_cachedFriends[i].compare(target, Qt::CaseInsensitive) == 0) {
            m_cachedFriends.removeAt(i);
            changedFriends = true;
        }
    }
    if (changedFriends) {
        m_storage->setFriends(m_cachedFriends);
        emit friendsListChanged(m_cachedFriends);
    }

    Domain::Message msg;
    msg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg.conversationId = "dms:" + target;
    msg.senderId = m_storage->username();
    msg.type = "friend_reject";
    msg.text = "Rejected friend request";
    msg.timestamp = QDateTime::currentSecsSinceEpoch();

    emit requestSendDomainMessage(msg);
    emit rejectFriendResult(true, "Rejected " + target, target);
}

void FriendService::removeFriend(const QString &username) {
    QString target = username.trimmed();
    if (target.isEmpty()) return;

    for (int i = m_cachedFriends.size() - 1; i >= 0; --i) {
        if (m_cachedFriends[i].compare(target, Qt::CaseInsensitive) == 0) {
            m_cachedFriends.removeAt(i);
        }
    }
    m_storage->setFriends(m_cachedFriends);
    emit friendsListChanged(m_cachedFriends);

    bool hasAuth = (m_transport && !m_transport->authToken().trimmed().isEmpty()) ||
                   (m_storage && !m_storage->authToken().trimmed().isEmpty());
    if (hasAuth && m_transport) {
        QJsonObject payload;
        payload["username"] = target;
        QByteArray deleteData = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        m_transport->deleteResource(Constants::EP_FRIENDS, this, [target](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
            Q_UNUSED(data);
            Q_UNUSED(error);
            Q_UNUSED(errStr);
            qDebug() << "[FriendService] removeFriend backend status:" << statusCode << "for:" << target;
        }, deleteData);
    }

    emit removeFriendResult(true, "Removed " + target, target);
}

void FriendService::startHeartbeat() {
    if (!m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->start();
    }
    checkFriendsStatus();
}

void FriendService::stopHeartbeat() {
    if (m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
    }
    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_peerStatuses.clear();
    }
    for (const QString &f : m_cachedFriends) {
        emit friendStatusUpdated(f.trimmed().toLower(), "offline");
    }
}

} // namespace Services
} // namespace NeoNect
