// src/transport/websocketclient.h
#pragma once

#include <QObject>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>
#include <QByteArray>
#include <QString>
#include <QRandomGenerator>

namespace NeoNect {
namespace Transport {

enum class WebSocketState {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    Closing
};

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient() override;

    void open(const QString &serverUrl, const QString &deviceId, const QString &token);
    void close();

    bool isConnected() const { return m_state == WebSocketState::Connected; }
    WebSocketState state() const { return m_state; }

    void sendTextMessage(const QString &text);
    void sendPing(const QByteArray &data = QByteArray());

signals:
    void connected();
    void disconnected();
    void textMessageReceived(const QString &message);
    void errorOccurred(const QString &error);
    void stateChanged(WebSocketState newState);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onReconnectTimer();

private:
    void setState(WebSocketState newState);
    void performHandshake();
    void processIncomingData();
    void processFrames();
    void sendPong(const QByteArray &data);
    void sendFrame(quint8 opcode, const QByteArray &data);

    QSslSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    QString m_serverUrl;
    QString m_deviceId;
    QString m_token;
    QString m_expectedAcceptKey;

    WebSocketState m_state{WebSocketState::Disconnected};
    bool m_shouldBeConnected{false};
    int m_reconnectAttempts{0};

    QByteArray m_readBuffer;
    bool m_handshakeComplete{false};
};

} // namespace Transport
} // namespace NeoNect
