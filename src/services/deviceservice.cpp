// src/services/deviceservice.cpp
#include "deviceservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>

namespace Avila {
namespace Services {

DeviceService::DeviceService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Storage::ISettingsRepository> storage,
                             QObject *parent)
    : QObject(parent), m_transport(std::move(transport)), m_storage(std::move(storage)) {
}

void DeviceService::registerDevice(const QString &deviceId, const QString &publicKey) {
    if (m_storage->authToken().isEmpty()) {
        emit deviceRegistrationResult(false, "Unauthorized: No authentication token.");
        return;
    }

    QJsonObject body;
    body["device_id"] = deviceId;
    body["public_key"] = publicKey;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_DEVICE_REGISTER, postData, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        bool success = (error == QNetworkReply::NoError);
        auto doc = QJsonDocument::fromJson(data);
        QString msg = success ? "Device registered" : (doc.isNull() ? "Device registration failed" : doc.object().value("error").toString());
        emit deviceRegistrationResult(success, msg);
    });
}

void DeviceService::fetchDevicePublicKey(const QString &deviceId) {
    if (deviceId.trimmed().isEmpty()) return;

    QMap<QString, QString> params;
    params["device_id"] = deviceId.trimmed();

    m_transport->get(Constants::EP_DEVICE_KEY, params, [this, deviceId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        if (error == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.object().contains("public_key")) {
                emit deviceKeyFetched(deviceId, doc.object().value("public_key").toString());
                return;
            }
        }
        emit deviceKeyFetched(deviceId, QString());
    });
}

} // namespace Services
} // namespace Avila
