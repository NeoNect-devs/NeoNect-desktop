#pragma once
#include <QObject>

class TestStorageContext : public QObject {
    Q_OBJECT
private slots:
    void testCreationAndPaths();
    void testPreAuthFailClosed();
    void testDeterministicRecreation();
    void testIsolation();
    void testResourceCleanup();
};
