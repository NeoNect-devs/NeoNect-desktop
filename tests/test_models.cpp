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
    stickerMsg["mediaUrl"] = "qrc:/qt/qml/NeoNect/assets/stickers/duck/duck_happy.svg";
    stickerMsg["fileName"] = "Happy";
    stickerMsg["fromMe"] = true;
    stickerMsg["senderName"] = "Me";
    stickerMsg["status"] = "sent";
    model.insertMessageItem(stickerMsg);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageTypeRole).toString(), "sticker");
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MediaUrlRole).toString(), "qrc:/qt/qml/NeoNect/assets/stickers/duck/duck_happy.svg");
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

    // 3. Image file auto-detection
    QVariantMap imgFileMsg;
    imgFileMsg["messageId"] = "img_003";
    imgFileMsg["messageType"] = "file";
    imgFileMsg["mediaUrl"] = "file:///C:/Users/test/photo.PNG";
    imgFileMsg["fileName"] = "photo.PNG";
    model.insertMessageItem(imgFileMsg);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(2), ChatMessageModel::MessageTypeRole).toString(), "image");

    // 4. Video file auto-detection
    QVariantMap vidFileMsg;
    vidFileMsg["messageId"] = "vid_004";
    vidFileMsg["messageType"] = "file";
    vidFileMsg["mediaUrl"] = "file:///C:/Users/test/clip.mp4";
    vidFileMsg["fileName"] = "clip.mp4";
    model.insertMessageItem(vidFileMsg);
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(3), ChatMessageModel::MessageTypeRole).toString(), "video");

    // 5. Audio file auto-detection
    QVariantMap audioFileMsg;
    audioFileMsg["messageId"] = "aud_005";
    audioFileMsg["messageType"] = "file";
    audioFileMsg["mediaUrl"] = "file:///C:/Users/test/song.mp3";
    audioFileMsg["fileName"] = "song.mp3";
    model.insertMessageItem(audioFileMsg);
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.data(model.index(4), ChatMessageModel::MessageTypeRole).toString(), "audio");

    // 6. Generic document remains file
    QVariantMap docFileMsg;
    docFileMsg["messageId"] = "doc_006";
    docFileMsg["messageType"] = "file";
    docFileMsg["mediaUrl"] = "file:///C:/Users/test/document.pdf";
    docFileMsg["fileName"] = "document.pdf";
    model.insertMessageItem(docFileMsg);
    QCOMPARE(model.rowCount(), 6);
    QCOMPARE(model.data(model.index(5), ChatMessageModel::MessageTypeRole).toString(), "file");
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

    // Test onMessageRemoved removes the item
    model.onMessageRemoved("", "msg_err_1");
    QCOMPARE(model.rowCount(), 0);
}

void TestModels::testAudioManagerRecordingAndPlayback() {
    auto *audio = new AudioManager(this);
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

void TestModels::testAudioManagerVolumePersistence() {
    auto *audio = new AudioManager(this);
    QVERIFY(audio != nullptr);

    // Ensure known baseline state
    audio->setMuted(false);
    audio->setVolume(1.0);

    // 1. Basic Volume Setting and Signal Emission
    QSignalSpy volumeSpy(audio, &AudioManager::volumeChanged);
    QSignalSpy muteSpy(audio, &AudioManager::isMutedChanged);

    audio->setVolume(0.75);
    QVERIFY(qAbs(audio->volume() - 0.75) < 0.001);
    QCOMPARE(volumeSpy.count(), 1);

    // 2. Clamping bounds [0.0, 1.0]
    audio->setVolume(1.5);
    QCOMPARE(audio->volume(), 1.0);

    audio->setVolume(-0.25);
    QCOMPARE(audio->volume(), 0.0);

    // 3. Mute Controls and auto-unmute on volume increase
    audio->setMuted(true);
    QVERIFY(audio->isMuted());
    QVERIFY(muteSpy.count() >= 1);

    audio->toggleMute();
    QVERIFY(!audio->isMuted());

    audio->setMuted(true);
    QVERIFY(audio->isMuted());
    audio->setVolume(0.65);
    QVERIFY(!audio->isMuted()); // Auto-unmuted when setting non-zero volume
    QVERIFY(qAbs(audio->volume() - 0.65) < 0.001);

    // 4. Persistence verification across separate instances
    audio->setVolume(0.42);
    audio->setMuted(true);

    auto *audio2 = new AudioManager(this);
    QVERIFY(qAbs(audio2->volume() - 0.42) < 0.001);
    QVERIFY(audio2->isMuted());

    // Restore to default clean state
    audio2->setVolume(1.0);
    audio2->setMuted(false);
}

void TestModels::testMediaRequestMessageItem() {
    ChatMessageModel model;
    QVariantMap map;
    map["id"] = "req-123";
    map["type"] = "media_request";
    map["text"] = "Vacation picture";
    map["fileName"] = "holiday.png";
    map["mediaUrl"] = "file:///holiday.png";
    map["fileSize"] = 1048576LL;
    map["errorText"] = "image";
    map["status"] = "pending";
    map["fromMe"] = false;
    map["senderName"] = "Alice";

    model.insertMessageItem(map);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageTypeRole).toString(), QString("media_request"));
    QCOMPARE(model.data(model.index(0), ChatMessageModel::FileNameRole).toString(), QString("holiday.png"));
    QCOMPARE(model.data(model.index(0), ChatMessageModel::FileSizeRole).toLongLong(), 1048576LL);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::ErrorTextRole).toString(), QString("image"));
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), QString("pending"));
}

void TestModels::testMediaRequestFallbackCategoryDetection() {
    ChatMessageModel model;
    QVariantMap map;
    map["id"] = "req-456";
    map["type"] = "media_request";
    map["fileName"] = "clip.mp4";
    map["mediaUrl"] = "file:///clip.mp4";
    map["status"] = "pending";

    model.insertMessageItem(map);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageTypeRole).toString(), QString("media_request"));
    QCOMPARE(model.data(model.index(0), ChatMessageModel::ErrorTextRole).toString(), QString("video"));
}

void TestModels::testConversationSwitchCaching() {
    ChatMessageModel model;
    
    // Set active conversation to alice
    model.setActiveConversation("dms:alice");
    model.insertMessage("Hello Alice", true, "Me", "", "text", "", "", 0, 0, {}, "sent", "msg-1");
    model.insertMessage("Hi there!", false, "Alice", "A", "text", "", "", 0, 0, {}, "sent", "msg-2");
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.hasCachedConversation("dms:alice"));

    // Switch to bob (cold conversation)
    model.setActiveConversation("dms:bob");
    QCOMPARE(model.rowCount(), 0);
    model.insertMessage("Hey Bob", true, "Me", "", "text", "", "", 0, 0, {}, "sent", "msg-3");
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(model.hasCachedConversation("dms:bob"));

    // Switch back to alice (cached conversation) - must be restored immediately with zero async delay
    model.setActiveConversation("dms:alice");
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::TextRole).toString(), QString("Hello Alice"));
    QCOMPARE(model.data(model.index(1), ChatMessageModel::TextRole).toString(), QString("Hi there!"));

    // Switch back to bob
    model.setActiveConversation("dms:bob");
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::TextRole).toString(), QString("Hey Bob"));
}

void TestModels::testDraftAndScrollPersistence() {
    ChatMessageModel model;
    
    // Save drafts for different channels
    QVariantMap attMap;
    attMap["name"] = "document.pdf";
    attMap["url"] = "file:///document.pdf";
    attMap["size"] = 12345;

    model.saveDraft("dms:alice", "Unfinished text for Alice");
    model.saveDraft("dms:bob", "Draft for Bob with attachment", attMap);

    QCOMPARE(model.getDraftText("dms:alice"), QString("Unfinished text for Alice"));
    QVERIFY(model.getDraftAttachment("dms:alice").isEmpty());

    QCOMPARE(model.getDraftText("dms:bob"), QString("Draft for Bob with attachment"));
    QCOMPARE(model.getDraftAttachment("dms:bob").value("name").toString(), QString("document.pdf"));

    // Save scroll positions
    model.saveScrollPosition("dms:alice", 420.5, false);
    model.saveScrollPosition("dms:bob", 0.0, true);

    QCOMPARE(model.getSavedScrollPosition("dms:alice"), 420.5);
    QCOMPARE(model.isSavedAtBottom("dms:alice"), false);

    QCOMPARE(model.getSavedScrollPosition("dms:bob"), 0.0);
    QCOMPARE(model.isSavedAtBottom("dms:bob"), true);
}

void TestModels::testScrollPositionMemoryAnchor() {
    ChatMessageModel model;
    model.setActiveConversation("dms:alice");

    model.insertMessage("Message 1", true, "Me", "", "text", "", "", 0, 0, {}, "sent", "msg-1", 1000);
    model.insertMessage("Message 2", false, "Alice", "", "text", "", "", 0, 0, {}, "sent", "msg-2", 2000);
    model.insertMessage("Message 3", true, "Me", "", "text", "", "", 0, 0, {}, "sent", "msg-3", 3000);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.indexOfMessageId("msg-1"), 0);
    QCOMPARE(model.indexOfMessageId("msg-2"), 1);
    QCOMPARE(model.indexOfMessageId("msg-3"), 2);
    QCOMPARE(model.indexOfMessageId("msg-nonexistent"), -1);

    QCOMPARE(model.getMessageIdAt(0), QString("msg-1"));
    QCOMPARE(model.getMessageIdAt(1), QString("msg-2"));
    QCOMPARE(model.getMessageIdAt(2), QString("msg-3"));
    QCOMPARE(model.getMessageIdAt(99), QString());

    // Initially unset conversation defaults to wasAtEnd = true
    QVERIFY(!model.hasChatPosition("dms:alice"));
    QVariantMap initialMem = model.getChatPosition("dms:alice");
    QVERIFY(initialMem.value("wasAtEnd").toBool());
    QVERIFY(initialMem.value("messageId").toString().isEmpty());

    // User was scrolled reading Message 2 with a 14.5px offset
    model.saveChatPosition("dms:alice", "msg-2", 14.5, false);
    QVERIFY(model.hasChatPosition("dms:alice"));
    QVERIFY(model.hasScrollMemory("dms:alice"));

    QVariantMap savedMem = model.getChatPosition("dms:alice");
    QCOMPARE(savedMem.value("messageId").toString(), QString("msg-2"));
    QCOMPARE(savedMem.value("lastVisibleMessageId").toString(), QString("msg-2"));
    QCOMPARE(savedMem.value("pixelOffset").toReal(), 14.5);
    QCOMPARE(savedMem.value("offset").toReal(), 14.5);
    QCOMPARE(savedMem.value("wasAtEnd").toBool(), false);

    // Switch conversation to bob, who was scrolled at msg-b1 with 32.0px offset
    model.setActiveConversation("dms:bob");
    model.insertMessage("Bob msg 1", false, "Bob", "", "text", "", "", 0, 0, {}, "sent", "msg-b1", 4000);
    model.insertMessage("Bob msg 2", false, "Bob", "", "text", "", "", 0, 0, {}, "sent", "msg-b2", 4001);
    model.saveChatPosition("dms:bob", "msg-b1", 32.0, false);

    QVariantMap bobMem = model.getChatPosition("dms:bob");
    QCOMPARE(bobMem.value("messageId").toString(), QString("msg-b1"));
    QCOMPARE(bobMem.value("offset").toReal(), 32.0);
    QCOMPARE(bobMem.value("wasAtEnd").toBool(), false);

    // Switch conversation to charlie, who was at the bottom
    model.setActiveConversation("dms:charlie");
    model.insertMessage("Charlie msg 1", false, "Charlie", "", "text", "", "", 0, 0, {}, "sent", "msg-c1", 5000);
    model.saveChatPosition("dms:charlie", "", 0.0, true);

    QVariantMap charlieMem = model.getChatPosition("dms:charlie");
    QVERIFY(charlieMem.value("wasAtEnd").toBool());

    // Cycle 1: Switch back to alice - verify memory is intact and resolved
    model.setActiveConversation("dms:alice");
    QVariantMap aliceRestored = model.getChatPosition("dms:alice");
    QCOMPARE(aliceRestored.value("messageId").toString(), QString("msg-2"));
    QCOMPARE(aliceRestored.value("offset").toReal(), 14.5);
    QCOMPARE(aliceRestored.value("wasAtEnd").toBool(), false);
    QCOMPARE(model.indexOfMessageId(aliceRestored.value("messageId").toString()), 1);

    // Cycle 2: Switch back to bob - verify memory is intact and resolved
    model.setActiveConversation("dms:bob");
    QVariantMap bobRestored = model.getChatPosition("dms:bob");
    QCOMPARE(bobRestored.value("messageId").toString(), QString("msg-b1"));
    QCOMPARE(bobRestored.value("offset").toReal(), 32.0);
    QCOMPARE(bobRestored.value("wasAtEnd").toBool(), false);
    QCOMPARE(model.indexOfMessageId(bobRestored.value("messageId").toString()), 0);

    // Cycle 3: Switch back to charlie - verify still at bottom
    model.setActiveConversation("dms:charlie");
    QVariantMap charlieRestored = model.getChatPosition("dms:charlie");
    QVERIFY(charlieRestored.value("wasAtEnd").toBool());

    // Verify case-insensitive trimmed keying
    QVERIFY(model.hasChatPosition("  DMS:ALICE  "));
    QCOMPARE(model.getChatPosition("  DMS:ALICE  ").value("messageId").toString(), QString("msg-2"));
}

void TestModels::testIdempotentMessageMerge() {
    ChatMessageModel model;
    model.setActiveConversation("dms:alice");

    // Populate initial conversation
    QVariantMap m1;
    m1["id"] = "msg-100";
    m1["text"] = "Message 1";
    m1["status"] = "sending";
    m1["timestamp"] = 1000LL;

    QVariantList list1;
    list1.append(m1);
    model.onConversationLoaded("dms:alice", list1);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), QString("sending"));

    // Server acknowledges delivery: same message with status sent + 1 new message
    QVariantMap m1_updated = m1;
    m1_updated["status"] = "sent";

    QVariantMap m2;
    m2["id"] = "msg-101";
    m2["text"] = "Message 2";
    m2["status"] = "sent";
    m2["timestamp"] = 2000LL;

    QVariantList list2;
    list2.append(m1_updated);
    list2.append(m2);

    // Reconnection or sync load: should update m1 status and insert m2 cleanly
    model.onConversationLoaded("dms:alice", list2);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MessageIdRole).toString(), QString("msg-100"));
    QCOMPARE(model.data(model.index(0), ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(model.data(model.index(1), ChatMessageModel::MessageIdRole).toString(), QString("msg-101"));
}

void TestModels::testMediaDimensionsRole() {
    ChatMessageModel model;
    QVariantMap imgMap;
    imgMap["id"] = "img-501";
    imgMap["type"] = "image";
    imgMap["mediaUrl"] = "file:///dummy.png";
    imgMap["mediaWidth"] = 1920;
    imgMap["mediaHeight"] = 1080;

    model.insertMessageItem(imgMap);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MediaWidthRole).toInt(), 1920);
    QCOMPARE(model.data(model.index(0), ChatMessageModel::MediaHeightRole).toInt(), 1080);
}

