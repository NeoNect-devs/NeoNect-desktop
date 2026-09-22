// src/transport/httptransport.cpp
#include "httptransport.h"
#include <QUrlQuery>

namespace NeoNect {
namespace Transport {

HttpTransport::HttpTransport(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam) {
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        m_ownsNam = true;
    }
}

QString HttpTransport::cleanUrl(const QString &input) {
    QString trimmed = input.trimmed();
    while (trimmed.endsWith('/')) {
        trimmed.chop(1);
    }
    if (trimmed.isEmpty()) {
        return trimmed;
    }
    if (trimmed.contains("://")) {
        return trimmed;
    }
    static const QStringList localPrefixes = {
        "localhost", "127.0.0.1", "0.0.0.0", "host.docker.internal",
        "192.168.", "10.", "172."
    };
    for (const QString &prefix : localPrefixes) {
        if (trimmed.startsWith(prefix, Qt::CaseInsensitive)) {
            return "http://" + trimmed;
        }
    }
    if (trimmed.endsWith(".local", Qt::CaseInsensitive) ||
        trimmed.contains(":8080") ||
        trimmed.contains(":3000") ||
        trimmed.contains(":8000") ||
        trimmed.contains(":80")) {
        return "http://" + trimmed;
    }
    return "https://" + trimmed;
}

void HttpTransport::setBaseUrl(const QString &url) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_baseUrl = cleanUrl(url);
}

QString HttpTransport::baseUrl() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_baseUrl;
}

void HttpTransport::setAuthToken(const QString &token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authToken = token;
}

QString HttpTransport::authToken() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_authToken;
}

QUrl HttpTransport::buildUrl(const QString &endpoint, const QMap<QString, QString> &queryParams) const {
    QString base;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        base = m_baseUrl;
    }

    QUrl url(base + (endpoint.startsWith('/') ? endpoint : "/" + endpoint));
    if (!queryParams.isEmpty()) {
        QUrlQuery query;
        for (auto it = queryParams.cbegin(); it != queryParams.cend(); ++it) {
            query.addQueryItem(it.key(), it.value());
        }
        url.setQuery(query);
    }
    return url;
}

QNetworkRequest HttpTransport::createRequest(const QUrl &url, bool includeJsonHeader) const {
    QNetworkRequest req(url);
    if (includeJsonHeader) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }

    QString token;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        token = m_authToken;
    }

    if (!token.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
        req.setRawHeader("Cookie", "neonect_sid=" + token.toUtf8());
    }
    return req;
}

void HttpTransport::handleReply(QNetworkReply *reply, const QObject* context, HttpResponseCallback callback) {
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    if (context && callback) {
        connect(reply, &QNetworkReply::finished, context, [reply, callback]() {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray data = reply->readAll();
            QNetworkReply::NetworkError error = reply->error();
            QString errStr = reply->errorString();
            callback(statusCode, data, error, errStr);
        });
    }
}

QNetworkReply* HttpTransport::get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject* context, HttpResponseCallback callback) {
    QUrl url = buildUrl(endpoint, queryParams);
    QNetworkRequest req = createRequest(url, false);
    QNetworkReply *reply = m_nam->get(req);
    handleReply(reply, context, std::move(callback));
    return reply;
}

QNetworkReply* HttpTransport::post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, HttpResponseCallback callback) {
    QUrl url = buildUrl(endpoint, {});
    QNetworkRequest req = createRequest(url, true);
    QNetworkReply *reply = m_nam->post(req, jsonData);
    handleReply(reply, context, std::move(callback));
    return reply;
}

QNetworkReply* HttpTransport::deleteResource(const QString &endpoint, const QObject* context, HttpResponseCallback callback, const QByteArray &jsonData) {
    QUrl url = buildUrl(endpoint, {});
    bool hasBody = !jsonData.isEmpty();
    QNetworkRequest req = createRequest(url, hasBody);
    QNetworkReply *reply = hasBody ? m_nam->sendCustomRequest(req, "DELETE", jsonData) : m_nam->sendCustomRequest(req, "DELETE");
    handleReply(reply, context, std::move(callback));
    return reply;
}

} // namespace Transport
} // namespace NeoNect
