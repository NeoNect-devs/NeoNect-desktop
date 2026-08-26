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
};
