// src/core/networkmanager.cpp
#include "networkmanager.h"
#include "cryptomanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>

NetworkManager* NetworkManager::instance() {
    static NetworkManager _instance;
    return &_instance;
}

NetworkManager::NetworkManager(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
}

void NetworkManager::setIsLoading(bool loading) {
    if (m_isLoading != loading) {
        m_isLoading = loading;
        emit isLoadingChanged();
    }
}

QString NetworkManager::cleanUrl(const QString &input) {
    QString trimmed = input.trimmed();
    if (trimmed.endsWith("/")) {
        trimmed.chop(1);
    }
    return trimmed.contains("://") ? trimmed : "http://" + trimmed;
}

void NetworkManager::verifyServer(const QString &address) {
    m_serverUrl = cleanUrl(address);
    emit serverUrlChanged();
    setIsLoading(true);

    // Primary Danisa Health Endpoint: /api/v1/health
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/health"));
    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setIsLoading(false);

        if (reply->error() != QNetworkReply::NoError) {
            // Fallback check to legacy /health
            QNetworkRequest fallbackReq(QUrl(m_serverUrl + "/health"));
            QNetworkReply *fallbackReply = m_nam->get(fallbackReq);

            connect(fallbackReply, &QNetworkReply::finished, this, [this, fallbackReply]() {
                fallbackReply->deleteLater();
                if (fallbackReply->error() != QNetworkReply::NoError) {
                    emit verificationResult(false, "Handshake failed: Unable to connect to host.");
                } else {
                    auto doc = QJsonDocument::fromJson(fallbackReply->readAll());
                    if (!doc.isNull() && (doc.object().value("status").toString() == "success" || doc.object().value("status").toString() == "ok")) {
                        emit verificationResult(true, "Connected to Danisa Server");
                    } else {
                        emit verificationResult(false, "Invalid server response.");
                    }
                }
            });
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull() && (doc.object().value("status").toString() == "success" || doc.object().value("status").toString() == "ok" || doc.object().contains("available"))) {
            emit verificationResult(true, "Connected to Danisa Server");
        } else {
            // Even if raw 200 OK without JSON, treat as success if valid HTTP status
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                emit verificationResult(true, "Connected to Danisa Server");
            } else {
                emit verificationResult(false, "Security mismatch: Invalid server node response.");
            }
        }
    });
}

void NetworkManager::checkUsernameAvailability(const QString &username) {
    if (username.trimmed().isEmpty()) {
        emit availabilityResult(username, false, "Username cannot be empty");
        return;
    }

    QUrl url(m_serverUrl + "/api/v1/users/availability");
    QUrlQuery query;
    query.addQueryItem("u", username.trimmed());
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, username, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit availabilityResult(username, false, "Server check failed");
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isNull() && doc.object().contains("available")) {
            bool isAvailable = doc.object().value("available").toBool();
            emit availabilityResult(username, isAvailable, isAvailable ? "Username available" : "Username is taken");
        } else {
            emit availabilityResult(username, true, "Username available");
        }
    });
}

void NetworkManager::registerUser(const QString &username, const QString &password) {
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/users"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    setIsLoading(true);

    QJsonObject body;
    body["username"] = username.trimmed();
    body["password"] = password;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setIsLoading(false);

        auto rawData = reply->readAll();
        auto doc = QJsonDocument::fromJson(rawData);

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError && statusCode != 201 && statusCode != 200) {
            QString errStr = "Registration failed.";
            if (!doc.isNull() && doc.object().contains("error")) {
                errStr = doc.object().value("error").toString();
            } else if (!doc.isNull() && doc.object().contains("message")) {
                errStr = doc.object().value("message").toString();
            }
            emit registrationResult(false, errStr);
            return;
        }

        emit registrationResult(true, "Registration successful!");
    });
}

void NetworkManager::loginUser(const QString &username, const QString &password) {
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/auth"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    setIsLoading(true);

    QJsonObject body;
    body["username"] = username.trimmed();
    body["password"] = password;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, username, reply]() {
        reply->deleteLater();
        setIsLoading(false);

        auto rawData = reply->readAll();
        auto doc = QJsonDocument::fromJson(rawData);

        if (reply->error() != QNetworkReply::NoError || doc.isNull() || !doc.object().contains("token")) {
            QString err = "Invalid username or password.";
            if (!doc.isNull() && doc.object().contains("error")) {
                err = doc.object().value("error").toString();
            } else if (!doc.isNull() && doc.object().contains("message")) {
                err = doc.object().value("message").toString();
            }
            emit loginResult(false, err);
            return;
        }

        m_token = doc.object().value("token").toString();
        m_currentUsername = username.trimmed();
        emit tokenChanged();
        emit currentUsernameChanged();
        emit loginResult(true, m_token);

        // Perform automatic device registration following authentication
        registerDevice(CryptoManager::instance()->getDeviceId(), CryptoManager::instance()->getDevicePublicKey());
    });
}

void NetworkManager::registerDevice(const QString &deviceId, const QString &publicKey) {
    if (m_token.isEmpty()) return;

    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/device/register"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    QJsonObject body;
    body["device_id"] = deviceId;
    body["public_key"] = publicKey;

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        bool success = (reply->error() == QNetworkReply::NoError);
        auto doc = QJsonDocument::fromJson(reply->readAll());
        QString msg = success ? "Device registered" : (doc.object().value("error").toString());
        emit deviceRegistrationResult(success, msg);
    });
}

void NetworkManager::fetchUserProfile() {
    if (m_token.isEmpty()) return;

    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/users/me"));
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isNull() && doc.object().contains("username")) {
                m_currentUsername = doc.object().value("username").toString();
                emit currentUsernameChanged();
                emit userProfileFetched(true, m_currentUsername);
                return;
            }
        }
        emit userProfileFetched(false, "");
    });
}

void NetworkManager::sendSecurePayload(const QString &channelId, const QString &cipher, const QString &nonce) {
    QNetworkRequest request(QUrl(m_serverUrl + "/api/v1/relay/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());

    QJsonObject payload;
    payload["from_device_id"] = CryptoManager::instance()->getDeviceId();
    payload["to_username"] = channelId;
    payload["ciphertext"] = cipher;
    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, channelId, reply]() {
        emit secureMessageTransmitted(channelId, reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
    });
}

