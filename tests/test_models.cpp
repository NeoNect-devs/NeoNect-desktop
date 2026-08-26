// tests/test_models.cpp
#include "test_models.h"
#include <QtTest>
#include "../src/core/chatmessagemodel.h"

void TestModels::testInitialEmptyModel() {
    ChatMessageModel model;
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.data(model.index(0), ChatMessageModel::TextRole).isValid());
}

void TestModels::testOutgoingMessageInsertion() {
    ChatMessageModel model;
    model.insertOutgoingMessage("Hello World!");

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::TextRole).toString(), "Hello World!");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::FromMeRole).toBool(), true);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::SenderNameRole).toString(), "Me");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::FirstInBlockRole).toBool(), true);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::LastInBlockRole).toBool(), true);
}

void TestModels::testConsecutiveMessageBlockGrouping() {
    ChatMessageModel model;
    model.insertMessage("First from Alice", false, "Alice", "A");
    model.insertMessage("Second from Alice", false, "Alice", "A");
    model.insertMessage("Reply from Bob", true, "Me", "");

    QCOMPARE(model.rowCount(), 3);

    // First Alice message: isFirstInBlock = true, isLastInBlock = false
    QCOMPARE(model.data(model.index(0), ChatMessageModel::FirstInBlockRole).toBool(), true);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::LastInBlockRole).toBool(), false);

    // Second Alice message: isFirstInBlock = false, isLastInBlock = true
    QCOMPARE(model.data(model.index(1), ChatMessageModel::FirstInBlockRole).toBool(), false);
    QCOMPARE(model.data(model.index(1), ChatMessageModel::LastInBlockRole).toBool(), true);

    // Bob reply: isFirstInBlock = true, isLastInBlock = true
    QCOMPARE(model.data(model.index(2), ChatMessageModel::FirstInBlockRole).toBool(), true);
    QCOMPARE(model.data(model.index(2), ChatMessageModel::LastInBlockRole).toBool(), true);
}

void TestModels::testClearViewportStore() {
    ChatMessageModel model;
    model.insertMessage("Msg 1", false, "Alice", "A");
    model.insertMessage("Msg 2", true, "Me", "");
    QCOMPARE(model.rowCount(), 2);

    model.clearActiveViewportStore();
    QCOMPARE(model.rowCount(), 0);
}

void TestModels::testRoleNames() {
    ChatMessageModel model;
    auto roles = model.roleNames();

    QCOMPARE(roles.value(ChatMessageModel::TextRole), QByteArray("text"));
    QCOMPARE(roles.value(ChatMessageModel::FromMeRole), QByteArray("fromMe"));
    QCOMPARE(roles.value(ChatMessageModel::SenderNameRole), QByteArray("senderName"));
    QCOMPARE(roles.value(ChatMessageModel::SenderAvatarRole), QByteArray("senderAvatar"));
    QCOMPARE(roles.value(ChatMessageModel::FirstInBlockRole), QByteArray("isFirstInBlock"));
    QCOMPARE(roles.value(ChatMessageModel::LastInBlockRole), QByteArray("isLastInBlock"));
}
