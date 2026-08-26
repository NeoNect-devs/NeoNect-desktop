// src/services/friendservice.cpp
#include "friendservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace Avila {
namespace Services {

FriendService::FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Storage::ISettingsRepository> storage,
                             QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage)) {
}

QStringList FriendService::friends() const {
    return m_storage->friends();
}

void FriendService::loadFriends() {
    QStringList list = m_storage->friends();
    if (list.isEmpty()) {
        list = QStringList{"alex", "beatrice", "charlie"};
        m_storage->setFriends(list);
    }
    emit friendsListChanged(list);
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
    QString currentUser = m_storage->username().toLower();
    QStringList friendList = m_storage->friends();
    QDateTime now = QDateTime::currentDateTime();

    std::lock_guard<std::mutex> lock(m_presenceMutex);
    for (const QString &friendName : friendList) {
        QString target = friendName.trimmed().toLower();
        if (target.isEmpty() || target == currentUser) continue;

        if (m_lastSeen.contains(target) && m_lastSeen[target].secsTo(now) <= Constants::PRESENCE_TIMEOUT_SECS) {
            emit friendStatusUpdated(target, "online");
        } else {
            emit friendStatusUpdated(target, "offline");
        }
    }
}

void FriendService::addFriend(const QString &username) {
    QString target = username.trimmed().toLower();
    QString currentUsername = m_storage->username().toLower();

    if (target.isEmpty()) {
        emit addFriendResult(false, "Username cannot be empty.", target);
        return;
    }

    if (target == currentUsername) {
        emit addFriendResult(false, "You cannot add yourself as a friend.", target);
        return;
    }

    QStringList currentFriends = m_storage->friends();
    if (currentFriends.contains(target, Qt::CaseInsensitive)) {
        emit addFriendResult(false, "User is already in your Direct Messages list.", target);
        return;
    }

    // Check user availability/existence on server
    QMap<QString, QString> params;
    params["u"] = target;

    m_transport->get(Constants::EP_USERS_AVAILABILITY, params, [this, target, currentFriends](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        auto doc = QJsonDocument::fromJson(data);
        bool exists = false;

        if (error == QNetworkReply::NoError && !doc.isNull() && doc.object().contains("available")) {
            // available == false means username is already registered on backend
            exists = !doc.object().value("available").toBool();
        } else {
            // If offline/custom node, allow adding
            exists = true;
        }

        if (exists) {
            QStringList updated = m_storage->friends();
            if (!updated.contains(target, Qt::CaseInsensitive)) {
                updated.append(target);
                m_storage->setFriends(updated);
            }
            emit friendsListChanged(updated);
            emit addFriendResult(true, "Friend added successfully!", target);
        } else {
            emit addFriendResult(false, "User '@" + target + "' does not exist on the network.", target);
        }
    });
}

} // namespace Services
} // namespace Avila
