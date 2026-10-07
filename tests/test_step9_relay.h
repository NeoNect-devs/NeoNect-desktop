#pragma once

#include <QObject>
#include <QtTest>
#include <memory>
#include "../src/services/relayservice.h"
#include "../src/transport/IIncomingEnvelopeHandler.h"
#include "mocks/mockhttptransport.h"
#include "../src/storage/capabilitiesrepository.h"
#include "../src/storage/settingsrepository.h"

namespace NeoNect {
namespace Tests {

class MockEnvelopeHandler : public Transport::IIncomingEnvelopeHandler {
public:
    int callCount = 0;
    QByteArray lastEnvelope;
    Transport::TransportMetadata lastMetadata;
    NeoNect::VoidResult nextResult = NeoNect::VoidResult::ok(std::monostate{});

    NeoNect::VoidResult handleEnvelope(const QByteArray &envelope, const Transport::TransportMetadata &metadata) override {
        callCount++;
        lastEnvelope = envelope;
        lastMetadata = metadata;
        return nextResult;
    }
};

class TestStep9Relay : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    // Send
    void testSendOpaqueEnvelope();
    void testSendOversizedEnvelopeRejected();
    void testCapabilitiesValidation();
    void testUnknownCapabilitiesRejection();
    void testDynamicWebSocketIntegration();
    void testSendTransportFailureRetries();

    // Receive
    void testReceiveValidEnvelopePoll();
    void testReceiveMalformedEnvelope();
    void testWebSocketAndPollShareBoundary();

    // Offline / Duplicate
    void testDuplicateDeliveryHandled();

    // Error Behavior
    void testErrorBehavior();

private:
    std::shared_ptr<Testing::MockHttpTransport> m_transport;
    std::shared_ptr<Storage::SettingsRepository> m_storage;
    std::shared_ptr<MockEnvelopeHandler> m_handler;
    std::shared_ptr<Storage::CapabilitiesRepository> m_capabilities;
    std::unique_ptr<Services::RelayService> m_relay;
};

} // namespace Tests
} // namespace NeoNect
