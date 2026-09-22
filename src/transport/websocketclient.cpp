// src/transport/websocketclient.cpp
#include "websocketclient.h"
#include <QDebug>
#include <QCryptographicHash>
#include <cstring>

namespace NeoNect {
namespace Transport {

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QSslSocket(this);
    m_socket->ignoreSslErrors(); // For local/self-signed certs in test/dev environments

    connect(m_socket, &QSslSocket::connected, this, &WebSocketClient::onSocketConnected);
    connect(m_socket, &QSslSocket::disconnected, this, &WebSocketClient::onSocketDisconnected);
    connect(m_socket, &QSslSocket::readyRead, this, &WebSocketClient::onSocketReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &WebSocketClient::onSocketError);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &WebSocketClient::onReconnectTimer);
}

WebSocketClient::~WebSocketClient() {
    m_shouldBeConnected = false;
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
    if (m_socket) {
        if (m_state == WebSocketState::Connected) {
            sendFrame(0x08, QByteArray()); // Close frame
        }
        m_socket->abort();
    }
}

void WebSocketClient::setState(WebSocketState newState) {
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(m_state);
    }
}

void WebSocketClient::open(const QString &serverUrl, const QString &deviceId, const QString &token) {
    m_serverUrl = serverUrl.trimmed();
    m_deviceId = deviceId.trimmed();
    m_token = token.trimmed();
    m_shouldBeConnected = true;
    m_reconnectAttempts = 0;

    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_readBuffer.clear();
    m_handshakeComplete = false;

    if (m_serverUrl.isEmpty() || m_deviceId.isEmpty()) {
        qWarning() << "[WebSocketClient] Cannot open: serverUrl or deviceId is empty";
        return;
    }

    setState(WebSocketState::Connecting);

    QUrl url(m_serverUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        url = QUrl("http://" + m_serverUrl);
    }

    bool isSsl = (url.scheme().toLower() == "https" || url.scheme().toLower() == "wss");
    QString host = url.host();
    if (host.isEmpty()) {
        host = "localhost";
    }

    int port = url.port();
    if (port <= 0) {
        port = isSsl ? 443 : 8080;
    }

    qDebug() << "[WebSocketClient] Connecting to" << host << "port" << port << "(SSL:" << isSsl << ")";

    if (isSsl) {
        m_socket->setPeerVerifyName(host);
        m_socket->connectToHostEncrypted(host, static_cast<quint16>(port));
    } else {
        m_socket->connectToHost(host, static_cast<quint16>(port));
    }
}

void WebSocketClient::close() {
    m_shouldBeConnected = false;
    m_reconnectAttempts = 0;
    m_deviceId.clear();
    m_token.clear();
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    if (m_state == WebSocketState::Connected) {
        setState(WebSocketState::Closing);
        sendFrame(0x08, QByteArray());
        m_socket->disconnectFromHost();
    } else if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    } else {
        setState(WebSocketState::Disconnected);
    }
}

void WebSocketClient::onSocketConnected() {
    qDebug() << "[WebSocketClient] Socket connected, sending HTTP upgrade request";
    setState(WebSocketState::Handshaking);
    performHandshake();
}

void WebSocketClient::performHandshake() {
    QUrl url(m_serverUrl);
    if (!url.isValid() || url.scheme().isEmpty()) {
        url = QUrl("http://" + m_serverUrl);
    }

    bool isSsl = (url.scheme().toLower() == "https" || url.scheme().toLower() == "wss");
    QString host = url.host();
    if (host.isEmpty()) host = "localhost";
    int port = url.port();

    // Standard RFC 7230 §5.4: Omit default ports (80/443) from Host header
    QString hostHeader;
    if (port > 0 && ((isSsl && port != 443) || (!isSsl && port != 80))) {
        hostHeader = host.contains(':') ? QString("[%1]:%2").arg(host).arg(port) : QString("%1:%2").arg(host).arg(port);
    } else {
        hostHeader = host;
    }

    QString origin = QString("%1://%2").arg(isSsl ? "https" : "http", host);
    if (port > 0 && ((isSsl && port != 443) || (!isSsl && port != 80))) {
        origin += QString(":%1").arg(port);
    }

    // 16-byte random nonce for Sec-WebSocket-Key
    QByteArray nonce(16, Qt::Uninitialized);
    QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(nonce.data()), 4);
    QString secKey = nonce.toBase64();

    QString path = QString("/api/v1/relay/ws?device_id=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(m_deviceId)));

    QString request = QString(
        "GET %1 HTTP/1.1\r\n"
        "Host: %2\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Origin: %3\r\n"
        "User-Agent: NeoNectDesktop/1.0\r\n"
        "Sec-WebSocket-Key: %4\r\n"
        "Sec-WebSocket-Version: 13\r\n"
    ).arg(path, hostHeader, origin, secKey);

    if (!m_token.isEmpty()) {
        request += QString("Authorization: Bearer %1\r\n").arg(m_token);
        request += QString("Cookie: neonect_sid=%1\r\n").arg(m_token);
    }
    request += "\r\n";

    m_socket->write(request.toUtf8());
    m_socket->flush();
}

void WebSocketClient::onSocketReadyRead() {
    m_readBuffer.append(m_socket->readAll());
    processIncomingData();
}

void WebSocketClient::processIncomingData() {
    if (!m_handshakeComplete) {
        // Look for end of HTTP headers: \r\n\r\n
        int headerEnd = m_readBuffer.indexOf("\r\n\r\n");
        if (headerEnd == -1) {
            headerEnd = m_readBuffer.indexOf("\n\n");
            if (headerEnd == -1) {
                return; // Wait for complete HTTP response headers
            }
        }

        int headersLen = (m_readBuffer.indexOf("\r\n\r\n") != -1) ? (headerEnd + 4) : (headerEnd + 2);
        QByteArray headerData = m_readBuffer.left(headerEnd);
        m_readBuffer.remove(0, headersLen);

        QString headerStr = QString::fromUtf8(headerData);
        QString firstLine = headerStr.section("\n", 0, 0).trimmed();

        if (firstLine.contains("101")) {
            qDebug() << "[WebSocketClient] WebSocket handshake successful:" << firstLine;
            m_handshakeComplete = true;
            m_reconnectAttempts = 0;
            setState(WebSocketState::Connected);
            emit connected();
        } else {
            qWarning() << "[WebSocketClient] WebSocket upgrade failed:" << firstLine << "\nResponse Headers:\n" << headerStr;
            emit errorOccurred("Handshake failed: " + firstLine);
            m_socket->disconnectFromHost();
            return;
        }
    }

    if (m_handshakeComplete) {
        processFrames();
    }
}

void WebSocketClient::processFrames() {
    while (m_readBuffer.size() >= 2) {
        const quint8 *ptr = reinterpret_cast<const quint8*>(m_readBuffer.constData());
        quint8 b0 = ptr[0];
        quint8 b1 = ptr[1];

        quint8 opcode = b0 & 0x0F;
        bool masked = (b1 & 0x80) != 0;
        quint64 payloadLen = (b1 & 0x7F);
        int headerLen = 2;

        if (payloadLen == 126) {
            if (m_readBuffer.size() < 4) return;
            payloadLen = (static_cast<quint64>(ptr[2]) << 8) | ptr[3];
            headerLen += 2;
        } else if (payloadLen == 127) {
            if (m_readBuffer.size() < 10) return;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLen = (payloadLen << 8) | ptr[2 + i];
            }
            headerLen += 8;
        }

        if (masked) {
            if (m_readBuffer.size() < headerLen + 4) return;
            headerLen += 4;
        }

        quint64 totalFrameLen = headerLen + payloadLen;
        if (static_cast<quint64>(m_readBuffer.size()) < totalFrameLen) {
            return; // Incomplete frame, wait for more data
        }

        QByteArray payload;
        if (payloadLen > 0) {
            payload = m_readBuffer.mid(masked ? (headerLen - 4 + 4) : headerLen, static_cast<int>(payloadLen));
            if (masked) {
                const quint8 *maskKey = reinterpret_cast<const quint8*>(m_readBuffer.constData() + headerLen - 4);
                for (int i = 0; i < payload.size(); ++i) {
                    payload[i] = payload[i] ^ maskKey[i % 4];
                }
            }
        }

        m_readBuffer.remove(0, static_cast<int>(totalFrameLen));

        // Handle frame by opcode
        switch (opcode) {
        case 0x01: // Text frame
            emit textMessageReceived(QString::fromUtf8(payload));
            break;

        case 0x08: // Close frame
            qDebug() << "[WebSocketClient] Server sent close frame";
            sendFrame(0x08, QByteArray());
            m_socket->disconnectFromHost();
            break;

        case 0x09: // Ping frame -> respond with Pong containing identical payload
            sendPong(payload);
            break;

        case 0x0A: // Pong frame
            // Received response to client keepalive ping
            break;

        default:
            break;
        }
    }
}

void WebSocketClient::sendFrame(quint8 opcode, const QByteArray &data) {
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray frame;
    quint8 b0 = 0x80 | (opcode & 0x0F); // FIN = 1
    frame.append(static_cast<char>(b0));

    quint64 size = static_cast<quint64>(data.size());
    if (size <= 125) {
        frame.append(static_cast<char>(0x80 | (size & 0x7F))); // MASK bit = 1
    } else if (size <= 65535) {
        frame.append(static_cast<char>(0x80 | 126));
        frame.append(static_cast<char>((size >> 8) & 0xFF));
        frame.append(static_cast<char>(size & 0xFF));
    } else {
        frame.append(static_cast<char>(0x80 | 127));
        for (int i = 7; i >= 0; --i) {
            frame.append(static_cast<char>((size >> (i * 8)) & 0xFF));
        }
    }

    // Client frames must be masked (RFC 6455 5.1)
    quint32 maskVal = QRandomGenerator::global()->generate();
    char mask[4];
    std::memcpy(mask, &maskVal, 4);
    frame.append(mask, 4);

    for (int i = 0; i < data.size(); ++i) {
        frame.append(static_cast<char>(data[i] ^ mask[i % 4]));
    }

    m_socket->write(frame);
    m_socket->flush();
}

void WebSocketClient::sendPong(const QByteArray &data) {
    sendFrame(0x0A, data);
}

void WebSocketClient::sendPing(const QByteArray &data) {
    sendFrame(0x09, data);
}

void WebSocketClient::sendTextMessage(const QString &text) {
    sendFrame(0x01, text.toUtf8());
}

void WebSocketClient::onSocketDisconnected() {
    qDebug() << "[WebSocketClient] Disconnected from server (was" << (m_state == WebSocketState::Connected ? "connected" : "connecting") << ")";
    bool wasConnected = (m_state == WebSocketState::Connected);
    setState(WebSocketState::Disconnected);
    m_handshakeComplete = false;
    m_readBuffer.clear();

    if (wasConnected) {
        emit disconnected();
    }

    if (m_shouldBeConnected) {
        int backoffMs = qMin(1000 * (1 << qMin(m_reconnectAttempts, 4)), 15000);
        m_reconnectAttempts++;
        qDebug() << "[WebSocketClient] Scheduling reconnect in" << backoffMs << "ms (attempt" << m_reconnectAttempts << ")";
        m_reconnectTimer->start(backoffMs);
    }
}

void WebSocketClient::onSocketError(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError);
    QString err = m_socket ? m_socket->errorString() : "Unknown socket error";
    qWarning() << "[WebSocketClient] Socket error:" << err;
    emit errorOccurred(err);
}

void WebSocketClient::onReconnectTimer() {
    if (m_shouldBeConnected && m_state == WebSocketState::Disconnected) {
        qDebug() << "[WebSocketClient] Attempting automatic reconnection...";
        int savedAttempts = m_reconnectAttempts;
        open(m_serverUrl, m_deviceId, m_token);
        m_reconnectAttempts = savedAttempts;
    }
}

} // namespace Transport
} // namespace NeoNect
