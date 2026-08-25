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

public:
    static NetworkManager* instance();
    explicit NetworkManager(QObject *parent = nullptr);

    QString serverUrl() const { return m_serverUrl; }
    QString token() const { return m_token; }

    Q_INVOKABLE void verifyServer(const QString &address);
    Q_INVOKABLE void loginUser(const QString &username, const QString &password);
    Q_INVOKABLE void sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce);

signals:
    void serverUrlChanged();
    void tokenChanged();
    void verificationResult(bool success, const QString &message);
    void loginResult(bool success, const QString &tokenOrError);
    void secureMessageTransmitted(const QString &channelId, bool success);

private:
    QString cleanUrl(const QString &input);
    QNetworkAccessManager *m_nam;
    QString m_serverUrl;
    QString m_token;
};
