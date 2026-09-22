// src/core/networkmanager.cpp
#include "networkmanager.h"
#include "cryptomanager.h"
#include "../transport/httptransport.h"
#include "../storage/settingsrepository.h"
#include "../crypto/cryptoservice.h"
#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <QImage>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>
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
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(m_idleTimeoutMs);
    connect(m_idleTimer, &QTimer::timeout, this, &NetworkManager::onIdleTimeout);
    if (m_userStatusPreference == "online") {
        m_idleTimer->start(m_idleTimeoutMs);
    }

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
            emit displayNameChanged();

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
            m_friendService->startHeartbeat();
            m_friendService->checkFriendsStatus();
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
            if (msg.type == "presence_status") {
                QString reportedStatus = msg.text.trimmed().toLower();
                if (reportedStatus == "idle") reportedStatus = "afk";
                m_friendService->setPeerStatus(sender, reportedStatus);
                continue;
            }
            if (msg.type != "typing_start" && msg.type != "typing_stop" && !sender.isEmpty() && (myUser.isEmpty() || sender != myUser)) {
                updateConversationActivity(msg.senderId, msg.timestamp);
            }
            if (!sender.isEmpty() && (myUser.isEmpty() || sender != myUser)) {
                m_friendService->updateLastSeen(sender);
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

    connect(m_relayService.get(), &NeoNect::Services::RelayService::peerAvatarUpdated, this, [this](const QString &username, const QString &avatarUrl) {
        emit peerAvatarUpdated(username.trimmed().toLower(), avatarUrl);
        emit avatarUrlChanged();
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::serverConnected, this, [this]() {
        emit isConnectedChanged();
        emit effectiveStatusChanged();
        m_friendService->checkFriendsStatus();
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::serverDisconnected, this, [this]() {
        emit isConnectedChanged();
        emit effectiveStatusChanged();
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

QString NetworkManager::displayName() const {
    if (!m_storage) return QString();
    QString custom = m_storage->displayName().trimmed();
    if (!custom.isEmpty()) return custom;
    QString u = m_storage->username().trimmed();
    if (u.isEmpty()) return QString();
    return u.left(1).toUpper() + u.mid(1);
}

void NetworkManager::setDisplayName(const QString &name) {
    if (!m_storage) return;
    m_storage->setDisplayName(name.trimmed());
    emit displayNameChanged();
    emit openConversationsChanged();
    emit friendsChanged();
}

QString NetworkManager::getDisplayName(const QString &username) const {
    QString clean = username.trimmed().toLower();
    if (clean.isEmpty()) return QString();
    if (m_storage && clean == m_storage->username().trimmed().toLower()) {
        return displayName();
    }
    if (m_storage) {
        QString peerName = m_storage->peerDisplayName(clean).trimmed();
        if (!peerName.isEmpty()) return peerName;
    }
    return username.left(1).toUpper() + username.mid(1);
}

void NetworkManager::setPeerDisplayName(const QString &username, const QString &displayName) {
    QString clean = username.trimmed().toLower();
    if (clean.isEmpty() || !m_storage) return;
    m_storage->setPeerDisplayName(clean, displayName.trimmed());
    emit peerDisplayNameUpdated(clean, displayName.trimmed());
    emit openConversationsChanged();
    emit friendsChanged();
}

QString NetworkManager::avatarUrl() const {
    if (!m_storage) return QString();
    QString localUrl = m_storage->avatarUrl().trimmed();
    if (!localUrl.isEmpty()) {
        QString path = localUrl;
        if (path.startsWith("file:///")) {
            path = QUrl(path).toLocalFile();
        } else if (path.startsWith("file://")) {
            path = path.mid(7);
        }
        if (QFile::exists(path)) {
            return localUrl;
        }
    }
    QString profile = m_storage->profile();
    QString username = m_storage->username().trimmed().toLower();
    if (!username.isEmpty()) {
        QString avatarPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars/" + (profile.isEmpty() ? "" : profile + "_") + username + "_avatar.jpg";
        if (QFile::exists(avatarPath)) {
            return QUrl::fromLocalFile(avatarPath).toString();
        }
    }
    return QString();
}

bool NetworkManager::setAvatar(const QString &filePathOrUrl) {
    if (filePathOrUrl.trimmed().isEmpty()) {
        clearAvatar();
        return true;
    }
    QString localPath = filePathOrUrl;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(localPath).toLocalFile();
    } else if (localPath.startsWith("file://")) {
        localPath = localPath.mid(7);
    }

    QImage image;
    if (!image.load(localPath)) {
        qWarning() << "[NetworkManager] setAvatar failed to load image from:" << localPath;
        return false;
    }

    // 1. Crop to 1:1 aspect ratio (center crop)
    int cropSize = qMin(image.width(), image.height());
    int xOffset = (image.width() - cropSize) / 2;
    int yOffset = (image.height() - cropSize) / 2;
    QImage squareImage = image.copy(xOffset, yOffset, cropSize, cropSize);

    // 2. Resize to standard high-res square (512x512 max)
    int targetSize = qMin(cropSize, 512);
    if (squareImage.width() > targetSize || squareImage.height() > targetSize) {
        squareImage = squareImage.scaled(targetSize, targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    // 3. Compress ensuring size is strictly under 1MB (1024 * 1024 bytes)
    QByteArray compressedData;
    int quality = 90;
    while (quality >= 30) {
        compressedData.clear();
        QBuffer buffer(&compressedData);
        buffer.open(QIODevice::WriteOnly);
        squareImage.save(&buffer, "JPEG", quality);
        buffer.close();

        if (compressedData.size() <= 1024 * 1024) {
            break;
        }
        quality -= 10;
    }

    while (compressedData.size() > 1024 * 1024 && squareImage.width() > 128) {
        squareImage = squareImage.scaled(squareImage.width() / 2, squareImage.height() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        compressedData.clear();
        QBuffer buffer(&compressedData);
        buffer.open(QIODevice::WriteOnly);
        squareImage.save(&buffer, "JPEG", 75);
        buffer.close();
    }

    // 4. Save to AppData avatar location
    QString profile = m_storage ? m_storage->profile() : "";
    QString username = m_storage ? m_storage->username().trimmed().toLower() : "user";
    QString avatarDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars/";
    QDir().mkpath(avatarDir);

    QString savedLocalPath = avatarDir + (profile.isEmpty() ? "" : profile + "_") + username + "_avatar.jpg";
    QFile file(savedLocalPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[NetworkManager] setAvatar failed to write to:" << savedLocalPath;
        return false;
    }
    file.write(compressedData);
    file.close();

    QString fileUrl = QUrl::fromLocalFile(savedLocalPath).toString();
    if (m_storage) {
        m_storage->setAvatarUrl(fileUrl);
    }
    emit avatarUrlChanged();

    // Broadcast avatar update to open conversations / friends
    if (m_relayService && m_friendService) {
        QStringList friendList = m_friendService->friends();
        for (const QString &fr : friendList) {
            if (fr.trimmed().isEmpty()) continue;
            NeoNect::Domain::Message avMsg;
            avMsg.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            avMsg.conversationId = "dms:" + fr.trimmed().toLower();
            avMsg.senderId = currentUsername();
            avMsg.type = "avatar_update";
            avMsg.text = QString::fromLatin1(compressedData.toBase64());
            avMsg.timestamp = QDateTime::currentMSecsSinceEpoch();
            m_relayService->sendDomainMessage(avMsg);
        }
    }

    return true;
}

void NetworkManager::clearAvatar() {
    if (m_storage) {
        m_storage->setAvatarUrl(QString());
    }
    emit avatarUrlChanged();
}

QString NetworkManager::getAvatarUrl(const QString &username) const {
    QString u = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (u.isEmpty() || u == myUser) {
        return avatarUrl();
    }
    if (m_storage) {
        QString url = m_storage->peerAvatarUrl(u);
        if (!url.isEmpty()) return url;
    }
    QString profile = m_storage ? m_storage->profile() : "";
    QString peerAvatarPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars/peers/" + (profile.isEmpty() ? "" : profile + "_") + u + "_avatar.jpg";
    if (QFile::exists(peerAvatarPath)) {
        return QUrl::fromLocalFile(peerAvatarPath).toString();
    }
    return QString();
}

void NetworkManager::setPeerAvatarUrl(const QString &username, const QString &avatarUrl) {
    QString u = username.trimmed().toLower();
    if (u.isEmpty() || !m_storage) return;
    m_storage->setPeerAvatarUrl(u, avatarUrl.trimmed());
    emit peerAvatarUpdated(u, avatarUrl.trimmed());
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
    emit displayNameChanged();
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
    m_isAutoIdle = false;
    emit openConversationsChanged();
    emit tokenChanged();
    emit currentUsernameChanged();
    emit displayNameChanged();
    emit isConnectedChanged();
    emit effectiveStatusChanged();
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
    for (const auto &conv : m_openConversations) {
        QString name = conv.toMap().value("name").toString().trimmed().toLower();
        if (!name.isEmpty() && name != "saved-messages" && name != "friends") {
            m_friendService->checkUserStatus(name);
        }
    }
}

void NetworkManager::checkUserStatus(const QString &username) {
    m_friendService->checkUserStatus(username);
}

QString NetworkManager::getFriendStatus(const QString &username) const {
    if (m_friendService) {
        return m_friendService->getPeerStatus(username);
    }
    return QStringLiteral("offline");
}

QVariantMap NetworkManager::allFriendStatuses() const {
    if (m_friendService) {
        return m_friendService->allPeerStatuses();
    }
    return QVariantMap();
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
    QVariantList list = m_openConversations;
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap map = list.at(i).toMap();
        QString name = map.value("name").toString();
        map["displayName"] = getDisplayName(name);
        list[i] = map;
    }
    return list;
}

void NetworkManager::openDirectConversation(const QString &username, qint64 activityTimestamp) {
    QString lower = username.trimmed().toLower();
    QString myUser = currentUsername().trimmed().toLower();
    if (myUser.isEmpty() && m_storage) {
        myUser = m_storage->username().trimmed().toLower();
    }
    if (lower.isEmpty() || lower == "saved-messages" || lower == "friends" || (!myUser.isEmpty() && lower == myUser)) return;

    checkUserStatus(lower);

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

QString NetworkManager::effectiveStatus() const {
    if (!isConnected() || token().isEmpty()) {
        return "offline";
    }
    if (m_userStatusPreference == "offline") {
        return "offline";
    }
    if (m_userStatusPreference == "dnd") {
        return "dnd";
    }
    if (m_userStatusPreference == "afk" || m_userStatusPreference == "idle") {
        return "afk";
    }
    if (m_userStatusPreference == "online") {
        return m_isAutoIdle ? "afk" : "online";
    }
    return m_userStatusPreference;
}

void NetworkManager::setUserStatus(const QString &status) {
    QString st = status.trimmed().toLower();
    if (st == "idle") st = "afk";
    if (st != "online" && st != "afk" && st != "dnd" && st != "offline") {
        st = "online";
    }

    if (m_userStatusPreference != st) {
        m_userStatusPreference = st;
        m_isAutoIdle = false;

        emit userStatusChanged();
        emit effectiveStatusChanged();
        emit isIdleChanged();
        emit isInvisibleChanged(isInvisible());
        emit isDndChanged(isDnd());

        if (st == "online" && m_idleTimer) {
            m_idleTimer->start(m_idleTimeoutMs);
        } else if (m_idleTimer) {
            m_idleTimer->stop();
        }
    }
}

void NetworkManager::reportActivity() {
    if (m_userStatusPreference == "online") {
        if (m_isAutoIdle) {
            m_isAutoIdle = false;
            emit isIdleChanged();
            emit effectiveStatusChanged();
        }
        if (m_idleTimer) {
            m_idleTimer->start(m_idleTimeoutMs);
        }
    }
}

void NetworkManager::setIdleTimeout(int timeoutMs) {
    m_idleTimeoutMs = timeoutMs > 0 ? timeoutMs : 120000;
    if (m_idleTimer) {
        m_idleTimer->setInterval(m_idleTimeoutMs);
        if (m_userStatusPreference == "online" && !m_isAutoIdle) {
            m_idleTimer->start(m_idleTimeoutMs);
        }
    }
}

void NetworkManager::onIdleTimeout() {
    if (m_userStatusPreference == "online" && !m_isAutoIdle) {
        m_isAutoIdle = true;
        emit isIdleChanged();
        emit effectiveStatusChanged();
    }
}

