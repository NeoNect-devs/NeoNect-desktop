#include <QtTest>
#include <QByteArray>
#include <QTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QThread>
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "../../src/crypto/cryptoservice.h"
#include "../../src/storage/sqlmessagerepository.h"
#include "../../src/storage/settingsrepository.h"
#include "../../src/domain/message.h"
#include "../../src/core/chatmessagemodel.h"
#include "../../src/services/relayservice.h"
#include "../../src/services/messageservice.h"
#include "../../src/core/application.h"
#include "../../tests/mocks/mockhttptransport.h"

using namespace NeoNect;
using namespace NeoNect::Domain;

// Utility for RSS checking on Linux
long getRSS() {
    QFile f("/proc/self/statm");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    QString line = f.readLine();
    QStringList parts = line.split(" ", Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return parts[1].toLong() * 4096; // Pages to bytes
    }
    return 0;
}


class StressHttpTransport : public QObject, public Transport::IHttpTransport {
    Q_OBJECT
public:
    void setBaseUrl(const QString&) override {}
    QString baseUrl() const override { return ""; }
    void setAuthToken(const QString&) override {}
    QString authToken() const override { return ""; }
    
    int nextStatus = 200;
    QByteArray nextBody = "{}";
    
    QNetworkReply* get(const QString&, const QMap<QString, QString>&, const QObject* context, Transport::HttpResponseCallback cb) override {
        if (context) QMetaObject::invokeMethod(const_cast<QObject*>(context), [cb, this](){ cb(nextStatus, nextBody, QNetworkReply::NoError, ""); }, Qt::QueuedConnection);
        return nullptr;
    }
    QNetworkReply* post(const QString&, const QByteArray&, const QObject* context, Transport::HttpResponseCallback cb) override {
        if (context) QMetaObject::invokeMethod(const_cast<QObject*>(context), [cb, this](){ cb(nextStatus, nextBody, QNetworkReply::NoError, ""); }, Qt::QueuedConnection);
        return nullptr;
    }
    QNetworkReply* deleteResource(const QString&, const QObject* context, Transport::HttpResponseCallback cb, const QByteArray&) override {
        if (context) QMetaObject::invokeMethod(const_cast<QObject*>(context), [cb, this](){ cb(nextStatus, nextBody, QNetworkReply::NoError, ""); }, Qt::QueuedConnection);
        return nullptr;
    }
};

class StressTests : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;
    
private slots:
    void initTestCase() {
        qRegisterMetaType<std::vector<NeoNect::Domain::Message>>("std::vector<NeoNect::Domain::Message>");
    }

    // 1. 100k Workloads
    void testLargeSqlWorkload() {
        QString dbPath = m_tempDir.path() + "/stress.db";
        auto repo = std::make_shared<Storage::SqlMessageRepository>(dbPath);
        
        std::vector<Message> msgs;
        int count = 100000;
        msgs.reserve(count);
        for(int i = 0; i < count; i++) {
            Message msg;
            msg.id = QString("msg_%1").arg(i);
            msg.conversationId = "stress_conv";
            msg.senderId = "user";
            msg.text = "Hello world";
            msg.timestamp = 1000 + i;
            msgs.push_back(msg);
        }
        
        QEventLoop loop;
        repo->saveMessagesAsync(msgs, this, [&](bool success) {
            QVERIFY(success);
            loop.quit();
        });
        loop.exec();
        
        repo->getMessagesAsync("stress_conv", count, 0, this, [&](const std::vector<Message>& results) {
            QCOMPARE(results.size(), count);
            loop.quit();
        });
        loop.exec();
    }

    // 2. 5,000 Message Relay Batches
    void testRelayBatch() {
        auto transport = std::make_shared<StressHttpTransport>();
        auto crypto = std::make_shared<Crypto::CryptoService>();
        auto settings = std::make_shared<Storage::SettingsRepository>(m_tempDir.path() + "/settings.ini");
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
        Services::RelayService relay(transport, settings, crypto);
        crypto->setMasterKey(QByteArray(32, 'a'));
        
        QJsonArray msgs;
        for (int i = 0; i < 5000; i++) {
            QJsonObject payload;
            payload["message_id"] = QString("batch_%1").arg(i);
            payload["sender_id"] = "alice";
            
            auto enc = crypto->encryptAesGcm(QJsonDocument(payload).toJson());
            QJsonObject msg;
            msg["id"] = i;
            msg["ciphertext"] = QString::fromLatin1(enc.cipherWithTag.toBase64());
            msg["nonce"] = QString::fromLatin1(enc.nonce.toBase64());
            msgs.append(msg);
        }
        QJsonObject root; root["messages"] = msgs;
        transport->nextStatus = 200; transport->nextBody = QJsonDocument(root).toJson();
        
        QSignalSpy spy(&relay, &Services::RelayService::incomingDomainMessagesReceived);
        relay.pollPendingMessages();
        spy.wait(1000);
        QCOMPARE(spy.count(), 1);
        auto batch = qvariant_cast<std::vector<Message>>(spy.first().at(0));
        QCOMPARE(batch.size(), 5000);
    }
    
    // 3. Rapid Send/Receive
    void testRapidSendReceive() {
        auto transport = std::make_shared<StressHttpTransport>();
        auto repo = std::make_shared<Storage::SqlMessageRepository>(m_tempDir.path() + "/rapid.db");
        auto crypto = std::make_shared<Crypto::CryptoService>();
        auto settings = std::make_shared<Storage::SettingsRepository>(m_tempDir.path() + "/settings_rapid.ini");
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
        
        Services::MessageService msgSvc(repo);
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
        crypto->setMasterKey(QByteArray(32, 'a'));
        
        transport->nextStatus = 200; transport->nextBody = "{}";
        
        // Spam 1000 messages
        for(int i = 0; i < 1000; i++) {
            msgSvc.sendMessage("dms:bob", "text", "rapid text");
        }
        
        // Simulate rapid incoming
        std::vector<Message> incoming;
        for(int i = 0; i < 1000; i++) {
            Message m;
            m.id = QString("inc_%1").arg(i);
            m.conversationId = "dms:bob";
            m.text = "rapid inc";
            incoming.push_back(m);
        }
        msgSvc.handleIncomingMessages(incoming);
        
        QTest::qWait(100); // Give async queue time
    }

    // 4. Reconnect & Repeated 401s
    void testRepeated401s() {
        auto transport = std::make_shared<StressHttpTransport>();
        auto crypto = std::make_shared<Crypto::CryptoService>();
        auto settings = std::make_shared<Storage::SettingsRepository>(m_tempDir.path() + "/settings_401.ini");
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
        Services::RelayService relay(transport, settings, crypto);
        crypto->setMasterKey(QByteArray(32, 'a'));
        settings->setAuthToken("bad_token");
        
        transport->nextStatus = 401; transport->nextBody = "{}"; // Device registration fail
        QSignalSpy spyReg(&relay, &Services::RelayService::deviceRegistrationRequested);
        
        for (int i = 0; i < 50; i++) {
            transport->nextStatus = 401; transport->nextBody = "{}";
            relay.pollPendingMessages();
        }
        spyReg.wait(1000);
        QVERIFY(spyReg.count() > 0);
    }

    // 5. Active Shutdown
    void testActiveShutdown() {

        for (int i = 0; i < 100; i++) {
            auto transport = std::make_shared<StressHttpTransport>();
            auto crypto = std::make_shared<Crypto::CryptoService>();
            auto settings = std::make_shared<Storage::SettingsRepository>(m_tempDir.path() + QString("/set_%1.ini").arg(i));
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
            auto repo = std::make_shared<Storage::SqlMessageRepository>(m_tempDir.path() + QString("/db_%1.sqlite").arg(i));
            
            auto relay = std::make_unique<Services::RelayService>(transport, settings, crypto);
            auto msgSvc = std::make_unique<Services::MessageService>(repo);
            
            transport->nextStatus = 200;
            transport->nextBody = "{}";
            relay->pollPendingMessages();
            msgSvc->sendMessage("dms:bob", "text", "hello");
            
            // Destroy immediately while async operations are running
            msgSvc.reset();
            relay.reset();
        }

    }

    // 6. Conversation Switching
    void testConversationSwitching() {
        ChatMessageModel model;
        for (int i = 0; i < 1000; i++) {
            model.setActiveConversation(QString("dms:%1").arg(i));
        }
    }

    // 7. Large QML Lists
    void testLargeQmlList() {
        // Just verify model works with 100k
        ChatMessageModel model;
        QVariantList msgs;
        for (int i = 0; i < 100000; i++) {
            QVariantMap m;
            m["id"] = QString::number(i);
            m["text"] = "text";
            msgs.append(m);
        }
        model.setActiveConversation("dms:large");
        model.onConversationLoaded("dms:large", msgs);
        QCOMPARE(model.rowCount(), 100000);
    }

    // 8. Memory Growth
    void testMemoryGrowth() {
        long startRSS = getRSS();
        for (int i = 0; i < 5; i++) {
            testLargeSqlWorkload(); // Heavy workload
        }
        long endRSS = getRSS();
        if (startRSS > 0) {
            // Check that it didn't grow unbounded (e.g. not >100MB growth)
            long growthMB = (endRSS - startRSS) / (1024 * 1024);
            QVERIFY(growthMB < 200);
        }
    }

    // 9. Malformed Input Fuzzing
    void testMalformedInput() {
        auto transport = std::make_shared<StressHttpTransport>();
        auto crypto = std::make_shared<Crypto::CryptoService>();
        auto settings = std::make_shared<Storage::SettingsRepository>(m_tempDir.path() + "/fuzz.ini");
        settings->setAuthToken("token");
        settings->setDeviceId("dev");
        Services::RelayService relay(transport, settings, crypto);
        crypto->setMasterKey(QByteArray(32, 'a'));
        crypto->setMasterKey(QByteArray(32, 'a'));
        
        QSignalSpy spy(&relay, &Services::RelayService::incomingDomainMessagesReceived);

        // Bad JSON
        transport->nextStatus = 200; transport->nextBody = "{ bad json }";
        relay.pollPendingMessages();
        
        // Missing fields
        transport->nextStatus = 200; transport->nextBody = "{\"messages\":[{}]}";
        relay.pollPendingMessages();

        // Bad Base64
        transport->nextStatus = 200; transport->nextBody = "{\"messages\":[{\"ciphertext\":\"!@#\",\"nonce\":\"bad\"}]}";
        relay.pollPendingMessages();
        
        QTest::qWait(100);
        QCOMPARE(spy.count(), 0);
    }

    // 10. Database Failure
    void testDbFailure() {
        // Use read-only or invalid dir
        auto repo = std::make_shared<Storage::SqlMessageRepository>("/invalid_dir/db.sqlite");
        
        QEventLoop loop;
        Message msg;
        repo->saveMessageAsync(msg, this, [&](bool success) {
            QVERIFY(!success);
            loop.quit();
        });
        loop.exec();
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    StressTests tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "main_stress.moc"
