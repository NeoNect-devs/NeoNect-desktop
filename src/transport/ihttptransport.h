// src/transport/ihttptransport.h
#pragma once
#include <QString>
#include <QByteArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>

namespace Avila {
namespace Transport {

using HttpResponseCallback = std::function<void(int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errorString)>;

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;

    virtual void setBaseUrl(const QString &url) = 0;
    virtual QString baseUrl() const = 0;

    virtual void setAuthToken(const QString &token) = 0;
    virtual QString authToken() const = 0;

    virtual void get(const QString &endpoint, const QMap<QString, QString> &queryParams, HttpResponseCallback callback) = 0;
    virtual void post(const QString &endpoint, const QByteArray &jsonData, HttpResponseCallback callback) = 0;
    virtual void deleteResource(const QString &endpoint, HttpResponseCallback callback) = 0;
};

} // namespace Transport
} // namespace Avila
