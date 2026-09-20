// src/core/networkmanager.cpp
#include "networkmanager.h"
#include "cryptomanager.h"
#include "../transport/httptransport.h"
#include "../storage/settingsrepository.h"
#include "../crypto/cryptoservice.h"
#include <QDebug>
#include <QDateTime>
#include <algorithm>

NetworkManager::NetworkManager(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport,
                               std::shared_ptr<NeoNect::Storage::ISettingsRepository> storage,
                               std::shared_ptr<NeoNect::Crypto::ICryptoService> cryptoService,
                               std::shared_ptr<NeoNect::Services::AuthService> authService,
                               std::shared_ptr<NeoNect::Services::DeviceService> deviceService,
                               std::shared_ptr<NeoNect::Services::RelayService> relayService,
                               std::shared_ptr<NeoNect::Services::FriendService> friendService,
                               QObject *parent)
    : QObject(parent), m_transport(std::move(transport)),
      m_storage(std::move(storage)), m_cryptoService(std::move(cryptoService)),
      m_authService(std::move(authService)), m_deviceService(std::move(deviceService)),
      m_relayService(std::move(relayService)), m_friendService(std::move(friendService))
{
    if (m_storage) {
        QVariantList raw = m_storage->openConversations();
        QString myUser = m_storage->username().trimmed().toLower();
        for (const auto &c : raw) {
            QString name = c.toMap().value("name").toString().trimmed().toLower();
            if (!name.isEmpty() && name != "saved-messages" && name != "friends" && (myUser.isEmpty() || name != myUser)) {
                m_openConversations.append(c);
            }
        }
        sortOpenConversations();
        m_storage->setOpenConversations(m_openConversations);
    }
    if (m_authService) {
        setupServiceSignals();
    }
}

void NetworkManager::initializeCustom(std::shared_ptr<NeoNect::Transport::IHttpTransport> transport) {
    if (!transport) return;
    m_transport = transport;
    m_transport->setBaseUrl(m_storage->serverUrl());
    m_transport->setAuthToken(QString());

    m_authService = std::make_shared<NeoNect::Services::AuthService>(m_transport, m_storage, nullptr);
    m_deviceService = std::make_shared<NeoNect::Services::DeviceService>(m_transport, m_storage, nullptr);
    m_relayService = std::make_shared<NeoNect::Services::RelayService>(m_transport, m_storage, m_cryptoService, nullptr);
    m_friendService = std::make_shared<NeoNect::Services::FriendService>(m_transport, m_storage, nullptr);

    setupServiceSignals();
    m_friendService->loadFriends();
    // Auto-login removed
}

void NetworkManager::setupServiceSignals() {
    // Auth Service Connections
    connect(m_authService.get(), &NeoNect::Services::AuthService::verificationResult, this, [this](bool success, const QString &message) {
        setIsLoading(false);
        emit serverUrlChanged();
        emit verificationResult(success, message);

        if (!m_pendingBookmarkUsername.isEmpty()) {
            QString user = m_pendingBookmarkUsername;
            QString pass = m_pendingBookmarkPassword;
            m_pendingBookmarkUsername.clear();
            m_pendingBookmarkPassword.clear();

            if (success) {
                loginUser(user, pass);
            } else {
                emit loginResult(false, QString("Could not connect to server: %1").arg(message));
            }
        }
    });

    connect(m_authService.get(), &NeoNect::Services::AuthService::availabilityResult, this, [this](const QString &username, bool available, const QString &error) {
        emit availabilityResult(username, available, error);
    });

    connect(m_authService.get(), &NeoNect::Services::AuthService::registrationResult, this, [this](bool success, const QString &message) {
        setIsLoading(false);
        emit registrationResult(success, message);
    });

    connect(m_authService.get(), &NeoNect::Services::AuthService::loginResult, this, [this](bool success, const QString &tokenOrError) {
        setIsLoading(false);
        if (success) {
            m_sessionToken = tokenOrError;
            m_transport->setAuthToken(tokenOrError);
            emit tokenChanged();
            emit currentUsernameChanged();

            // Reload user-scoped open conversations
            m_openConversations.clear();
            if (m_storage) {
                QVariantList raw = m_storage->openConversations();
                QString myUser = m_storage->username().trimmed().toLower();
                for (const auto &c : raw) {
                    QString name = c.toMap().value("name").toString().trimmed().toLower();
                    if (!name.isEmpty() && name != "saved-messages" && name != "friends" && (myUser.isEmpty() || name != myUser)) {
                        m_openConversations.append(c);
                    }
                }
                sortOpenConversations();
                m_storage->setOpenConversations(m_openConversations);
            }
            emit openConversationsChanged();

            m_friendService->loadFriends();
            autoRegisterDevice();
            m_authService->fetchUserProfile();
        } else {
            m_pendingBookmarkUsername.clear();
            m_pendingBookmarkPassword.clear();
        }
        emit loginResult(success, tokenOrError);
    });

    connect(m_authService.get(), &NeoNect::Services::AuthService::userProfileFetched, this, [this](bool success, const QString &username) {
        if (success) {
            emit currentUsernameChanged();
        }
        emit userProfileFetched(success, username);
    });

    // Device Service Connections
    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::deviceRegistrationResult, this, [this](bool success, const QString &message) {
        if (success) {
            m_relayService->startPolling();
            m_friendService->startHeartbeat();
        }
        emit deviceRegistrationResult(success, message);
    });

    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::deviceKeyFetched, this, &NetworkManager::deviceKeyFetched);
    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::deviceRevocationResult, this, &NetworkManager::deviceRevocationResult);
    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::recipientKeysFetched, this, &NetworkManager::recipientKeysFetched);

    // Relay Service Connections
    connect(m_relayService.get(), &NeoNect::Services::RelayService::incomingDomainMessagesReceived, this, [this](const std::vector<NeoNect::Domain::Message> &msgs) {
        QString myUser = currentUsername().trimmed().toLower();
        for (const auto &msg : msgs) {
            QString sender = msg.senderId.trimmed().toLower();
            if (msg.type != "typing_start" && msg.type != "typing_stop" && !sender.isEmpty() && (myUser.isEmpty() || sender != myUser)) {
                updateConversationActivity(msg.senderId, msg.timestamp);
            }
            emit incomingRelayMessageReceived(msg.senderId, msg.conversationId, msg.text, msg.timestamp);
        }
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::sessionUnauthorized, this, [this](const QString &message) {
        m_sessionToken.clear();
        m_openConversations.clear();
        emit openConversationsChanged();
        emit tokenChanged();
        emit currentUsernameChanged();
        emit isConnectedChanged();
        emit loginResult(false, message);
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::deviceRegistrationRequested, this, &NetworkManager::autoRegisterDevice);

    connect(m_relayService.get(), &NeoNect::Services::RelayService::serverConnected, this, [this]() {
        emit isConnectedChanged();
        m_friendService->checkFriendsStatus();
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::serverDisconnected, this, [this]() {
        emit isConnectedChanged();
        for (const QString &f : m_friendService->friends()) {
            emit friendStatusUpdated(f.trimmed().toLower(), "offline");
        }
    });

    // Friend Service Connections
    connect(m_friendService.get(), &NeoNect::Services::FriendService::friendsListChanged, this, [this](const QStringList&) {
        emit friendsChanged();
    });
    connect(m_friendService.get(), &NeoNect::Services::FriendService::pendingRequestsChanged, this, [this](const QStringList&) {
        emit pendingRequestsChanged();
    });

    connect(m_friendService.get(), &NeoNect::Services::FriendService::addFriendResult, this, &NetworkManager::addFriendResult);
    connect(m_friendService.get(), &NeoNect::Services::FriendService::acceptFriendResult, this, &NetworkManager::acceptFriendResult);
    connect(m_friendService.get(), &NeoNect::Services::FriendService::rejectFriendResult, this, &NetworkManager::rejectFriendResult);
    connect(m_friendService.get(), &NeoNect::Services::FriendService::removeFriendResult, this, &NetworkManager::removeFriendResult);
    connect(m_friendService.get(), &NeoNect::Services::FriendService::friendStatusUpdated, this, &NetworkManager::friendStatusUpdated);
}

void NetworkManager::autoRegisterDevice() {
    registerDevice(m_storage->deviceId(), m_storage->publicKey());
}

void NetworkManager::setIsLoading(bool loading) {
    if (m_isLoading != loading) {
        m_isLoading = loading;
        emit isLoadingChanged();
    }
}

QString NetworkManager::serverUrl() const {
    return m_storage->serverUrl();
}

QString NetworkManager::token() const {
    return m_sessionToken;
}

QString NetworkManager::currentUsername() const {
    return m_sessionToken.isEmpty() ? QString() : m_storage->username();
}

bool NetworkManager::isConnected() const {
    return m_relayService ? m_relayService->isConnected() : false;
}

QStringList NetworkManager::friends() const {
    return m_friendService->friends();
}

QStringList NetworkManager::pendingRequests() const {
    return m_friendService->pendingRequests();
}

QVariantList NetworkManager::bookmarks() const {
    return m_storage->bookmarks();
}

void NetworkManager::saveBookmark(const QString &name, const QString &serverUrl, const QString &username, const QString &password, const QString &id) {
    QVariantMap bm;
    if (!id.trimmed().isEmpty()) {
        bm["id"] = id.trimmed();
    }
    bm["name"] = name.trimmed().isEmpty() ? serverUrl.trimmed() : name.trimmed();
    bm["serverUrl"] = serverUrl.trimmed();
    bm["username"] = username.trimmed();
    bm["password"] = password;
    bm["updatedAt"] = QDateTime::currentMSecsSinceEpoch();

    if (!id.trimmed().isEmpty()) {
        m_storage->updateBookmark(bm);
    } else {
        m_storage->addBookmark(bm);
    }
    emit bookmarksChanged();
}

void NetworkManager::deleteBookmark(const QString &id) {
    m_storage->removeBookmark(id);
    emit bookmarksChanged();
}

void NetworkManager::connectBookmark(const QString &id) {
    if (id.isEmpty()) return;
    QVariantList list = m_storage->bookmarks();
    QVariantMap target;
    for (const auto &item : list) {
        QVariantMap m = item.toMap();
        if (m.value("id").toString() == id) {
            target = m;
            break;
        }
    }
    if (target.isEmpty()) {
        emit loginResult(false, "Bookmark not found.");
        return;
    }

    QString sUrl = target.value("serverUrl").toString().trimmed();
    QString uName = target.value("username").toString().trimmed();
    QString pWord = target.value("password").toString();

    m_storage->setServerUrl(sUrl);
    m_transport->setBaseUrl(sUrl);
    emit serverUrlChanged();

    target["lastConnected"] = QDateTime::currentMSecsSinceEpoch();
    m_storage->updateBookmark(target);
    emit bookmarksChanged();

    setIsLoading(true);
    m_pendingBookmarkUsername = uName;
    m_pendingBookmarkPassword = pWord;
    m_authService->verifyServer(sUrl);
}

void NetworkManager::setProfile(const QString &profileName) {
    m_storage->setProfile(profileName);
    m_transport->setBaseUrl(m_storage->serverUrl());
    m_transport->setAuthToken(m_sessionToken);

    emit serverUrlChanged();
    emit tokenChanged();
    emit currentUsernameChanged();
    emit bookmarksChanged();

    if (m_storage) {
        m_openConversations = m_storage->openConversations();
        sortOpenConversations();
        emit openConversationsChanged();
    }

    m_friendService->loadFriends();
    // Auto-login removed
}

void NetworkManager::verifyServer(const QString &address) {
    setIsLoading(true);
    m_authService->verifyServer(address);
}

void NetworkManager::checkUsernameAvailability(const QString &username) {
    m_authService->checkUsernameAvailability(username);
}

void NetworkManager::registerUser(const QString &username, const QString &password) {
    setIsLoading(true);
    m_authService->registerUser(username, password);
}

void NetworkManager::loginUser(const QString &username, const QString &password) {
    setIsLoading(true);
    m_authService->loginUser(username, password);
}

void NetworkManager::logoutUser() {
    m_sessionToken.clear();
    m_pendingBookmarkUsername.clear();
    m_pendingBookmarkPassword.clear();
    m_transport->setAuthToken(QString());
    m_relayService->stopPolling();
    m_friendService->stopHeartbeat();
    m_authService->logoutUser();
    m_friendService->loadFriends();
    m_openConversations.clear();
    emit openConversationsChanged();
    emit tokenChanged();
    emit currentUsernameChanged();
    emit isConnectedChanged();
}

void NetworkManager::registerDevice(const QString &deviceId, const QString &publicKey) {
    m_deviceService->registerDevice(deviceId, publicKey);
}

void NetworkManager::fetchDevicePublicKey(const QString &deviceId) {
    m_deviceService->fetchDevicePublicKey(deviceId);
}

void NetworkManager::revokeDevice(const QString &deviceId) {
    m_deviceService->revokeDevice(deviceId);
}

void NetworkManager::fetchRecipientKeys(const QString &username) {
    m_deviceService->fetchRecipientKeys(username);
}

void NetworkManager::fetchUserProfile() {
    m_authService->fetchUserProfile();
}




void NetworkManager::pollPendingMessages() {
    m_relayService->pollPendingMessages();
}

void NetworkManager::acknowledgeMessage(qint64 messageId) {
    m_relayService->acknowledgeMessage(messageId);
}

void NetworkManager::addFriend(const QString &username) {
    m_friendService->addFriend(username);
}

void NetworkManager::acceptFriend(const QString &username) {
    m_friendService->acceptFriend(username);
}

void NetworkManager::rejectFriend(const QString &username) {
    m_friendService->rejectFriend(username);
}

void NetworkManager::removeFriend(const QString &username) {
    m_friendService->removeFriend(username);
}

void NetworkManager::checkFriendsStatus() {
    m_friendService->checkFriendsStatus();
}

void NetworkManager::checkUserStatus(const QString &username) {
    m_friendService->checkUserStatus(username);
}

void NetworkManager::sortOpenConversations() {
    std::sort(m_openConversations.begin(), m_openConversations.end(), [](const QVariant &a, const QVariant &b) {
        qint64 tA = a.toMap().value("lastActivity").toLongLong();
        qint64 tB = b.toMap().value("lastActivity").toLongLong();
        if (tA != tB) {
            return tA > tB; // More recent activity first
        }
        return a.toMap().value("name").toString().compare(b.toMap().value("name").toString(), Qt::CaseInsensitive) < 0;
    });
}

QVariantList NetworkManager::openConversations() const {
    return m_openConversations;
}

void NetworkManager::openDirectConversation(const QString &username, qint64 activityTimestamp) {
    QString lower = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (myUser.isEmpty() && m_storage) {
        myUser = m_storage->username().trimmed().toLower();
    }
    if (lower.isEmpty() || lower == "saved-messages" || lower == "friends" || (!myUser.isEmpty() && lower == myUser)) return;

    qint64 ts = activityTimestamp > 0 ? activityTimestamp : QDateTime::currentMSecsSinceEpoch();

    bool found = false;
    for (int i = 0; i < m_openConversations.size(); ++i) {
        QVariantMap map = m_openConversations.at(i).toMap();
        if (map.value("name").toString().toLower() == lower) {
            map["lastActivity"] = ts;
            m_openConversations[i] = map;
            found = true;
            break;
        }
    }

    if (!found) {
        QVariantMap map;
        map["name"] = lower;
        map["lastActivity"] = ts;
        map["unreadCount"] = 0;
        m_openConversations.append(map);
    }

    sortOpenConversations();
    if (m_storage) {
        m_storage->setOpenConversations(m_openConversations);
    }
    emit openConversationsChanged();
}

void NetworkManager::closeDirectConversation(const QString &username) {
    QString lower = username.trimmed().toLower();
    if (lower.isEmpty()) return;

    bool removed = false;
    for (int i = 0; i < m_openConversations.size(); ++i) {
        if (m_openConversations.at(i).toMap().value("name").toString().toLower() == lower) {
            m_openConversations.removeAt(i);
            removed = true;
            break;
        }
    }

    if (removed) {
        if (m_storage) {
            m_storage->setOpenConversations(m_openConversations);
        }
        emit openConversationsChanged();
    }
}

void NetworkManager::updateConversationActivity(const QString &username, qint64 activityTimestamp) {
    QString lower = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (myUser.isEmpty() && m_storage) {
        myUser = m_storage->username().trimmed().toLower();
    }
    if (lower.isEmpty() || lower == "saved-messages" || lower == "friends" || (!myUser.isEmpty() && lower == myUser)) return;
    openDirectConversation(lower, activityTimestamp);
}

int NetworkManager::unreadCount(const QString &username) const {
    QString lower = username.trimmed().toLower();
    for (const QVariant &item : m_openConversations) {
        QVariantMap map = item.toMap();
        if (map.value("name").toString().toLower() == lower) {
            return map.value("unreadCount", 0).toInt();
        }
    }
    return 0;
}

void NetworkManager::markConversationAsRead(const QString &username) {
    QString lower = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (myUser.isEmpty() && m_storage) {
        myUser = m_storage->username().trimmed().toLower();
    }
    if (lower.isEmpty() || lower == "saved-messages" || lower == "friends" || (!myUser.isEmpty() && lower == myUser)) return;

    bool updated = false;
    for (int i = 0; i < m_openConversations.size(); ++i) {
        QVariantMap map = m_openConversations.at(i).toMap();
        if (map.value("name").toString().toLower() == lower) {
            if (map.value("unreadCount", 0).toInt() > 0) {
                map["unreadCount"] = 0;
                m_openConversations[i] = map;
                updated = true;
            }
            break;
        }
    }

    if (updated) {
        if (m_storage) {
            m_storage->setOpenConversations(m_openConversations);
        }
        emit openConversationsChanged();
    }
}

void NetworkManager::incrementUnreadCount(const QString &username) {
    QString lower = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (myUser.isEmpty() && m_storage) {
        myUser = m_storage->username().trimmed().toLower();
    }
    if (lower.isEmpty() || lower == "saved-messages" || lower == "friends" || (!myUser.isEmpty() && lower == myUser)) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool found = false;
    for (int i = 0; i < m_openConversations.size(); ++i) {
        QVariantMap map = m_openConversations.at(i).toMap();
        if (map.value("name").toString().toLower() == lower) {
            int current = map.value("unreadCount", 0).toInt();
            map["unreadCount"] = current + 1;
            map["lastActivity"] = now;
            m_openConversations[i] = map;
            found = true;
            break;
        }
    }

    if (!found) {
        QVariantMap map;
        map["name"] = lower;
        map["lastActivity"] = now;
        map["unreadCount"] = 1;
        m_openConversations.append(map);
    }

    sortOpenConversations();

    if (m_storage) {
        m_storage->setOpenConversations(m_openConversations);
    }
    emit openConversationsChanged();
}
