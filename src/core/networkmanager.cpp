// src/core/networkmanager.cpp
#include "networkmanager.h"
#include "cryptomanager.h"
#include "../transport/httptransport.h"
#include "../storage/settingsrepository.h"
#include "../crypto/cryptoservice.h"
#include <QDebug>
#include <QDateTime>

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
            m_relayService->startPolling();
            m_friendService->startHeartbeat();
            autoRegisterDevice();
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
        m_relayService->startPolling();
            m_friendService->startHeartbeat();
        emit deviceRegistrationResult(success, message);
    });

    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::deviceKeyFetched, this, &NetworkManager::deviceKeyFetched);
    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::deviceRevocationResult, this, &NetworkManager::deviceRevocationResult);
    connect(m_deviceService.get(), &NeoNect::Services::DeviceService::recipientKeysFetched, this, &NetworkManager::recipientKeysFetched);

    // Relay Service Connections




    connect(m_relayService.get(), &NeoNect::Services::RelayService::sessionUnauthorized, this, [this](const QString &message) {
        emit tokenChanged();
        emit currentUsernameChanged();
        emit loginResult(false, message);
    });

    connect(m_relayService.get(), &NeoNect::Services::RelayService::deviceRegistrationRequested, this, &NetworkManager::autoRegisterDevice);

    // Friend Service Connections
    connect(m_friendService.get(), &NeoNect::Services::FriendService::friendsListChanged, this, [this](const QStringList&) {
        emit friendsChanged();
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
    emit tokenChanged();
    emit currentUsernameChanged();
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
