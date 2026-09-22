/**
 * @file ihttptransport.h
 * @brief Abstract interface defining HTTP client transport operations.
 * @author NeoNect Development Team
 * @date 2026
 *
 * @details
 * Declares the HTTP transport layer abstraction for the NeoNect client. Services communicate
 * with the RESTful backend via this contract, allowing tests to inject mock network drivers.
 *
 * @par Design Patterns:
 * - <b>Strategy / Adapter Pattern</b>: Decouples REST client business logic from `QNetworkAccessManager`.
 * - <b>Asynchronous Callback with Context Guard</b>: Guarantees memory safety when objects are destroyed during in-flight requests.
 */

#pragma once
#include <QString>
#include <QByteArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>

namespace NeoNect {
namespace Transport {

/**
 * @brief Asynchronous completion callback for HTTP requests.
 * @param statusCode HTTP response status code (e.g. 200, 401, 404). 0 if a network/connection error occurred.
 * @param data Response payload byte buffer.
 * @param error Qt network error enum indicating failure category.
 * @param errorString Human-readable error description.
 */
using HttpResponseCallback = std::function<void(int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errorString)>;

/**
 * @class IHttpTransport
 * @brief Interface contract for sending asynchronous HTTP requests.
 */
class IHttpTransport {
public:
    /**
     * @brief Virtual destructor for clean polymorphic disposal.
     */
    virtual ~IHttpTransport() = default;

    /**
     * @brief Configures the server base URL (e.g. `"http://localhost:8080"`).
     * @param url Server base URL.
     */
    virtual void setBaseUrl(const QString &url) = 0;

    /**
     * @brief Retrieves the active server base URL.
     */
    virtual QString baseUrl() const = 0;

    /**
     * @brief Configures the bearer authorization token attached to subsequent requests.
     * @param token Authentication token string.
     */
    virtual void setAuthToken(const QString &token) = 0;

    /**
     * @brief Retrieves the currently configured authorization token.
     */
    virtual QString authToken() const = 0;

    /**
     * @brief Dispatches an asynchronous HTTP GET request.
     * @param endpoint Target route path (e.g. `"/api/v1/auth/verify"`).
     * @param queryParams Map of URL query string key-value pairs.
     * @param context Lifetime tracking QObject. If destroyed before reply arrives, callback is safely ignored.
     * @param callback Completion handler.
     * @return Underlying QNetworkReply pointer (managed by transport).
     */
    virtual QNetworkReply* get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject* context, HttpResponseCallback callback) = 0;

    /**
     * @brief Dispatches an asynchronous HTTP POST request with JSON payload.
     * @param endpoint Target route path.
     * @param jsonData UTF-8 serialized JSON payload.
     * @param context Lifetime tracking QObject.
     * @param callback Completion handler.
     * @return Underlying QNetworkReply pointer.
     */
    virtual QNetworkReply* post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, HttpResponseCallback callback) = 0;

    /**
     * @brief Dispatches an asynchronous HTTP DELETE request.
     * @param endpoint Target route path.
     * @param context Lifetime tracking QObject.
     * @param callback Completion handler.
     * @param jsonData Optional payload data.
     * @return Underlying QNetworkReply pointer.
     */
    virtual QNetworkReply* deleteResource(const QString &endpoint, const QObject* context, HttpResponseCallback callback, const QByteArray &jsonData = QByteArray()) = 0;
};

} // namespace Transport
} // namespace NeoNect
