// src/services/relayservice.cpp
#include "relayservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>

namespace Avila {
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
    // Immediately fetch queued messages on startup / coming online
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

void RelayService::sendRelayMessage(const QString &toUsername, const QString &plainText, const QString &messageId) {
    QVariantMap data;
    data["content"] = plainText;
    data["text"] = plainText;
    data["type"] = "text";
    data["messageId"] = messageId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : messageId;
    sendRichRelayMessage(toUsername, data);
}

void RelayService::sendRichRelayMessage(const QString &toUsername, const QVariantMap &messageData) {
    QString targetUser = toUsername.trimmed().toLower();
    QString token = m_storage->authToken();
    QString currentUsername = m_storage->username();
    QString deviceId = m_storage->deviceId();
    QString msgId = messageData.value("messageId").toString();
    if (msgId.isEmpty()) {
        msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (token.isEmpty() || targetUser.isEmpty()) {
        emit secureMessageTransmitted(targetUser, false);
        emit messageTransmissionStatus(targetUser, msgId, false, "Missing session token or recipient");
        return;
    }

    QJsonObject packet = QJsonObject::fromVariantMap(messageData);
    packet["sender"] = currentUsername;
    packet["target"] = targetUser;
    packet["messageId"] = msgId;
    if (!packet.contains("timestamp") || packet.value("timestamp").toInteger() == 0) {
        packet["timestamp"] = QDateTime::currentSecsSinceEpoch();
    }
    if (!packet.contains("content") && packet.contains("text")) {
        packet["content"] = packet.value("text");
    }

    QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);

    QJsonObject payload;
    payload["from_device_id"] = deviceId;
    payload["to_username"] = targetUser;
    payload["ciphertext"] = QString::fromLatin1(packetBytes.toBase64());
    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_RELAY_SEND, postData, [this, targetUser, msgId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        bool ok = (error == QNetworkReply::NoError);
        QString errorMsg = ok ? "" : (errStr.isEmpty() ? "Network relay transmission failed" : errStr);
        if (!ok) {
            qDebug() << "[RelayService] sendRelayMessage failed for" << targetUser << "Error:" << errStr << "Status:" << statusCode << "Response:" << data;
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

    m_transport->get(Constants::EP_RELAY_POLL, params, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        m_isPollingActive = false;

        if (statusCode == 401) {
            handle401Error();
            return;
        }

        if (error != QNetworkReply::NoError) {
            return;
        }

        // Reset retry counter on successful connection
        m_retry401Count = 0;

        auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.object().contains("messages")) {
            return;
        }

        QJsonArray messages = doc.object().value("messages").toArray();
        for (const QJsonValue &val : messages) {
            QJsonObject obj = val.toObject();
            qint64 msgId = obj.value("id").toInteger();

            // Skip message if already processed in this session
            if (m_processedMessageIds.find(msgId) != m_processedMessageIds.end()) {
                acknowledgeMessage(msgId);
                continue;
            }
            m_processedMessageIds.insert(msgId);

            QString base64Cipher = obj.value("ciphertext").toString();
            qint64 timestamp = obj.value("timestamp").toInteger();

            QByteArray decodedBytes = QByteArray::fromBase64(base64Cipher.toLatin1());
            QString textContent = QString::fromUtf8(decodedBytes);
            QString sender = "Anonymous";
            QString target = "general";
            QString type = "text";
            QString mediaUrl = "";
            QString fileName = "";
            qint64 fileSize = 0;
            int duration = 0;
            QVariantList waveform;
            QString messageUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

            // Parse structured JSON packet if present
            auto packetDoc = QJsonDocument::fromJson(decodedBytes);
            QVariantMap richMap;
            if (!packetDoc.isNull() && packetDoc.isObject()) {
                QJsonObject packetObj = packetDoc.object();
                richMap = packetObj.toVariantMap();
                if (packetObj.contains("sender")) {
                    sender = packetObj.value("sender").toString();
                }
                if (packetObj.contains("target")) {
                    target = packetObj.value("target").toString();
                }
                if (packetObj.contains("content")) {
                    textContent = packetObj.value("content").toString();
                } else if (packetObj.contains("text")) {
                    textContent = packetObj.value("text").toString();
                }
                if (packetObj.contains("type")) {
                    type = packetObj.value("type").toString();
                }
                if (packetObj.contains("mediaUrl")) {
                    mediaUrl = packetObj.value("mediaUrl").toString();
                }
                if (packetObj.contains("fileName")) {
                    fileName = packetObj.value("fileName").toString();
                }
                if (packetObj.contains("fileSize")) {
                    fileSize = packetObj.value("fileSize").toInteger();
                }
                if (packetObj.contains("duration")) {
                    duration = static_cast<int>(packetObj.value("duration").toInteger());
                }
                if (packetObj.contains("waveform")) {
                    waveform = packetObj.value("waveform").toArray().toVariantList();
                }
                if (packetObj.contains("messageId")) {
                    messageUuid = packetObj.value("messageId").toString();
                }
                if (packetObj.contains("timestamp") && packetObj.value("timestamp").toInteger() > 0) {
                    timestamp = packetObj.value("timestamp").toInteger();
                }
            } else {
                richMap["content"] = textContent;
                richMap["text"] = textContent;
                richMap["type"] = "text";
                richMap["sender"] = sender;
                richMap["target"] = target;
            }

            richMap["sender"] = sender;
            richMap["target"] = target;
            richMap["text"] = textContent;
            richMap["type"] = type;
            richMap["mediaUrl"] = mediaUrl;
            richMap["fileName"] = fileName;
            richMap["fileSize"] = fileSize;
            richMap["duration"] = duration;
            richMap["waveform"] = waveform;
            richMap["messageId"] = messageUuid;
            richMap["timestamp"] = timestamp;

            emit incomingRelayMessageReceived(sender, target, textContent, timestamp);
            emit incomingRichMessageReceived(richMap);

            // Acknowledge receipt to remove message from server queue
            acknowledgeMessage(msgId);
        }
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
    m_transport->post(Constants::EP_RELAY_ACK, postData, [](int, const QByteArray&, QNetworkReply::NetworkError, const QString&) {});
}

} // namespace Services
} // namespace Avila
