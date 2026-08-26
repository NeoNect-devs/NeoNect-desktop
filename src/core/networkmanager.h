// src/core/networkmanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"
#include "../crypto/icryptoservice.h"
#include "../services/authservice.h"
#include "../services/deviceservice.h"
#include "../services/relayservice.h"
#include "../services/friendservice.h"

class NetworkManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY currentUsernameChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QStringList friends READ friends NOTIFY friendsChanged)

public:
    static NetworkManager* instance();
    explicit NetworkManager(std::shared_ptr<Avila::Transport::IHttpTransport> transport = nullptr,
                            std::shared_ptr<Avila::Storage::ISettingsRepository> storage = nullptr,
                            std::shared_ptr<Avila::Crypto::ICryptoService> cryptoService = nullptr,
                            QObject *parent = nullptr);
    ~NetworkManager() override = default;

    void initializeCustom(std::shared_ptr<Avila::Transport::IHttpTransport> transport);

    QString serverUrl() const;
    QString token() const;
    QString currentUsername() const;
    bool isLoading() const { return m_isLoading; }
    QStringList friends() const;

    Q_INVOKABLE void setProfile(const QString &profileName);
    Q_INVOKABLE void verifyServer(const QString &address);

    Q_INVOKABLE void checkUsernameAvailability(const QString &username);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);
    Q_INVOKABLE void logoutUser();
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &publicKey);
    Q_INVOKABLE void fetchDevicePublicKey(const QString &deviceId);
    Q_INVOKABLE void fetchUserProfile();
    Q_INVOKABLE void sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce);

    // E2EE Relay & Direct Chat
    Q_INVOKABLE void sendRelayMessage(const QString &toUsername, const QString &plainText);
    Q_INVOKABLE void pollPendingMessages();
    Q_INVOKABLE void acknowledgeMessage(qint64 messageId);

    // Friends & DM Management
    Q_INVOKABLE void addFriend(const QString &username);
    Q_INVOKABLE void checkFriendsStatus();

signals:
    void serverUrlChanged();
    void tokenChanged();
    void currentUsernameChanged();
    void isLoadingChanged();
    void friendsChanged();

    void verificationResult(bool success, const QString &message);
    void availabilityResult(const QString &username, bool available, const QString &error);
    void registrationResult(bool success, const QString &message);
    void loginResult(bool success, const QString &tokenOrError);
    void deviceRegistrationResult(bool success, const QString &message);
    void deviceKeyFetched(const QString &deviceId, const QString &publicKey);
    void userProfileFetched(bool success, const QString &username);
    void secureMessageTransmitted(const QString &channelId, bool success);

    void incomingRelayMessageReceived(const QString &fromUsername, const QString &text, qint64 timestamp);
    void addFriendResult(bool success, const QString &message, const QString &username);
    void friendStatusUpdated(const QString &username, const QString &status);

private:
    void setIsLoading(bool loading);
    void setupServiceSignals();
    void autoRegisterDevice();

    std::shared_ptr<Avila::Transport::IHttpTransport> m_transport;
    std::shared_ptr<Avila::Storage::ISettingsRepository> m_storage;
    std::shared_ptr<Avila::Crypto::ICryptoService> m_cryptoService;

    std::shared_ptr<Avila::Services::AuthService> m_authService;
    std::shared_ptr<Avila::Services::DeviceService> m_deviceService;
    std::shared_ptr<Avila::Services::RelayService> m_relayService;
    std::shared_ptr<Avila::Services::FriendService> m_friendService;

    bool m_isLoading{false};
};
