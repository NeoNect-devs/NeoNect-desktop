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
};
