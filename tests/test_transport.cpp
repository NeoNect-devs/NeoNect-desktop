#include "test_transport.h"
#include <QSslSocket>
#include <QSignalSpy>
#include <QFile>

namespace NeoNect {
namespace Transport {

// Basic frame assembly for tests
QByteArray makeFrame(quint8 opcode, const QByteArray &data, bool fin = true) {
    QByteArray frame;
    frame.append(static_cast<char>((fin ? 0x80 : 0x00) | (opcode & 0x0F)));
    quint64 size = data.size();
    if (size <= 125) {
        frame.append(static_cast<char>(size));
    } else if (size <= 65535) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((size >> 8) & 0xFF));
        frame.append(static_cast<char>(size & 0xFF));
    } else {
        frame.append(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) {
            frame.append(static_cast<char>((size >> (i * 8)) & 0xFF));
        }
    }
    // Server frames are NOT masked
    frame.append(data);
    return frame;
}

void TestTransport::initTestCase() {
    QString certPath = QString(QT_TESTCASE_SOURCEDIR) + "/tests/certs/server.crt";
    QFile certFile(certPath);
    certFile.open(QIODevice::ReadOnly);
    m_cert = QSslCertificate(certFile.readAll());

    QString keyPath = QString(QT_TESTCASE_SOURCEDIR) + "/tests/certs/server.key";
    QFile keyFile(keyPath);
    keyFile.open(QIODevice::ReadOnly);
    m_key = QSslKey(keyFile.readAll(), QSsl::Rsa);

    QVERIFY(!m_cert.isNull());
    QVERIFY(!m_key.isNull());
}

void TestTransport::cleanupTestCase() {}
void TestTransport::init() {}
void TestTransport::cleanup() {
    stopServer();
}

class SslTestServer : public QTcpServer {
public:
    explicit SslTestServer(QObject* parent = nullptr) : QTcpServer(parent) {}

protected:
    void incomingConnection(qintptr socketDescriptor) override {
        QSslSocket* sslSocket = new QSslSocket(this);
        sslSocket->setSocketDescriptor(socketDescriptor);
        addPendingConnection(sslSocket);
    }
};

void TestTransport::setupServer(bool ssl) {
    if (ssl) {
        m_server = new SslTestServer(this);
    } else {
        m_server = new QTcpServer(this);
    }
    m_server->listen(QHostAddress::LocalHost, 0);
    m_serverPort = m_server->serverPort();
}

void TestTransport::stopServer() {
    if (m_serverConnection) {
        m_serverConnection->close();
        m_serverConnection->deleteLater();
        m_serverConnection = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

QByteArray TestTransport::generateHandshakeResponse() {
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n";
}

void TestTransport::startClient(WebSocketClient* client, bool ssl) {
    setupServer(ssl);

    QString url = ssl ? QString("wss://127.0.0.1:%1").arg(m_serverPort) : QString("ws://127.0.0.1:%1").arg(m_serverPort);

    client->open(url, "test-device", "test-token");
    QTest::qWait(10);

    QTRY_VERIFY_WITH_TIMEOUT(m_server->hasPendingConnections(), 2000);

    if (ssl) {
        QSslSocket* sslSocket = static_cast<QSslSocket*>(m_server->nextPendingConnection());

        sslSocket->setLocalCertificate(m_cert);
        sslSocket->setPrivateKey(m_key);
        sslSocket->startServerEncryption();
        QTRY_VERIFY_WITH_TIMEOUT(sslSocket->isEncrypted(), 2000);
        m_serverConnection = sslSocket;
    } else {
        m_serverConnection = m_server->nextPendingConnection();
    }

    QVERIFY(m_serverConnection);
    QTRY_VERIFY_WITH_TIMEOUT(m_serverConnection->bytesAvailable() > 0, 2000);
    m_serverConnection->readAll(); // Read handshake
    m_serverConnection->write(generateHandshakeResponse());

    QSignalSpy spy(client, &WebSocketClient::connected);
    QVERIFY(spy.wait(2000));
}

void TestTransport::testUnfragmentedText() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    m_serverConnection->write(makeFrame(0x01, "Hello"));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toString(), QString("Hello"));
}

void TestTransport::testUnfragmentedBinary() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::binaryMessageReceived);
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x02, QByteArray::fromHex("001122")));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toByteArray(), QByteArray::fromHex("001122"));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(client.state(), WebSocketState::Connected);
}

void TestTransport::testFragmentedText() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    m_serverConnection->write(makeFrame(0x01, "Hel", false));
    m_serverConnection->write(makeFrame(0x00, "lo", true));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toString(), QString("Hello"));
}

void TestTransport::testFragmentedBinary() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::binaryMessageReceived);
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x02, QByteArray::fromHex("00"), false));
    m_serverConnection->write(makeFrame(0x00, QByteArray::fromHex("11"), true));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toByteArray(), QByteArray::fromHex("0011"));
    QCOMPARE(errorSpy.count(), 0);
}

void TestTransport::testContinuationSequence() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    m_serverConnection->write(makeFrame(0x01, "A", false));
    m_serverConnection->write(makeFrame(0x00, "B", false));
    m_serverConnection->write(makeFrame(0x00, "C", true));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toString(), QString("ABC"));
}

void TestTransport::testPingDuringFragmentation() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    m_serverConnection->write(makeFrame(0x01, "A", false));
    m_serverConnection->write(makeFrame(0x09, "ping"));
    QTRY_VERIFY_WITH_TIMEOUT(m_serverConnection->bytesAvailable() > 0, 1000);
    QByteArray pongResp = m_serverConnection->readAll();
    QVERIFY(pongResp.size() > 0);

    m_serverConnection->write(makeFrame(0x00, "B", true));

    QVERIFY(spy.wait(1000));
    QCOMPARE(spy.first().first().toString(), QString("AB"));
}

void TestTransport::testPong() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x0A, "pong"));

    QTest::qWait(500);
    QCOMPARE(errorSpy.count(), 0);
}

void TestTransport::testClose() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy spy(&client, &WebSocketClient::disconnected);
    m_serverConnection->write(makeFrame(0x08, ""));

    QVERIFY(spy.wait(1000));
    QCOMPARE(client.state(), WebSocketState::Disconnected);
}

void TestTransport::testInvalidContinuation() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x00, "orphan", true));

    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testInvalidNewDataDuringFragmentation() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x01, "part1", false));
    m_serverConnection->write(makeFrame(0x01, "part2", true));

    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testFragmentedControl() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x09, "ping", false));

    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testOversizedControl() {
    WebSocketClient client;
    startClient(&client);

    QByteArray bigPing(126, 'X');
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x09, bigPing, true));

    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testMaskedServerFrameRejected() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    QByteArray frame = makeFrame(0x01, "masked_payload", true);
    frame[1] = frame[1] | 0x80;
    frame.insert(2, QByteArray(4, ' '));

    m_serverConnection->write(frame);
    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testExact4MiBUnfragmented() {
    WebSocketClient client;
    startClient(&client);

    constexpr quint64 limit = 4 * 1024 * 1024;
    QByteArray data(static_cast<int>(limit), 'A');
    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x01, data, true));

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.first().first().toString().size(), static_cast<int>(limit));
    QCOMPARE(errorSpy.count(), 0);
}

void TestTransport::testExact4MiBFragmented() {
    WebSocketClient client;
    startClient(&client);

    constexpr quint64 limit = 4 * 1024 * 1024;
    constexpr int half = static_cast<int>(limit / 2);
    QByteArray part1(half, 'B');
    QByteArray part2(static_cast<int>(limit) - half, 'C');
    QSignalSpy spy(&client, &WebSocketClient::textMessageReceived);
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x01, part1, false));
    m_serverConnection->write(makeFrame(0x00, part2, true));

    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.first().first().toString().size(), static_cast<int>(limit));
    QCOMPARE(errorSpy.count(), 0);
}

void TestTransport::testMessageTooLarge() {
    WebSocketClient client;
    startClient(&client);

    constexpr quint64 limit = 4 * 1024 * 1024;
    QByteArray bigData(static_cast<int>(limit) + 1, 'X');
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x01, bigData, true));

    QVERIFY(errorSpy.wait(5000));
}

void TestTransport::testFragmentedMessageTooLarge() {
    WebSocketClient client;
    startClient(&client);

    constexpr quint64 limit = 4 * 1024 * 1024;
    constexpr int half = static_cast<int>(limit / 2);
    QByteArray part1(half, 'D');
    QByteArray part2(half + 1, 'E');
    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x01, part1, false));
    m_serverConnection->write(makeFrame(0x00, part2, true));

    QVERIFY(errorSpy.wait(5000));
}

void TestTransport::testMalformedFrame() {
    WebSocketClient client;
    startClient(&client);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    m_serverConnection->write(makeFrame(0x05, "unknown"));

    QVERIFY(errorSpy.wait(1000));
}

void TestTransport::testTlsValidationFailure() {
    WebSocketClient client;
    setupServer(true);

    QString url = QString("wss://127.0.0.1:%1").arg(m_serverPort);

    QSignalSpy errorSpy(&client, &WebSocketClient::errorOccurred);
    client.open(url, "test-device", "test-token");
    QTest::qWait(10);

    QTRY_VERIFY_WITH_TIMEOUT(m_server->hasPendingConnections(), 2000);
    QSslSocket* sslSocket = static_cast<QSslSocket*>(m_server->nextPendingConnection());
    sslSocket->setLocalCertificate(m_cert);
    sslSocket->setPrivateKey(m_key);
    sslSocket->startServerEncryption();

    QVERIFY(errorSpy.wait(2000));

    sslSocket->close();
    sslSocket->deleteLater();
}

void TestTransport::testTlsConnectionSuccess() {
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();
    auto certs = config.caCertificates();
    certs.append(m_cert);
    config.setCaCertificates(certs);
    QSslConfiguration::setDefaultConfiguration(config);

    WebSocketClient client;
    setupServer(true);

    QString url = QString("wss://127.0.0.1:%1").arg(m_serverPort);

    client.open(url, "test-device", "test-token");
    if (QSslSocket* clientSocket = client.findChild<QSslSocket*>()) {
        clientSocket->ignoreSslErrors();
    }
    QTest::qWait(10);

    QTRY_VERIFY_WITH_TIMEOUT(m_server->hasPendingConnections(), 2000);
    QSslSocket* sslSocket = static_cast<QSslSocket*>(m_server->nextPendingConnection());
    connect(sslSocket, &QSslSocket::sslErrors, [](const QList<QSslError> &errors){
        for (const auto& e : errors) qDebug() << "[Server SSL Error]" << e.errorString();
    });
    connect(sslSocket, &QAbstractSocket::errorOccurred, [](QAbstractSocket::SocketError e){
        qDebug() << "[Server Socket Error]" << e;
    });
    sslSocket->setLocalCertificate(m_cert);
    sslSocket->setPrivateKey(m_key);
    sslSocket->startServerEncryption();

    QTRY_VERIFY_WITH_TIMEOUT(sslSocket->isEncrypted(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(sslSocket->bytesAvailable() > 0, 2000);
    sslSocket->readAll();
    sslSocket->write(generateHandshakeResponse());

    QSignalSpy spy(&client, &WebSocketClient::connected);
    QVERIFY(spy.wait(2000));

    sslSocket->close();
    sslSocket->deleteLater();
}

} // namespace Transport
} // namespace NeoNect
