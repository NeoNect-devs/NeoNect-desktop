#pragma once

#include <QObject>

class TestDoubleRatchet : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Initialization
    void testAliceInitialization();
    void testBobInitialization();

    // Symmetric Ratchet
    void testSymmetricRatchet();
    void testMissingChainKeys();

    // First Message
    void testFirstMessage();

    // Bidirectional Ratchet
    void testBidirectionalRatchet();

    // Out of order and Replay
    void testOutOfOrderAndReplay();

    // Multiple DH Chains
    void testMultipleDHChains();

    // MAX_SKIP and Counter boundaries
    void testMaxSkip();
    void testCounterBoundaries();

    // Failure atomicity
    void testFailureAtomicity();

    // Deterministic vectors
    void testDeterministicVectors();
};
