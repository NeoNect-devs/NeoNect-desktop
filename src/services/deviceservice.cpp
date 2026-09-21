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
    registerDeviceInternal(deviceId, publicKey, 0);
}

void DeviceService::registerDeviceInternal(const QString &deviceId, const QString &publicKey, int attempt) {
    if (attempt >= 3) {
        emit deviceRegistrationResult(false, "Device registration failed: Maximum retry attempts reached.");
        return;
    }

    if (m_storage->authToken().isEmpty()) {
        emit deviceRegistrationResult(false, "Unauthorized: No authentication token.");
        return;
    }

    QString effectiveDevId = deviceId.trimmed();
    if (effectiveDevId.isEmpty()) {
        effectiveDevId = m_storage->deviceId();
        if (effectiveDevId.isEmpty()) {
            QString prof = m_storage->profile();
            QString user = m_storage->username().trimmed().toLower();
            QString prefix = prof.isEmpty() ? "" : prof + "-";
            if (!user.isEmpty()) prefix += user + "-";
            effectiveDevId = QString("neonect-dev-%1%2").arg(prefix,
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

    m_transport->post(Constants::EP_DEVICE_REGISTER, postData, this, [this, effectiveDevId, effectivePubKey, attempt](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(errStr);
        auto doc = QJsonDocument::fromJson(data);
        QString errVal = (!doc.isNull() && doc.isObject()) ? doc.object().value("error").toString() : QString();

        // 1. Success
        if (error == QNetworkReply::NoError || statusCode == 201 || statusCode == 200) {
            emit deviceRegistrationResult(true, "Device registered");
            return;
        }

        // 2. Maximum device limit reached (HTTP 400 with "maximum device limit reached")
        if (statusCode == 400 && errVal.contains("maximum device limit reached", Qt::CaseInsensitive)) {
            QString user = m_storage->username().trimmed().toLower();
            if (!user.isEmpty()) {
                QMap<QString, QString> params;
                params["u"] = user;
                m_transport->get(Constants::EP_RELAY_KEYS, params, this, [this, effectiveDevId, effectivePubKey, attempt](int sCode, const QByteArray &dData, QNetworkReply::NetworkError, const QString &) {
                    if (sCode == 200) {
                        auto devDoc = QJsonDocument::fromJson(dData);
                        QJsonArray devArr = devDoc.object().value("devices").toArray();
                        for (const auto &val : devArr) {
                            if (val.toObject().value("device_id").toString() == effectiveDevId) {
                                emit deviceRegistrationResult(true, "Device already registered");
                                return;
                            }
                        }
                        // Prune oldest device to stay within 10-device limit
                        if (!devArr.isEmpty()) {
                            QString oldestDevId = devArr.first().toObject().value("device_id").toString();
                            QJsonObject revokeBody;
                            revokeBody["device_id"] = oldestDevId;
                            m_transport->deleteResource(Constants::EP_DEVICE, this, [this, effectiveDevId, effectivePubKey, attempt](int delCode, const QByteArray &, QNetworkReply::NetworkError, const QString &) {
                                if (delCode == 200) {
                                    registerDeviceInternal(effectiveDevId, effectivePubKey, attempt + 1);
                                } else {
                                    emit deviceRegistrationResult(false, "Device registration failed: maximum device limit reached");
                                }
                            }, QJsonDocument(revokeBody).toJson(QJsonDocument::Compact));
                            return;
                        }
                    }
                    emit deviceRegistrationResult(false, "Device registration failed: maximum device limit reached");
                });
                return;
            }
        }

        // 3. Conflict (HTTP 409): Verify if device belongs to current user
        if (statusCode == 409) {
            QString user = m_storage->username().trimmed().toLower();
            if (!user.isEmpty()) {
                QMap<QString, QString> params;
                params["u"] = user;
                m_transport->get(Constants::EP_RELAY_KEYS, params, this, [this, effectiveDevId, effectivePubKey, attempt](int sCode, const QByteArray &dData, QNetworkReply::NetworkError, const QString &) {
                    if (sCode == 200) {
                        auto devDoc = QJsonDocument::fromJson(dData);
                        QJsonArray devArr = devDoc.object().value("devices").toArray();
                        for (const auto &val : devArr) {
                            if (val.toObject().value("device_id").toString() == effectiveDevId) {
                                emit deviceRegistrationResult(true, "Device already registered");
                                return;
                            }
                        }
                    }
                    // Not owned by current user; generate a fresh device ID and re-register
                    QString prof = m_storage->profile();
                    QString u = m_storage->username().trimmed().toLower();
                    QString prefix = prof.isEmpty() ? "" : prof + "-";
                    if (!u.isEmpty()) prefix += u + "-";
                    QString newDevId = QString("neonect-dev-%1%2").arg(prefix, QUuid::createUuid().toString(QUuid::WithoutBraces));
                    m_storage->setDeviceId(newDevId);
                    registerDeviceInternal(newDevId, effectivePubKey, attempt + 1);
                });
                return;
            } else {
                emit deviceRegistrationResult(true, "Device already registered");
                return;
            }
        }

        QString msg = errVal.isEmpty() ? "Device registration failed" : errVal;
        emit deviceRegistrationResult(false, msg);
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
