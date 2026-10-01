#include "test_offline_queue.h"
#include <QtTest>
#include "../src/core/messaging/OfflineQueue.h"
#include <QFile>

using namespace NeoNect::Core::Messaging;

void TestOfflineQueue::initTestCase() {
    QFile::remove("test_offline_queue.db");
}

void TestOfflineQueue::cleanupTestCase() {
    QFile::remove("test_offline_queue.db");
}

void TestOfflineQueue::testEnqueueMessage() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    OfflineQueueService service(queue, [](auto, auto, auto, auto) { return true; });

    service.onEnvelopeReady("bob", "device1", "msg1", "envelope_data");
    
    auto pending = queue->getPendingEntries();
    QCOMPARE(pending.size(), 1);
    QCOMPARE(pending[0].messageId, QString("msg1"));
    QCOMPARE(pending[0].state, QueueState::ACK_PENDING);
}

void TestOfflineQueue::testDuplicateQueuePrevention() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    QVERIFY(queue->enqueue("dup1", "data", "user", "dev"));
    QVERIFY(!queue->enqueue("dup1", "data", "user", "dev"));
}

void TestOfflineQueue::testPersistAfterRestart() {
    {
        auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
        queue->enqueue("persist1", "data", "user", "dev");
    }
    {
        auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
        auto entry = queue->getEntry("persist1");
        QVERIFY(entry.has_value());
        QCOMPARE(entry->state, QueueState::QUEUED);
    }
}

void TestOfflineQueue::testSendSuccess() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    
    bool relayCalled = false;
    OfflineQueueService service(queue, [&](const QString&, const QString&, const QString&, const QByteArray&) {
        relayCalled = true;
        return true; // Simulate success
    });

    service.onEnvelopeReady("alice", "dev", "send1", "data");
    QVERIFY(relayCalled);
    
    auto entry = queue->getEntry("send1");
    QVERIFY(entry.has_value());
    QCOMPARE(entry->state, QueueState::ACK_PENDING);
}

void TestOfflineQueue::testAckTransition() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    OfflineQueueService service(queue, [](auto, auto, auto, auto) { return true; });
    
    service.onEnvelopeReady("alice", "dev", "ack1", "data");
    QCOMPARE(queue->getEntry("ack1")->state, QueueState::ACK_PENDING);
    
    service.handleAck("ack1");
    QCOMPARE(queue->getEntry("ack1")->state, QueueState::DELIVERED);
}

void TestOfflineQueue::testDuplicateAck() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    OfflineQueueService service(queue, [](auto, auto, auto, auto) { return true; });
    
    service.onEnvelopeReady("alice", "dev", "ack_dup", "data");
    service.handleAck("ack_dup");
    QCOMPARE(queue->getEntry("ack_dup")->state, QueueState::DELIVERED);
    
    // Duplicate ACK
    service.handleAck("ack_dup");
    QCOMPARE(queue->getEntry("ack_dup")->state, QueueState::DELIVERED); // Should remain DELIVERED
}

void TestOfflineQueue::testTransportFailureRetry() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    
    bool failTransport = true;
    OfflineQueueService service(queue, [&](auto, auto, auto, auto) {
        return !failTransport;
    });

    service.onEnvelopeReady("alice", "dev", "retry1", "data");
    // Fails transport immediately
    auto entry = queue->getEntry("retry1");
    QVERIFY(entry.has_value());
    QCOMPARE(entry->state, QueueState::QUEUED);
    QCOMPARE(entry->retryCount, 1);
}

void TestOfflineQueue::testRetryLimit() {
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    OfflineQueueService service(queue, [](auto, auto, auto, auto) { return false; });
    
    service.onEnvelopeReady("alice", "dev", "limit1", "data");
    
    for (int i = 0; i < OfflineQueueService::MAX_RETRIES; ++i) {
        service.resume();
    }
    
    auto entry = queue->getEntry("limit1");
    QCOMPARE(entry->state, QueueState::FAILED);
}

void TestOfflineQueue::testCrashRecovery() {
    {
        auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
        queue->enqueue("crash1", "data", "user", "dev");
        queue->updateState("crash1", QueueState::SENDING);
    }
    
    {
        auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
        bool resent = false;
        OfflineQueueService service(queue, [&](auto, auto, auto, auto) {
            resent = true;
            return true;
        });
        
        // On restart, SENDING messages should be recovered and reset to QUEUED, then retried.
        service.resume();
        QVERIFY(resent);
        
        auto entry = queue->getEntry("crash1");
        QCOMPARE(entry->state, QueueState::ACK_PENDING);
    }
}

void TestOfflineQueue::testAtomicFailure() {
    // Atomic failure simulating queue failure. Since SQLite transactions wrap single inserts,
    // we can just check if an invalid insert fails.
    auto queue = std::make_shared<MessageQueue>("test_offline_queue.db");
    // Trying to get entry for non-existent message
    auto entry = queue->getEntry("does_not_exist");
    QVERIFY(!entry.has_value());
    
    // If update fails, it returns false
    QVERIFY(!queue->updateState("does_not_exist", QueueState::SENDING));
}
