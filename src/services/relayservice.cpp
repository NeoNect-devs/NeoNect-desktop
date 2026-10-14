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
                           std::shared_ptr<Storage::ICapabilitiesRepository> capabilities,
                           std::shared_ptr<Transport::IIncomingEnvelopeHandler> envelopeHandler,
                           QObject *parent)
    : QObject(parent),
      m_transport(std::move(transport)),
      m_storage(std::move(storage)),
      m_capabilities(std::move(capabilities)),
      m_envelopeHandler(std::move(envelopeHandler)),
      m_wsClient(std::make_unique<Transport::WebSocketClient>(this)),
      m_pollTimer(new QTimer(this)),
      m_retryPollTimer(new QTimer(this)) {

    m_pollTimer->setInterval(Constants::RELAY_POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, &RelayService::pollPendingMessages);

    m_retryPollTimer->setSingleShot(true);
    m_retryPollTimer->setInterval(250);
    connect(m_retryPollTimer, &QTimer::timeout, this, &RelayService::pollPendingMessages);

    connect(this, &RelayService::serverConnected, this, [this]() {
        m_isWsConnected = true;
    });

    connect(this, &RelayService::serverDisconnected, this, [this]() {
        m_isWsConnected = false;
    });

    connect(m_wsClient.get(), &Transport::WebSocketClient::connected, this, [this]() {
        qDebug() << "[RelayService] Connected to server WebSocket relay. Online status active.";
        emit serverConnected();
        pollPendingMessages();
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

    if (m_wsClient && m_capabilities) {
        auto cap = m_capabilities->maxEnvelopeBytes();
        if (cap.has_value()) {
            // Reconcile transport limit with payload: Base64 expansion is exactly 4/3 of the raw bytes.
            // We add a 512-byte margin for the JSON wrapper overhead (DeliveryMessage fields).
            quint64 requiredWsSize = ((static_cast<quint64>(cap.value()) + 2) / 3) * 4 + 512;
            m_wsClient->setMaxMessageSize(qMax(Constants::WS_MAX_MESSAGE_SIZE, requiredWsSize));
        }
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
    ++m_pollGeneration;
    m_isPollingActive = false;
    m_repollRequested = false;
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }
    if (m_retryPollTimer && m_retryPollTimer->isActive()) {
        m_retryPollTimer->stop();
    }
    if (m_wsClient) {
        m_wsClient->close();
    }
    m_isWsConnected = false;
    m_inFlightMessageIds.clear();
    m_processedMessageIds.clear();
    m_processedMessageOrder.clear();
    m_failedMessageAttempts.clear();
    emit serverDisconnected();
}

void RelayService::recordProcessedMessageId(qint64 messageId) {
    if (messageId <= 0) {
        return;
    }
    auto [it, inserted] = m_processedMessageIds.insert(messageId);
    if (!inserted) {
        return;
    }
    m_processedMessageOrder.push_back(messageId);
    while (m_processedMessageOrder.size() > kMaxProcessedMessageIds) {
        const qint64 oldest = m_processedMessageOrder.front();
        m_processedMessageOrder.pop_front();
        m_processedMessageIds.erase(oldest);
    }
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
    if (msg.conversationId == "dms:saved-messages" || msg.conversationId == "saved-messages") {
        emit secureMessageTransmitted(msg.conversationId, true);
        emit messageTransmissionStatus(msg.conversationId, msg.id, true, QString());
        return;
    }
    qWarning() << "[RelayService] sendDomainMessage is deprecated and disconnected in Step 9. Use sendEncryptedEnvelope.";
    emit secureMessageTransmitted(msg.conversationId, false);
}

void RelayService::sendEncryptedEnvelope(const QString &recipientUsername,
                                         const QString &recipientDeviceId,
                                         const QString &messageId,
                                         const QByteArray &envelopeBytes) {
    if (recipientUsername == "saved-messages" || recipientUsername == "dms:saved-messages") {
        emit secureMessageTransmitted(recipientUsername, true);
        emit messageTransmissionStatus(recipientUsername, messageId, true, QString());
        return;
    }

    QString token = m_storage->authToken();
    QString deviceId = m_storage->deviceId().trimmed();

    if (token.isEmpty() || recipientUsername.isEmpty()) {
        qDebug() << "[RelayService] sendEncryptedEnvelope failed: Missing token or recipient";
        emit secureMessageTransmitted(recipientUsername, false);
        emit messageTransmissionStatus(recipientUsername, messageId, false, "Missing session token or recipient");
        return;
    }

    if (!m_capabilities) {
        qDebug() << "[RelayService] sendEncryptedEnvelope failed: capabilities not initialized";
        emit secureMessageTransmitted(recipientUsername, false);
        emit messageTransmissionStatus(recipientUsername, messageId, false, "Internal error: Missing capabilities context");
        return;
    }

    auto maxEnvelope = m_capabilities->maxEnvelopeBytes();
    if (!maxEnvelope.has_value()) {
        qDebug() << "[RelayService] sendEncryptedEnvelope failed: unknown max envelope capability";
        emit secureMessageTransmitted(recipientUsername, false);
        emit messageTransmissionStatus(recipientUsername, messageId, false, "Server capabilities unknown or unsupported");
        return;
    }

    if (envelopeBytes.size() > maxEnvelope.value()) {
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
        bool ok = (error == QNetworkReply::NoError && (statusCode == 200 || statusCode == 201));
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
    if (token.isEmpty() || deviceId.isEmpty()) {
        return;
    }
    if (m_isPollingActive) {
        m_repollRequested = true;
        return;
    }

    m_isPollingActive = true;
    m_repollRequested = false;
    const uint64_t generation = m_pollGeneration;

    QMap<QString, QString> params;
    params["device_id"] = deviceId;

    m_transport->get(Constants::EP_RELAY_POLL, params, this, [this, generation, deviceId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        if (generation != m_pollGeneration || deviceId != m_storage->deviceId()) {
            return;
        }
        m_isPollingActive = false;

        if (statusCode == 401) {
            handle401Error();
            return;
        }

        if (error != QNetworkReply::NoError || (statusCode != 0 && statusCode != 200) || data.isEmpty()) {
            if (m_pollTimer->isActive() && m_retryPollTimer && !m_retryPollTimer->isActive()) {
                m_retryPollTimer->start();
            }
            return;
        }

        m_retry401Count = 0;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            if (m_pollTimer->isActive() && m_retryPollTimer && !m_retryPollTimer->isActive()) {
                m_retryPollTimer->start();
            }
            return;
        }

        QJsonObject root = doc.object();
        if (!root.contains("messages") || !root.value("messages").isArray()) return;

        QJsonArray messages = root.value("messages").toArray();
        QSet<QString> blockedSenderDevicesInPass;
        bool needsRetryPoll = false;

        for (const QJsonValue &val : messages) {
            if (!val.isObject()) continue;
            QJsonObject msgObj = val.toObject();
            const QString senderDeviceId = msgObj.value("sender_device_id").toString();
            if (!senderDeviceId.isEmpty() && blockedSenderDevicesInPass.contains(senderDeviceId)) {
                needsRetryPoll = true;
                continue;
            }

            const RelayItemOutcome outcome = processIncomingRelayItem(msgObj);
            if (generation != m_pollGeneration) {
                return;
            }

            if (outcome == RelayItemOutcome::TransientFailure) {
                needsRetryPoll = true;
                if (!senderDeviceId.isEmpty()) {
                    blockedSenderDevicesInPass.insert(senderDeviceId);
                }
            } else if (outcome == RelayItemOutcome::RetryableCryptoFailure) {
                needsRetryPoll = true;
            }
        }

        if (m_repollRequested) {
            m_repollRequested = false;
            QTimer::singleShot(0, this, &RelayService::pollPendingMessages);
        } else if (needsRetryPoll && m_pollTimer->isActive() && m_retryPollTimer && !m_retryPollTimer->isActive()) {
            m_retryPollTimer->start();
        }
    });
}

void RelayService::onWebSocketMessageReceived(const QString &text) {
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;

    QJsonObject msgObj = doc.object();
    qDebug() << "[RelayService] WebSocket real-time delivery received. MsgId:" << msgObj.value("id").toInteger();
    const RelayItemOutcome outcome = processIncomingRelayItem(msgObj);
    if ((outcome == RelayItemOutcome::TransientFailure || outcome == RelayItemOutcome::RetryableCryptoFailure) &&
        m_pollTimer->isActive() && m_retryPollTimer && !m_retryPollTimer->isActive()) {
        m_retryPollTimer->start();
    }
}

RelayService::RelayItemOutcome RelayService::processIncomingRelayItem(const QJsonObject &msgObj) {
    qint64 msgId = msgObj.value("id").toInteger();
    QString base64Cipher = msgObj.value("ciphertext").toString();

    if (m_processedMessageIds.find(msgId) != m_processedMessageIds.end()) {
        acknowledgeMessage(msgId);
        return RelayItemOutcome::ProcessedOrSkipped;
    }

    if (m_inFlightMessageIds.find(msgId) != m_inFlightMessageIds.end()) {
        return RelayItemOutcome::ProcessedOrSkipped;
    }

    if (base64Cipher.isEmpty()) {
        recordProcessedMessageId(msgId);
        acknowledgeMessage(msgId);
        return RelayItemOutcome::PermanentlyInvalidAcked;
    }

    QByteArray envelopeBytes = QByteArray::fromBase64(base64Cipher.toLatin1());

    if (!m_capabilities) {
        qDebug() << "[RelayService] Incoming envelope rejected: capabilities not initialized";
        return RelayItemOutcome::TransientFailure;
    }

    auto maxEnvelope = m_capabilities->maxEnvelopeBytes();
    if (!maxEnvelope.has_value()) {
        qDebug() << "[RelayService] Incoming envelope rejected: unknown max envelope capability";
        return RelayItemOutcome::TransientFailure;
    }

    if (envelopeBytes.size() > maxEnvelope.value()) {
        qDebug() << "[RelayService] Incoming envelope exceeds maximum payload size limit.";
        recordProcessedMessageId(msgId);
        acknowledgeMessage(msgId);
        return RelayItemOutcome::PermanentlyInvalidAcked;
    }

    if (m_envelopeHandler) {
        Transport::TransportMetadata metadata;
        metadata.messageId = msgId;
        metadata.senderUserId = msgObj.value("sender_id").toString();
        metadata.senderDeviceId = msgObj.value("sender_device_id").toString();
        metadata.recipientDeviceId = msgObj.value("recipient_device_id").toString();

        m_inFlightMessageIds.insert(msgId);
        auto result = m_envelopeHandler->handleEnvelope(envelopeBytes, metadata);
        m_inFlightMessageIds.erase(msgId);

        if (result.success) {
            m_failedMessageAttempts.erase(msgId);
            recordProcessedMessageId(msgId);
            acknowledgeMessage(msgId);
            return RelayItemOutcome::ProcessedOrSkipped;
        } else {
            qDebug() << "[RelayService] Envelope processing failed:" << result.message;

            const bool isTransientLocalFailure =
                result.message == "Local message persistence failed" ||
                result.message == "Storage context unavailable" ||
                result.message == "No local identity" ||
                result.message == "Failed to write chunk to disk" ||
                result.message == "Store not available" ||
                result.message == "Failed to open spool" ||
                result.message == "Seek failed" ||
                result.message == "Spool flush failed" ||
                result.message == "Spool durability failed" ||
                result.message == "DB commit failed";

            if (isTransientLocalFailure) {
                return RelayItemOutcome::TransientFailure;
            }

            int attempts = ++m_failedMessageAttempts[msgId];
            bool isPermanentlyInvalid =
                result.message == "envelope too short" ||
                result.message == "invalid envelope" ||
                result.message == "Unhandled envelope" ||
                result.message.contains("malformed", Qt::CaseInsensitive) ||
                result.message == "InitialEnvelope missing sender identity key" ||
                result.message == "Decrypted InitialEnvelope payload is not a valid RatchetEnvelope" ||
                result.message == "DoubleRatchet Receive failed on initial message" ||
                result.message == "Authenticated session user does not match payload senderId" ||
                result.message == "Invalid FILE_CHUNK envelope" ||
                result.message == "Sender cannot receive chunks" ||
                result.message == "Invalid chunk index" ||
                result.message == "Malformed chunk length" ||
                (attempts >= 2 &&
                 (result.message == "Ratchet payload decryption failed" ||
                  result.message == "DoubleRatchet Receive failed" ||
                  result.message == "InitialEnvelope decryption failed" ||
                  result.message == "X3DH respond failed" ||
                  result.message == "Session not found" ||
                  result.message == "No session found for incoming RatchetEnvelope" ||
                  result.message == "Sender identity verification failed" ||
                  result.message == "Remote identity key mismatch for existing session" ||
                  result.message == "Transfer not found" ||
                  result.message == "Transfer not active" ||
                  result.message == "Chunk authentication failed"));
            if (isPermanentlyInvalid) {
                m_failedMessageAttempts.erase(msgId);
                recordProcessedMessageId(msgId);
                acknowledgeMessage(msgId);
                return RelayItemOutcome::PermanentlyInvalidAcked;
            }
            if (result.message == "Sender identity verification failed") {
                return RelayItemOutcome::TransientFailure;
            }
            return RelayItemOutcome::RetryableCryptoFailure;
        }
    } else {
        qWarning() << "[RelayService] No envelope handler registered. Dropping packet.";
        return RelayItemOutcome::TransientFailure;
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
    const uint64_t generation = m_pollGeneration;
    m_transport->post(Constants::EP_RELAY_ACK, postData, this, [this, generation](int statusCode, const QByteArray&, QNetworkReply::NetworkError error, const QString&) {
        if (generation != m_pollGeneration) {
            return;
        }
        if (error != QNetworkReply::NoError || (statusCode != 0 && statusCode != 200)) {
            if (m_pollTimer && m_pollTimer->isActive() && m_retryPollTimer && !m_retryPollTimer->isActive()) {
                m_retryPollTimer->start();
            }
        }
    });
}

void RelayService::setEnvelopeHandler(std::shared_ptr<Transport::IIncomingEnvelopeHandler> handler) {
    m_envelopeHandler = std::move(handler);
}

void RelayService::setCapabilities(std::shared_ptr<Storage::ICapabilitiesRepository> capabilities) {
    m_capabilities = std::move(capabilities);
}

} // namespace Services
} // namespace NeoNect
