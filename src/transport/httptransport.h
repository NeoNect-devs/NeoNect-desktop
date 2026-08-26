// src/transport/httptransport.h
#pragma once
#include "ihttptransport.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <memory>
#include <mutex>

namespace Avila {
namespace Transport {

class HttpTransport : public QObject, public IHttpTransport {
    Q_OBJECT
public:
    explicit HttpTransport(QNetworkAccessManager *nam = nullptr, QObject *parent = nullptr);
    ~HttpTransport() override = default;

    void setBaseUrl(const QString &url) override;
    QString baseUrl() const override;

    void setAuthToken(const QString &token) override;
    QString authToken() const override;

    void get(const QString &endpoint, const QMap<QString, QString> &queryParams, HttpResponseCallback callback) override;
    void post(const QString &endpoint, const QByteArray &jsonData, HttpResponseCallback callback) override;
    void deleteResource(const QString &endpoint, HttpResponseCallback callback) override;

    static QString cleanUrl(const QString &input);

private:
    QUrl buildUrl(const QString &endpoint, const QMap<QString, QString> &queryParams) const;
    QNetworkRequest createRequest(const QUrl &url, bool includeJsonHeader = false) const;
    void handleReply(QNetworkReply *reply, HttpResponseCallback callback);

    QNetworkAccessManager *m_nam;
    bool m_ownsNam{false};
    mutable std::mutex m_mutex;
    QString m_baseUrl;
    QString m_authToken;
};

} // namespace Transport
} // namespace Avila
