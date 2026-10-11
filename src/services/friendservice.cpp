#include "friendservice.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include "../common/constants.h"

namespace NeoNect {
namespace Services {

static QString cleanUsername(const QString &username) {
    QString target = username.trimmed();
    while (target.startsWith('@')) {
        target = target.mid(1).trimmed();
    }
    return target;
}

FriendService::FriendService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Storage::ISettingsRepository> storage,
                             QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage))
{
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(6000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &FriendService::checkFriendsStatus);

    m_syncTimer = new QTimer(this);
    m_syncTimer->setInterval(Constants::FRIEND_SYNC_INTERVAL_MS);
    connect(m_syncTimer, &QTimer::timeout, this, &FriendService::loadFriends);

    if (m_storage) {
        m_sessionUsername = cleanUsername(m_storage->username()).toLower();
        m_cachedFriends = m_storage->friends();
        m_cachedPending = m_storage->pendingRequests();
        m_cachedOutgoing = m_storage->outgoingRequests();
        for (const QString &out : m_cachedOutgoing) {
            QString clean = cleanUsername(out).toLower();
            if (!clean.isEmpty()) {
                m_knownOutgoing.insert(clean);
            }
        }
    }
}

bool FriendService::ensureAuthToken() {
    if (!m_transport) {
        return false;
    }
    QString transportToken = m_transport->authToken().trimmed();
    QString storageToken = m_storage ? m_storage->authToken().trimmed() : QString();
    if (transportToken.isEmpty() && !storageToken.isEmpty()) {
        m_transport->setAuthToken(storageToken);
        transportToken = storageToken;
    } else if (!transportToken.isEmpty() && !storageToken.isEmpty() && transportToken != storageToken) {
        if (transportToken != QStringLiteral("unauthorized")) {
            m_transport->setAuthToken(storageToken);
            transportToken = storageToken;
        }
    }
    return !transportToken.isEmpty();
}

bool FriendService::hasInFlightMutationForUser(const QString &lowerUsername) const {
    if (lowerUsername.isEmpty()) {
        return false;
    }
    return m_inFlightMutations.contains(QStringLiteral("add:") + lowerUsername) ||
           m_inFlightMutations.contains(QStringLiteral("accept:") + lowerUsername) ||
           m_inFlightMutations.contains(QStringLiteral("reject:") + lowerUsername) ||
           m_inFlightMutations.contains(QStringLiteral("cancel:") + lowerUsername) ||
           m_inFlightMutations.contains(QStringLiteral("remove:") + lowerUsername);
}

void FriendService::syncSessionContext() {
    QString currentStorageUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
    if (currentStorageUser != m_sessionUsername) {
        ++m_sessionGeneration;
        m_sessionUsername = currentStorageUser;
        m_loadingFriends = false;
        m_loadingRequests = false;
        m_friendsRefreshPending = false;
        m_requestsRefreshPending = false;
        m_lastFriendsFetchSucceeded = false;
        m_inFlightMutations.clear();
        m_knownOutgoing.clear();
        if (m_storage) {
            m_cachedFriends = m_storage->friends();
            m_cachedPending = m_storage->pendingRequests();
            m_cachedOutgoing = m_storage->outgoingRequests();
            for (const QString &out : m_cachedOutgoing) {
                QString clean = cleanUsername(out).toLower();
                if (!clean.isEmpty()) {
                    m_knownOutgoing.insert(clean);
                }
            }
        } else {
            m_cachedFriends.clear();
            m_cachedPending.clear();
            m_cachedOutgoing.clear();
        }
    }
}

void FriendService::invalidateSession() {
    m_sessionUsername = QStringLiteral("\x01_invalidated");
    syncSessionContext();
}

QStringList FriendService::friends() const {
    return m_cachedFriends;
}

QStringList FriendService::pendingRequests() const {
    return m_cachedPending;
}

QStringList FriendService::outgoingRequests() const {
    return m_cachedOutgoing;
}

void FriendService::requestFriendsRefresh() {
    syncSessionContext();
    if (m_loadingFriends) {
        m_friendsRefreshPending = true;
    }
    if (m_loadingRequests) {
        m_requestsRefreshPending = true;
    }
    loadFriends();
}

void FriendService::requestFriendRequestsRefresh() {
    syncSessionContext();
    if (m_loadingRequests) {
        m_requestsRefreshPending = true;
        return;
    }
    loadFriendRequests();
}

void FriendService::loadFriends() {
    syncSessionContext();

    if (!ensureAuthToken()) {
        if (m_storage) {
            m_cachedFriends = m_storage->friends();
            m_cachedPending = m_storage->pendingRequests();
            m_cachedOutgoing = m_storage->outgoingRequests();
        }
        emit friendsListChanged(m_cachedFriends);
        emit pendingRequestsChanged(m_cachedPending);
        emit outgoingRequestsChanged(m_cachedOutgoing);
        return;
    }

    fetchFriends();
    loadFriendRequests();
}

void FriendService::fetchFriends() {
    syncSessionContext();

    if (!ensureAuthToken() || !m_transport || m_loadingFriends) {
        return;
    }

    m_loadingFriends = true;
    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    m_transport->get(Constants::EP_FRIENDS, {}, this, [this, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_loadingFriends = false;
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }
        if (m_friendsRefreshPending) {
            m_friendsRefreshPending = false;
            fetchFriends();
            return;
        }

        if (error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 201)) {
            auto doc = QJsonDocument::fromJson(data);
            if (doc.isObject() && doc.object().contains("friends")) {
                m_lastFriendsFetchSucceeded = true;
                QJsonArray arr = doc.object().value("friends").toArray();
                QStringList serverFriends;
                QStringList newlyAccepted;
                for (const auto &v : arr) {
                    QString f = cleanUsername(v.toString());
                    if (!f.isEmpty() && !serverFriends.contains(f, Qt::CaseInsensitive)) {
                        serverFriends.append(f);
                        if (!m_cachedFriends.contains(f, Qt::CaseInsensitive) &&
                            (m_cachedOutgoing.contains(f, Qt::CaseInsensitive) || m_knownOutgoing.contains(f.toLower()))) {
                            newlyAccepted.append(f);
                        }
                        m_knownOutgoing.remove(f.toLower());
                    }
                }

                QStringList newlyDeclinedAfterAsyncRequests;
                if (!m_loadingRequests && !m_requestsRefreshPending) {
                    for (auto it = m_knownOutgoing.begin(); it != m_knownOutgoing.end(); ) {
                        const QString &knownOut = *it;
                        if (!m_cachedOutgoing.contains(knownOut, Qt::CaseInsensitive) &&
                            !serverFriends.contains(knownOut, Qt::CaseInsensitive) &&
                            !hasInFlightMutationForUser(knownOut)) {
                            newlyDeclinedAfterAsyncRequests.append(knownOut);
                            it = m_knownOutgoing.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                m_cachedFriends = serverFriends;
                if (m_storage) {
                    m_storage->setFriends(m_cachedFriends);
                }
                emit friendsListChanged(m_cachedFriends);
                for (const QString &acceptedPeer : newlyAccepted) {
                    emit friendAccepted(acceptedPeer);
                }
                for (const QString &declinedPeer : newlyDeclinedAfterAsyncRequests) {
                    emit friendRejected(declinedPeer);
                }
                checkFriendsStatus();
                return;
            }
        }
        m_lastFriendsFetchSucceeded = false;
    });
}

void FriendService::loadFriendRequests() {
    syncSessionContext();

    if (!ensureAuthToken() || !m_transport || m_loadingRequests) {
        return;
    }

    m_loadingRequests = true;
    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    m_transport->get(Constants::EP_FRIEND_REQUESTS, {}, this, [this, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_loadingRequests = false;
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }
        if (m_requestsRefreshPending) {
            m_requestsRefreshPending = false;
            loadFriendRequests();
            return;
        }

        if (error == QNetworkReply::NoError && statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) {
                return;
            }

            QJsonObject dataObj = doc.object().value("data").toObject();

            QStringList serverIncoming;
            QStringList newlyArrivedIncoming;
            const QJsonArray inArr = dataObj.value("incoming").toArray();
            for (const auto &v : inArr) {
                QString u = cleanUsername(v.toString());
                if (!u.isEmpty() && !serverIncoming.contains(u, Qt::CaseInsensitive)) {
                    serverIncoming.append(u);
                    if (!m_cachedPending.contains(u, Qt::CaseInsensitive)) {
                        newlyArrivedIncoming.append(u);
                    }
                }
            }

            QStringList serverOutgoing;
            const QJsonArray outArr = dataObj.value("outgoing").toArray();
            for (const auto &v : outArr) {
                QString u = cleanUsername(v.toString());
                if (!u.isEmpty() && !serverOutgoing.contains(u, Qt::CaseInsensitive)) {
                    serverOutgoing.append(u);
                    m_knownOutgoing.insert(u.toLower());
                }
            }

            QStringList newlyDeclined;
            if (!m_loadingFriends && !m_friendsRefreshPending && m_lastFriendsFetchSucceeded) {
                for (const QString &prevOut : m_cachedOutgoing) {
                    if (!serverOutgoing.contains(prevOut, Qt::CaseInsensitive) &&
                        !m_cachedFriends.contains(prevOut, Qt::CaseInsensitive) &&
                        !hasInFlightMutationForUser(prevOut.toLower())) {
                        newlyDeclined.append(prevOut);
                        m_knownOutgoing.remove(prevOut.toLower());
                    }
                }
            }

            m_cachedPending = serverIncoming;
            m_cachedOutgoing = serverOutgoing;
            if (m_storage) {
                m_storage->setPendingRequests(m_cachedPending);
                m_storage->setOutgoingRequests(m_cachedOutgoing);
            }
            emit pendingRequestsChanged(m_cachedPending);
            emit outgoingRequestsChanged(m_cachedOutgoing);

            for (const QString &sender : newlyArrivedIncoming) {
                emit friendRequestReceived(sender);
            }
            for (const QString &decliner : newlyDeclined) {
                emit friendRejected(decliner);
            }
        }
    });
}

void FriendService::setPeerStatus(const QString &username, const QString &status) {
    QString target = cleanUsername(username).toLower();
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

QString FriendService::getPeerStatus(const QString &username) const {
    QString target = cleanUsername(username).toLower();
    if (target.isEmpty()) return QStringLiteral("offline");
    std::lock_guard<std::mutex> lock(m_presenceMutex);
    return m_peerStatuses.value(target, QStringLiteral("offline"));
}

QVariantMap FriendService::allPeerStatuses() const {
    std::lock_guard<std::mutex> lock(m_presenceMutex);
    QVariantMap map;
    for (auto it = m_peerStatuses.begin(); it != m_peerStatuses.end(); ++it) {
        map.insert(it.key(), it.value());
    }
    return map;
}

void FriendService::updateLastSeen(const QString &username) {
    QString target = cleanUsername(username).toLower();
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
    QString u = cleanUsername(username).toLower();
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
    QString target = cleanUsername(username);
    if (target.isEmpty()) {
        emit addFriendResult(false, "Username cannot be empty.", QString());
        return;
    }

    syncSessionContext();

    QString myUsername = m_storage ? cleanUsername(m_storage->username()) : QString();
    if (!myUsername.isEmpty() && target.compare(myUsername, Qt::CaseInsensitive) == 0) {
        emit addFriendResult(false, "You cannot add yourself as a friend.", target);
        return;
    }

    if (!ensureAuthToken() || !m_transport) {
        emit addFriendResult(false, "Authentication required. Please log in again.", target);
        return;
    }

    const QString mutKey = QStringLiteral("add:") + target.toLower();
    if (m_inFlightMutations.contains(mutKey)) {
        return;
    }
    m_inFlightMutations.insert(mutKey);

    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    QJsonObject payload;
    payload["username"] = target;
    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_FRIENDS, postData, this, [this, target, mutKey, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_inFlightMutations.remove(mutKey);
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        if (error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 201)) {
            if (!m_cachedOutgoing.contains(target, Qt::CaseInsensitive)) {
                m_cachedOutgoing.append(target);
                if (m_storage) {
                    m_storage->setOutgoingRequests(m_cachedOutgoing);
                }
                emit outgoingRequestsChanged(m_cachedOutgoing);
            }
            m_knownOutgoing.insert(target.toLower());
            requestFriendRequestsRefresh();
            emit addFriendResult(true, "Friend request sent to @" + target, target);
            return;
        }

        if (statusCode == 400) {
            if (errVal.contains("yourself", Qt::CaseInsensitive)) {
                emit addFriendResult(false, "You cannot add yourself as a friend.", target);
            } else {
                emit addFriendResult(false, errVal.isEmpty() ? "Invalid friend request." : errVal, target);
            }
            return;
        }

        if (statusCode == 401) {
            emit addFriendResult(false, "Authentication required. Please log in again.", target);
            return;
        }

        if (statusCode == 404) {
            emit addFriendResult(false, "User '@" + target + "' does not exist on the network.", target);
            return;
        }

        if (statusCode == 409) {
            if (errVal.contains("already sent", Qt::CaseInsensitive)) {
                requestFriendRequestsRefresh();
                emit addFriendResult(false, "Friend request already sent to @" + target + ".", target);
            } else if (errVal.contains("already received", Qt::CaseInsensitive)) {
                requestFriendRequestsRefresh();
                emit addFriendResult(false, "@" + target + " has already sent you a friend request.", target);
            } else {
                requestFriendsRefresh();
                emit addFriendResult(false, target + " is already your friend.", target);
            }
            return;
        }

        if (statusCode == 429) {
            emit addFriendResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }

        emit addFriendResult(false, errVal.isEmpty() ? "Failed to communicate with server." : errVal, target);
    });
}

void FriendService::handleTransmissionStatus(const QString &targetUser, const QString &messageId, bool success, const QString &errorMessage) {
    Q_UNUSED(targetUser);
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
    if (sender.isEmpty()) return;

    QString myUsername = m_storage ? m_storage->username().trimmed() : QString();
    if (!myUsername.isEmpty() && sender.compare(myUsername, Qt::CaseInsensitive) == 0) {
        return;
    }

    if (msg.type == "friend_request") {
        if (m_cachedFriends.contains(sender, Qt::CaseInsensitive)) {
            return;
        }
        if (!m_cachedPending.contains(sender, Qt::CaseInsensitive)) {
            m_cachedPending.append(sender);
            if (m_storage) {
                m_storage->setPendingRequests(m_cachedPending);
            }
            emit pendingRequestsChanged(m_cachedPending);
            emit friendRequestReceived(sender);
        }
    } else if (msg.type == "friend_accept") {
        for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
            if (m_cachedPending[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedPending.removeAt(i);
            }
        }
        for (int i = m_cachedOutgoing.size() - 1; i >= 0; --i) {
            if (m_cachedOutgoing[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedOutgoing.removeAt(i);
            }
        }
        m_knownOutgoing.remove(sender.toLower());
        if (m_storage) {
            m_storage->setPendingRequests(m_cachedPending);
            m_storage->setOutgoingRequests(m_cachedOutgoing);
        }
        emit pendingRequestsChanged(m_cachedPending);
        emit outgoingRequestsChanged(m_cachedOutgoing);

        if (!m_cachedFriends.contains(sender, Qt::CaseInsensitive)) {
            m_cachedFriends.append(sender);
            if (m_storage) {
                m_storage->setFriends(m_cachedFriends);
            }
            emit friendsListChanged(m_cachedFriends);
        }
        emit friendAccepted(sender);
    } else if (msg.type == "friend_reject") {
        for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
            if (m_cachedPending[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedPending.removeAt(i);
            }
        }
        for (int i = m_cachedOutgoing.size() - 1; i >= 0; --i) {
            if (m_cachedOutgoing[i].compare(sender, Qt::CaseInsensitive) == 0) {
                m_cachedOutgoing.removeAt(i);
            }
        }
        m_knownOutgoing.remove(sender.toLower());
        if (m_storage) {
            m_storage->setPendingRequests(m_cachedPending);
            m_storage->setOutgoingRequests(m_cachedOutgoing);
        }
        emit pendingRequestsChanged(m_cachedPending);
        emit outgoingRequestsChanged(m_cachedOutgoing);
        emit friendRejected(sender);
    }
}

void FriendService::acceptFriend(const QString &username) {
    QString target = cleanUsername(username);
    if (target.isEmpty()) {
        emit acceptFriendResult(false, "Username cannot be empty.", QString());
        return;
    }

    syncSessionContext();

    if (!ensureAuthToken() || !m_transport) {
        emit acceptFriendResult(false, "Authentication required. Please log in again.", target);
        return;
    }

    const QString lowerTarget = target.toLower();
    if (m_inFlightMutations.contains(QStringLiteral("reject:") + lowerTarget)) {
        emit acceptFriendResult(false, "Another request action for @" + target + " is already in progress.", target);
        return;
    }
    const QString mutKey = QStringLiteral("accept:") + lowerTarget;
    if (m_inFlightMutations.contains(mutKey)) {
        return;
    }
    m_inFlightMutations.insert(mutKey);

    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    QJsonObject payload;
    payload["username"] = target;
    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_FRIEND_REQUESTS_ACCEPT, postData, this, [this, target, mutKey, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_inFlightMutations.remove(mutKey);
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        if (error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 201)) {
            for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
                if (m_cachedPending[i].compare(target, Qt::CaseInsensitive) == 0) {
                    m_cachedPending.removeAt(i);
                }
            }
            for (int i = m_cachedOutgoing.size() - 1; i >= 0; --i) {
                if (m_cachedOutgoing[i].compare(target, Qt::CaseInsensitive) == 0) {
                    m_cachedOutgoing.removeAt(i);
                }
            }
            m_knownOutgoing.remove(target.toLower());
            if (!m_cachedFriends.contains(target, Qt::CaseInsensitive)) {
                m_cachedFriends.append(target);
            }
            if (m_storage) {
                m_storage->setPendingRequests(m_cachedPending);
                m_storage->setOutgoingRequests(m_cachedOutgoing);
                m_storage->setFriends(m_cachedFriends);
            }
            emit pendingRequestsChanged(m_cachedPending);
            emit outgoingRequestsChanged(m_cachedOutgoing);
            emit friendsListChanged(m_cachedFriends);
            emit acceptFriendResult(true, "Accepted friend request from @" + target, target);
            requestFriendsRefresh();
            return;
        }

        if (statusCode == 400) {
            emit acceptFriendResult(false, errVal.isEmpty() ? "Invalid friend request." : errVal, target);
            return;
        }
        if (statusCode == 401) {
            emit acceptFriendResult(false, "Authentication required. Please log in again.", target);
            return;
        }
        if (statusCode == 403) {
            emit acceptFriendResult(false, "You are not authorized to accept this friend request.", target);
            return;
        }
        if (statusCode == 404) {
            requestFriendRequestsRefresh();
            emit acceptFriendResult(false, "Friend request from '@" + target + "' was not found.", target);
            return;
        }
        if (statusCode == 429) {
            emit acceptFriendResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }

        emit acceptFriendResult(false, errVal.isEmpty() ? "Failed to accept friend request." : errVal, target);
    });
}

void FriendService::rejectFriend(const QString &username) {
    QString target = cleanUsername(username);
    if (target.isEmpty()) {
        emit rejectFriendResult(false, "Username cannot be empty.", QString());
        return;
    }

    syncSessionContext();

    if (!ensureAuthToken() || !m_transport) {
        emit rejectFriendResult(false, "Authentication required. Please log in again.", target);
        return;
    }

    const QString lowerTarget = target.toLower();
    if (m_inFlightMutations.contains(QStringLiteral("accept:") + lowerTarget)) {
        emit rejectFriendResult(false, "Another request action for @" + target + " is already in progress.", target);
        return;
    }
    const QString mutKey = QStringLiteral("reject:") + lowerTarget;
    if (m_inFlightMutations.contains(mutKey)) {
        return;
    }
    m_inFlightMutations.insert(mutKey);

    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    QJsonObject payload;
    payload["username"] = target;
    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_FRIEND_REQUESTS_DECLINE, postData, this, [this, target, mutKey, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_inFlightMutations.remove(mutKey);
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        if (error == QNetworkReply::NoError && statusCode == 200) {
            for (int i = m_cachedPending.size() - 1; i >= 0; --i) {
                if (m_cachedPending[i].compare(target, Qt::CaseInsensitive) == 0) {
                    m_cachedPending.removeAt(i);
                }
            }
            if (m_storage) {
                m_storage->setPendingRequests(m_cachedPending);
            }
            emit pendingRequestsChanged(m_cachedPending);
            emit rejectFriendResult(true, "Declined friend request from @" + target, target);
            requestFriendRequestsRefresh();
            return;
        }

        if (statusCode == 400) {
            emit rejectFriendResult(false, errVal.isEmpty() ? "Invalid friend request." : errVal, target);
            return;
        }
        if (statusCode == 401) {
            emit rejectFriendResult(false, "Authentication required. Please log in again.", target);
            return;
        }
        if (statusCode == 403) {
            emit rejectFriendResult(false, "You are not authorized to decline this friend request.", target);
            return;
        }
        if (statusCode == 404) {
            requestFriendRequestsRefresh();
            emit rejectFriendResult(false, "Friend request from '@" + target + "' was not found.", target);
            return;
        }
        if (statusCode == 429) {
            emit rejectFriendResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }

        emit rejectFriendResult(false, errVal.isEmpty() ? "Failed to decline friend request." : errVal, target);
    });
}

void FriendService::cancelFriendRequest(const QString &username) {
    QString target = cleanUsername(username);
    if (target.isEmpty()) {
        emit cancelFriendRequestResult(false, "Username cannot be empty.", QString());
        return;
    }

    syncSessionContext();

    if (!ensureAuthToken() || !m_transport) {
        emit cancelFriendRequestResult(false, "Authentication required. Please log in again.", target);
        return;
    }

    const QString mutKey = QStringLiteral("cancel:") + target.toLower();
    if (m_inFlightMutations.contains(mutKey)) {
        return;
    }
    m_inFlightMutations.insert(mutKey);

    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    QJsonObject payload;
    payload["username"] = target;
    QByteArray deleteData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->deleteResource(Constants::EP_FRIEND_REQUESTS, this, [this, target, mutKey, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_inFlightMutations.remove(mutKey);
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        if (error == QNetworkReply::NoError && statusCode == 200) {
            for (int i = m_cachedOutgoing.size() - 1; i >= 0; --i) {
                if (m_cachedOutgoing[i].compare(target, Qt::CaseInsensitive) == 0) {
                    m_cachedOutgoing.removeAt(i);
                }
            }
            m_knownOutgoing.remove(target.toLower());
            if (m_storage) {
                m_storage->setOutgoingRequests(m_cachedOutgoing);
            }
            emit outgoingRequestsChanged(m_cachedOutgoing);
            emit cancelFriendRequestResult(true, "Canceled friend request to @" + target, target);
            requestFriendRequestsRefresh();
            return;
        }

        if (statusCode == 400) {
            emit cancelFriendRequestResult(false, errVal.isEmpty() ? "Invalid friend request." : errVal, target);
            return;
        }
        if (statusCode == 401) {
            emit cancelFriendRequestResult(false, "Authentication required. Please log in again.", target);
            return;
        }
        if (statusCode == 403) {
            emit cancelFriendRequestResult(false, "You are not authorized to cancel this friend request.", target);
            return;
        }
        if (statusCode == 404) {
            requestFriendRequestsRefresh();
            emit cancelFriendRequestResult(false, "Friend request to '@" + target + "' was not found.", target);
            return;
        }
        if (statusCode == 429) {
            emit cancelFriendRequestResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }

        emit cancelFriendRequestResult(false, errVal.isEmpty() ? "Failed to cancel friend request." : errVal, target);
    }, deleteData);
}

void FriendService::removeFriend(const QString &username) {
    QString target = cleanUsername(username);
    if (target.isEmpty()) {
        emit removeFriendResult(false, "Username cannot be empty.", QString());
        return;
    }

    syncSessionContext();

    if (!ensureAuthToken() || !m_transport) {
        emit removeFriendResult(false, "Authentication required. Please log in again.", target);
        return;
    }

    const QString mutKey = QStringLiteral("remove:") + target.toLower();
    if (m_inFlightMutations.contains(mutKey)) {
        return;
    }
    m_inFlightMutations.insert(mutKey);

    const uint64_t capturedGen = m_sessionGeneration;
    const QString capturedUser = m_sessionUsername;

    QJsonObject payload;
    payload["username"] = target;
    QByteArray deleteData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->deleteResource(Constants::EP_FRIENDS, this, [this, target, mutKey, capturedGen, capturedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (m_sessionGeneration == capturedGen) {
            m_inFlightMutations.remove(mutKey);
        }
        const QString activeUser = m_storage ? cleanUsername(m_storage->username()).toLower() : QString();
        if (m_sessionGeneration != capturedGen || activeUser != capturedUser) {
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        if (error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 204)) {
            for (int i = m_cachedFriends.size() - 1; i >= 0; --i) {
                if (m_cachedFriends[i].compare(target, Qt::CaseInsensitive) == 0) {
                    m_cachedFriends.removeAt(i);
                }
            }
            {
                std::lock_guard<std::mutex> lock(m_presenceMutex);
                m_peerStatuses.remove(target.toLower());
                m_lastSeen.remove(target.toLower());
            }
            if (m_storage) {
                m_storage->setFriends(m_cachedFriends);
            }
            emit friendsListChanged(m_cachedFriends);
            emit removeFriendResult(true, "Removed @" + target + " from friends", target);
            requestFriendsRefresh();
            return;
        }

        if (statusCode == 400) {
            emit removeFriendResult(false, errVal.isEmpty() ? "Invalid friend removal request." : errVal, target);
            return;
        }
        if (statusCode == 401) {
            emit removeFriendResult(false, "Authentication required. Please log in again.", target);
            return;
        }
        if (statusCode == 404) {
            emit removeFriendResult(false, "User '@" + target + "' does not exist on the network.", target);
            return;
        }
        if (statusCode == 429) {
            emit removeFriendResult(false, "Rate limit exceeded. Please try again later.", target);
            return;
        }

        emit removeFriendResult(false, errVal.isEmpty() ? "Failed to remove friend." : errVal, target);
    }, deleteData);
}

void FriendService::startHeartbeat() {
    if (!m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->start();
    }
    if (!m_syncTimer->isActive()) {
        m_syncTimer->start();
    }
    checkFriendsStatus();
}

void FriendService::stopHeartbeat() {
    if (m_heartbeatTimer->isActive()) {
        m_heartbeatTimer->stop();
    }
    if (m_syncTimer->isActive()) {
        m_syncTimer->stop();
    }
    invalidateSession();
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
