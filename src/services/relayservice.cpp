#include "relayservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>

namespace NeoNect {
namespace Services {

RelayService::RelayService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           std::shared_ptr<Crypto::ICryptoService> cryptoService,
                           QObject *parent)
    : QObject(parent),
      m_transport(std::move(transport)),
      m_storage(std::move(storage)),
      m_cryptoService(std::move(cryptoService)),
      m_wsClient(std::make_unique<Transport::WebSocketClient>(this)),
      m_pollTimer(new QTimer(this)) {

    m_pollTimer->setInterval(Constants::RELAY_POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &RelayService::pollPendingMessages);

    connect(this, &RelayService::serverConnected, this, [this]() {
        m_isWsConnected = true;
    });

    connect(this, &RelayService::serverDisconnected, this, [this]() {
        m_isWsConnected = false;
    });

    connect(m_wsClient.get(), &Transport::WebSocketClient::connected, this, [this]() {
        qDebug() << "[RelayService] Connected to server WebSocket relay. Online status active.";
        emit serverConnected();
    });

    connect(m_wsClient.get(), &Transport::WebSocketClient::disconnected, this, [this]() {
        qDebug() << "[RelayService] Disconnected from server WebSocket relay.";
        emit serverDisconnected();
    });

    connect(m_wsClient.get(), &Transport::WebSocketClient::textMessageReceived,
            this, &RelayService::onWebSocketMessageReceived);

    connect(m_wsClient.get(), &Transport::WebSocketClient::errorOccurred, this, [this](const QString &err) {
        qWarning() << "[RelayService] WebSocket client error:" << err;
        if (err.contains("401") || err.contains("Unauthorized", Qt::CaseInsensitive)) {
            handle401Error();
        }
    });
}

void RelayService::startPolling() {
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }

    bool isMock = m_transport && m_transport->baseUrl().contains("mock.neonect.local");
    if (isMock) {
        m_isWsConnected = true;
        emit serverConnected();
    } else if (m_wsClient) {
        QString devId = m_storage->deviceId().trimmed();
        QString token = m_storage->authToken().trimmed();
        if (!devId.isEmpty() && !token.isEmpty()) {
            if (!m_wsClient->isConnected() || m_wsClient->deviceId() != devId || m_wsClient->token() != token) {
                qDebug() << "[RelayService] Opening WebSocket connection for presence & delivery:" << devId;
                m_wsClient->open(m_transport->baseUrl(), devId, token);
            }
        }
    }

    pollPendingMessages();
}

void RelayService::stopPolling() {
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }
    if (m_wsClient) {
        m_wsClient->close();
    }
    m_isWsConnected = false;
    emit serverDisconnected();
}

bool RelayService::isPolling() const {
    return m_pollTimer->isActive();
}

bool RelayService::isConnected() const {
    if (m_wsClient && m_wsClient->isConnected()) {
        return true;
    }
    if (m_isWsConnected) {
        return true;
    }
    if (m_transport && m_transport->baseUrl().contains("mock.neonect.local")) {
        return isPolling() && !m_storage->authToken().isEmpty();
    }
    return false;
}

void RelayService::handle401Error() {
    if (m_retry401Count < 2) {
        m_retry401Count++;
        qDebug() << "[RelayService] 401 Unauthorized encountered. Requesting device re-registration attempt" << m_retry401Count;
        emit deviceRegistrationRequested();
        return;
    }

    m_retry401Count = 0;
    qDebug() << "[RelayService] Permanent 401 Session Expired.";
    stopPolling();
    m_storage->clearSession();
    m_transport->setAuthToken(QString());
    emit sessionUnauthorized("Session expired or unauthorized. Please log in again.");
}

void RelayService::sendDomainMessage(const Domain::Message &msg) {
    QString token = m_storage->authToken();
    QString deviceId = m_storage->deviceId().trimmed();
    if (deviceId.isEmpty()) {
        QString prof = m_storage->profile();
        QString user = m_storage->username().trimmed().toLower();
        QString prefix = prof.isEmpty() ? "" : prof + "-";
        if (!user.isEmpty()) prefix += user + "-";
        deviceId = QString("neonect-dev-%1%2").arg(prefix,
                                                    QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_storage->setDeviceId(deviceId);
    }
    QString currentUsername = m_storage->username();
    
    QString targetUser;
    if (msg.conversationId.startsWith("dms:")) {
        targetUser = msg.conversationId.mid(4);
    } else {
        // Fallback for unexpected format (backend enforces target users)
        targetUser = msg.conversationId;
    }

    if (targetUser.compare("saved-messages", Qt::CaseInsensitive) == 0) {
        qDebug() << "[RelayService] sendDomainMessage: Skipping network relay for saved-messages.";
        emit secureMessageTransmitted(targetUser, true);
        emit messageTransmissionStatus(targetUser, msg.id, true, "");
        return;
    }

    if (token.isEmpty() || targetUser.isEmpty()) {
        qDebug() << "[RelayService] sendDomainMessage failed: token.isEmpty()=" << token.isEmpty() << "targetUser=" << targetUser << "conversationId=" << msg.conversationId;
        emit secureMessageTransmitted(targetUser, false);
        emit messageTransmissionStatus(targetUser, msg.id, false, "Missing session token or recipient");
        return;
    }

    QJsonObject packet;
    packet["sender"] = currentUsername;
    packet["target"] = targetUser;
    packet["messageId"] = msg.id;
    packet["timestamp"] = msg.timestamp > 0 ? msg.timestamp : QDateTime::currentSecsSinceEpoch();
    packet["type"] = msg.type;
    packet["content"] = msg.text;
    packet["text"] = msg.text;
    
    if (!msg.mediaUrl.isEmpty()) packet["mediaUrl"] = msg.mediaUrl;
    if (!msg.fileName.isEmpty()) packet["fileName"] = msg.fileName;
    if (msg.fileSize > 0) packet["fileSize"] = msg.fileSize;
    if (msg.duration > 0) packet["duration"] = msg.duration;
    if (!msg.errorText.isEmpty()) packet["mediaType"] = msg.errorText;
    
    QJsonArray waveArray = QJsonDocument::fromJson(msg.waveform).array();
    if (!waveArray.isEmpty()) packet["waveform"] = waveArray;

    QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);

    auto encrypted = m_cryptoService->encryptAesGcm(packetBytes);
    if (!encrypted.success) {
        qDebug() << "[RelayService] sendDomainMessage failed - E2EE encryption error:" << encrypted.errorMessage;
        emit secureMessageTransmitted(targetUser, false);
        emit messageTransmissionStatus(targetUser, msg.id, false, "E2EE Encryption failed: " + encrypted.errorMessage);
        return;
    }

    QJsonObject payload;
    payload["from_device_id"] = deviceId;
    payload["to_username"] = targetUser;
    payload["ciphertext"] = QString::fromLatin1(encrypted.envelope.toBase64());
    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QString msgId = msg.id;

    qDebug() << "[RelayService] Transmitting domain message to:" << targetUser << "from device:" << deviceId << "type:" << msg.type << "msgId:" << msgId;

    m_transport->post(Constants::EP_RELAY_SEND, postData, this, [this, targetUser, msgId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        bool ok = (error == QNetworkReply::NoError || statusCode == 200 || statusCode == 201);
        QString errorMsg = ok ? "" : (errStr.isEmpty() ? "Network relay transmission failed" : errStr);
        if (ok) {
            qDebug() << "[RelayService] sendRelayMessage succeeded for" << targetUser << "status:" << statusCode;
        } else {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("error")) {
                errorMsg = doc.object().value("error").toString();
            } else if (statusCode == 422) {
                errorMsg = "Recipient has no active registered devices";
            } else if (statusCode == 404) {
                errorMsg = "Recipient user not found";
            } else if (statusCode == 401 || statusCode == 403) {
                errorMsg = "Unauthorized or device ownership failure";
            }
            qDebug() << "[RelayService] sendRelayMessage failed for" << targetUser << "Error:" << errorMsg << "Status:" << statusCode;
            if (statusCode == 401 || data.contains("unauthorized")) {
                handle401Error();
            }
        }
        emit secureMessageTransmitted(targetUser, ok);
        emit messageTransmissionStatus(targetUser, msgId, ok, errorMsg);
    });
}

void RelayService::pollPendingMessages() {
    QString token = m_storage->authToken();
    QString deviceId = m_storage->deviceId();
    if (token.isEmpty() || deviceId.isEmpty() || m_isPollingActive) {
        return;
    }

    m_isPollingActive = true;

    QMap<QString, QString> params;
    params["device_id"] = deviceId;

    m_transport->get(Constants::EP_RELAY_POLL, params, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        m_isPollingActive = false;

        if (statusCode == 401) {
            handle401Error();
            return;
        }

        if (error != QNetworkReply::NoError || data.isEmpty()) {
            return;
        }

        m_retry401Count = 0;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) return;
        
        QJsonObject root = doc.object();
        if (!root.contains("messages") || !root.value("messages").isArray()) return;
        
        QJsonArray messages = root.value("messages").toArray();
        for (const QJsonValue &val : messages) {
            if (!val.isObject()) continue;
            QJsonObject msgObj = val.toObject();
            qint64 msgId = msgObj.value("id").toInteger();
            QString base64Cipher = msgObj.value("ciphertext").toString();
            processIncomingRelayItem(msgId, base64Cipher);
        }
    });
}

void RelayService::onWebSocketMessageReceived(const QString &text) {
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject msgObj = doc.object();
    qint64 msgId = msgObj.value("id").toInteger();
    QString base64Cipher = msgObj.value("ciphertext").toString();

    qDebug() << "[RelayService] WebSocket real-time delivery received. MsgId:" << msgId;
    processIncomingRelayItem(msgId, base64Cipher);
}

void RelayService::processIncomingRelayItem(qint64 msgId, const QString &base64Cipher) {
    if (m_processedMessageIds.find(msgId) != m_processedMessageIds.end()) {
        acknowledgeMessage(msgId);
        return;
    }
    m_processedMessageIds.insert(msgId);

    if (base64Cipher.isEmpty()) {
        acknowledgeMessage(msgId);
        return;
    }

    QByteArray envelopeBytes = QByteArray::fromBase64(base64Cipher.toLatin1());
    
    QByteArray decodedBytes = m_cryptoService->decryptAesGcmEnvelope(envelopeBytes);
    if (decodedBytes.isEmpty()) {
        qDebug() << "[RelayService] Failed to decrypt message (authentication failed or missing key)";
        acknowledgeMessage(msgId);
        return;
    }

    QString textContent = QString::fromUtf8(decodedBytes);
    QString sender = "Anonymous";
    QString target = "dms:" + m_storage->username();
    QString type = "text";
    QString mediaUrl = "";
    QString fileName = "";
    QString mediaCategory = "";
    qint64 fileSize = 0;
    int duration = 0;
    QVariantList waveform;
    QString messageUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    qint64 timestamp = 0;

    auto packetDoc = QJsonDocument::fromJson(decodedBytes);
    if (!packetDoc.isNull() && packetDoc.isObject()) {
        QJsonObject packetObj = packetDoc.object();
        if (packetObj.contains("sender")) sender = packetObj.value("sender").toString();
        if (packetObj.contains("target")) target = packetObj.value("target").toString();
        if (packetObj.contains("content")) textContent = packetObj.value("content").toString();
        else if (packetObj.contains("text")) textContent = packetObj.value("text").toString();
        if (packetObj.contains("type")) type = packetObj.value("type").toString();
        if (packetObj.contains("mediaUrl")) mediaUrl = packetObj.value("mediaUrl").toString();
        if (packetObj.contains("fileName")) fileName = packetObj.value("fileName").toString();
        if (packetObj.contains("fileSize")) fileSize = packetObj.value("fileSize").toInteger();
        if (packetObj.contains("duration")) duration = static_cast<int>(packetObj.value("duration").toInteger());
        if (packetObj.contains("waveform")) waveform = packetObj.value("waveform").toArray().toVariantList();
        if (packetObj.contains("messageId")) messageUuid = packetObj.value("messageId").toString();
        if (packetObj.contains("mediaType")) mediaCategory = packetObj.value("mediaType").toString();
        if (packetObj.contains("timestamp") && packetObj.value("timestamp").toInteger() > 0) {
            timestamp = packetObj.value("timestamp").toInteger();
        }
    }

    qDebug() << "[RelayService] Decrypted packet from:" << sender << "type:" << type;

    if (type == "friend_request" || type == "friend_accept" || type == "friend_reject") {
        Domain::Message friendMsg;
        friendMsg.serverId = msgId;
        friendMsg.id = messageUuid;
        friendMsg.senderId = sender;
        friendMsg.type = type;
        friendMsg.text = textContent;
        friendMsg.timestamp = (timestamp <= 0) ? QDateTime::currentSecsSinceEpoch() : timestamp;
        friendMsg.conversationId = "dms:" + sender.toLower();

        qDebug() << "[RelayService] Emitting incomingFriendPacket for:" << sender << "type:" << type;
        emit incomingFriendPacket(friendMsg);
        acknowledgeMessage(msgId);
        return;
    }

    if (type == "typing_start" || type == "typing_stop") {
        Domain::Message typingMsg;
        typingMsg.serverId = msgId;
        typingMsg.id = messageUuid;
        typingMsg.senderId = sender;
        typingMsg.conversationId = "dms:" + sender.toLower();
        typingMsg.type = type;
        typingMsg.timestamp = (timestamp <= 0) ? QDateTime::currentSecsSinceEpoch() : timestamp;

        emit incomingDomainMessagesReceived({typingMsg});
        acknowledgeMessage(msgId);
        return;
    }

    if (type == "message_seen") {
        Domain::Message seenMsg;
        seenMsg.serverId = msgId;
        seenMsg.id = messageUuid;
        seenMsg.senderId = sender;
        seenMsg.conversationId = "dms:" + sender.toLower();
        seenMsg.type = type;
        seenMsg.text = textContent;
        seenMsg.timestamp = (timestamp <= 0) ? QDateTime::currentSecsSinceEpoch() : timestamp;

        emit incomingDomainMessagesReceived({seenMsg});
        acknowledgeMessage(msgId);
        return;
    }

    Domain::Message domainMsg;
    domainMsg.serverId = msgId;
    domainMsg.id = messageUuid;
    domainMsg.senderId = sender;
    domainMsg.type = type;
    domainMsg.text = textContent;
    domainMsg.mediaUrl = mediaUrl;
    domainMsg.fileName = fileName;
    domainMsg.fileSize = fileSize;
    domainMsg.duration = duration;
    domainMsg.errorText = mediaCategory;
    
    QJsonArray waveArray;
    for (const QVariant &v : waveform) waveArray.append(v.toInt());
    if (!waveArray.isEmpty()) {
        domainMsg.waveform = QJsonDocument(waveArray).toJson(QJsonDocument::Compact);
    }
    
    domainMsg.status = (type == "media_request") ? Domain::MessageStatus::Pending : Domain::MessageStatus::Seen;
    domainMsg.timestamp = (timestamp <= 0) ? QDateTime::currentSecsSinceEpoch() : timestamp;
    
    domainMsg.conversationId = "dms:" + domainMsg.senderId.toLower();

    emit incomingDomainMessageReceived(domainMsg);
    emit incomingDomainMessagesReceived({domainMsg});
    acknowledgeMessage(msgId);
}

void RelayService::acknowledgeMessage(qint64 messageId) {
    QString token = m_storage->authToken();
    QString deviceId = m_storage->deviceId();
    if (token.isEmpty() || deviceId.isEmpty()) return;

    QJsonObject body;
    body["device_id"] = deviceId;
    body["message_id"] = messageId;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    m_transport->post(Constants::EP_RELAY_ACK, postData, this, [](int, const QByteArray&, QNetworkReply::NetworkError, const QString&) {});
}

} // namespace Services
} // namespace NeoNect
