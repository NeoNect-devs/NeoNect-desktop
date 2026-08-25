// src/core/networkmanager.cpp
#include "networkmanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkRequest>

NetworkManager* NetworkManager::instance() {
    static NetworkManager _instance;
    return &_instance;
}

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

QString NetworkManager::cleanUrl(const QString &input) {
    QString trimmed = input.trimmed();
    return trimmed.contains("://") ? trimmed : "http://" + trimmed;
}

void NetworkManager::verifyServer(const QString &address) {
    m_serverUrl = cleanUrl(address);
    emit serverUrlChanged();

    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/status"));
    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit verificationResult(false, "Handshake failed. Host unreachable.");
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull() && doc.object().value("server_type").toString() == "avila_node") {
            emit verificationResult(true, "Connected to authentic Avila Server");
        } else {
            emit verificationResult(false, "Security mismatch: Node footprint untrusted.");
        }
    });
}

void NetworkManager::loginUser(const QString &username, const QString &password) {
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/auth/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginResult(false, "Invalid user credentials footprint mapping.");
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        m_token = doc.object().value("token").toString();
        emit tokenChanged();
        emit loginResult(true, m_token);
    });
}

void NetworkManager::sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce) {
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/channels/" + channelId + "/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    QJsonObject payload;
    payload["encrypted_body"] = cipher;
    payload["nonce"] = nonce;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, channelId, reply]() {
        emit secureMessageTransmitted(channelId, reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
    });
}
