// tests/test_storage.h
#pragma once
#include <QObject>

class TestStorage : public QObject {
    Q_OBJECT

private slots:
    void testProfileIsolation();
    void testGettersAndSetters();
    void testClearSession();
};
