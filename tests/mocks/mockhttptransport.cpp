// tests/mocks/mockhttptransport.cpp
#include "mockhttptransport.h"
#include "../../src/common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QFile>
#include <QDir>
#include <QSaveFile>

namespace NeoNect {
namespace Testing {

static QString sharedStateFilePath() {
    return QDir::tempPath() + "/neonect_mock_shared_state.json";
}

MockHttpTransport::MockHttpTransport(bool enableSharedStorage, bool enableEchoBot, QObject *parent)
    : QObject(parent), m_enableSharedStorage(enableSharedStorage), m_enableEchoBot(enableEchoBot) {
    if (m_enableSharedStorage) {
        loadSharedState();
    }

    seedUser("alex", "password123");
    seedUser("beatrice", "password123");
    seedUser("charlie", "password123");
    seedUser("alice", "password123");
    seedUser("bob", "password123");

    if (m_enableSharedStorage) {
        saveSharedState();
    }
}

void MockHttpTransport::loadSharedState() {
    if (!m_enableSharedStorage) return;
    QFile file(sharedStateFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    auto doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) return;

    auto root = doc.object();
    m_nextUserId = root.value("next_user_id").toInteger(100);
    m_nextMessageId = root.value("next_message_id").toInteger(1000);

    // Load users
    auto usersObj = root.value("users").toObject();
    for (auto it = usersObj.begin(); it != usersObj.end(); ++it) {
        auto uObj = it.value().toObject();
        MockUser u;
        u.id = uObj.value("id").toInteger();
        u.username = it.key();
        u.password = uObj.value("password").toString();
        u.sessionToken = uObj.value("session_token").toString();

        auto devObj = uObj.value("devices").toObject();
        for (auto dit = devObj.begin(); dit != devObj.end(); ++dit) {
            u.devices[dit.key()] = dit.value().toString();
        }
        m_users[u.username] = u;
        if (!u.sessionToken.isEmpty()) {
            m_tokenToUser[u.sessionToken] = u.username;
        }
    }

    // Load delivery queue
    m_deliveryQueue.clear();
    auto queueArr = root.value("delivery_queue").toArray();
    for (const auto &val : queueArr) {
        auto qObj = val.toObject();
        MockQueuedMessage item;
        item.id = qObj.value("id").toInteger();
        item.deviceId = qObj.value("device_id").toString();
        item.toUsername = qObj.value("to_username").toString();
        item.ciphertextBase64 = qObj.value("ciphertext").toString();
        // nonce is inside envelope now
        item.timestamp = qObj.value("timestamp").toInteger();
        m_deliveryQueue.push_back(item);
    }
}

void MockHttpTransport::saveSharedState() {
    if (!m_enableSharedStorage) return;

    QJsonObject root;
    root["next_user_id"] = static_cast<qint64>(m_nextUserId);
    root["next_message_id"] = m_nextMessageId;

    QJsonObject usersObj;
    for (auto it = m_users.begin(); it != m_users.end(); ++it) {
        QJsonObject uObj;
        uObj["id"] = static_cast<qint64>(it.value().id);
        uObj["password"] = it.value().password;
        uObj["session_token"] = it.value().sessionToken;

        QJsonObject devObj;
        for (auto dit = it.value().devices.begin(); dit != it.value().devices.end(); ++dit) {
            devObj[dit.key()] = dit.value();
        }
        uObj["devices"] = devObj;
        usersObj[it.key()] = uObj;
    }
    root["users"] = usersObj;

    QJsonArray queueArr;
    for (const auto &item : m_deliveryQueue) {
        QJsonObject qObj;
        qObj["id"] = item.id;
        qObj["device_id"] = item.deviceId;
        qObj["to_username"] = item.toUsername;
        qObj["ciphertext"] = item.ciphertextBase64;
        // nonce is inside envelope now
        qObj["timestamp"] = item.timestamp;
        queueArr.append(qObj);
    }
    root["delivery_queue"] = queueArr;

    QSaveFile saveFile(sharedStateFilePath());
    if (saveFile.open(QIODevice::WriteOnly)) {
        saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        saveFile.commit();
    }
}

void MockHttpTransport::setBaseUrl(const QString &url) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_baseUrl = url;
}

QString MockHttpTransport::baseUrl() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_baseUrl;
}

void MockHttpTransport::setAuthToken(const QString &token) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_enableSharedStorage) {
        loadSharedState();
    }
    m_activeToken = token;
    if (!token.isEmpty()) {
        for (const auto &user : m_users) {
            if (token.contains(user.username, Qt::CaseInsensitive)) {
                m_tokenToUser[token] = user.username;
                return;
            }
        }
        if (!m_users.isEmpty()) {
            m_tokenToUser[token] = m_users.first().username;
        }
    }
}

QString MockHttpTransport::authToken() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_activeToken;
}

void MockHttpTransport::seedUser(const QString &username, const QString &password) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString cleanUser = username.trimmed().toLower();
    if (!m_users.contains(cleanUser)) {
        MockUser user;
        user.id = m_nextUserId++;
        user.username = cleanUser;
        user.password = password;
        user.sessionToken = "mock-token-" + cleanUser + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        user.devices["mock-dev-" + cleanUser] = "MOCK_BASE64_PUBLIC_KEY_" + cleanUser.toUpper();
        m_users[cleanUser] = user;
        m_tokenToUser[user.sessionToken] = cleanUser;
    }
}

void MockHttpTransport::setSimulateNetworkError(bool simulate) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_simulateNetworkError = simulate;
}

void MockHttpTransport::setSimulateHttpError(int statusCode) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_simulateHttpError = statusCode;
}

int MockHttpTransport::queuedMessageCount(const QString &deviceId) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    int count = 0;
    for (const auto &item : m_deliveryQueue) {
        if (item.deviceId == deviceId) {
            count++;
        }
    }
    return count;
}

void MockHttpTransport::clearAllQueues() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_deliveryQueue.clear();
    if (m_enableSharedStorage) {
        saveSharedState();
    }
    return;
}

QNetworkReply* MockHttpTransport::get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject* context, Transport::HttpResponseCallback callback) {
    emit requestHandled("GET", endpoint);

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_enableSharedStorage) {
            loadSharedState();
        }
        if (m_simulateNetworkError) {
            callback(0, QByteArray(), QNetworkReply::ConnectionRefusedError, "Simulated network failure");
            return nullptr;
        }
        if (m_simulateHttpError > 0) {
            callback(m_simulateHttpError, QByteArray("{\"error\":\"Simulated HTTP Error\"}"), QNetworkReply::InternalServerError, "Simulated HTTP error");
            return nullptr;
        }
        if (m_simulatedResponses.contains(endpoint)) {
            auto pair = m_simulatedResponses.value(endpoint);
            callback(pair.second, pair.first, pair.second >= 400 ? QNetworkReply::UnknownServerError : QNetworkReply::NoError, "");
            return nullptr;
        }
    }

    if (endpoint == Constants::EP_HEALTH) {
        handleHealth(callback);
    } else if (endpoint == Constants::EP_PRESENCE) {
        handlePresence(queryParams, callback);
    } else if (endpoint == Constants::EP_SECURITY_VERIFY) {
        handleSecurityVerify(callback);
    } else if (endpoint == Constants::EP_USERS_AVAILABILITY) {
        handleAvailability(queryParams, callback);
    } else if (endpoint == Constants::EP_USERS_ME) {
        handleUsersMe(callback);
    } else if (endpoint == Constants::EP_DEVICE_KEY) {
        handleDeviceKey(queryParams, callback);
    } else if (endpoint == Constants::EP_RELAY_KEYS) {
        handleRelayKeys(queryParams, callback);
    } else if (endpoint == Constants::EP_RELAY_POLL) {
        handleRelayPoll(queryParams, callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

QNetworkReply* MockHttpTransport::post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, Transport::HttpResponseCallback callback) {
    emit requestHandled("POST", endpoint);
    emit rawRequestData(jsonData);

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_enableSharedStorage) {
            loadSharedState();
        }
        if (m_simulateNetworkError) {
            callback(0, QByteArray(), QNetworkReply::ConnectionRefusedError, "Simulated network failure");
            return nullptr;
        }
        if (m_simulateHttpError > 0) {
            callback(m_simulateHttpError, QByteArray("{\"error\":\"Simulated HTTP Error\"}"), QNetworkReply::InternalServerError, "Simulated HTTP error");
            return nullptr;
        }
        if (m_simulatedResponses.contains(endpoint)) {
            auto pair = m_simulatedResponses.value(endpoint);
            callback(pair.second, pair.first, pair.second >= 400 ? QNetworkReply::UnknownServerError : QNetworkReply::NoError, "");
            return nullptr;
        }
    }

    if (endpoint == Constants::EP_AUTH) {
        handleAuth(jsonData, callback);
    } else if (endpoint == Constants::EP_USERS) {
        handleUsers(jsonData, callback);
    } else if (endpoint == Constants::EP_DEVICE_REGISTER) {
        handleDeviceRegister(jsonData, callback);
    } else if (endpoint == Constants::EP_RELAY_SEND) {
        handleRelaySend(jsonData, callback);
    } else if (endpoint == Constants::EP_RELAY_ACK) {
        handleRelayAck(jsonData, callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

QNetworkReply* MockHttpTransport::deleteResource(const QString &endpoint, const QObject* context, Transport::HttpResponseCallback callback, const QByteArray &jsonData) {
    emit requestHandled("DELETE", endpoint);
    if (endpoint == Constants::EP_AUTH) {
        handleAuthDelete(callback);
    } else if (endpoint == Constants::EP_DEVICE) {
        handleDeviceDelete(jsonData, callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

void MockHttpTransport::handleHealth(Transport::HttpResponseCallback callback) {
    QJsonObject res;
    res["status"] = "success";
    res["node"] = "mock-danisa-embedded";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handlePresence(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString username = queryParams.value("u").trimmed().toLower();
    bool online = false;
    if (m_users.contains(username)) {
        online = !m_users[username].devices.isEmpty() || !m_users[username].sessionToken.isEmpty();
    }
    QJsonObject res;
    res["online"] = online;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleSecurityVerify(Transport::HttpResponseCallback callback) {
    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleAvailability(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString username = queryParams.value("u").trimmed().toLower();
    bool exists = m_users.contains(username);

    QJsonObject res;
    res["available"] = !exists;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleUsers(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto doc = QJsonDocument::fromJson(data);
    QString username = doc.object().value("username").toString().trimmed().toLower();
    QString password = doc.object().value("password").toString();

    if (username.length() < Constants::MIN_USERNAME_LENGTH) {
        QJsonObject err;
        err["error"] = "username too short";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    if (password.length() < Constants::MIN_PASSWORD_LENGTH) {
        QJsonObject err;
        err["error"] = "password must be at least 8 characters long";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    bool hasLetter = false, hasNumber = false;
    for (const QChar &ch : password) {
        if (ch.isLetter()) hasLetter = true;
        if (ch.isDigit()) hasNumber = true;
    }
    if (!hasLetter || !hasNumber) {
        QJsonObject err;
        err["error"] = "password must contain both letters and numbers";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    if (m_users.contains(username)) {
        QJsonObject err;
        err["error"] = "username is already taken";
        callback(409, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Conflict");
        return;
    }

    MockUser user;
    user.id = m_nextUserId++;
    user.username = username;
    user.password = password;
    user.sessionToken = "mock-token-" + username + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m_users[username] = user;
    m_tokenToUser[user.sessionToken] = username;

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    res["user_id"] = static_cast<qint64>(user.id);
    callback(201, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleAuth(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto doc = QJsonDocument::fromJson(data);
    QString username = doc.object().value("username").toString().trimmed().toLower();
    QString password = doc.object().value("password").toString();

    if (!m_users.contains(username) || m_users[username].password != password) {
        QJsonObject err;
        err["error"] = "Invalid username or password.";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    auto &user = m_users[username];
    if (user.sessionToken.isEmpty()) {
        user.sessionToken = "mock-token-" + username + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    }
    m_tokenToUser[user.sessionToken] = username;
    m_activeToken = user.sessionToken;

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    res["token"] = user.sessionToken;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleAuthDelete(Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_activeToken.isEmpty()) {
        m_tokenToUser.remove(m_activeToken);
        m_activeToken.clear();
        if (m_enableSharedStorage) {
            saveSharedState();
        }
    }
    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleUsersMe(Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString username = m_tokenToUser[m_activeToken];
    QJsonObject res;
    res["username"] = username;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleDeviceRegister(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString username = m_tokenToUser[m_activeToken];
    auto doc = QJsonDocument::fromJson(data);
    QString deviceId = doc.object().value("device_id").toString();
    QString publicKey = doc.object().value("public_key").toString();

    // Replace placeholder seed device with real client device
    m_users[username].devices.remove("mock-dev-" + username);
    m_users[username].devices[deviceId] = publicKey;

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(201, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleDeviceKey(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString deviceId = queryParams.value("device_id");

    for (const auto &user : m_users) {
        if (user.devices.contains(deviceId)) {
            QJsonObject res;
            res["device_id"] = deviceId;
            res["public_key"] = user.devices[deviceId];
            callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
            return;
        }
    }

    QJsonObject err;
    err["error"] = "device not found";
    callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
}

void MockHttpTransport::handleDeviceDelete(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto doc = QJsonDocument::fromJson(data);
    QString deviceId = doc.object().value("device_id").toString().trimmed();
    if (deviceId.isEmpty()) {
        QJsonObject err;
        err["error"] = "device_id required";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    for (auto &u : m_users) {
        u.devices.remove(deviceId);
    }
    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleRelayKeys(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString username = queryParams.value("u").trimmed().toLower();
    if (!m_users.contains(username)) {
        QJsonObject err;
        err["error"] = "user not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }
    QJsonArray devArr;
    const auto &devs = m_users[username].devices;
    for (auto it = devs.begin(); it != devs.end(); ++it) {
        QJsonObject dObj;
        dObj["device_id"] = it.key();
        dObj["public_key"] = it.value();
        devArr.append(dObj);
    }
    QJsonObject res;
    res["devices"] = devArr;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleRelaySend(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    auto doc = QJsonDocument::fromJson(data);
    QString fromDeviceId = doc.object().value("from_device_id").toString();
    QString toUsername = doc.object().value("to_username").toString().trimmed().toLower();
    QString ciphertext = doc.object().value("ciphertext").toString();
    qint64 timestamp = doc.object().value("timestamp").toInteger();

    // Handle channel broadcast (e.g. general)
    if (toUsername == "general") {
        for (const auto &user : m_users) {
            for (auto it = user.devices.cbegin(); it != user.devices.cend(); ++it) {
                if (it.key() != fromDeviceId) {
                    MockQueuedMessage item;
                    item.id = m_nextMessageId++;
                    item.deviceId = it.key();
                    item.toUsername = user.username;
                    item.ciphertextBase64 = ciphertext;
                    item.timestamp = timestamp;
                    m_deliveryQueue.push_back(std::move(item));
                }
            }
        }
    } else {
        if (!m_users.contains(toUsername)) {
            QJsonObject err;
            err["error"] = "recipient not found";
            callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
            return;
        }

        const auto &recipient = m_users[toUsername];
        if (recipient.devices.isEmpty()) {
            QJsonObject err;
            err["error"] = "recipient has no registered devices";
            callback(422, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Unprocessable Entity");
            return;
        }

        for (auto it = recipient.devices.cbegin(); it != recipient.devices.cend(); ++it) {
            if (it.key() != fromDeviceId) {
                MockQueuedMessage item;
                item.id = m_nextMessageId++;
                item.deviceId = it.key();
                item.toUsername = toUsername;
                item.ciphertextBase64 = ciphertext;
                item.timestamp = timestamp;
                m_deliveryQueue.push_back(std::move(item));
            }
        }
    }

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(201, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleRelayPoll(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString deviceId = queryParams.value("device_id");

    QJsonArray msgs;
    for (const auto &item : m_deliveryQueue) {
        if (item.deviceId == deviceId) {
            QJsonObject m;
            m["id"] = item.id;
            m["ciphertext"] = item.ciphertextBase64;
            // The real backend does NOT return a nonce, it was removed for Phase 10B.
            m["timestamp"] = item.timestamp;
            msgs.append(m);
        }
    }

    QJsonObject res;
    res["messages"] = msgs;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleRelayAck(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    auto doc = QJsonDocument::fromJson(data);
    QString deviceId = doc.object().value("device_id").toString();
    qint64 messageId = doc.object().value("message_id").toInteger();

    for (auto it = m_deliveryQueue.begin(); it != m_deliveryQueue.end(); ) {
        if (it->id == messageId && it->deviceId == deviceId) {
            it = m_deliveryQueue.erase(it);
        } else {
            ++it;
        }
    }

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

} // namespace Testing
} // namespace NeoNect

namespace NeoNect {
namespace Testing {

void MockHttpTransport::tamperLastMessageCiphertext(const QString &deviceId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto it = m_deliveryQueue.rbegin(); it != m_deliveryQueue.rend(); ++it) {
        if (it->deviceId == deviceId) {
            QByteArray decoded = QByteArray::fromBase64(it->ciphertextBase64.toLatin1());
            if (decoded.size() > 0) {
                decoded[0] = decoded[0] ^ 0xFF; // flip bits
                it->ciphertextBase64 = QString::fromLatin1(decoded.toBase64());
            }
            break;
        }
    }
}

void MockHttpTransport::tamperLastMessageNonce(const QString &deviceId) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (auto it = m_deliveryQueue.rbegin(); it != m_deliveryQueue.rend(); ++it) {
        if (it->deviceId == deviceId) {
            QByteArray decoded = QByteArray::fromBase64(it->ciphertextBase64.toLatin1());
            if (decoded.size() > 5) {
                decoded[2] = static_cast<char>(decoded[2] ^ 0xFF); // flip a bit in the envelope
                it->ciphertextBase64 = QString::fromLatin1(decoded.toBase64());
            }
            break;
        }
    }
}

void MockHttpTransport::setSimulatedResponse(const QString &endpoint, const QByteArray &data, int statusCode) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_simulatedResponses[endpoint] = qMakePair(data, statusCode);
}

} // namespace Testing
} // namespace NeoNect
