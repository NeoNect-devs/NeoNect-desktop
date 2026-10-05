#pragma once

#include <QObject>
#include <QtTest>

class TestProductionE2EE : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testOutgoingProductionPath();
    void testFirstMessageProductionPath();
    void testFirstMessageFailures();
    void testApplicationIntegrationFirstMessage();
    void testApplicationIntegrationIncomingPath();
    void testBidirectionalEstablishedSession();
    void testIdentityRestart();
    void testSessionRestart();
    void testPreKeyRestart();
    void testSkippedKeyRestart();
    void testLogoutReloginIntegration();
};
