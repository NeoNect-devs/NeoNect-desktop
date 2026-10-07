#include <QtTest>
#include <QEventLoop>
#include <QDir>
#include "test_messages.h"
#include "core/messaging/MessageStorage.h"
#include "services/messageservice.h"
#include "core/chatmessagemodel.h"

using namespace NeoNect;

void TestMessages::initTestCase() {
    QDir dir;
    dir.mkpath("test_messages_db");
}

void TestMessages::cleanupTestCase() {
    QDir dir("test_messages_db");
    dir.removeRecursively();
}

void TestMessages::testSqliteInsertAndRetrieve() {
    QString dbPath = "test_messages_db/test1.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    
    Domain::Message msg;
    msg.id = "msg1";
    msg.conversationId = "dms:alice";
    msg.senderId = "alice";
    msg.text = "Hello!";
    msg.timestamp = 1000;
    
    NeoNect::Core::Messaging::Message cm;
    cm.messageId = msg.id;
    cm.conversationId = msg.conversationId;
    cm.senderId = msg.senderId;
    cm.timestamp = msg.timestamp;
    cm.plaintext = msg.text;
    bool successResult = repo->saveMessage(cm);
    QVERIFY(successResult);

    std::vector<Domain::Message> fetched;
    auto coreMsgs = repo->getConversationMessages("dms:alice");
    for (const auto& cm : coreMsgs) {
        Domain::Message dm;
        dm.id = cm.messageId;
        dm.text = cm.plaintext;
        fetched.push_back(dm);
    }
    
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched[0].id, QString("msg1"));
    QCOMPARE(fetched[0].text, QString("Hello!"));
}

void TestMessages::testMessageOrdering() {
    QString dbPath = "test_messages_db/test2.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    
    for (int i = 1; i <= 3; ++i) {
        Domain::Message msg;
        msg.id = QString("msg%1").arg(i);
        msg.conversationId = "dms:bob";
        msg.senderId = "bob";
        msg.timestamp = i * 1000; // 1000, 2000, 3000
        
        NeoNect::Core::Messaging::Message cm;
    cm.messageId = msg.id;
    cm.conversationId = msg.conversationId;
    cm.senderId = msg.senderId;
    cm.timestamp = msg.timestamp;
    cm.plaintext = msg.text;
    bool successResult = repo->saveMessage(cm);
    }
    
    std::vector<Domain::Message> fetched;
    auto coreMsgs = repo->getConversationMessages("dms:bob");
    for (const auto& cm : coreMsgs) {
        Domain::Message dm;
        dm.id = cm.messageId;
        dm.text = cm.plaintext;
        fetched.push_back(dm);
    }
    
    QCOMPARE(fetched.size(), 3);
    // Should be returned in chronological order by getMessagesAsync because of prepend
    QCOMPARE(fetched[0].id, QString("msg1"));
    QCOMPARE(fetched[2].id, QString("msg3"));
}

void TestMessages::testDuplicateServerIdHandling() {
    QString dbPath = "test_messages_db/test3.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    
    Domain::Message msg;
    msg.id = "local1";
    msg.serverId = 123;
    msg.conversationId = "dms:charlie";
    msg.senderId = "charlie";
    
    NeoNect::Core::Messaging::Message cm;
    cm.messageId = msg.id;
    cm.serverId = msg.serverId;
    cm.conversationId = msg.conversationId;
    cm.senderId = msg.senderId;
    cm.timestamp = msg.timestamp;
    cm.plaintext = msg.text;
    bool successResult = repo->saveMessage(cm);
    
    // Insert another with same server ID
    Domain::Message msg2;
    msg2.id = "local2";
    msg2.serverId = 123;
    msg2.conversationId = "dms:charlie";
    msg2.senderId = "charlie";
    
    NeoNect::Core::Messaging::Message cm2;
    cm2.messageId = msg2.id;
    cm2.serverId = msg2.serverId;
    cm2.conversationId = msg2.conversationId;
    cm2.senderId = msg2.senderId;
    cm2.timestamp = msg2.timestamp;
    cm2.plaintext = msg2.text;
    bool saveSuccess = repo->saveMessage(cm2);
    
    // SQLite UNIQUE constraint on server_id > 0 with INSERT OR REPLACE replaces it
    QVERIFY(saveSuccess);
    
    std::vector<Domain::Message> fetched;
    auto coreMsgs = repo->getConversationMessages("dms:charlie");
    for (const auto& cm : coreMsgs) {
        Domain::Message dm;
        dm.id = cm.messageId;
        dm.text = cm.plaintext;
        fetched.push_back(dm);
    }
    
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched[0].id, QString("local2"));
}

void TestMessages::testRepositoryReopenPersistence() {
    QString dbPath = "test_messages_db/test4.db";
    QFile::remove(dbPath);
    
    {
        auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
        Domain::Message msg;
        msg.id = "persist1";
        msg.conversationId = "dms:dave";
        msg.senderId = "dave";
        NeoNect::Core::Messaging::Message cm;
    cm.messageId = msg.id;
    cm.conversationId = msg.conversationId;
    cm.senderId = msg.senderId;
    cm.timestamp = msg.timestamp;
    cm.plaintext = msg.text;
    bool successResult = repo->saveMessage(cm);
    } // repo destroyed
    
    {
        auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
        std::vector<Domain::Message> fetched;
        auto coreMsgs = repo->getConversationMessages("dms:dave");
    for (const auto& cm : coreMsgs) {
        Domain::Message dm;
        dm.id = cm.messageId;
        dm.text = cm.plaintext;
        fetched.push_back(dm);
    }
        QCOMPARE(fetched.size(), 1);
        QCOMPARE(fetched[0].id, QString("persist1"));
    }
}

void TestMessages::testMessageServiceIntegration() {
    QString dbPath = "test_messages_db/test5.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    Services::MessageService service(repo);
    service.setCurrentUserId("me");
    
    QSignalSpy spyAdded(&service, &Services::MessageService::messageAdded);
    QSignalSpy spyTransmit(&service, &Services::MessageService::transmitMessage);
    
    service.sendMessage("dms:eve", "Hi Eve!");
    
    QEventLoop loop;
    while (spyTransmit.isEmpty()) {
        loop.processEvents();
    }
    
    QCOMPARE(spyAdded.count(), 1);
    QCOMPARE(spyTransmit.count(), 1);
    
    QVariantMap addedMap = spyAdded.first().at(1).toMap();
    QCOMPARE(addedMap.value("text").toString(), QString("Hi Eve!"));
    QCOMPARE(addedMap.value("fromMe").toBool(), true);
}

void TestMessages::testMessageModelPopulation() {
    ChatMessageModel model;
    
    QVariantList msgs;
    QVariantMap m1;
    m1["id"] = "m1";
    m1["text"] = "One";
    m1["senderId"] = "eve";
    
    QVariantMap m2;
    m2["id"] = "m2";
    m2["text"] = "Two";
    m2["senderId"] = "eve";
    
    msgs << m1 << m2;
    
    model.setActiveConversation("dms:eve");
    model.onConversationLoaded("dms:eve", msgs);
    
    QCOMPARE(model.rowCount(), 2);
}

void TestMessages::testTypingStatusTransmissionAndHandling() {
    QString dbPath = "test_messages_db/test6.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    Services::MessageService service(repo);
    service.setCurrentUserId("me");

    QSignalSpy spyTransmit(&service, &Services::MessageService::transmitMessage);
    QSignalSpy spyPeerTyping(&service, &Services::MessageService::peerTypingStatusChanged);

    // 1. sendTyping(..., true) emits transmitMessage with type == "typing_start"
    service.sendTyping("dms:bob", true);
    QCOMPARE(spyTransmit.count(), 1);
    Domain::Message sentMsg = spyTransmit.first().at(0).value<Domain::Message>();
    QCOMPARE(sentMsg.type, QString("typing_start"));
    QCOMPARE(sentMsg.conversationId, QString("dms:bob"));

    // 2. sendTyping(..., false) emits transmitMessage with type == "typing_stop"
    service.sendTyping("dms:bob", false);
    QCOMPARE(spyTransmit.count(), 2);
    Domain::Message stopMsg = spyTransmit.at(1).at(0).value<Domain::Message>();
    QCOMPARE(stopMsg.type, QString("typing_stop"));

    // 3. Incoming typing_start packet triggers peerTypingStatusChanged(convId, senderId, true)
    Domain::Message incomingStart;
    incomingStart.id = "type_1";
    incomingStart.conversationId = "dms:bob";
    incomingStart.senderId = "bob";
    incomingStart.type = "typing_start";
    service.handleIncomingMessages({ incomingStart });

    QCOMPARE(spyPeerTyping.count(), 1);
    QCOMPARE(spyPeerTyping.first().at(0).toString(), QString("dms:bob"));
    QCOMPARE(spyPeerTyping.first().at(1).toString(), QString("bob"));
    QCOMPARE(spyPeerTyping.first().at(2).toBool(), true);

    // 4. Incoming regular message auto-clears typing status
    Domain::Message regularMsg;
    regularMsg.id = "reg_1";
    regularMsg.conversationId = "dms:bob";
    regularMsg.senderId = "bob";
    regularMsg.type = "text";
    regularMsg.text = "Hello!";
    service.handleIncomingMessages({ regularMsg });

    QCOMPARE(spyPeerTyping.count(), 2);
    QCOMPARE(spyPeerTyping.at(1).at(2).toBool(), false);
}

void TestMessages::testMessageDeliveryStatusTransitions() {
    QString dbPath = "test_messages_db/test7.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    Services::MessageService service(repo);
    service.setCurrentUserId("me");

    ChatMessageModel model;
    model.setActiveConversation("dms:alice");

    QObject::connect(&service, &Services::MessageService::messageAdded,
                     &model, &ChatMessageModel::onMessageAdded);
    QObject::connect(&service, &Services::MessageService::messageUpdated,
                     &model, &ChatMessageModel::onMessageUpdated);

    // Send a message -> model gets it in "sending" status
    service.sendMessage("dms:alice", "Testing status transition");
    QCOMPARE(model.rowCount(), 1);
    QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, ChatMessageModel::StatusRole).toString(), QString("sending"));
    QString msgId = model.data(idx, ChatMessageModel::MessageIdRole).toString();
    QVERIFY(!msgId.isEmpty());

    // Delivery confirmation arrives -> status transitions to "sent"
    service.handleMessageDeliveryStatus(msgId, true, "");
    QCOMPARE(model.data(idx, ChatMessageModel::StatusRole).toString(), QString("sent"));
}

void TestMessages::testMediaTransferProgressSenderSide() {
    QString dbPath = "test_messages_db/test8.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Core::Messaging::SqliteMessageStorage>(dbPath);
    Services::MessageService service(repo);
    service.setCurrentUserId("me");

    ChatMessageModel model;
    model.setActiveConversation("dms:bob");

    QObject::connect(&service, &Services::MessageService::messageAdded,
                     &model, &ChatMessageModel::onMessageAdded);
    QObject::connect(&service, &Services::MessageService::messageUpdated,
                     &model, &ChatMessageModel::onMessageUpdated);
    QObject::connect(&service, &Services::MessageService::mediaTransferProgress,
                     &model, &ChatMessageModel::onMediaTransferProgress);

    QSignalSpy spyProgress(&service, &Services::MessageService::mediaTransferProgress);

    // Send an image message with 3MB file size
    qint64 testSize = 1024 * 1024 * 3;
    service.sendMessage("dms:bob", "Look at this photo", "image", "file:///photo.png", "photo.png", testSize, 0, {});

    QCOMPARE(model.rowCount(), 1);
    QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, ChatMessageModel::StatusRole).toString(), QString("sending"));
    QCOMPARE(model.data(idx, ChatMessageModel::MessageTypeRole).toString(), QString("image"));
    QCOMPARE(model.data(idx, ChatMessageModel::FileSizeRole).toLongLong(), testSize);
    QVERIFY(model.data(idx, ChatMessageModel::TransferProgressRole).toReal() > 0.0);
    QString msgId = model.data(idx, ChatMessageModel::MessageIdRole).toString();
    QVERIFY(!msgId.isEmpty());

    // Delivery confirmation arrives while transfer is still active;
    // status should remain "sending" (transfer progress must NOT be cleared prematurely!)
    service.handleMessageDeliveryStatus(msgId, true, "");
    QCOMPARE(model.data(idx, ChatMessageModel::StatusRole).toString(), QString("sending"));

    // Wait for transfer progress timer to emit updates
    QTest::qWait(200);
    QVERIFY(spyProgress.count() >= 2);
    qreal intermediateProg = model.data(idx, ChatMessageModel::TransferProgressRole).toReal();
    QVERIFY(intermediateProg >= 0.05);

    // Wait for the full transfer animation to complete (25 steps * 60ms = ~1500ms)
    QTest::qWait(1600);
    QCOMPARE(model.data(idx, ChatMessageModel::StatusRole).toString(), QString("sent"));
    QCOMPARE(model.data(idx, ChatMessageModel::TransferProgressRole).toReal(), 1.0);
    QCOMPARE(model.data(idx, ChatMessageModel::TransferBytesRole).toLongLong(), testSize);
}

void TestMessages::testCoreMessageMappingText() {
    NeoNect::Domain::Message msg;
    msg.id = "txt1";
    msg.serverId = 100;
    msg.conversationId = "dms:eve";
    msg.senderId = "me";
    msg.text = "hello text";
    msg.timestamp = 12345;
    msg.status = NeoNect::Domain::MessageStatus::Sent;

    // Simulate outgoing mapping (Domain -> Core)
    NeoNect::Core::Messaging::Message coreMsg;
    coreMsg.messageId = msg.id;
    coreMsg.serverId = msg.serverId;
    coreMsg.conversationId = msg.conversationId;
    coreMsg.senderId = msg.senderId;
    coreMsg.timestamp = msg.timestamp;
    coreMsg.plaintext = msg.text;
    coreMsg.type = msg.type;
    coreMsg.mediaUrl = msg.mediaUrl;
    coreMsg.fileName = msg.fileName;
    coreMsg.fileSize = msg.fileSize;
    coreMsg.duration = msg.duration;
    coreMsg.waveform = msg.waveform;
    coreMsg.mediaWidth = msg.mediaWidth;
    coreMsg.mediaHeight = msg.mediaHeight;
    
    // Validate core fields
    QCOMPARE(coreMsg.type, QString("text"));
    QCOMPARE(coreMsg.mediaUrl, QString(""));
    QCOMPARE(coreMsg.fileSize, 0);

    // Simulate incoming mapping (Core -> Domain)
    NeoNect::Domain::Message dm;
    dm.id = coreMsg.messageId;
    dm.serverId = coreMsg.serverId;
    dm.conversationId = coreMsg.conversationId;
    dm.senderId = coreMsg.senderId;
    dm.timestamp = coreMsg.timestamp;
    dm.text = coreMsg.plaintext;
    dm.type = coreMsg.type;
    dm.mediaUrl = coreMsg.mediaUrl;
    dm.fileName = coreMsg.fileName;
    dm.fileSize = coreMsg.fileSize;
    dm.duration = coreMsg.duration;
    dm.waveform = coreMsg.waveform;
    dm.mediaWidth = coreMsg.mediaWidth;
    dm.mediaHeight = coreMsg.mediaHeight;
    
    QCOMPARE(dm.id, msg.id);
    QCOMPARE(dm.text, msg.text);
    QCOMPARE(dm.type, QString("text"));
}

void TestMessages::testCoreMessageMappingMedia() {
    NeoNect::Domain::Message msg;
    msg.id = "media1";
    msg.conversationId = "dms:eve";
    msg.senderId = "me";
    msg.text = "";
    msg.type = "voice_note";
    msg.mediaUrl = "file:///tmp/voice.wav";
    msg.fileName = "voice.wav";
    msg.fileSize = 4096;
    msg.duration = 15;
    msg.waveform = QByteArray("\x01\x02\x03", 3);
    msg.mediaWidth = 0;
    msg.mediaHeight = 0;

    // Simulate outgoing mapping (Domain -> Core)
    NeoNect::Core::Messaging::Message coreMsg;
    coreMsg.messageId = msg.id;
    coreMsg.conversationId = msg.conversationId;
    coreMsg.senderId = msg.senderId;
    coreMsg.plaintext = msg.text;
    coreMsg.type = msg.type;
    coreMsg.mediaUrl = msg.mediaUrl;
    coreMsg.fileName = msg.fileName;
    coreMsg.fileSize = msg.fileSize;
    coreMsg.duration = msg.duration;
    coreMsg.waveform = msg.waveform;
    coreMsg.mediaWidth = msg.mediaWidth;
    coreMsg.mediaHeight = msg.mediaHeight;
    
    QCOMPARE(coreMsg.type, QString("voice_note"));
    QCOMPARE(coreMsg.mediaUrl, QString("file:///tmp/voice.wav"));
    QCOMPARE(coreMsg.fileSize, 4096);
    QCOMPARE(coreMsg.duration, 15);
    QCOMPARE(coreMsg.waveform.size(), 3);

    // Simulate incoming mapping (Core -> Domain)
    NeoNect::Domain::Message dm;
    dm.id = coreMsg.messageId;
    dm.conversationId = coreMsg.conversationId;
    dm.senderId = coreMsg.senderId;
    dm.text = coreMsg.plaintext;
    dm.type = coreMsg.type;
    dm.mediaUrl = coreMsg.mediaUrl;
    dm.fileName = coreMsg.fileName;
    dm.fileSize = coreMsg.fileSize;
    dm.duration = coreMsg.duration;
    dm.waveform = coreMsg.waveform;
    dm.mediaWidth = coreMsg.mediaWidth;
    dm.mediaHeight = coreMsg.mediaHeight;
    
    QCOMPARE(dm.id, msg.id);
    QCOMPARE(dm.type, QString("voice_note"));
    QCOMPARE(dm.mediaUrl, QString("file:///tmp/voice.wav"));
    QCOMPARE(dm.fileName, QString("voice.wav"));
    QCOMPARE(dm.fileSize, 4096);
    QCOMPARE(dm.duration, 15);
    QCOMPARE(dm.waveform.size(), 3);
    QCOMPARE(dm.mediaWidth, 0);
    
    // Also test image mapping
    NeoNect::Domain::Message imgMsg;
    imgMsg.id = "img1";
    imgMsg.type = "image";
    imgMsg.mediaUrl = "file:///tmp/photo.jpg";
    imgMsg.fileName = "photo.jpg";
    imgMsg.fileSize = 100000;
    imgMsg.mediaWidth = 1920;
    imgMsg.mediaHeight = 1080;
    
    NeoNect::Core::Messaging::Message coreImg;
    coreImg.messageId = imgMsg.id;
    coreImg.type = imgMsg.type;
    coreImg.mediaUrl = imgMsg.mediaUrl;
    coreImg.fileName = imgMsg.fileName;
    coreImg.fileSize = imgMsg.fileSize;
    coreImg.mediaWidth = imgMsg.mediaWidth;
    coreImg.mediaHeight = imgMsg.mediaHeight;
    
    NeoNect::Domain::Message dmImg;
    dmImg.id = coreImg.messageId;
    dmImg.type = coreImg.type;
    dmImg.mediaUrl = coreImg.mediaUrl;
    dmImg.fileName = coreImg.fileName;
    dmImg.fileSize = coreImg.fileSize;
    dmImg.mediaWidth = coreImg.mediaWidth;
    dmImg.mediaHeight = coreImg.mediaHeight;
    
    QCOMPARE(dmImg.id, QString("img1"));
    QCOMPARE(dmImg.type, QString("image"));
    QCOMPARE(dmImg.mediaUrl, QString("file:///tmp/photo.jpg"));
    QCOMPARE(dmImg.fileSize, 100000);
    QCOMPARE(dmImg.mediaWidth, 1920);
    QCOMPARE(dmImg.mediaHeight, 1080);
}
