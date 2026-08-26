// src/services/authservice.cpp
#include "authservice.h"
#include "../transport/httptransport.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace Avila {
namespace Services {

AuthService::AuthService(std::shared_ptr<Transport::IHttpTransport> transport,
                         std::shared_ptr<Storage::ISettingsRepository> storage,
                         QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage)) {
}

void AuthService::verifyServer(const QString &address) {
    QString cleanUrl = Transport::HttpTransport::cleanUrl(address);
    m_transport->setBaseUrl(cleanUrl);
    m_storage->setServerUrl(cleanUrl);

    m_transport->get(Constants::EP_HEALTH, {}, [this, cleanUrl](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (error == QNetworkReply::NoError || statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && (doc.object().value("status").toString() == "success" ||
                                 doc.object().value("status").toString() == "ok" ||
                                 doc.object().contains("available"))) {
                emit verificationResult(true, "Connected to Danisa Server");
                return;
            }
            if (statusCode == 200) {
                emit verificationResult(true, "Connected to Danisa Server");
                return;
            }
        }

        // Fallback endpoint check
        m_transport->get(Constants::EP_HEALTH_FALLBACK, {}, [this](int fallbackStatus, const QByteArray &fallbackData, QNetworkReply::NetworkError fallbackErr, const QString &fallbackErrStr) {
            Q_UNUSED(fallbackErrStr);
            if (fallbackErr == QNetworkReply::NoError || fallbackStatus == 200) {
                auto doc = QJsonDocument::fromJson(fallbackData);
                if (!doc.isNull() && (doc.object().value("status").toString() == "success" || doc.object().value("status").toString() == "ok")) {
                    emit verificationResult(true, "Connected to Danisa Server");
                    return;
                }
            }
            emit verificationResult(false, "Handshake failed: Unable to connect to host.");
        });
    });
}

void AuthService::checkUsernameAvailability(const QString &username) {
    QString trimmedUser = username.trimmed();
    if (trimmedUser.isEmpty()) {
        emit availabilityResult(username, false, "Username cannot be empty");
        return;
    }

    QMap<QString, QString> params;
    params["u"] = trimmedUser;

    m_transport->get(Constants::EP_USERS_AVAILABILITY, params, [this, trimmedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        if (error != QNetworkReply::NoError) {
            emit availabilityResult(trimmedUser, false, "Server check failed");
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.object().contains("available")) {
            bool isAvailable = doc.object().value("available").toBool();
            emit availabilityResult(trimmedUser, isAvailable, isAvailable ? "Username available" : "Username is taken");
        } else {
            emit availabilityResult(trimmedUser, true, "Username available");
        }
    });
}

void AuthService::registerUser(const QString &username, const QString &password) {
    QJsonObject body;
    body["username"] = username.trimmed();
    body["password"] = password;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_USERS, postData, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        auto doc = QJsonDocument::fromJson(data);
        if (error != QNetworkReply::NoError && statusCode != 201 && statusCode != 200) {
            QString errMsg = "Registration failed.";
            if (!doc.isNull()) {
                if (doc.object().contains("error")) {
                    errMsg = doc.object().value("error").toString();
                } else if (doc.object().contains("message")) {
                    errMsg = doc.object().value("message").toString();
                }
            }
            emit registrationResult(false, errMsg);
            return;
        }

        emit registrationResult(true, "Registration successful!");
    });
}

void AuthService::loginUser(const QString &username, const QString &password) {
    QJsonObject body;
    body["username"] = username.trimmed();
    body["password"] = password;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QString cleanUser = username.trimmed().toLower();

    m_transport->post(Constants::EP_AUTH, postData, [this, cleanUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        auto doc = QJsonDocument::fromJson(data);

        if (error != QNetworkReply::NoError || doc.isNull() || !doc.object().contains("token")) {
            QString errMsg = "Invalid username or password.";
            if (!doc.isNull()) {
                if (doc.object().contains("error")) {
                    errMsg = doc.object().value("error").toString();
                } else if (doc.object().contains("message")) {
                    errMsg = doc.object().value("message").toString();
                }
            }
            emit loginResult(false, errMsg);
            return;
        }

        QString token = doc.object().value("token").toString();
        m_storage->setAuthToken(token);
        m_storage->setUsername(cleanUser);
        m_transport->setAuthToken(token);

        emit loginResult(true, token);
    });
}

void AuthService::logoutUser() {
    m_transport->deleteResource(Constants::EP_AUTH, [](int, const QByteArray&, QNetworkReply::NetworkError, const QString&) {});
    m_storage->clearSession();
    m_transport->setAuthToken(QString());
}

void AuthService::fetchUserProfile() {
    if (m_storage->authToken().isEmpty()) {
        emit userProfileFetched(false, QString());
        return;
    }

    m_transport->get(Constants::EP_USERS_ME, {}, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        if (error == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("username")) {
                QString user = doc.object().value("username").toString();
                m_storage->setUsername(user);
                emit userProfileFetched(true, user);
                return;
            }
        }
        emit userProfileFetched(false, QString());
    });
}

} // namespace Services
} // namespace Avila
