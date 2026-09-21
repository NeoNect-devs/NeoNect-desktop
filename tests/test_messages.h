#pragma once
#include <QObject>

class TestMessages : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void testSqliteInsertAndRetrieve();
    void testMessageOrdering();
    void testDuplicateServerIdHandling();
    void testRepositoryReopenPersistence();
    void testMessageServiceIntegration();
    void testMessageModelPopulation();
    void testTypingStatusTransmissionAndHandling();
    void testMessageDeliveryStatusTransitions();
    void testMediaTransferProgressSenderSide();
};
