// tests/mocks/mockhttptransport.h
#pragma once
#include "../../src/transport/ihttptransport.h"
#include <QObject>
#include <QMap>
#include <QStringList>
#include <QDateTime>
#include <mutex>
#include <vector>

namespace Avila {
namespace Testing {

struct MockQueuedMessage {
    qint64 id{0};
    QString deviceId;
    QString toUsername;
    QString ciphertextBase64;
    qint64 timestamp{0};
};

struct MockUser {
    int64_t id{0};
    QString username;
    QString password;
    QString sessionToken;
    QMap<QString, QString> devices; // device_id -> public_key
};

/**
 * @brief In-memory / File-backed mock HTTP transport simulating the Danisa REST API.
 * Supports multi-process communication across separate client instances via shared state.
 */
class MockHttpTransport : public QObject, public Transport::IHttpTransport {
    Q_OBJECT
public:
    explicit MockHttpTransport(bool enableSharedStorage = false, bool enableEchoBot = false, QObject *parent = nullptr);
    ~MockHttpTransport() override = default;

    void setBaseUrl(const QString &url) override;
    QString baseUrl() const override;

    void setAuthToken(const QString &token) override;
    QString authToken() const override;

    void get(const QString &endpoint, const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback) override;
    void post(const QString &endpoint, const QByteArray &jsonData, Transport::HttpResponseCallback callback) override;
    void deleteResource(const QString &endpoint, Transport::HttpResponseCallback callback) override;

    // Test Control Helpers
    void seedUser(const QString &username, const QString &password);
    void setSimulateNetworkError(bool simulate);
    void setSimulateHttpError(int statusCode);
    int queuedMessageCount(const QString &deviceId) const;
    void clearAllQueues();

signals:
    void requestHandled(const QString &method, const QString &endpoint);

private:
    void loadSharedState();
    void saveSharedState();

    void handleHealth(Transport::HttpResponseCallback callback);
    void handleUsers(const QByteArray &data, Transport::HttpResponseCallback callback);
    void handleAvailability(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback);
    void handleAuth(const QByteArray &data, Transport::HttpResponseCallback callback);
    void handleAuthDelete(Transport::HttpResponseCallback callback);
    void handleUsersMe(Transport::HttpResponseCallback callback);
    void handleDeviceRegister(const QByteArray &data, Transport::HttpResponseCallback callback);
    void handleDeviceKey(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback);
    void handleRelaySend(const QByteArray &data, Transport::HttpResponseCallback callback);
    void handleRelayPoll(const QMap<QString, QString> &queryParams, Transport::HttpResponseCallback callback);
    void handleRelayAck(const QByteArray &data, Transport::HttpResponseCallback callback);

    mutable std::recursive_mutex m_mutex;
    QString m_baseUrl{"http://mock.avila.local"};
    QString m_activeToken;
    bool m_enableSharedStorage{false};
    bool m_enableEchoBot{false};
    bool m_simulateNetworkError{false};
    int m_simulateHttpError{0};

    int64_t m_nextUserId{100};
    qint64 m_nextMessageId{1000};
    QMap<QString, MockUser> m_users; // username -> MockUser
    QMap<QString, QString> m_tokenToUser; // token -> username
    std::vector<MockQueuedMessage> m_deliveryQueue;
};

} // namespace Testing
} // namespace Avila
