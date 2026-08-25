// src/core/networkmanager.h
#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class NetworkManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token NOTIFY tokenChanged)
    Q_PROPERTY(QString currentUsername READ currentUsername NOTIFY currentUsernameChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

public:
    static NetworkManager* instance();
    explicit NetworkManager(QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString token() const { return m_token; }
    QString currentUsername() const { return m_currentUsername; }
    bool isLoading() const { return m_isLoading; }

    Q_INVOKABLE void verifyServer(const QString &address);
    Q_INVOKABLE void checkUsernameAvailability(const QString &username);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password);
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);
    Q_INVOKABLE void registerDevice(const QString &deviceId, const QString &publicKey);
    Q_INVOKABLE void fetchUserProfile();
    Q_INVOKABLE void sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce);

signals:
    void serverUrlChanged();
    void tokenChanged();
    void currentUsernameChanged();
    void isLoadingChanged();
    void verificationResult(bool success, const QString &message);
    void availabilityResult(const QString &username, bool available, const QString &error);
    void registrationResult(bool success, const QString &message);
    void loginResult(bool success, const QString &tokenOrError);
    void deviceRegistrationResult(bool success, const QString &message);
    void userProfileFetched(bool success, const QString &username);
    void secureMessageTransmitted(const QString &channelId, bool success);

private:
    void setIsLoading(bool loading);
    QString cleanUrl(const QString &input);

    QNetworkAccessManager *m_nam;
    QString m_serverUrl;
    QString m_token;
    QString m_currentUsername;
    bool m_isLoading{false};
};

