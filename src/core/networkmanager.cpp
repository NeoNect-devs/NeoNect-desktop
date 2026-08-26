// src/core/networkmanager.cpp
#include "networkmanager.h"
#include "cryptomanager.h"
#include "../transport/httptransport.h"
#include "../storage/settingsrepository.h"
#include "../crypto/cryptoservice.h"
#include <QDebug>

NetworkManager* NetworkManager::instance() {
    static NetworkManager _instance;
    return &_instance;
}

NetworkManager::NetworkManager(std::shared_ptr<Avila::Transport::IHttpTransport> transport,
                               std::shared_ptr<Avila::Storage::ISettingsRepository> storage,
                               std::shared_ptr<Avila::Crypto::ICryptoService> cryptoService,
                               QObject *parent)
    : QObject(parent),
      m_storage(storage ? storage : std::make_shared<Avila::Storage::SettingsRepository>()),
      m_cryptoService(cryptoService ? cryptoService : CryptoManager::instance()->service()),
      m_transport(transport ? transport : std::make_shared<Avila::Transport::HttpTransport>()) {

    // Configure transport with initial storage settings
    m_transport->setBaseUrl(m_storage->serverUrl());
    m_transport->setAuthToken(m_storage->authToken());

    // Instantiate domain services with dependency injection
    m_authService = std::make_shared<Avila::Services::AuthService>(m_transport, m_storage, nullptr);
    m_deviceService = std::make_shared<Avila::Services::DeviceService>(m_transport, m_storage, nullptr);
    m_relayService = std::make_shared<Avila::Services::RelayService>(m_transport, m_storage, m_cryptoService, nullptr);
    m_friendService = std::make_shared<Avila::Services::FriendService>(m_transport, m_storage, nullptr);

    setupServiceSignals();

    m_friendService->loadFriends();

    // Auto-login / reconnect if session token already exists in storage
    if (!m_storage->authToken().isEmpty()) {
        emit tokenChanged();
        emit currentUsernameChanged();
        m_relayService->startPolling();
        autoRegisterDevice();
    }
}

void NetworkManager::initializeCustom(std::shared_ptr<Avila::Transport::IHttpTransport> transport) {
    if (!transport) return;
    m_transport = transport;
    m_transport->setBaseUrl(m_storage->serverUrl());
    m_transport->setAuthToken(m_storage->authToken());

    m_authService = std::make_shared<Avila::Services::AuthService>(m_transport, m_storage, nullptr);
    m_deviceService = std::make_shared<Avila::Services::DeviceService>(m_transport, m_storage, nullptr);
    m_relayService = std::make_shared<Avila::Services::RelayService>(m_transport, m_storage, m_cryptoService, nullptr);
    m_friendService = std::make_shared<Avila::Services::FriendService>(m_transport, m_storage, nullptr);

    setupServiceSignals();
    m_friendService->loadFriends();

    if (!m_storage->authToken().isEmpty()) {
        m_relayService->startPolling();
        autoRegisterDevice();
    }
}

void NetworkManager::setupServiceSignals() {
    // Auth Service Connections
    connect(m_authService.get(), &Avila::Services::AuthService::verificationResult, this, [this](bool success, const QString &message) {
        setIsLoading(false);
        emit serverUrlChanged();
        emit verificationResult(success, message);
    });

    connect(m_authService.get(), &Avila::Services::AuthService::availabilityResult, this, [this](const QString &username, bool available, const QString &error) {
        emit availabilityResult(username, available, error);
    });

    connect(m_authService.get(), &Avila::Services::AuthService::registrationResult, this, [this](bool success, const QString &message) {
        setIsLoading(false);
        emit registrationResult(success, message);
    });

    connect(m_authService.get(), &Avila::Services::AuthService::loginResult, this, [this](bool success, const QString &tokenOrError) {
        setIsLoading(false);
        if (success) {
            emit tokenChanged();
            emit currentUsernameChanged();
            m_relayService->startPolling();
            autoRegisterDevice();
        }
        emit loginResult(success, tokenOrError);
    });

    connect(m_authService.get(), &Avila::Services::AuthService::userProfileFetched, this, [this](bool success, const QString &username) {
        if (success) {
            emit currentUsernameChanged();
        }
        emit userProfileFetched(success, username);
    });

    // Device Service Connections
    connect(m_deviceService.get(), &Avila::Services::DeviceService::deviceRegistrationResult, this, [this](bool success, const QString &message) {
        m_relayService->startPolling();
        emit deviceRegistrationResult(success, message);
    });

    connect(m_deviceService.get(), &Avila::Services::DeviceService::deviceKeyFetched, this, &NetworkManager::deviceKeyFetched);

    // Relay Service Connections
    connect(m_relayService.get(), &Avila::Services::RelayService::incomingRelayMessageReceived, this, [this](const QString &fromUsername, const QString &text, qint64 timestamp) {
        if (fromUsername.toLower() != currentUsername().toLower() && fromUsername != "Anonymous") {
            m_friendService->updateLastSeen(fromUsername);
        }
        emit incomingRelayMessageReceived(fromUsername, text, timestamp);
    });

    connect(m_relayService.get(), &Avila::Services::RelayService::secureMessageTransmitted, this, [this](const QString &targetUser, bool success) {
        if (success) {
            m_friendService->updateLastSeen(targetUser);
        }
        emit secureMessageTransmitted(targetUser, success);
    });

    connect(m_relayService.get(), &Avila::Services::RelayService::sessionUnauthorized, this, [this](const QString &message) {
        emit tokenChanged();
        emit currentUsernameChanged();
        emit loginResult(false, message);
    });

    connect(m_relayService.get(), &Avila::Services::RelayService::deviceRegistrationRequested, this, &NetworkManager::autoRegisterDevice);

    // Friend Service Connections
    connect(m_friendService.get(), &Avila::Services::FriendService::friendsListChanged, this, [this](const QStringList&) {
        emit friendsChanged();
    });

    connect(m_friendService.get(), &Avila::Services::FriendService::addFriendResult, this, &NetworkManager::addFriendResult);
    connect(m_friendService.get(), &Avila::Services::FriendService::friendStatusUpdated, this, &NetworkManager::friendStatusUpdated);
}

void NetworkManager::autoRegisterDevice() {
    registerDevice(CryptoManager::instance()->getDeviceId(), CryptoManager::instance()->getDevicePublicKey());
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
    return m_storage->authToken();
}

QString NetworkManager::currentUsername() const {
    return m_storage->username();
}

QStringList NetworkManager::friends() const {
    return m_friendService->friends();
}

void NetworkManager::setProfile(const QString &profileName) {
    m_storage->setProfile(profileName);
    m_transport->setBaseUrl(m_storage->serverUrl());
    m_transport->setAuthToken(m_storage->authToken());

    emit serverUrlChanged();
    emit tokenChanged();
    emit currentUsernameChanged();

    m_friendService->loadFriends();

    if (!m_storage->authToken().isEmpty()) {
        m_relayService->startPolling();
        autoRegisterDevice();
    }
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
    m_relayService->stopPolling();
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

void NetworkManager::fetchUserProfile() {
    m_authService->fetchUserProfile();
}

void NetworkManager::sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce) {
    Q_UNUSED(nonce);
    sendRelayMessage(channelId, cipher);
}

void NetworkManager::sendRelayMessage(const QString &toUsername, const QString &plainText) {
    m_relayService->sendRelayMessage(toUsername, plainText);
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

void NetworkManager::checkFriendsStatus() {
    m_friendService->checkFriendsStatus();
}
