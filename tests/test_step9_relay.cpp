// tests/test_step9_relay.cpp
#include "test_step9_relay.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>

namespace NeoNect {
namespace Tests {

void TestStep9Relay::init() {
    m_transport = std::make_shared<Testing::MockHttpTransport>();
    m_storage = std::make_shared<Storage::SettingsRepository>("NeoNectTest_Step9");
    m_storage->clearSession();
    m_storage->setAuthToken("test-token");
    m_storage->setDeviceId("test-device-id");

    m_handler = std::make_shared<MockEnvelopeHandler>();
    m_capabilities = std::make_shared<Storage::CapabilitiesRepository>();
    m_capabilities->replace(std::nullopt, 4 * 1024 * 1024, std::nullopt, Storage::CapabilityState::VALID);
    m_relay = std::make_unique<Services::RelayService>(m_transport, m_storage, m_capabilities, m_handler);
}

void TestStep9Relay::cleanup() {
    m_relay.reset();
    m_handler.reset();
    m_storage->clearSession();
    m_storage.reset();
    m_transport.reset();
}

void TestStep9Relay::testSendOpaqueEnvelope() {
    QByteArray opaqueEnvelope = "fake_opaque_bytes";

    QByteArray capturedData;
    connect(m_transport.get(), &Testing::MockHttpTransport::rawRequestData, this, [&](const QByteArray &data) {
        capturedData = data;
    });

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-1", opaqueEnvelope);

    QVERIFY(!capturedData.isEmpty());
    QJsonDocument doc = QJsonDocument::fromJson(capturedData);
    QJsonObject obj = doc.object();

    QCOMPARE(obj["to_username"].toString(), QString("alice"));
    QCOMPARE(obj["to_device_id"].toString(), QString("alice-device-1"));
    QCOMPARE(obj["message_id"].toString(), QString("msg-uuid-1"));
    QCOMPARE(obj["ciphertext"].toString(), QString::fromLatin1(opaqueEnvelope.toBase64()));
}

void TestStep9Relay::testSendOversizedEnvelopeRejected() {
    QByteArray hugeEnvelope(4 * 1024 * 1024, 'A'); // 5 MB
    QSignalSpy spyStatus(m_relay.get(), &Services::RelayService::messageTransmissionStatus);

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-2", hugeEnvelope);

    QCOMPARE(spyStatus.count(), 1);
    QVariantList args = spyStatus.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("alice"));
    QCOMPARE(args.at(1).toString(), QString("msg-uuid-2"));
    QCOMPARE(args.at(2).toBool(), false); // Success = false
    QVERIFY(args.at(3).toString().contains("exceeds maximum payload size"));
}

void TestStep9Relay::testSendTransportFailureRetries() {
    m_transport->setSimulateNetworkError(true);

    QSignalSpy spyStatus(m_relay.get(), &Services::RelayService::messageTransmissionStatus);
    QByteArray opaqueEnvelope = "fake_opaque_bytes";

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-3", opaqueEnvelope);

    QCOMPARE(spyStatus.count(), 1);
    QVariantList args = spyStatus.takeFirst();
    QCOMPARE(args.at(2).toBool(), false);
}

void TestStep9Relay::testReceiveValidEnvelopePoll() {
    QJsonObject mockResponse;
    QJsonArray messages;
    QJsonObject msg;
    msg["id"] = 12345;
    msg["ciphertext"] = QString::fromLatin1(QByteArray("fake_opaque_bytes").toBase64());
    msg["sender_device_id"] = "bob-device-1";
    messages.append(msg);
    mockResponse["messages"] = messages;

    m_transport->setSimulatedResponse("/api/v1/relay/poll", QJsonDocument(mockResponse).toJson());

    m_handler->nextResult = NeoNect::VoidResult::fail("not accepted yet");
    m_relay->pollPendingMessages();

    QCOMPARE(m_handler->callCount, 1);
    QCOMPARE(m_handler->lastEnvelope, QByteArray("fake_opaque_bytes"));
    QCOMPARE(m_handler->lastMetadata.messageId, 12345LL);
    QCOMPARE(m_handler->lastMetadata.senderDeviceId, QString("bob-device-1"));

    m_handler->nextResult = NeoNect::VoidResult::ok(std::monostate{});
    m_relay->pollPendingMessages();
    QCOMPARE(m_handler->callCount, 2);
}

void TestStep9Relay::testReceiveMalformedEnvelope() {
    QJsonObject mockResponse;
    QJsonArray messages;
    QJsonObject msg;
    msg["id"] = 12346;
    msg["ciphertext"] = QString::fromLatin1(QByteArray("toolong").toBase64());
    messages.append(msg);
    mockResponse["messages"] = messages;

    m_transport->setSimulatedResponse("/api/v1/relay/poll", QJsonDocument(mockResponse).toJson());

    m_handler->nextResult = NeoNect::VoidResult::fail("invalid envelope");
    m_relay->pollPendingMessages();

    QCOMPARE(m_handler->callCount, 1);
}

void TestStep9Relay::testWebSocketAndPollShareBoundary() {
    QJsonObject msg;
    msg["id"] = 5555;
    msg["ciphertext"] = QString::fromLatin1(QByteArray("ws_bytes").toBase64());

    QString wsText = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));

    QMetaObject::invokeMethod(m_relay.get(), "onWebSocketMessageReceived", Q_ARG(QString, wsText));

    QCOMPARE(m_handler->callCount, 1);
    QCOMPARE(m_handler->lastEnvelope, QByteArray("ws_bytes"));
    QCOMPARE(m_handler->lastMetadata.messageId, 5555LL);
}

void TestStep9Relay::testDuplicateDeliveryHandled() {
    QJsonObject msg;
    msg["id"] = 7777;
    msg["ciphertext"] = QString::fromLatin1(QByteArray("dup_bytes").toBase64());

    QString wsText = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));

    m_handler->nextResult = NeoNect::VoidResult::ok(std::monostate{});
    QMetaObject::invokeMethod(m_relay.get(), "onWebSocketMessageReceived", Q_ARG(QString, wsText));
    QCOMPARE(m_handler->callCount, 1);

    QMetaObject::invokeMethod(m_relay.get(), "onWebSocketMessageReceived", Q_ARG(QString, wsText));
    QCOMPARE(m_handler->callCount, 1);
}

void TestStep9Relay::testErrorBehavior() {
    m_transport->setSimulateHttpError(401);

    QSignalSpy spyAuth(m_relay.get(), &Services::RelayService::deviceRegistrationRequested);
    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg", "data");

    QCOMPARE(spyAuth.count(), 1);
}

void TestStep9Relay::testCapabilitiesValidation() {
    m_capabilities->replace(std::nullopt, 100, std::nullopt, Storage::CapabilityState::VALID);
    QByteArray exactEnv(100, 'A');
    QByteArray overEnv(101, 'A');

    QSignalSpy spyStatus(m_relay.get(), &Services::RelayService::messageTransmissionStatus);

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-1", exactEnv);
    QCOMPARE(spyStatus.count(), 1);
    QCOMPARE(spyStatus.takeFirst().at(2).toBool(), true);

    spyStatus.clear();

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-2", overEnv);
    QCOMPARE(spyStatus.count(), 1);
    QVariantList args = spyStatus.takeFirst();
    QCOMPARE(args.at(2).toBool(), false);
    QVERIFY(args.at(3).toString().contains("exceeds maximum payload size"));
}

void TestStep9Relay::testUnknownCapabilitiesRejection() {
    m_capabilities->replace(std::nullopt, std::nullopt, std::nullopt, Storage::CapabilityState::UNKNOWN);
    QSignalSpy spyStatus(m_relay.get(), &Services::RelayService::messageTransmissionStatus);

    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-1", QByteArray("data"));
    QCOMPARE(spyStatus.count(), 1);
    QVERIFY(spyStatus.takeFirst().at(3).toString().contains("unknown or unsupported"));

    spyStatus.clear();
    m_capabilities->replace(std::nullopt, std::nullopt, std::nullopt, Storage::CapabilityState::UNSUPPORTED);
    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-2", QByteArray("data"));
    QCOMPARE(spyStatus.count(), 1);
    QVERIFY(spyStatus.takeFirst().at(3).toString().contains("unknown or unsupported"));

    spyStatus.clear();
    m_capabilities->replace(std::nullopt, std::nullopt, std::nullopt, Storage::CapabilityState::INVALID);
    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-3", QByteArray("data"));
    QCOMPARE(spyStatus.count(), 1);
    QVERIFY(spyStatus.takeFirst().at(3).toString().contains("unknown or unsupported"));
    spyStatus.clear();
    m_capabilities->replace(std::nullopt, std::nullopt, std::nullopt, Storage::CapabilityState::UNAVAILABLE);
    m_relay->sendEncryptedEnvelope("alice", "alice-device-1", "msg-uuid-4", QByteArray("data"));
    QCOMPARE(spyStatus.count(), 1);
    QVERIFY(spyStatus.takeFirst().at(3).toString().contains("unknown or unsupported"));
}

void TestStep9Relay::testDynamicWebSocketIntegration() {
    // 1. Establish capability
    quint64 capSize = 5 * 1024 * 1024; // 5 MB
    m_capabilities->replace(std::nullopt, capSize, std::nullopt, Storage::CapabilityState::VALID);

    // 2. Set credentials so RelayService establishes WS
    m_storage->setAuthToken("fake_token");
    m_storage->setDeviceId("fake_device");

    // 3. Trigger WS open
    m_relay->startPolling();

    // 4. Verify exact limit injected into WS client
    auto* wsClient = m_relay->findChild<Transport::WebSocketClient*>();
    QVERIFY(wsClient != nullptr);

    quint64 expectedWsLimit = ((capSize + 2) / 3) * 4 + 512;
    QCOMPARE(wsClient->maxMessageSize(), expectedWsLimit);
}

} // namespace Tests
} // namespace NeoNect
