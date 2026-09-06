#pragma once
#include "storage/imessagerepository.h"
#include <QObject>
#include <QThread>
#include <QString>

namespace NeoNect {
namespace Storage {

class SqlMessageRepositoryWorker;

class SqlMessageRepository : public QObject, public IMessageRepository {
    Q_OBJECT
public:
    explicit SqlMessageRepository(const QString& dbPath, QObject* parent = nullptr);
    ~SqlMessageRepository() override;

    void saveMessageAsync(const Domain::Message &msg, const QObject* context, SaveCallback callback) override;
    void saveMessagesAsync(const std::vector<Domain::Message> &msgs, const QObject* context, SaveCallback callback) override;
    void updateMessageStatusAsync(const QString &id, Domain::MessageStatus status, const QString &errorText, const QObject* context, SaveCallback callback) override;
    void getMessagesAsync(const QString &conversationId, int limit, qint64 beforeTimestamp, const QObject* context, FetchCallback callback) override;

private:
    QThread* m_workerThread;
    SqlMessageRepositoryWorker* m_worker;
};

}
}
