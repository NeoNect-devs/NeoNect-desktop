// tests/test_models.h
#pragma once
#include <QObject>

class TestModels : public QObject {
    Q_OBJECT

private slots:
    void testInitialEmptyModel();
    void testOutgoingMessageInsertion();
    void testConsecutiveMessageBlockGrouping();
    void testClearViewportStore();
    void testRoleNames();
};
