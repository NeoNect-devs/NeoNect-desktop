#include <QtTest>
#include <QEventLoop>
#include <QDir>
#include "test_messages.h"
#include "storage/sqlmessagerepository.h"
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
    auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
    
    Domain::Message msg;
    msg.id = "msg1";
    msg.conversationId = "dms:alice";
    msg.senderId = "alice";
    msg.text = "Hello!";
    msg.timestamp = 1000;
    
    QEventLoop loop;
    bool successResult = false;
    repo->saveMessageAsync(msg, this, [&](bool success) {
        successResult = success;
        loop.quit();
    });
    loop.exec();
    QVERIFY(successResult);

    std::vector<Domain::Message> fetched;
    repo->getMessagesAsync("dms:alice", 50, 0, this, [&](const std::vector<Domain::Message>& msgs) {
        fetched = msgs;
        loop.quit();
    });
    loop.exec();
    
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched[0].id, QString("msg1"));
    QCOMPARE(fetched[0].text, QString("Hello!"));
}

void TestMessages::testMessageOrdering() {
    QString dbPath = "test_messages_db/test2.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
    QEventLoop loop;
    
    for (int i = 1; i <= 3; ++i) {
        Domain::Message msg;
        msg.id = QString("msg%1").arg(i);
        msg.conversationId = "dms:bob";
        msg.senderId = "bob";
        msg.timestamp = i * 1000; // 1000, 2000, 3000
        
        repo->saveMessageAsync(msg, this, [&](bool) { loop.quit(); });
        loop.exec();
    }
    
    std::vector<Domain::Message> fetched;
    repo->getMessagesAsync("dms:bob", 50, 0, this, [&](const std::vector<Domain::Message>& msgs) {
        fetched = msgs;
        loop.quit();
    });
    loop.exec();
    
    QCOMPARE(fetched.size(), 3);
    // Should be returned in chronological order by getMessagesAsync because of prepend
    QCOMPARE(fetched[0].id, QString("msg1"));
    QCOMPARE(fetched[2].id, QString("msg3"));
}

void TestMessages::testDuplicateServerIdHandling() {
    QString dbPath = "test_messages_db/test3.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
    QEventLoop loop;
    
    Domain::Message msg;
    msg.id = "local1";
    msg.serverId = 123;
    msg.conversationId = "dms:charlie";
    msg.senderId = "charlie";
    
    repo->saveMessageAsync(msg, this, [&](bool) { loop.quit(); });
    loop.exec();
    
    // Insert another with same server ID
    Domain::Message msg2;
    msg2.id = "local2";
    msg2.serverId = 123;
    msg2.conversationId = "dms:charlie";
    msg2.senderId = "charlie";
    
    bool saveSuccess = true;
    repo->saveMessageAsync(msg2, this, [&](bool success) { 
        saveSuccess = success;
        loop.quit(); 
    });
    loop.exec();
    
    // SQLite UNIQUE constraint on server_id > 0 with INSERT OR REPLACE replaces it
    QVERIFY(saveSuccess);
    
    std::vector<Domain::Message> fetched;
    repo->getMessagesAsync("dms:charlie", 50, 0, this, [&](const std::vector<Domain::Message>& msgs) {
        fetched = msgs;
        loop.quit();
    });
    loop.exec();
    
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched[0].id, QString("local2"));
}

void TestMessages::testRepositoryReopenPersistence() {
    QString dbPath = "test_messages_db/test4.db";
    QFile::remove(dbPath);
    
    QEventLoop loop;
    {
        auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
        Domain::Message msg;
        msg.id = "persist1";
        msg.conversationId = "dms:dave";
        msg.senderId = "dave";
        repo->saveMessageAsync(msg, this, [&](bool) { loop.quit(); });
        loop.exec();
    } // repo destroyed
    
    {
        auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
        std::vector<Domain::Message> fetched;
        repo->getMessagesAsync("dms:dave", 50, 0, this, [&](const std::vector<Domain::Message>& msgs) {
            fetched = msgs;
            loop.quit();
        });
        loop.exec();
        QCOMPARE(fetched.size(), 1);
        QCOMPARE(fetched[0].id, QString("persist1"));
    }
}

void TestMessages::testMessageServiceIntegration() {
    QString dbPath = "test_messages_db/test5.db";
    QFile::remove(dbPath);
    auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
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

// QTEST_MAIN(TestMessages) // Do not do this since CMake builds multiple cpp files in one executable? Wait, I will just add it to test_main or similar.
