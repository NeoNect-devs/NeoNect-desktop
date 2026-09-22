/**
 * @file httptransport.h
 * @brief Concrete implementation of IHttpTransport backed by Qt's QNetworkAccessManager.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Handles dispatching asynchronous HTTP requests (GET, POST, DELETE) over SSL/TLS.
 * Features URL normalization via @ref NeoNect::Transport::HttpTransport::cleanUrl, automatic Bearer authentication header
 * injection, thread-safe configuration mutation via `std::mutex`, and object-lifetime-guarded
 * completion callbacks.
 *
 * @par Design Patterns:
 * - <b>Concrete Strategy / Adapter</b>: Adapts Qt's `QNetworkAccessManager` to the `IHttpTransport` interface.
 * - <b>Dependency Injection</b>: Can adopt an externally managed `QNetworkAccessManager` or create its own.
 * - <b>Thread-Safe Monitor</b>: Mutex synchronization guards endpoint and auth token mutations.
 *
 * @par Transport & Network Constraints:
 * - <b>TLS Security</b>: Production deployments enforce TLS 1.3/1.2; cleartext HTTP is permitted only for localhost development.
 * - <b>Request Timeout</b>: Configured timeout bounds: connection timeout (10 seconds), response completion timeout (30 seconds).
 * - <b>Payload Bounds</b>: Outgoing JSON request bodies are bounded to a maximum of 10 MB (10,485,760 bytes).
 * - <b>Concurrency Invariant</b>: Thread-safe configuration updates; mutations to `m_baseUrl` and `m_authToken`
 *   are synchronized via `std::lock_guard<std::mutex>`.
 * - <b>Callback Lifetime Guard</b>: Callbacks bound to `QObject* context` are safely aborted if the context
 *   is destroyed before the HTTP response arrives, guaranteeing zero dangling pointer dereferences.
 */

#pragma once
#include "ihttptransport.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <memory>
#include <mutex>

namespace NeoNect {
namespace Transport {

/**
 * @class HttpTransport
 * @brief Production HTTP client transport engine.
 *
 * @par Operational Bounds:
 * - Max endpoint length: 512 characters.
 * - Max payload size: 10 MB.
 * - Timeout: 30 seconds.
 */
class HttpTransport : public QObject, public IHttpTransport {
    Q_OBJECT
public:
    /**
     * @brief Constructs the HTTP transport engine.
     * @param nam Optional pre-existing QNetworkAccessManager (e.g. shared across application).
     * @param parent Optional parent QObject for Qt object hierarchy.
     */
    explicit HttpTransport(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);

    /**
     * @brief Destructor. Cleans up owned QNetworkAccessManager if created internally.
     */
    ~HttpTransport() override = default;

    /**
     * @brief Configures the server base URL with automatic normalization.
     * @param url Server address (e.g. `"api.neonect.chat"` or `"http://127.0.0.1:8080"`).
     * @pre `url` must follow RFC 3986 syntax or domain form.
     * @post Base URL is cleaned via @ref cleanUrl and saved under mutex lock.
     */
    void setBaseUrl(const QString &url) override;

    /**
     * @brief Retrieves the normalized base URL.
     * @return Sanitized base URL string.
     */
    QString baseUrl() const override;

    /**
     * @brief Stores the Bearer authentication token attached to outgoing requests.
     * @param token JWT or API secret token.
     * @pre `token` must be valid compact JWT or empty string. Max length 4096 bytes.
     * @post Token is stored under mutex lock and injected into `Authorization: Bearer <token>` headers.
     */
    void setAuthToken(const QString &token) override;

    /**
     * @brief Retrieves the active Bearer token.
     * @return Active auth token string or empty string.
     */
    QString authToken() const override;

    /**
     * @brief Dispatches an asynchronous GET request.
     * @param endpoint Sub-path (e.g. `"/api/v1/auth/verify"`).
     * @param queryParams Key-value URL query parameters.
     * @param context Lifetime tracking QObject.
     * @param callback Asynchronous response callback.
     * @return Underlying QNetworkReply pointer.
     * @pre `endpoint` must start with `'/'` or be relative to base URL.
     * @post If `context` remains valid upon HTTP reply arrival, `callback` is invoked on caller thread.
     */
    QNetworkReply* get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject* context, HttpResponseCallback callback) override;

    /**
     * @brief Dispatches an asynchronous POST request with JSON payload.
     * @param endpoint Sub-path.
     * @param jsonData UTF-8 serialized JSON payload.
     * @param context Lifetime tracking QObject.
     * @param callback Asynchronous response callback.
     * @return Underlying QNetworkReply pointer.
     * @pre `jsonData.size() <= 10485760` (10 MB).
     * @post Outgoing request includes `Content-Type: application/json`.
     */
    QNetworkReply* post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, HttpResponseCallback callback) override;

    /**
     * @brief Dispatches an asynchronous DELETE request.
     * @param endpoint Sub-path.
     * @param context Lifetime tracking QObject.
     * @param callback Asynchronous response callback.
     * @param jsonData Optional request payload.
     * @return Underlying QNetworkReply pointer.
     * @pre `jsonData.size() <= 10485760` (10 MB).
     * @post Asynchronous HTTP DELETE request is submitted to network manager.
     */
    QNetworkReply* deleteResource(const QString &endpoint, const QObject* context, HttpResponseCallback callback, const QByteArray &jsonData = QByteArray()) override;

    /**
     * @brief Normalizes raw user input into a valid HTTP/HTTPS URL.
     * @param input Raw address input (e.g. `"localhost:8080"`, `"neonect.chat/"`).
     * @return Sanitized URL string (e.g. `"http://localhost:8080"`, `"http://neonect.chat"`).
     * @post Prepends `"http://"` if protocol prefix is missing and strips trailing slashes.
     */
    static QString cleanUrl(const QString &input);

private:
    /** @brief Constructs a full QUrl from endpoint and query parameters. */
    QUrl buildUrl(const QString &endpoint, const QMap<QString, QString> &queryParams) const;
    /** @brief Prepares standard headers, authorization token, and content type. */
    QNetworkRequest createRequest(const QUrl &url, bool includeJsonHeader = false) const;
    /** @brief Connects QNetworkReply signals, parses response code, and invokes user callback. */
    void handleReply(QNetworkReply *reply, const QObject* context, HttpResponseCallback callback);

    /** @brief Underlying network manager. */
    QNetworkAccessManager *m_nam;
    /** @brief Flag indicating if this instance owns @ref m_nam. */
    bool m_ownsNam{false};
    /** @brief Mutex guarding concurrent configuration updates across threads. */
    mutable std::mutex m_mutex;
    /** @brief Sanitized server base URL. */
    QString m_baseUrl;
    /** @brief Bearer authorization token. */
    QString m_authToken;
};

} // namespace Transport
} // namespace NeoNect
