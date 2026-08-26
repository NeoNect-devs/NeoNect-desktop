// src/services/deviceservice.h
#pragma once
#include <QObject>
#include <memory>
#include "../transport/ihttptransport.h"
#include "../storage/isettingsrepository.h"

namespace Avila {
namespace Services {

class DeviceService : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Storage::ISettingsRepository> storage,
                           QObject *parent = nullptr);
    ~DeviceService() override = default;

    void registerDevice(const QString &deviceId, const QString &publicKey);
    void fetchDevicePublicKey(const QString &deviceId);

signals:
    void deviceRegistrationResult(bool success, const QString &message);
    void deviceKeyFetched(const QString &deviceId, const QString &publicKey);

private:
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Storage::ISettingsRepository> m_storage;
};

} // namespace Services
} // namespace Avila
