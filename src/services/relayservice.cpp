#include "relayservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>

namespace NeoNect {
namespace Services {

static QString normalizeLocalFilePath(const QString &rawPath) {
    if (rawPath.isEmpty()) return QString();
    QString path = rawPath;
    QUrl url(rawPath);
    if (url.isLocalFile()) {
        path = url.toLocalFile();
    } else if (path.startsWith("file:///")) {
        path = path.mid(8);
    } else if (path.startsWith("file://")) {
        path = path.mid(7);
    }
#ifdef _WIN32
    if (path.startsWith("/") && path.length() >= 3 && path.at(2) == ':') {
        path = path.mid(1);
    }
#endif
    return path;
}

RelayService::RelayService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           std::shared_ptr<Transport::IIncomingEnvelopeHandler> envelopeHandler,
                           QObject *parent)
    : QObject(parent),
      m_transport(std::move(transport)),
      m_storage(std::move(storage)),
      m_envelopeHandler(std::move(envelopeHandler)),
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
    qWarning() << "[RelayService] sendDomainMessage is deprecated and disconnected in Step 9. Use sendEncryptedEnvelope.";
    emit secureMessageTransmitted(msg.conversationId, false);
}

void RelayService::sendEncryptedEnvelope(const QString &recipientUsername,
                                         const QString &recipientDeviceId,
                                         const QString &messageId,
                                         const QByteArray &envelopeBytes) {
    QString token = m_storage->authToken();
    QString deviceId = m_storage->deviceId().trimmed();
    
    if (token.isEmpty() || recipientUsername.isEmpty()) {
        qDebug() << "[RelayService] sendEncryptedEnvelope failed: Missing token or recipient";
        emit secureMessageTransmitted(recipientUsername, false);
        emit messageTransmissionStatus(recipientUsername, messageId, false, "Missing session token or recipient");
        return;
    }

    if (envelopeBytes.size() > Constants::WS_MAX_MESSAGE_SIZE) {
        qDebug() << "[RelayService] sendEncryptedEnvelope failed: envelope exceeds max size";
        emit secureMessageTransmitted(recipientUsername, false);
        emit messageTransmissionStatus(recipientUsername, messageId, false, "Envelope exceeds maximum payload size");
        return;
    }

    QJsonObject payload;
    payload["from_device_id"] = deviceId;
    payload["protocol_version"] = 2;
    payload["ciphertext"] = QString::fromLatin1(envelopeBytes.toBase64());
    payload["timestamp"] = QDateTime::currentSecsSinceEpoch();
    
    if (!messageId.isEmpty()) {
        payload["message_id"] = messageId;
    }
    if (!recipientDeviceId.isEmpty()) {
        payload["recipient_device_id"] = recipientDeviceId;
    } else {
        payload["recipient_device_id"] = "default_device"; // Fallback, though should not happen in correct flow
    }

    QByteArray postData = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qDebug() << "[RelayService] Transmitting opaque envelope to:" << recipientUsername << "deviceId:" << recipientDeviceId << "msgId:" << messageId;

    m_transport->post(Constants::EP_RELAY_SEND, postData, this, [this, recipientUsername, messageId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        bool ok = (error == QNetworkReply::NoError || statusCode == 200 || statusCode == 201);
        QString errorMsg = ok ? "" : (errStr.isEmpty() ? "Network relay transmission failed" : errStr);
        if (ok) {
            qDebug() << "[RelayService] sendEncryptedEnvelope succeeded for" << recipientUsername;
        } else {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("error")) {
                errorMsg = doc.object().value("error").toString();
            } else if (statusCode == 422) {
                errorMsg = "Recipient has no active registered devices";
            } else if (statusCode == 404) {
                errorMsg = "Recipient user not found";
            } else if (statusCode == 401) {
                errorMsg = "Unauthorized: Session expired or invalid token";
            } else if (statusCode == 403) {
                errorMsg = "Forbidden: Cannot message user without mutual friendship";
            }
            qDebug() << "[RelayService] sendEncryptedEnvelope failed for" << recipientUsername << "Error:" << errorMsg << "Status:" << statusCode;
            if (statusCode == 401) {
                handle401Error();
            }
        }
        emit secureMessageTransmitted(recipientUsername, ok);
        emit messageTransmissionStatus(recipientUsername, messageId, ok, errorMsg);
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
            processIncomingRelayItem(msgObj);
        }
    });
}

void RelayService::onWebSocketMessageReceived(const QString &text) {
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject msgObj = doc.object();
    qDebug() << "[RelayService] WebSocket real-time delivery received. MsgId:" << msgObj.value("id").toInteger();
    processIncomingRelayItem(msgObj);
}

void RelayService::processIncomingRelayItem(const QJsonObject &msgObj) {
    qint64 msgId = msgObj.value("id").toInteger();
    QString base64Cipher = msgObj.value("ciphertext").toString();

    if (m_processedMessageIds.find(msgId) != m_processedMessageIds.end()) {
        acknowledgeMessage(msgId);
        return;
    }

    if (base64Cipher.isEmpty()) {
        acknowledgeMessage(msgId);
        return;
    }

    QByteArray envelopeBytes = QByteArray::fromBase64(base64Cipher.toLatin1());
    
    if (envelopeBytes.size() > Constants::WS_MAX_MESSAGE_SIZE) {
        qDebug() << "[RelayService] Incoming envelope exceeds maximum payload size limit.";
        acknowledgeMessage(msgId); // Discard oversized message
        return;
    }

    if (m_envelopeHandler) {
        Transport::TransportMetadata metadata;
        metadata.messageId = msgId;
        // The server might send these fields; fallback to empty if missing
        metadata.senderDeviceId = msgObj.value("sender_device_id").toString();
        metadata.recipientDeviceId = msgObj.value("recipient_device_id").toString();

        auto result = m_envelopeHandler->handleEnvelope(envelopeBytes, metadata);
        if (result.success) {
            // ACK only after successful acceptance boundary
            acknowledgeMessage(msgId);
            m_processedMessageIds.insert(msgId);
        } else {
            // Log rejection, but do not retry malformed envelopes if they are fundamentally invalid.
            qDebug() << "[RelayService] Envelope processing failed:" << result.message;
            if (result.message == "invalid envelope" || result.message.contains("malformed", Qt::CaseInsensitive)) {
                // If the message is irreversibly malformed, just ACK it to stop retry loops
                acknowledgeMessage(msgId);
                m_processedMessageIds.insert(msgId);
            }
        }
    } else {
        qWarning() << "[RelayService] No envelope handler registered. Dropping packet.";
    }
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

void RelayService::setEnvelopeHandler(std::shared_ptr<Transport::IIncomingEnvelopeHandler> handler) {
    m_envelopeHandler = std::move(handler);
}

} // namespace Services
} // namespace NeoNect
