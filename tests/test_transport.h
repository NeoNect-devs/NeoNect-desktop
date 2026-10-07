#pragma once

#include <QObject>
#include <QtTest>
#include <QTcpServer>
#include <QSslServer>
#include <QSslKey>
#include "transport/websocketclient.h"

namespace NeoNect {
namespace Transport {

class TestTransport : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testUnfragmentedText();
    void testUnfragmentedBinary();
    void testFragmentedText();
    void testFragmentedBinary();
    void testContinuationSequence();
    void testPingDuringFragmentation();
    void testPong();
    void testClose();
    void testInvalidContinuation();
    void testInvalidNewDataDuringFragmentation();
    void testFragmentedControl();
    void testOversizedControl();
    void testMaskedServerFrameRejected();
    void testExact4MiBUnfragmented();
    void testExact4MiBFragmented();
    void testMessageTooLarge();
    void testDynamicMaxMessageSize();
    void testFragmentedMessageTooLarge();
    void testMalformedFrame();
    void testTlsValidationFailure();
    void testTlsConnectionSuccess();

private:
    void setupServer(bool ssl = false);
    void stopServer();
    void startClient(WebSocketClient* client, bool ssl = false);
    QByteArray generateHandshakeResponse();

    QTcpServer* m_server = nullptr;
    quint16 m_serverPort = 0;
    QTcpSocket* m_serverConnection = nullptr;
    QSslCertificate m_cert;
    QSslKey m_key;
};

} // namespace Transport
} // namespace NeoNect
