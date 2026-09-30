#pragma once
#include <QObject>

class TestX3DHStep3 : public QObject {
    Q_OBJECT
private slots:
    void testIdentityKey();
    void testSignedPreKey();
    void testOneTimePreKeys();
};
