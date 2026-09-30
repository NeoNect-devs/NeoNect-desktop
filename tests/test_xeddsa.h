#pragma once
#include <QObject>

class TestXEdDSA : public QObject {
    Q_OBJECT

private slots:
    void testReferenceVector();
    void testSignVerifyCycle();
};
