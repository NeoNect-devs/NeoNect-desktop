// tests/test_models.cpp
#include "test_models.h"
#include <QtTest>
#include "../src/core/chatmessagemodel.h"
#include "../src/core/audiomanager.h"

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
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), "sent");
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
    QCOMPARE(roles.value(ChatMessageModel::MessageIdRole), QByteArray("messageId"));
    QCOMPARE(roles.value(ChatMessageModel::MessageTypeRole), QByteArray("messageType"));
    QCOMPARE(roles.value(ChatMessageModel::MediaUrlRole), QByteArray("mediaUrl"));
    QCOMPARE(roles.value(ChatMessageModel::FileNameRole), QByteArray("fileName"));
    QCOMPARE(roles.value(ChatMessageModel::FileSizeRole), QByteArray("fileSize"));
    QCOMPARE(roles.value(ChatMessageModel::DurationRole), QByteArray("duration"));
    QCOMPARE(roles.value(ChatMessageModel::WaveformRole), QByteArray("waveform"));
    QCOMPARE(roles.value(ChatMessageModel::StatusRole), QByteArray("status"));
    QCOMPARE(roles.value(ChatMessageModel::ErrorTextRole), QByteArray("errorText"));
    QCOMPARE(roles.value(ChatMessageModel::TimestampRole), QByteArray("timestamp"));
}

void TestModels::testRichMessageInsertion() {
    ChatMessageModel model;

    // 1. Sticker message
    QVariantMap stickerMsg;
    stickerMsg["messageId"] = "stk_001";
    stickerMsg["messageType"] = "sticker";
    stickerMsg["mediaUrl"] = "qrc:/qt/qml/Avila/assets/stickers/duck/duck_happy.svg";
    stickerMsg["fileName"] = "Happy";
    stickerMsg["fromMe"] = true;
    stickerMsg["senderName"] = "Me";
    stickerMsg["status"] = "sent";
    model.insertMessageItem(stickerMsg);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageTypeRole).toString(), "sticker");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MediaUrlRole).toString(), "qrc:/qt/qml/Avila/assets/stickers/duck/duck_happy.svg");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageIdRole).toString(), "stk_001");

    // 2. Voice message
    QVariantMap voiceMsg;
    voiceMsg["messageId"] = "voice_002";
    voiceMsg["messageType"] = "voice";
    voiceMsg["duration"] = 12;
    voiceMsg["waveform"] = QVariantList{0.2, 0.5, 0.8, 0.4};
    voiceMsg["fromMe"] = false;
    voiceMsg["senderName"] = "Alex";
    voiceMsg["status"] = "sent";
    model.insertMessageItem(voiceMsg);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(1), ChatMessageModel::MessageTypeRole).toString(), "voice");
    QCOMPARE(model.data(model.index(1), ChatMessageModel::DurationRole).toInt(), 12);
    QCOMPARE(model.data(model.index(1), ChatMessageModel::WaveformRole).toList().size(), 4);
}

void TestModels::testMessageStatusAndRetry() {
    ChatMessageModel model;
    model.insertMessage("Test failure", true, "Me", "", "text", "", "", 0, 0, {}, "sending", "msg_err_1");

    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), "sending");

    // Update to failed
    model.updateMessageStatus("msg_err_1", "failed", "Network unreachable");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), "failed");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::ErrorTextRole).toString(), "Network unreachable");

    // Retrieve by ID
    auto item = model.getMessageById("msg_err_1");
    QCOMPARE(item.value("messageId").toString(), "msg_err_1");
    QCOMPARE(item.value("status").toString(), "failed");

    // Retry / Update to sent
    model.updateMessageStatus("msg_err_1", "sent", "");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), "sent");
}

void TestModels::testAudioManagerRecordingAndPlayback() {
    auto *audio = AudioManager::instance();
    QVERIFY(audio != nullptr);

    // Test recording start/cancel
    audio->startRecording();
    QVERIFY(audio->isRecording());
    audio->cancelRecording();
    QVERIFY(!audio->isRecording());

    // Test playback controls
    audio->playAudio("test_msg_99", "file.wav", 10);
    QVERIFY(audio->isPlaying());
    QCOMPARE(audio->currentPlayingId(), "test_msg_99");

    audio->setPlaybackSpeed(1.5);
    QCOMPARE(audio->playbackSpeed(), 1.5);

    audio->seek("test_msg_99", 0.5);
    QCOMPARE(audio->playbackProgress(), 0.5);

    audio->pauseAudio();
    QVERIFY(!audio->isPlaying());
}
