// src/services/authservice.cpp
#include "authservice.h"
#include "../transport/httptransport.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace NeoNect {
namespace Services {

AuthService::AuthService(std::shared_ptr<Transport::IHttpTransport> transport,
                         std::shared_ptr<Storage::ISettingsRepository> storage,
                         QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage)) {
}

void AuthService::verifyServer(const QString &address) {
    QString cleanUrl = Transport::HttpTransport::cleanUrl(address);
    bool isAllowed = cleanUrl.startsWith("https://");
    if (!isAllowed) {
        static const QStringList localDevHosts = {
            "localhost", "127.0.0.1", "0.0.0.0", "host.docker.internal",
            "192.168.", "10.", "172.", ".local"
        };
        for (const QString &host : localDevHosts) {
            if (cleanUrl.contains(host)) {
                isAllowed = true;
                break;
            }
        }
    }
    if (!isAllowed) {
        emit verificationResult(false, "Insecure connection. HTTPS is strictly required.");
        return;
    }
    m_transport->setBaseUrl(cleanUrl);
    m_storage->setServerUrl(cleanUrl);

    m_transport->get(Constants::EP_HEALTH, {}, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (error == QNetworkReply::NoError || statusCode == 200) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && (doc.object().value("status").toString() == "success" ||
                                 doc.object().value("status").toString() == "ok" ||
                                 doc.object().contains("status"))) {
                emit verificationResult(true, "Connected to NeoNect Server");
                return;
            }
            if (statusCode == 200) {
                emit verificationResult(true, "Connected to NeoNect Server");
                return;
            }
        }
        emit verificationResult(false, "Handshake failed: Unable to connect to host.");
    });
}

void AuthService::checkUsernameAvailability(const QString &username) {
    QString trimmedUser = username.trimmed();
    if (trimmedUser.length() < Constants::MIN_USERNAME_LENGTH) {
        emit availabilityResult(username, false, "Username must be at least 3 characters");
        return;
    }

    QMap<QString, QString> params;
    params["u"] = trimmedUser;

    m_transport->get(Constants::EP_USERS_AVAILABILITY, params, this, [this, trimmedUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
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
    QString trimmedUser = username.trimmed();
    if (trimmedUser.length() < Constants::MIN_USERNAME_LENGTH) {
        emit registrationResult(false, "Username must be at least 3 characters long.");
        return;
    }

    if (password.length() < Constants::MIN_PASSWORD_LENGTH) {
        emit registrationResult(false, "Password must be at least 8 characters long.");
        return;
    }

    bool hasLetter = false;
    bool hasNumber = false;
    for (const QChar &ch : password) {
        if (ch.isLetter()) hasLetter = true;
        if (ch.isDigit()) hasNumber = true;
    }

    if (!hasLetter || !hasNumber) {
        emit registrationResult(false, "Password must contain both letters and numbers.");
        return;
    }

    QJsonObject body;
    body["username"] = trimmedUser;
    body["password"] = password;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_USERS, postData, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
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

    m_transport->post(Constants::EP_AUTH, postData, this, [this, cleanUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
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
    m_transport->deleteResource(Constants::EP_AUTH, this, [](int, const QByteArray&, QNetworkReply::NetworkError, const QString&) {});
    m_storage->clearSession();
    m_transport->setAuthToken(QString());
}

void AuthService::fetchUserProfile() {
    if (m_storage->authToken().isEmpty()) {
        emit userProfileFetched(false, QString());
        return;
    }

    m_transport->get(Constants::EP_USERS_ME, {}, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
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
} // namespace NeoNect
