// src/services/relayservice.cpp
#include "relayservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
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

void RelayService::sendRelayMessage(const QString &toUsername, const QString &plainText) {
    QString targetUser = toUsername.trimmed().toLower();
    QString token = m_storage->authToken();
    QString currentUsername = m_storage->username();
    QString deviceId = m_storage->deviceId();

    if (token.isEmpty() || targetUser.isEmpty() || plainText.isEmpty()) {
        emit secureMessageTransmitted(targetUser, false);
        return;
    }

    QJsonObject packet;
    packet["sender"] = currentUsername;
    packet["target"] = targetUser;
    packet["content"] = plainText;
    packet["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);

    QJsonObject payload;
    payload["from_device_id"] = deviceId;
    payload["to_username"] = targetUser;
    payload["ciphertext"] = QString::fromLatin1(packetBytes.toBase64());
    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();

    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_RELAY_SEND, postData, [this, targetUser](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        bool ok = (error == QNetworkReply::NoError);
        if (!ok) {
            qDebug() << "[RelayService] sendRelayMessage failed for" << targetUser << "Error:" << errStr << "Status:" << statusCode << "Response:" << data;
            if (statusCode == 401 || data.contains("unauthorized")) {
                handle401Error();
            }
        }
        emit secureMessageTransmitted(targetUser, ok);
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

            // Parse structured JSON packet if present
            auto packetDoc = QJsonDocument::fromJson(decodedBytes);
            if (!packetDoc.isNull() && packetDoc.isObject()) {
                QJsonObject packetObj = packetDoc.object();
                if (packetObj.contains("sender")) {
                    sender = packetObj.value("sender").toString();
                }
                if (packetObj.contains("content")) {
                    textContent = packetObj.value("content").toString();
                }
            }

            emit incomingRelayMessageReceived(sender, textContent, timestamp);

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
