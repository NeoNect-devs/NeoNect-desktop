#pragma once
#include <QObject>

class TestBackend : public QObject {
    Q_OBJECT
private slots:
    void testRandomBytes();
    void testX25519();
    void testSha256();
    void testHmacSha256();
    void testHkdfSha256();
    void testAeadGcm();
    void testSecurityConstraints();
};
