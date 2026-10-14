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
        auto pkObj = uObj.value("device_prekeys").toObject();
        for (auto pit = pkObj.begin(); pit != pkObj.end(); ++pit) {
            auto bObj = pit.value().toObject();
            MockDevicePreKeys bundle;
            auto spkObj = bObj.value("signed_curve_prekey").toObject();
            bundle.signedPreKey.keyId = spkObj.value("key_id").toInt();
            bundle.signedPreKey.publicKey = spkObj.value("public_key").toString();
            bundle.signedPreKey.signature = spkObj.value("signature").toString();
            auto opkArr = bObj.value("one_time_curve_prekeys").toArray();
            for (const auto &v : opkArr) {
                auto oObj = v.toObject();
                MockOneTimePreKey opk;
                opk.keyId = oObj.value("key_id").toInt();
                opk.publicKey = oObj.value("public_key").toString();
                bundle.oneTimePreKeys.push_back(opk);
            }
            u.devicePreKeys[pit.key()] = std::move(bundle);
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
        item.senderDeviceId = qObj.value("sender_device_id").toString();
        item.messageId = qObj.value("message_id").toString();
        item.protocolVersion = qObj.value("protocol_version").toInt(2);
        item.toUsername = qObj.value("to_username").toString();
        item.ciphertextBase64 = qObj.value("ciphertext").toString();
        // nonce is inside envelope now
        item.timestamp = qObj.value("timestamp").toInteger();
        m_deliveryQueue.push_back(item);
    }

    m_friendships.clear();
    auto friendsArr = root.value("friendships").toArray();
    for (const auto &val : friendsArr) {
        m_friendships.insert(val.toString());
    }

    m_friendRequests.clear();
    auto reqArr = root.value("friend_requests").toArray();
    for (const auto &val : reqArr) {
        m_friendRequests.insert(val.toString());
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

        QJsonObject pkObj;
        for (auto pit = it.value().devicePreKeys.begin(); pit != it.value().devicePreKeys.end(); ++pit) {
            QJsonObject bObj;
            QJsonObject spkObj;
            spkObj["key_id"] = pit.value().signedPreKey.keyId;
            spkObj["public_key"] = pit.value().signedPreKey.publicKey;
            spkObj["signature"] = pit.value().signedPreKey.signature;
            bObj["signed_curve_prekey"] = spkObj;

            QJsonArray opkArr;
            for (const auto &opk : pit.value().oneTimePreKeys) {
                QJsonObject oObj;
                oObj["key_id"] = opk.keyId;
                oObj["public_key"] = opk.publicKey;
                opkArr.append(oObj);
            }
            bObj["one_time_curve_prekeys"] = opkArr;
            pkObj[pit.key()] = bObj;
        }
        uObj["device_prekeys"] = pkObj;
        usersObj[it.key()] = uObj;
    }
    root["users"] = usersObj;

    QJsonArray queueArr;
    for (const auto &item : m_deliveryQueue) {
        QJsonObject qObj;
        qObj["id"] = item.id;
        qObj["device_id"] = item.deviceId;
        qObj["sender_device_id"] = item.senderDeviceId;
        qObj["message_id"] = item.messageId;
        qObj["protocol_version"] = item.protocolVersion;
        qObj["to_username"] = item.toUsername;
        qObj["ciphertext"] = item.ciphertextBase64;
        // nonce is inside envelope now
        qObj["timestamp"] = item.timestamp;
        queueArr.append(qObj);
    }
    root["delivery_queue"] = queueArr;

    QJsonArray friendsArr;
    for (const auto &pair : m_friendships) {
        friendsArr.append(pair);
    }
    root["friendships"] = friendsArr;

    QJsonArray reqArr;
    for (const auto &req : m_friendRequests) {
        reqArr.append(req);
    }
    root["friend_requests"] = reqArr;

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
        if (m_tokenToUser.contains(token)) {
            return;
        }
        for (const auto &user : m_users) {
            if (token.contains(user.username, Qt::CaseInsensitive)) {
                m_tokenToUser[token] = user.username;
                return;
            }
        }
        if (token.startsWith("mock-token-", Qt::CaseInsensitive)) {
            QString prefix = token.mid(11).trimmed();
            if (!prefix.isEmpty()) {
                for (const auto &user : m_users) {
                    if (user.username.startsWith(prefix, Qt::CaseInsensitive)) {
                        m_tokenToUser[token] = user.username;
                        return;
                    }
                }
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
        m_tokenToUser["mock-token-" + cleanUser] = cleanUser;
    }
    if (!m_activeToken.isEmpty() && m_activeToken.contains(cleanUser, Qt::CaseInsensitive)) {
        m_tokenToUser[m_activeToken] = cleanUser;
    }
}

void MockHttpTransport::bindToken(const QString &token, const QString &username) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString cleanUser = username.trimmed().toLower();
    if (!token.isEmpty() && !cleanUser.isEmpty()) {
        seedUser(cleanUser, "password123");
        m_tokenToUser[token] = cleanUser;
    }
}

void MockHttpTransport::seedFriendRequest(const QString &sender, const QString &recipient) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString s = sender.trimmed().toLower();
    QString r = recipient.trimmed().toLower();
    if (s.isEmpty() || r.isEmpty() || s == r) return;
    seedUser(s, "password123");
    seedUser(r, "password123");
    m_friendRequests.insert(s + "->" + r);
    if (m_enableSharedStorage) {
        saveSharedState();
    }
}

void MockHttpTransport::seedFriendship(const QString &user1, const QString &user2) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    QString u1 = user1.trimmed().toLower();
    QString u2 = user2.trimmed().toLower();
    if (u1.isEmpty() || u2.isEmpty() || u1 == u2) return;
    seedUser(u1, "password123");
    seedUser(u2, "password123");
    m_friendRequests.remove(u1 + "->" + u2);
    m_friendRequests.remove(u2 + "->" + u1);
    QString pairKey = (u1 < u2) ? (u1 + ":" + u2) : (u2 + ":" + u1);
    m_friendships.insert(pairKey);
    if (m_enableSharedStorage) {
        saveSharedState();
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
    Q_UNUSED(context);
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
    } else if (endpoint == Constants::EP_CAPABILITIES) {
        handleCapabilities(callback);
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
    } else if (endpoint == Constants::EP_FRIENDS) {
        handleFriendsGet(callback);
    } else if (endpoint == Constants::EP_FRIEND_REQUESTS) {
        handleFriendRequestsGet(callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

QNetworkReply* MockHttpTransport::post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, Transport::HttpResponseCallback callback) {
    Q_UNUSED(context);
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
    } else if (endpoint == Constants::EP_PREKEY_UPLOAD) {
        handlePreKeyUpload(jsonData, callback);
    } else if (endpoint == Constants::EP_PREKEY_CLAIM) {
        handlePreKeyClaim(jsonData, callback);
    } else if (endpoint == Constants::EP_RELAY_SEND) {
        handleRelaySend(jsonData, callback);
    } else if (endpoint == Constants::EP_RELAY_ACK) {
        handleRelayAck(jsonData, callback);
    } else if (endpoint == Constants::EP_FRIENDS) {
        handleFriends(jsonData, callback);
    } else if (endpoint == Constants::EP_FRIEND_REQUESTS_ACCEPT) {
        handleFriendRequestsAccept(jsonData, callback);
    } else if (endpoint == Constants::EP_FRIEND_REQUESTS_DECLINE) {
        handleFriendRequestsDecline(jsonData, callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

QNetworkReply* MockHttpTransport::deleteResource(const QString &endpoint, const QObject* context, Transport::HttpResponseCallback callback, const QByteArray &jsonData) {
    Q_UNUSED(context);
    emit requestHandled("DELETE", endpoint);
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
        handleAuthDelete(callback);
    } else if (endpoint == Constants::EP_DEVICE) {
        handleDeviceDelete(jsonData, callback);
    } else if (endpoint == Constants::EP_FRIENDS) {
        handleFriendsDelete(jsonData, callback);
    } else if (endpoint == Constants::EP_FRIEND_REQUESTS) {
        handleFriendRequestsDelete(jsonData, callback);
    } else {
        callback(404, QByteArray("{\"error\":\"Not Found\"}"), QNetworkReply::ContentNotFoundError, "Not Found");
    }
    return nullptr;
}

void MockHttpTransport::handleHealth(Transport::HttpResponseCallback callback) {
    QJsonObject res;
    res["status"] = "success";
    res["node"] = "mock-neonect-embedded";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleCapabilities(Transport::HttpResponseCallback callback) {
    QJsonObject limits;
    limits["max_http_body_bytes"] = 4194304;
    limits["max_envelope_bytes"] = 1048576;
    limits["max_devices_per_user"] = 10;

    QJsonObject res;
    res["version"] = 1;
    res["limits"] = limits;

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
    if (m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }
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

void MockHttpTransport::handlePreKeyUpload(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString username = m_tokenToUser[m_activeToken];
    auto doc = QJsonDocument::fromJson(data);
    auto obj = doc.object();
    QString deviceId = obj.value("device_id").toString();
    if (deviceId.isEmpty()) {
        QJsonObject err;
        err["error"] = "device_id required";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    MockDevicePreKeys bundle;
    auto spkObj = obj.value("signed_curve_prekey").toObject();
    bundle.signedPreKey.keyId = spkObj.value("key_id").toInt();
    bundle.signedPreKey.publicKey = spkObj.value("public_key").toString();
    bundle.signedPreKey.signature = spkObj.value("signature").toString();

    auto opkArr = obj.value("one_time_curve_prekeys").toArray();
    for (const auto &v : opkArr) {
        auto opkObj = v.toObject();
        MockOneTimePreKey opk;
        opk.keyId = opkObj.value("key_id").toInt();
        opk.publicKey = opkObj.value("public_key").toString();
        bundle.oneTimePreKeys.push_back(opk);
    }

    m_users[username].devicePreKeys[deviceId] = std::move(bundle);
    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handlePreKeyClaim(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || !m_tokenToUser.contains(m_activeToken)) {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    auto doc = QJsonDocument::fromJson(data);
    auto obj = doc.object();
    QString targetUser = obj.value("target_user").toString().trimmed().toLower();
    QString targetDevice = obj.value("target_device").toString();

    if (!m_users.contains(targetUser) ||
        !m_users[targetUser].devices.contains(targetDevice) ||
        !m_users[targetUser].devicePreKeys.contains(targetDevice)) {
        QJsonObject err;
        err["error"] = "prekey bundle not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    auto &user = m_users[targetUser];
    auto &prekeys = user.devicePreKeys[targetDevice];

    QJsonObject res;
    res["identity_key"] = user.devices[targetDevice];

    QJsonObject spkObj;
    spkObj["key_id"] = prekeys.signedPreKey.keyId;
    spkObj["public_key"] = prekeys.signedPreKey.publicKey;
    spkObj["signature"] = prekeys.signedPreKey.signature;
    res["signed_curve_prekey"] = spkObj;

    if (!prekeys.oneTimePreKeys.empty()) {
        auto opk = prekeys.oneTimePreKeys.front();
        prekeys.oneTimePreKeys.erase(prekeys.oneTimePreKeys.begin());
        QJsonObject opkObj;
        opkObj["key_id"] = opk.keyId;
        opkObj["public_key"] = opk.publicKey;
        res["one_time_curve_prekey"] = opkObj;
        if (m_enableSharedStorage) {
            saveSharedState();
        }
    }

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
    auto obj = doc.object();
    QString fromDeviceId = obj.value("from_device_id").toString();
    QString recipientDeviceId = obj.value("recipient_device_id").toString();
    if (recipientDeviceId.isEmpty()) {
        recipientDeviceId = obj.value("to_device_id").toString();
    }
    QString toUsername = obj.value("to_username").toString().trimmed().toLower();
    QString messageId = obj.value("message_id").toString();
    int protocolVersion = obj.value("protocol_version").toInt(2);
    QString ciphertext = obj.value("ciphertext").toString();
    qint64 timestamp = obj.value("timestamp").toInteger();

    if (!recipientDeviceId.isEmpty()) {
        QString resolvedUser = toUsername;
        if (resolvedUser.isEmpty()) {
            for (auto uit = m_users.cbegin(); uit != m_users.cend(); ++uit) {
                if (uit.value().devices.contains(recipientDeviceId)) {
                    resolvedUser = uit.key();
                    break;
                }
            }
        }
        MockQueuedMessage item;
        item.id = m_nextMessageId++;
        item.deviceId = recipientDeviceId;
        item.senderDeviceId = fromDeviceId;
        item.messageId = messageId;
        item.protocolVersion = protocolVersion;
        item.toUsername = resolvedUser;
        item.ciphertextBase64 = ciphertext;
        item.timestamp = timestamp;
        m_deliveryQueue.push_back(std::move(item));
    } else if (toUsername == "general") {
        // Handle channel broadcast (e.g. general)
        for (const auto &user : m_users) {
            for (auto it = user.devices.cbegin(); it != user.devices.cend(); ++it) {
                if (it.key() != fromDeviceId) {
                    MockQueuedMessage item;
                    item.id = m_nextMessageId++;
                    item.deviceId = it.key();
                    item.senderDeviceId = fromDeviceId;
                    item.messageId = messageId;
                    item.protocolVersion = protocolVersion;
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
                item.senderDeviceId = fromDeviceId;
                item.messageId = messageId;
                item.protocolVersion = protocolVersion;
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
            if (!item.senderDeviceId.isEmpty()) {
                m["sender_device_id"] = item.senderDeviceId;
            }
            if (!item.deviceId.isEmpty()) {
                m["recipient_device_id"] = item.deviceId;
            }
            if (!item.messageId.isEmpty()) {
                m["message_id"] = item.messageId;
            }
            if (item.protocolVersion > 0) {
                m["protocol_version"] = item.protocolVersion;
            }
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

QString MockHttpTransport::resolveCurrentUsername() const {
    if (!m_activeToken.isEmpty() && m_tokenToUser.contains(m_activeToken)) {
        return m_tokenToUser[m_activeToken].trimmed().toLower();
    }
    if (!m_activeToken.isEmpty()) {
        for (auto it = m_users.constBegin(); it != m_users.constEnd(); ++it) {
            if (m_activeToken.contains(it.key(), Qt::CaseInsensitive)) {
                return it.key();
            }
        }
        return m_activeToken.trimmed().toLower();
    }
    return QString();
}

void MockHttpTransport::handleFriends(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();

    auto doc = QJsonDocument::fromJson(data);
    QString targetUsername = doc.object().value("username").toString().trimmed().toLower();

    if (targetUsername.isEmpty()) {
        QJsonObject err;
        err["error"] = "bad request";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    if (currentUsername.isEmpty()) {
        currentUsername = (targetUsername == "alice") ? "bob" : "alice";
    }

    if (targetUsername == currentUsername) {
        QJsonObject err;
        err["error"] = "cannot add yourself";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    if (!m_users.isEmpty() && !m_users.contains(targetUsername)) {
        QJsonObject err;
        err["error"] = "resource not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    QString pairKey = (currentUsername < targetUsername)
        ? (currentUsername + ":" + targetUsername)
        : (targetUsername + ":" + currentUsername);

    if (m_friendships.contains(pairKey)) {
        QJsonObject err;
        err["error"] = "friendship already exists";
        callback(409, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentConflictError, "Conflict");
        return;
    }

    QString outKey = currentUsername + "->" + targetUsername;
    QString inKey = targetUsername + "->" + currentUsername;
    if (m_friendRequests.contains(outKey)) {
        QJsonObject err;
        err["error"] = "friend request already sent";
        callback(409, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentConflictError, "Conflict");
        return;
    }
    if (m_friendRequests.contains(inKey)) {
        QJsonObject err;
        err["error"] = "friend request already received";
        callback(409, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentConflictError, "Conflict");
        return;
    }

    m_friendRequests.insert(outKey);
    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(201, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendsGet(Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();

    QJsonArray friendsArr;
    for (const QString &pair : m_friendships) {
        QStringList parts = pair.split(':');
        if (parts.size() == 2) {
            if (parts[0].compare(currentUsername, Qt::CaseInsensitive) == 0) {
                friendsArr.append(parts[1]);
            } else if (parts[1].compare(currentUsername, Qt::CaseInsensitive) == 0) {
                friendsArr.append(parts[0]);
            }
        }
    }

    QJsonObject res;
    res["status"] = "success";
    res["friends"] = friendsArr;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendsDelete(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();

    auto doc = QJsonDocument::fromJson(data);
    QString targetUsername = doc.object().value("username").toString().trimmed().toLower();

    if (targetUsername.isEmpty() || targetUsername == currentUsername) {
        QJsonObject err;
        err["error"] = "bad request";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    if (!m_users.isEmpty() && !m_users.contains(targetUsername)) {
        QJsonObject err;
        err["error"] = "resource not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    QString pairKey = (currentUsername < targetUsername)
        ? (currentUsername + ":" + targetUsername)
        : (targetUsername + ":" + currentUsername);

    m_friendships.remove(pairKey);
    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendRequestsGet(Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();

    QJsonArray incomingArr;
    QJsonArray outgoingArr;
    for (const QString &req : m_friendRequests) {
        QStringList parts = req.split("->");
        if (parts.size() == 2) {
            if (parts[1].compare(currentUsername, Qt::CaseInsensitive) == 0) {
                incomingArr.append(parts[0]);
            } else if (parts[0].compare(currentUsername, Qt::CaseInsensitive) == 0) {
                outgoingArr.append(parts[1]);
            }
        }
    }

    QJsonObject dataObj;
    dataObj["incoming"] = incomingArr;
    dataObj["outgoing"] = outgoingArr;

    QJsonObject res;
    res["status"] = "success";
    res["data"] = dataObj;
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendRequestsAccept(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();
    auto doc = QJsonDocument::fromJson(data);
    QString targetUsername = doc.object().value("username").toString().trimmed().toLower();

    if (targetUsername.isEmpty() || targetUsername == currentUsername) {
        QJsonObject err;
        err["error"] = "bad request";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    QString outKey = currentUsername + "->" + targetUsername;
    QString inKey = targetUsername + "->" + currentUsername;

    if (m_friendRequests.contains(outKey)) {
        QJsonObject err;
        err["error"] = "unauthorized action on friend request";
        callback(403, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentOperationNotPermittedError, "Forbidden");
        return;
    }

    if (!m_friendRequests.contains(inKey)) {
        QJsonObject err;
        err["error"] = "resource not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    m_friendRequests.remove(inKey);
    QString pairKey = (currentUsername < targetUsername)
        ? (currentUsername + ":" + targetUsername)
        : (targetUsername + ":" + currentUsername);
    m_friendships.insert(pairKey);

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendRequestsDecline(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();
    auto doc = QJsonDocument::fromJson(data);
    QString targetUsername = doc.object().value("username").toString().trimmed().toLower();

    if (targetUsername.isEmpty() || targetUsername == currentUsername) {
        QJsonObject err;
        err["error"] = "bad request";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    QString outKey = currentUsername + "->" + targetUsername;
    QString inKey = targetUsername + "->" + currentUsername;

    if (m_friendRequests.contains(outKey)) {
        QJsonObject err;
        err["error"] = "unauthorized action on friend request";
        callback(403, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentOperationNotPermittedError, "Forbidden");
        return;
    }

    if (!m_friendRequests.contains(inKey)) {
        QJsonObject err;
        err["error"] = "resource not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    m_friendRequests.remove(inKey);

    if (m_enableSharedStorage) {
        saveSharedState();
    }

    QJsonObject res;
    res["status"] = "success";
    callback(200, QJsonDocument(res).toJson(QJsonDocument::Compact), QNetworkReply::NoError, QString());
}

void MockHttpTransport::handleFriendRequestsDelete(const QByteArray &data, Transport::HttpResponseCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_activeToken.isEmpty() || m_activeToken == "unauthorized") {
        QJsonObject err;
        err["error"] = "unauthorized";
        callback(401, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::AuthenticationRequiredError, "Unauthorized");
        return;
    }

    QString currentUsername = resolveCurrentUsername();
    auto doc = QJsonDocument::fromJson(data);
    QString targetUsername = doc.object().value("username").toString().trimmed().toLower();

    if (targetUsername.isEmpty() || targetUsername == currentUsername) {
        QJsonObject err;
        err["error"] = "bad request";
        callback(400, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ProtocolInvalidOperationError, "Bad Request");
        return;
    }

    QString outKey = currentUsername + "->" + targetUsername;
    QString inKey = targetUsername + "->" + currentUsername;

    if (m_friendRequests.contains(inKey)) {
        QJsonObject err;
        err["error"] = "unauthorized action on friend request";
        callback(403, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentOperationNotPermittedError, "Forbidden");
        return;
    }

    if (!m_friendRequests.contains(outKey)) {
        QJsonObject err;
        err["error"] = "resource not found";
        callback(404, QJsonDocument(err).toJson(QJsonDocument::Compact), QNetworkReply::ContentNotFoundError, "Not Found");
        return;
    }

    m_friendRequests.remove(outKey);

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
