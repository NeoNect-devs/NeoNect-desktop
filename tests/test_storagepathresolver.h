#pragma once
#include <QObject>

class TestStoragePathResolver : public QObject {
    Q_OBJECT
private slots:
    void testCanonicalServerUrl();
    void testServerKey();
    void testAccountKey();
    void testChatKey();
    void testPathSafetyAndHierarchy();
    void testDeterministicOutput();
};
