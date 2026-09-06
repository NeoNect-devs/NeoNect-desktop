// tests/test_services.h
#pragma once
#include <QObject>

class TestServices : public QObject {
    Q_OBJECT

private slots:
    void testAuthServiceFlow();
    void testDeviceServiceFlow();
    void testRelayServiceFlowAndDeduplication();
    void testFriendServiceFlow();
    void testNetworkManagerFacadeIntegration();
    void testTwoClientChatExchange();
    void testBookmarkConnectFlow();
    void testNotificationManagerFlow();
    void testCiphertextDoesNotContainPlaintext();
    void testRelayServiceTamperedMessageRejection();
    void testCallbackCannotReachDestroyedService();
    void testRequestCancellationOnServiceDestruction();
    void testPhase10ABackendProtocolCompliance();
};
