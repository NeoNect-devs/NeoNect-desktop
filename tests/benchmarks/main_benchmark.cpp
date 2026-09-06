#include <QtTest>
#include <QByteArray>
#include <QTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include "../../src/crypto/cryptoservice.h"
#include "../../src/storage/sqlmessagerepository.h"
#include "../../src/domain/message.h"
#include "../../src/core/chatmessagemodel.h"

using namespace NeoNect::Crypto;

class BenchmarkCrypto : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        m_crypto = std::make_shared<CryptoService>();
    }

    void benchmarkEncryption_4KB() { runEncryption(4096); }
    void benchmarkEncryption_64KB() { runEncryption(65536); }
    void benchmarkEncryption_1MB() { runEncryption(1024 * 1024); }
    void benchmarkEncryption_5MB() { runEncryption(5 * 1024 * 1024); }

    void benchmarkDecryption_4KB() { runDecryption(4096); }
    void benchmarkDecryption_64KB() { runDecryption(65536); }
    void benchmarkDecryption_1MB() { runDecryption(1024 * 1024); }
    void benchmarkDecryption_5MB() { runDecryption(5 * 1024 * 1024); }

private:
    void runEncryption(int size) {
        QByteArray plainText(size, 'A');
        QByteArray sharedKey(32, 'K');
        QByteArray nonce(12, 'N');
        
        QBENCHMARK {
            auto res = m_crypto->encryptAesGcm(plainText, sharedKey);
        }
    }

    void runDecryption(int size) {
        QByteArray plainText(size, 'A');
        QByteArray sharedKey(32, 'K');
        QByteArray nonce(12, 'N');
        auto res = m_crypto->encryptAesGcm(plainText, sharedKey);
        QByteArray cipher = res.cipherWithTag;
        QByteArray resNonce = res.nonce;

        QBENCHMARK {
            QByteArray decrypted = m_crypto->decryptAesGcm(cipher, resNonce, sharedKey);
        }
    }

    std::shared_ptr<CryptoService> m_crypto;
};

// We will use a custom main to run all benchmarks




using namespace NeoNect::Storage;
using namespace NeoNect::Domain;

class BenchmarkSql : public QObject {
    Q_OBJECT

private:
    std::shared_ptr<SqlMessageRepository> m_repo;
    QString m_dbPath;

private slots:
    void init() {
        m_dbPath = "test_bench_db.db";
        QFile::remove(m_dbPath);
        m_repo = std::make_shared<SqlMessageRepository>(m_dbPath);
    }

    void cleanup() {
        m_repo.reset();
        QFile::remove(m_dbPath);
    }

    void benchmarkSingleInsert() {
        Message msg;
        msg.id = "single_msg";
        msg.conversationId = "dms:test";
        msg.senderId = "test";
        msg.text = "Hello world";
        msg.timestamp = 1000;

        QEventLoop loop;
        QBENCHMARK {
            m_repo->saveMessageAsync(msg, this, [&](bool) {
                loop.quit();
            });
            loop.exec();
        }
    }

    void benchmarkBatchInsert_100() { runBatchInsert(100); }
    void benchmarkBatchInsert_1000() { runBatchInsert(1000); }
    
    void benchmarkRetrieval_100() { runRetrieval(100); }
    void benchmarkRetrieval_1000() { runRetrieval(1000); }
    void benchmarkRetrieval_10000() { runRetrieval(10000); }

private:
    void runBatchInsert(int count) {
        std::vector<Message> msgs;
        for (int i = 0; i < count; ++i) {
            Message msg;
            msg.id = QString("msg_%1").arg(i);
            msg.conversationId = "dms:batch";
            msg.senderId = "user";
            msg.text = "Batch message";
            msg.timestamp = 1000 + i;
            msgs.push_back(msg);
        }

        
        QBENCHMARK {
            QEventLoop loop;
            m_repo->saveMessagesAsync(msgs, this, [&](bool) {
                loop.quit();
            });
            loop.exec();
        }

    }

    void runRetrieval(int count) {
        
        QEventLoop loop;
        std::vector<Message> msgs;
        for (int i = 0; i < count; ++i) {
            Message msg;
            msg.id = QString("msg_rt_%1").arg(i);
            msg.conversationId = "dms:retrieve";
            msg.senderId = "user";
            msg.text = "Retrieve me";
            msg.timestamp = 1000 + i;
            msgs.push_back(msg);
        }
        m_repo->saveMessagesAsync(msgs, this, [&](bool) {
            loop.quit();
        });
        if (count > 0) loop.exec();


        QBENCHMARK {
            QEventLoop rloop;
            m_repo->getMessagesAsync("dms:retrieve", count, QDateTime::currentSecsSinceEpoch() * 1000, this, [&](const std::vector<Message>&) {
                rloop.quit();
            });
            rloop.exec();
        }
    }
};




using namespace NeoNect::Domain;

class BenchmarkSerialization : public QObject {
    Q_OBJECT

private slots:
    void benchmarkSerialize_Small() { runSerialize(100); }
    void benchmarkSerialize_Medium() { runSerialize(1000); }

    void benchmarkDeserialize_Small() { runDeserialize(100); }
    void benchmarkDeserialize_Medium() { runDeserialize(1000); }

private:
    Message createMockMessage(int sizeFactor) {
        Message msg;
        msg.id = QUuid::createUuid().toString();
        msg.senderId = "alice";
        msg.text = QString(sizeFactor, 'A');
        return msg;
    }

    void runSerialize(int textLength) {
        Message msg = createMockMessage(textLength);
        
        QBENCHMARK {
            QJsonObject packet;
            packet["messageId"] = msg.id;
            packet["sender"] = msg.senderId;
            packet["content"] = msg.text;
            packet["type"] = msg.type;
            QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);
        }
    }

    void runDeserialize(int textLength) {
        Message msg = createMockMessage(textLength);
        QJsonObject packet;
        packet["messageId"] = msg.id;
        packet["sender"] = msg.senderId;
        packet["content"] = msg.text;
        packet["type"] = msg.type;
        QByteArray packetBytes = QJsonDocument(packet).toJson(QJsonDocument::Compact);

        QBENCHMARK {
            auto packetDoc = QJsonDocument::fromJson(packetBytes);
            if (!packetDoc.isNull() && packetDoc.isObject()) {
                QJsonObject packetObj = packetDoc.object();
                Message domainMsg;
                if (packetObj.contains("messageId")) domainMsg.id = packetObj.value("messageId").toString();
                if (packetObj.contains("sender")) domainMsg.senderId = packetObj.value("sender").toString();
                if (packetObj.contains("content")) domainMsg.text = packetObj.value("content").toString();
                if (packetObj.contains("type")) domainMsg.type = packetObj.value("type").toString();
            }
        }
    }
};




using namespace NeoNect;
using namespace NeoNect::Domain;

class BenchmarkModel : public QObject {
    Q_OBJECT

private slots:
    void benchmarkInitialPopulation_100() { runPopulation(100); }
    void benchmarkInitialPopulation_1000() { runPopulation(1000); }
    void benchmarkInitialPopulation_10000() { runPopulation(10000); }

    void benchmarkIncrementalInsert_100() { runIncremental(100); }
    void benchmarkIncrementalInsert_1000() { runIncremental(1000); }

private:

    QVariantMap toVariantMap(const Message& msg) {
        QVariantMap map;
        map["id"] = msg.id;
        map["conversationId"] = msg.conversationId;
        map["senderId"] = msg.senderId;
        map["text"] = msg.text;
        map["timestamp"] = msg.timestamp;
        map["status"] = static_cast<int>(msg.status);
        return map;
    }
    std::vector<Message> createMessages(int count) {
        std::vector<Message> msgs;
        msgs.reserve(count);
        for (int i = 0; i < count; ++i) {
            Message msg;
            msg.id = QString("msg_%1").arg(i);
            msg.conversationId = "dms:test";
            msg.senderId = "alice";
            msg.text = "Hello world";
            msg.timestamp = 1000 + i;
            msgs.push_back(msg);
        }
        return msgs;
    }

    void runPopulation(int count) {
        auto msgs = createMessages(count);
        
        QBENCHMARK {
            ChatMessageModel model;
            model.setActiveConversation("dms:test");
            
            QVariantList vMsgs;
            for (const auto& m : msgs) vMsgs.append(toVariantMap(m));
            model.onConversationLoaded("dms:test", vMsgs);

        }
    }

    void runIncremental(int count) {
        auto msgs = createMessages(count);
        
        QBENCHMARK_ONCE {
            ChatMessageModel model;
            model.setActiveConversation("dms:test");
            for (const auto& msg : msgs) {
                
                model.onMessageAdded("dms:test", toVariantMap(msg));

            }
        }
    }
};




int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    int status = 0;

    { BenchmarkCrypto tc; status |= QTest::qExec(&tc, argc, argv); }
    { BenchmarkSql tc; status |= QTest::qExec(&tc, argc, argv); }
    { BenchmarkSerialization tc; status |= QTest::qExec(&tc, argc, argv); }
    { BenchmarkModel tc; status |= QTest::qExec(&tc, argc, argv); }

    return status;
}

#include "main_benchmark.moc"
