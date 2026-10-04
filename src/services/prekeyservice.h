#pragma once
#include <QObject>
#include <memory>
#include "../transport/ihttptransport.h"
#include "../crypto/x3dh/X3DHTypes.h"
#include "../crypto/IPreKeyStore.h"

namespace NeoNect {
namespace Services {

class PreKeyService : public QObject {
    Q_OBJECT
public:
    explicit PreKeyService(std::shared_ptr<Transport::IHttpTransport> transport,
                           std::shared_ptr<Crypto::IPreKeyStore> preKeyStore,
                           QObject *parent = nullptr);

    void uploadPreKeys(const QString& deviceId);
    void claimPreKeys(const QString& targetUser, const QString& targetDevice);

signals:
    void preKeysUploaded(bool success, const QString& errorMessage);
    void preKeyBundleClaimed(const QString& targetUser, const QString& targetDevice, std::optional<Crypto::X3DH::BobPreKeyBundle> bundle);

private:
    std::shared_ptr<Transport::IHttpTransport> m_transport;
    std::shared_ptr<Crypto::IPreKeyStore> m_preKeyStore;
};

} // namespace Services
} // namespace NeoNect
