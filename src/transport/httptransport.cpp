// src/transport/httptransport.cpp
#include "httptransport.h"
#include <QUrlQuery>

namespace Avila {
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
    if (trimmed.endsWith('/')) {
        trimmed.chop(1);
    }
    return trimmed.contains("://") ? trimmed : "http://" + trimmed;
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
    }
    return req;
}

void HttpTransport::handleReply(QNetworkReply *reply, HttpResponseCallback callback) {
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray data = reply->readAll();
        QNetworkReply::NetworkError error = reply->error();
        QString errStr = reply->errorString();
        if (callback) {
            callback(statusCode, data, error, errStr);
        }
    });
}

void HttpTransport::get(const QString &endpoint, const QMap<QString, QString> &queryParams, HttpResponseCallback callback) {
    QUrl url = buildUrl(endpoint, queryParams);
    QNetworkRequest req = createRequest(url, false);
    QNetworkReply *reply = m_nam->get(req);
    handleReply(reply, std::move(callback));
}

void HttpTransport::post(const QString &endpoint, const QByteArray &jsonData, HttpResponseCallback callback) {
    QUrl url = buildUrl(endpoint, {});
    QNetworkRequest req = createRequest(url, true);
    QNetworkReply *reply = m_nam->post(req, jsonData);
    handleReply(reply, std::move(callback));
}

void HttpTransport::deleteResource(const QString &endpoint, HttpResponseCallback callback) {
    QUrl url = buildUrl(endpoint, {});
    QNetworkRequest req = createRequest(url, false);
    QNetworkReply *reply = m_nam->sendCustomRequest(req, "DELETE");
    handleReply(reply, std::move(callback));
}

} // namespace Transport
} // namespace Avila
