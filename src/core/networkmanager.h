// src/core/networkmanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class NetworkManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY currentUsernameChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(QStringList friends READ friends NOTIFY friendsChanged)

public:
    static NetworkManager* instance();
    explicit NetworkManager(QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString token() const { return m_token; }
    QString currentUsername() const { return m_currentUsername; }
    bool isLoading() const { return m_isLoading; }
    QStringList friends() const { return m_friends; }

    Q_INVOKABLE void verifyServer(const QString &address);
    Q_INVOKABLE void checkUsernameAvailability(const QString &username);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);
    Q_INVOKABLE void logoutUser();
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &publicKey);
    Q_INVOKABLE void fetchDevicePublicKey(const QString &deviceId);
    Q_INVOKABLE void fetchUserProfile();
    Q_INVOKABLE void sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce);

    // Danisa E2EE Relay & Direct Chat
    Q_INVOKABLE void sendRelayMessage(const QString &toUsername, const QString &plainText);
    Q_INVOKABLE void pollPendingMessages();
    Q_INVOKABLE void acknowledgeMessage(qint64 messageId);

    // Friends & DM Management
    Q_INVOKABLE void addFriend(const QString &username);

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

private:
    void setIsLoading(bool loading);
    QString cleanUrl(const QString &input);
    void loadFriends();
    void saveFriends();
    void startPolling();
    void stopPolling();

    QNetworkAccessManager *m_nam;
    QTimer *m_pollTimer;
    QString m_serverUrl;
    QString m_token;
    QString m_currentUsername;
    QStringList m_friends;
    bool m_isLoading{false};
};


