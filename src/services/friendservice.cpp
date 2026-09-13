#include "friendservice.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../common/constants.h"

namespace NeoNect {
namespace Services {

FriendService::FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Storage::ISettingsRepository> storage,
                             QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage))
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(12000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &FriendService::checkFriendsStatus);
}

QStringList FriendService::friends() const {
    return m_cachedFriends;
}

QStringList FriendService::pendingRequests() const {
    return m_cachedPending;
}

void FriendService::loadFriends() {
    m_transport->get("/api/v1/friends", QMap<QString, QString>(), this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError err, const QString &errStr) {
        if (err == QNetworkReply::NoError && statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("friends")) {
                QJsonArray arr = doc.object().value("friends").toArray();
                QStringList list;
                for (int i = 0; i < arr.size(); i++) {
                    list.append(arr[i].toObject().value("username").toString());
                }
                m_cachedFriends = list;
                emit friendsListChanged(m_cachedFriends);
            }
        }
    });

    m_transport->get("/api/v1/friends/requests", QMap<QString, QString>(), this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError err, const QString &errStr) {
        if (err == QNetworkReply::NoError && statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("requests")) {
                QJsonArray arr = doc.object().value("requests").toArray();
                QStringList list;
                for (int i = 0; i < arr.size(); i++) {
                    list.append(arr[i].toObject().value("username").toString());
                }
                m_cachedPending = list;
                emit pendingRequestsChanged(m_cachedPending);
            }
        }
    });
}

void FriendService::updateLastSeen(const QString &username) {
    QString target = username.trimmed().toLower();
    if (target.isEmpty()) return;

    {
        std::lock_guard<std::mutex> lock(m_presenceMutex);
        m_lastSeen[target] = QDateTime::currentDateTime();
    }
    emit friendStatusUpdated(target, "online");
}

void FriendService::checkFriendsStatus() {
    QStringList friendList = m_cachedFriends;
    if (friendList.isEmpty()) {
        return;
    }

    QString csv = friendList.join(",");
    QMap<QString, QString> params;
    params.insert("users", csv);

    m_transport->get(Constants::EP_PRESENCE, params, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        if (error == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("presence")) {
                QJsonObject presenceObj = doc.object().value("presence").toObject();
                for (auto it = presenceObj.begin(); it != presenceObj.end(); ++it) {
                    QString username = it.key();
                    QString status = it.value().toObject().value("status").toString();
                    if (status == "online") {
                        updateLastSeen(username);
                    } else {
                        emit friendStatusUpdated(username, status);
                    }
                }
            }
        }
    });

    loadFriends();
}

void FriendService::addFriend(const QString &username) {
    QString target = username.trimmed();
    if (target.isEmpty()) return;

    QMap<QString, QString> params;
    params.insert("q", target);
    m_transport->get("/api/v1/friends/search", params, this, [this, target](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        if (error != QNetworkReply::NoError) {
            emit addFriendResult(false, "Failed to communicate with server.", target);
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.object().contains("exists") && doc.object().value("exists").toBool()) {
            QJsonObject payload;
            payload["username"] = target;
            QJsonDocument reqDoc(payload);
            m_transport->post("/api/v1/friends/request", reqDoc.toJson(QJsonDocument::Compact), this, [this, target](int sc, const QByteArray &d, QNetworkReply::NetworkError err, const QString &eStr) {
                if (err == QNetworkReply::NoError && sc == 200) {
                    emit addFriendResult(true, "Friend request sent to " + target, target);
                    loadFriends();
                } else if (sc == 409) {
                    emit addFriendResult(false, "Friend request already sent or relationship exists.", target);
                } else {
                    emit addFriendResult(false, "Failed to send request.", target);
                }
            });
        } else {
            emit addFriendResult(false, "User '@" + target + "' does not exist on the network.", target);
        }
    });
}

void FriendService::acceptFriend(const QString &username) {
    QJsonObject payload;
    payload["username"] = username;
    QJsonDocument reqDoc(payload);
    m_transport->post("/api/v1/friends/accept", reqDoc.toJson(QJsonDocument::Compact), this, [this, username](int sc, const QByteArray &d, QNetworkReply::NetworkError err, const QString &eStr) {
        if (err == QNetworkReply::NoError && sc == 200) {
            emit acceptFriendResult(true, "Accepted " + username, username);
            loadFriends();
        } else {
            emit acceptFriendResult(false, "Failed to accept.", username);
        }
    });
}

void FriendService::rejectFriend(const QString &username) {
    QJsonObject payload;
    payload["username"] = username;
    QJsonDocument reqDoc(payload);
    m_transport->post("/api/v1/friends/reject", reqDoc.toJson(QJsonDocument::Compact), this, [this, username](int sc, const QByteArray &d, QNetworkReply::NetworkError err, const QString &eStr) {
        if (err == QNetworkReply::NoError && sc == 200) {
            emit rejectFriendResult(true, "Rejected " + username, username);
            loadFriends();
        } else {
            emit rejectFriendResult(false, "Failed to reject.", username);
        }
    });
}

void FriendService::removeFriend(const QString &username) {
    QJsonObject payload;
    payload["username"] = username;
    QJsonDocument reqDoc(payload);
    m_transport->post("/api/v1/friends/remove", reqDoc.toJson(QJsonDocument::Compact), this, [this, username](int sc, const QByteArray &d, QNetworkReply::NetworkError err, const QString &eStr) {
        if (err == QNetworkReply::NoError && sc == 200) {
            emit removeFriendResult(true, "Removed " + username, username);
            loadFriends();
        } else {
            emit removeFriendResult(false, "Failed to remove.", username);
        }
    });
}

void FriendService::startHeartbeat() {
    if (!m_heartbeatTimer->isActive()) m_heartbeatTimer->start();
}

void FriendService::stopHeartbeat() {
    if (m_heartbeatTimer->isActive()) m_heartbeatTimer->stop();
}

} // namespace Services
} // namespace NeoNect
