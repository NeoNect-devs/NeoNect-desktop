// src/services/deviceservice.cpp
#include "deviceservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QRandomGenerator>

namespace NeoNect {
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

    QString effectiveDevId = deviceId.trimmed();
    if (effectiveDevId.isEmpty()) {
        effectiveDevId = m_storage->deviceId();
        if (effectiveDevId.isEmpty()) {
            QString prof = m_storage->profile();
            effectiveDevId = QString("neonect-dev-%1%2").arg(prof.isEmpty() ? "" : prof + "-",
                                                            QUuid::createUuid().toString(QUuid::WithoutBraces));
            m_storage->setDeviceId(effectiveDevId);
        }
    }

    QString effectivePubKey = publicKey.trimmed();
    if (effectivePubKey.isEmpty()) {
        effectivePubKey = m_storage->publicKey();
        if (effectivePubKey.isEmpty()) {
            QByteArray keyBytes(32, 0);
            QRandomGenerator::system()->generate(reinterpret_cast<quint32*>(keyBytes.data()),
                                                 reinterpret_cast<quint32*>(keyBytes.data() + keyBytes.size()));
            effectivePubKey = QString::fromLatin1(keyBytes.toBase64());
            m_storage->setPublicKey(effectivePubKey);
        }
    }

    QJsonObject body;
    body["device_id"] = effectiveDevId;
    body["public_key"] = effectivePubKey;

    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_DEVICE_REGISTER, postData, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        bool success = (error == QNetworkReply::NoError || statusCode == 201 || statusCode == 200 || statusCode == 409);
        auto doc = QJsonDocument::fromJson(data);
        QString msg = (statusCode == 409) ? "Device already registered" : (success ? "Device registered" : (doc.isNull() ? "Device registration failed" : doc.object().value("error").toString()));
        emit deviceRegistrationResult(success, msg);
    });
}

void DeviceService::fetchDevicePublicKey(const QString &deviceId) {
    if (deviceId.trimmed().isEmpty()) return;

    QMap<QString, QString> params;
    params["device_id"] = deviceId.trimmed();

    m_transport->get(Constants::EP_DEVICE_KEY, params, this, [this, deviceId](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
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

void DeviceService::revokeDevice(const QString &deviceId) {
    if (deviceId.trimmed().isEmpty()) {
        emit deviceRevocationResult(false, "Device ID cannot be empty.");
        return;
    }

    QJsonObject body;
    body["device_id"] = deviceId.trimmed();
    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);

    m_transport->deleteResource(Constants::EP_DEVICE, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        bool success = (error == QNetworkReply::NoError);
        auto doc = QJsonDocument::fromJson(data);
        QString msg = success ? "Device revoked" : (doc.isNull() ? "Failed to revoke device" : doc.object().value("error").toString());
        emit deviceRevocationResult(success, msg);
    }, postData);
}

void DeviceService::fetchRecipientKeys(const QString &username) {
    QString target = username.trimmed().toLower();
    if (target.isEmpty()) {
        emit recipientKeysFetched(username, {});
        return;
    }

    QMap<QString, QString> params;
    params["u"] = target;

    m_transport->get(Constants::EP_RELAY_KEYS, params, this, [this, target](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(statusCode);
        Q_UNUSED(errStr);
        QVariantList devicesList;
        if (error == QNetworkReply::NoError) {
            auto doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonArray devicesArr;
                if (doc.isObject() && doc.object().contains("devices")) {
                    devicesArr = doc.object().value("devices").toArray();
                } else if (doc.isArray()) {
                    devicesArr = doc.array();
                }

                for (const auto &v : devicesArr) {
                    QJsonObject dObj = v.toObject();
                    QVariantMap devMap;
                    devMap["device_id"] = dObj.value("device_id").toString();
                    devMap["public_key"] = dObj.value("public_key").toString();
                    devicesList.append(devMap);
                }
            }
        }
        emit recipientKeysFetched(target, devicesList);
    });
}

} // namespace Services
} // namespace NeoNect
