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
      m_pollTimer(new QTimer(this)) {

    m_pollTimer->setInterval(Constants::RELAY_POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &RelayService::pollPendingMessages);
}

void RelayService::startPolling() {
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
    pollPendingMessages();
}

void RelayService::stopPolling() {
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }
}

bool RelayService::isPolling() const {
    return m_pollTimer->isActive();
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
    QString deviceId = m_storage->deviceId();
    QString currentUsername = m_storage->username();
    
    QString targetUser;
    if (msg.conversationId.startsWith("dms:")) {
        targetUser = msg.conversationId.mid(4);
    } else {
        // Fallback for unexpected format (backend enforces target users)
        targetUser = msg.conversationId;
    }

    if (token.isEmpty() || targetUser.isEmpty()) {
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
    
    QJsonArray waveArray = QJsonDocument::fromJson(msg.waveform).array();
    if (!waveArray.isEmpty()) packet["waveform"] = waveArray;

    QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);

    auto encrypted = m_cryptoService->encryptAesGcm(packetBytes);
    if (!encrypted.success) {
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

    m_transport->post(Constants::EP_RELAY_SEND, postData, this, [this, targetUser, msgId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        bool ok = (error == QNetworkReply::NoError || statusCode == 200 || statusCode == 201);
        QString errorMsg = ok ? "" : (errStr.isEmpty() ? "Network relay transmission failed" : errStr);
        if (!ok) {
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
        std::vector<Domain::Message> batchMsgs;
        for (const QJsonValue &val : messages) {
            if (!val.isObject()) continue;
            QJsonObject msgObj = val.toObject();

            qint64 msgId = msgObj.value("id").toInteger();
            if (m_processedMessageIds.find(msgId) != m_processedMessageIds.end()) {
                acknowledgeMessage(msgId);
                continue;
            }
            m_processedMessageIds.insert(msgId);

            QString base64Cipher = msgObj.value("ciphertext").toString();

            if (base64Cipher.isEmpty()) {
                acknowledgeMessage(msgId);
                continue;
            }

            QByteArray envelopeBytes = QByteArray::fromBase64(base64Cipher.toLatin1());
            
            QByteArray decodedBytes = m_cryptoService->decryptAesGcmEnvelope(envelopeBytes);
            if (decodedBytes.isEmpty()) {
                qDebug() << "[RelayService] Failed to decrypt message (authentication failed or missing key)";
                acknowledgeMessage(msgId);
                continue;
            }

            QString textContent = QString::fromUtf8(decodedBytes);
            QString sender = "Anonymous";
            QString target = "dms:" + m_storage->username();
            QString type = "text";
            QString mediaUrl = "";
            QString fileName = "";
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
                if (packetObj.contains("timestamp") && packetObj.value("timestamp").toInteger() > 0) {
                    timestamp = packetObj.value("timestamp").toInteger();
                }
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
            
            QJsonArray waveArray;
            for (const QVariant &v : waveform) waveArray.append(v.toInt());
            if (!waveArray.isEmpty()) {
                domainMsg.waveform = QJsonDocument(waveArray).toJson(QJsonDocument::Compact);
            }
            
            domainMsg.status = Domain::MessageStatus::Seen;
            domainMsg.timestamp = (timestamp <= 0) ? QDateTime::currentSecsSinceEpoch() : timestamp;
            
            domainMsg.conversationId = "dms:" + domainMsg.senderId.toLower();

            batchMsgs.push_back(domainMsg);
            acknowledgeMessage(msgId);
        }
        if (!batchMsgs.empty()) emit incomingDomainMessagesReceived(batchMsgs);
    });
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
