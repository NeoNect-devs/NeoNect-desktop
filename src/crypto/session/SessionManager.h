#pragma once
#include <QString>
#include <QByteArray>
#include <memory>
#include <functional>
#include "../../transport/IIncomingEnvelopeHandler.h"
#include "../../storage/e2ee/ISecureE2EEStore.h"
#include "../x3dh/IX3DH.h"
#include "../doubleratchet/DoubleRatchet.h"
#include "../doubleratchet/IAEAD.h"
#include "../wire/WireCodec.h"
#include "../ICryptoBackend.h"
#include "../IPreKeyStore.h"
#include "../IXEdDSA.h"

namespace NeoNect {
namespace Crypto {
namespace Session {

class SessionManager : public Transport::IIncomingEnvelopeHandler {
public:
    using MessageReceivedCallback = std::function<void(const QString& sessionId, const QByteArray& plaintext)>;
    using EnvelopeSendCallback = std::function<bool(const QString& recipientUsername, const QString& recipientDeviceId, const QString& messageId, const QByteArray& envelopeBytes)>;

    SessionManager(
        std::weak_ptr<Storage::ISecureE2EEStore> store,
        std::shared_ptr<X3DH::IX3DH> x3dh,
        std::shared_ptr<DoubleRatchet::Engine> ratchet,
        std::shared_ptr<DoubleRatchet::IAEAD> aead,
        std::shared_ptr<ICryptoBackend> backend,
        std::shared_ptr<IPreKeyStore> preKeyStore,
        std::shared_ptr<IXEdDSA> xeddsa,
        EnvelopeSendCallback sendCb,
        MessageReceivedCallback receiveCb
    );

    // Initial session creation (Alice side)
    bool hasSession(const QString& sessionId);

    // Temporary compatibility bridge for Phase 2 StorageContext
    void setStore(std::weak_ptr<Storage::ISecureE2EEStore> store) { m_store = store; }

    ServiceResult<std::monostate> createSession(
        const QString& recipientUsername,
        const QString& recipientDeviceId,
        const X3DH::BobPreKeyBundle& bobBundle,
        const QString& messageId,
        const QByteArray& initialPlaintext
    );

    // Send flow
    ServiceResult<std::monostate> sendMessage(
        const QString& sessionId,
        const QByteArray& plaintext,
        const QString& messageId,
        const QString& recipientUsername,
        const QString& recipientDeviceId
    );

    // Receive flow (implements IIncomingEnvelopeHandler)
    VoidResult handleEnvelope(
        const QByteArray &envelopeBytes,
        const Transport::TransportMetadata &metadata) override;

private:
    std::weak_ptr<Storage::ISecureE2EEStore> m_store;
    std::shared_ptr<X3DH::IX3DH> m_x3dh;
    std::shared_ptr<DoubleRatchet::Engine> m_ratchet;
    std::shared_ptr<DoubleRatchet::IAEAD> m_aead;
    std::shared_ptr<ICryptoBackend> m_backend;
    std::shared_ptr<IPreKeyStore> m_preKeyStore;
    std::shared_ptr<IXEdDSA> m_xeddsa;
    EnvelopeSendCallback m_sendCb;
    MessageReceivedCallback m_receiveCb;

    QString generateSessionId(const QString& username, const QString& deviceId);

    // State mapping helpers
    Storage::E2EESession stateToSessionRecord(const DoubleRatchet::State& state, const QString& sessionId, const QByteArray& remoteIdentityKey, qint64 localIdentityId);
    DoubleRatchet::State sessionRecordToState(const Storage::E2EESession& session);
};

} // namespace Session
} // namespace Crypto
} // namespace NeoNect
