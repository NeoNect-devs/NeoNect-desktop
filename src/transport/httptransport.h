// src/transport/httptransport.h
#pragma once
#include "ihttptransport.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <memory>
#include <mutex>

namespace NeoNect {
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

    QNetworkReply* get(const QString &endpoint, const QMap<QString, QString> &queryParams, const QObject* context, HttpResponseCallback callback) override;
    QNetworkReply* post(const QString &endpoint, const QByteArray &jsonData, const QObject* context, HttpResponseCallback callback) override;
    QNetworkReply* deleteResource(const QString &endpoint, const QObject* context, HttpResponseCallback callback, const QByteArray &jsonData = QByteArray()) override;

    static QString cleanUrl(const QString &input);

private:
    QUrl buildUrl(const QString &endpoint, const QMap<QString, QString> &queryParams) const;
    QNetworkRequest createRequest(const QUrl &url, bool includeJsonHeader = false) const;
    void handleReply(QNetworkReply *reply, const QObject* context, HttpResponseCallback callback);

    QNetworkAccessManager *m_nam;
    bool m_ownsNam{false};
    mutable std::mutex m_mutex;
    QString m_baseUrl;
    QString m_authToken;
};

} // namespace Transport
} // namespace NeoNect
