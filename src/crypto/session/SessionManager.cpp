#include "SessionManager.h"
#include <QDateTime>

namespace NeoNect {
namespace Crypto {
namespace Session {

SessionManager::SessionManager(
    std::shared_ptr<Storage::ISecureE2EEStore> store,
    std::shared_ptr<X3DH::IX3DH> x3dh,
    std::shared_ptr<DoubleRatchet::Engine> ratchet,
    std::shared_ptr<DoubleRatchet::IAEAD> aead,
    std::shared_ptr<ICryptoBackend> backend,
    std::shared_ptr<IPreKeyStore> preKeyStore,
    std::shared_ptr<IXEdDSA> xeddsa,
    EnvelopeSendCallback sendCb,
    MessageReceivedCallback receiveCb)
    : m_store(std::move(store))
    , m_x3dh(std::move(x3dh))
    , m_ratchet(std::move(ratchet))
    , m_aead(std::move(aead))
    , m_backend(std::move(backend))
    , m_preKeyStore(std::move(preKeyStore))
    , m_xeddsa(std::move(xeddsa))
    , m_sendCb(std::move(sendCb))
    , m_receiveCb(std::move(receiveCb))
{
}

QString SessionManager::generateSessionId(const QString& username, const QString& deviceId) {
    return username + ":" + deviceId;
}

Storage::E2EESession SessionManager::stateToSessionRecord(const DoubleRatchet::State& state, const QString& sessionId, const QByteArray& remoteIdentityKey, qint64 localIdentityId) {
    Storage::E2EESession session;
    session.session_id = sessionId;
    session.remote_identity_key = remoteIdentityKey;
    session.local_identity_id = localIdentityId;
    
    if (state.DHs) {
        QByteArray dhsBytes;
        dhsBytes.append(QByteArray(reinterpret_cast<const char*>(state.DHs->first.data.data()), state.DHs->first.data.size()));
        dhsBytes.append(QByteArray(reinterpret_cast<const char*>(state.DHs->second.data.data()), state.DHs->second.data.size()));
        session.DHs = dhsBytes;
    }
    if (state.DHr) {
        session.DHr = QByteArray(reinterpret_cast<const char*>(state.DHr->data.data()), state.DHr->data.size());
    }
    if (state.RK) {
        session.RK = QByteArray(reinterpret_cast<const char*>(state.RK->data.data()), state.RK->data.size());
    }
    if (state.CKs) {
        session.CKs = QByteArray(reinterpret_cast<const char*>(state.CKs->data.data()), state.CKs->data.size());
    }
    if (state.CKr) {
        session.CKr = QByteArray(reinterpret_cast<const char*>(state.CKr->data.data()), state.CKr->data.size());
    }
    session.Ns = state.Ns;
    session.Nr = state.Nr;
    session.PN = state.PN;
    session.created_at = QDateTime::currentMSecsSinceEpoch();
    session.updated_at = session.created_at;
    session.version = 1;
    return session;
}

DoubleRatchet::State SessionManager::sessionRecordToState(const Storage::E2EESession& session) {
    DoubleRatchet::State state;
    if (!session.DHs.isEmpty() && session.DHs.size() == 64) {
        X25519PrivateKey priv;
        priv.data.resize(32);
        std::copy(session.DHs.constData(), session.DHs.constData() + 32, priv.data.data());
        X25519PublicKey pub;
        pub.data.resize(32);
        std::copy(session.DHs.constData() + 32, session.DHs.constData() + 64, pub.data.data());
        state.DHs = std::make_pair(std::move(priv), std::move(pub));
    }
    if (!session.DHr.isEmpty() && session.DHr.size() == 32) {
        X25519PublicKey pub;
        pub.data.resize(32);
        std::copy(session.DHr.constData(), session.DHr.constData() + 32, pub.data.data());
        state.DHr = std::move(pub);
    }
    if (!session.RK.isEmpty() && session.RK.size() == 32) {
        DoubleRatchet::RootKey rk;
        rk.data.resize(32);
        std::copy(session.RK.constData(), session.RK.constData() + 32, rk.data.data());
        state.RK = std::move(rk);
    }
    if (!session.CKs.isEmpty() && session.CKs.size() == 32) {
        DoubleRatchet::ChainKey cks;
        cks.data.resize(32);
        std::copy(session.CKs.constData(), session.CKs.constData() + 32, cks.data.data());
        state.CKs = std::move(cks);
    }
    if (!session.CKr.isEmpty() && session.CKr.size() == 32) {
        DoubleRatchet::ChainKey ckr;
        ckr.data.resize(32);
        std::copy(session.CKr.constData(), session.CKr.constData() + 32, ckr.data.data());
        state.CKr = std::move(ckr);
    }
    state.Ns = session.Ns;
    state.Nr = session.Nr;
    state.PN = session.PN;
    return state;
}

ServiceResult<std::monostate> SessionManager::createSession(
    const QString& recipientUsername,
    const QString& recipientDeviceId,
    const X3DH::BobPreKeyBundle& bobBundle,
    const QString& messageId,
    const QByteArray& initialPlaintext)
{
    auto identityRes = m_store->getIdentity();
    if (!identityRes.success) return ServiceResult<std::monostate>::fail(identityRes.message);
    
    IdentityKeyPair aliceIdentity;
    aliceIdentity.publicKey.data.resize(32);
    aliceIdentity.privateKey.data.resize(32);
    std::copy(identityRes.data.value().private_key.constData(), identityRes.data.value().private_key.constData() + 32, aliceIdentity.privateKey.data.data());
    std::copy(identityRes.data.value().public_key.constData(), identityRes.data.value().public_key.constData() + 32, aliceIdentity.publicKey.data.data());

    auto x3dhResult = m_x3dh->initiate(*m_backend, *m_xeddsa, aliceIdentity, bobBundle);
    if (!x3dhResult) {
        return ServiceResult<std::monostate>::fail("X3DH initiate failed");
    }

    DoubleRatchet::State state;
    m_ratchet->RatchetInitAlice(state, x3dhResult->sharedSecret, bobBundle.identityKey);

    auto ratchetKeyRes = m_ratchet->RatchetSendKey(state);
    if (!ratchetKeyRes) {
        return ServiceResult<std::monostate>::fail("Failed to derive ratchet message key");
    }

    Wire::RatchetHeader rHeader;
    rHeader.dh = state.DHs->second;
    rHeader.pn = state.PN;
    rHeader.n = ratchetKeyRes->first;

    QByteArray rAd = Wire::WireCodec::getRatchetMessageAAD(rHeader);
    ByteView msgKeyView{ratchetKeyRes->second.data.data(), static_cast<size_t>(ratchetKeyRes->second.data.size())};
    ByteView ptView{reinterpret_cast<const uint8_t*>(initialPlaintext.constData()), static_cast<size_t>(initialPlaintext.size())};
    ByteView rAdView{reinterpret_cast<const uint8_t*>(rAd.constData()), static_cast<size_t>(rAd.size())};
    
    auto rAeadRes = m_aead->encrypt(msgKeyView, ptView, rAdView);

    Wire::RatchetEnvelope rEnv;
    rEnv.version = Wire::WireCodec::CURRENT_VERSION;
    rEnv.header = rHeader;
    rEnv.ciphertext = rAeadRes.ciphertext;
    rEnv.tag = rAeadRes.tag;

    QByteArray rEnvBytes = Wire::WireCodec::encodeRatchetEnvelope(rEnv);

    ByteView skView{x3dhResult->sharedSecret.data(), static_cast<size_t>(x3dhResult->sharedSecret.size())};
    ByteView rEnvBytesView{reinterpret_cast<const uint8_t*>(rEnvBytes.constData()), static_cast<size_t>(rEnvBytes.size())};
    ByteView initAdView{reinterpret_cast<const uint8_t*>(x3dhResult->associatedData.constData()), static_cast<size_t>(x3dhResult->associatedData.size())};

    auto initAeadRes = m_aead->encrypt(skView, rEnvBytesView, initAdView);

    Wire::InitialEnvelope env;
    env.version = Wire::WireCodec::CURRENT_VERSION;
    env.senderIdentityKey = aliceIdentity.publicKey;
    env.senderEphemeralKey = x3dhResult->localEphemeralPublicKey.value();
    env.signedPreKeyId = x3dhResult->signedPreKeyId;
    env.oneTimePreKeyId = x3dhResult->oneTimePreKeyId;
    env.ciphertext = initAeadRes.ciphertext;
    env.tag = initAeadRes.tag;

    QByteArray envBytes = Wire::WireCodec::encodeInitialEnvelope(env);

    QString sessionId = generateSessionId(recipientUsername, recipientDeviceId);
    QByteArray remoteIdentity = QByteArray(reinterpret_cast<const char*>(bobBundle.identityKey.data.data()), bobBundle.identityKey.data.size());
    Storage::E2EESession sessionRecord = stateToSessionRecord(state, sessionId, remoteIdentity, identityRes.data.value().identity_id);

    Storage::SessionUpdateTx tx;
    tx.session = sessionRecord;
    auto saveRes = m_store->updateSessionState(tx);
    if (!saveRes.success) return saveRes;

    if (m_sendCb) {
        m_sendCb(recipientUsername, recipientDeviceId, messageId, envBytes);
    }

    return ServiceResult<std::monostate>::ok(std::monostate{});
}

ServiceResult<std::monostate> SessionManager::sendMessage(
    const QString& sessionId,
    const QByteArray& plaintext,
    const QString& messageId,
    const QString& recipientUsername,
    const QString& recipientDeviceId)
{
    auto sessionRes = m_store->getSession(sessionId);
    if (!sessionRes.success) return ServiceResult<std::monostate>::fail(sessionRes.message);

    DoubleRatchet::State state = sessionRecordToState(sessionRes.data.value());
    
    auto ratchetKeyRes = m_ratchet->RatchetSendKey(state);
    if (!ratchetKeyRes) {
        return ServiceResult<std::monostate>::fail("Failed to derive ratchet message key");
    }

    Wire::RatchetHeader header;
    header.dh = state.DHs->second;
    header.pn = state.PN;
    header.n = state.Ns - 1; // It was incremented in RatchetSendKey

    QByteArray ad = Wire::WireCodec::getRatchetMessageAAD(header);
    ByteView msgKeyView{ratchetKeyRes->second.data.data(), static_cast<size_t>(ratchetKeyRes->second.data.size())};
    ByteView ptView{reinterpret_cast<const uint8_t*>(plaintext.constData()), static_cast<size_t>(plaintext.size())};
    ByteView adView{reinterpret_cast<const uint8_t*>(ad.constData()), static_cast<size_t>(ad.size())};

    auto aeadRes = m_aead->encrypt(msgKeyView, ptView, adView);

    Wire::RatchetEnvelope env;
    env.version = Wire::WireCodec::CURRENT_VERSION;
    env.header = header;
    env.ciphertext = aeadRes.ciphertext;
    env.tag = aeadRes.tag;

    QByteArray envBytes = Wire::WireCodec::encodeRatchetEnvelope(env);

    Storage::E2EESession updatedSession = stateToSessionRecord(state, sessionId, sessionRes.data.value().remote_identity_key, sessionRes.data.value().local_identity_id);
    
    Storage::SessionUpdateTx tx;
    tx.session = updatedSession;
    auto saveRes = m_store->updateSessionState(tx);
    if (!saveRes.success) return saveRes;

    if (m_sendCb) {
        m_sendCb(recipientUsername, recipientDeviceId, messageId, envBytes);
    }

    return ServiceResult<std::monostate>::ok(std::monostate{});
}

VoidResult SessionManager::handleEnvelope(
    const QByteArray &envelopeBytes,
    const Transport::TransportMetadata &metadata)
{
    QString sessionId = metadata.senderDeviceId;
    
    auto initialEnvOpt = Wire::WireCodec::decodeInitialEnvelope(envelopeBytes);
    if (initialEnvOpt) {
        auto& env = initialEnvOpt.value();
        
        auto identityRes = m_store->getIdentity();
        if (!identityRes.success) return VoidResult::fail("No local identity");
        
        IdentityKeyPair bobIdentity;
        bobIdentity.publicKey.data.resize(32);
        bobIdentity.privateKey.data.resize(32);
        std::copy(identityRes.data.value().private_key.constData(), identityRes.data.value().private_key.constData() + 32, bobIdentity.privateKey.data.data());
        std::copy(identityRes.data.value().public_key.constData(), identityRes.data.value().public_key.constData() + 32, bobIdentity.publicKey.data.data());

        X3DH::InitialMessageMetadata x3dhMsg;
        x3dhMsg.aliceIdentityKey = env.senderIdentityKey;
        x3dhMsg.aliceEphemeralKey = env.senderEphemeralKey;
        x3dhMsg.bobSignedPreKeyId = env.signedPreKeyId;
        x3dhMsg.bobOneTimePreKeyId = env.oneTimePreKeyId;

        auto x3dhRes = m_x3dh->respond(*m_backend, *m_preKeyStore, x3dhMsg);
        if (!x3dhRes) return VoidResult::fail("X3DH respond failed");

        ByteView skView{x3dhRes->sharedSecret.data(), static_cast<size_t>(x3dhRes->sharedSecret.size())};
        ByteView ctView{reinterpret_cast<const uint8_t*>(env.ciphertext.constData()), static_cast<size_t>(env.ciphertext.size())};
        ByteView tagView{reinterpret_cast<const uint8_t*>(env.tag.data.constData()), static_cast<size_t>(env.tag.data.size())};
        ByteView adView{reinterpret_cast<const uint8_t*>(x3dhRes->associatedData.constData()), static_cast<size_t>(x3dhRes->associatedData.size())};
        
        auto decRatchetEnvBytesOpt = m_aead->decrypt(skView, ctView, tagView, adView);
        if (!decRatchetEnvBytesOpt) return VoidResult::fail("InitialEnvelope decryption failed");
        
        auto ratchetEnvOpt = Wire::WireCodec::decodeRatchetEnvelope(decRatchetEnvBytesOpt.value());
        if (!ratchetEnvOpt) return VoidResult::fail("Decrypted InitialEnvelope payload is not a valid RatchetEnvelope");
        
        auto sessionRes = m_store->getSession(sessionId);
        DoubleRatchet::State state;
        if (sessionRes.success) {
            state = sessionRecordToState(sessionRes.data.value());
        } else {
            m_ratchet->RatchetInitBob(state, x3dhRes->sharedSecret, std::make_pair(std::move(bobIdentity.privateKey), bobIdentity.publicKey));
        }

        DoubleRatchet::Header drHeader;
        drHeader.dh = ratchetEnvOpt->header.dh;
        drHeader.pn = ratchetEnvOpt->header.pn;
        drHeader.n = ratchetEnvOpt->header.n;
        
        auto skippedRes = m_store->getSkippedKey(sessionId, QByteArray(reinterpret_cast<const char*>(drHeader.dh.data.data()), drHeader.dh.data.size()), drHeader.n);
        if (skippedRes.success) {
            DoubleRatchet::SkippedKeyId id{drHeader.dh.data, drHeader.n};
            DoubleRatchet::MessageKey mk;
            mk.data.resize(32);
            std::copy(skippedRes.data.value().message_key.constData(), skippedRes.data.value().message_key.constData() + 32, mk.data.data());
            state.MKSKIPPED.emplace(id, std::move(mk));
        }

        auto receiveRes = m_ratchet->Receive(state, drHeader);
        if (!receiveRes) return VoidResult::fail("DoubleRatchet Receive failed on initial message");

        QByteArray rAd = Wire::WireCodec::getRatchetMessageAAD(ratchetEnvOpt->header);
        ByteView msgKeyView{receiveRes->second.data.data(), static_cast<size_t>(receiveRes->second.data.size())};
        ByteView rCtView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->ciphertext.constData()), static_cast<size_t>(ratchetEnvOpt->ciphertext.size())};
        ByteView rTagView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->tag.data.constData()), static_cast<size_t>(ratchetEnvOpt->tag.data.size())};
        ByteView rAdView{reinterpret_cast<const uint8_t*>(rAd.constData()), static_cast<size_t>(rAd.size())};

        auto decPtOpt = m_aead->decrypt(msgKeyView, rCtView, rTagView, rAdView);
        if (!decPtOpt) return VoidResult::fail("Ratchet payload decryption failed");

        if (!sessionRes.success && env.oneTimePreKeyId.has_value()) {
            m_store->consumeOneTimePreKeyAtomically(env.oneTimePreKeyId.value());
        }

        Storage::SessionUpdateTx tx;
        tx.session = stateToSessionRecord(receiveRes->first, sessionId, QByteArray(reinterpret_cast<const char*>(env.senderIdentityKey.data.data()), env.senderIdentityKey.data.size()), identityRes.data.value().identity_id);
        
        for (const auto& [id, mk] : receiveRes->first.MKSKIPPED) {
            if (state.MKSKIPPED.find(id) == state.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                skey.message_key = QByteArray(reinterpret_cast<const char*>(mk.data.data()), mk.data.size());
                skey.created_at = QDateTime::currentMSecsSinceEpoch();
                tx.new_skipped_keys.push_back(skey);
            }
        }
        for (const auto& [id, mk] : state.MKSKIPPED) {
            if (receiveRes->first.MKSKIPPED.find(id) == receiveRes->first.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                tx.deleted_skipped_keys.push_back(skey);
            }
        }

        auto saveRes = m_store->updateSessionState(tx);
        if (!saveRes.success) return VoidResult::fail(saveRes.message);

        if (m_receiveCb) {
            m_receiveCb(sessionId, decPtOpt.value());
        }

        return VoidResult::ok(std::monostate{});
    }

    auto ratchetEnvOpt = Wire::WireCodec::decodeRatchetEnvelope(envelopeBytes);
    if (ratchetEnvOpt) {
        auto sessionRes = m_store->getSession(sessionId);
        if (!sessionRes.success) return VoidResult::fail("Session not found");
        
        DoubleRatchet::State state = sessionRecordToState(sessionRes.data.value());
        
        DoubleRatchet::Header drHeader;
        drHeader.dh = ratchetEnvOpt->header.dh;
        drHeader.pn = ratchetEnvOpt->header.pn;
        drHeader.n = ratchetEnvOpt->header.n;
        
        auto skippedRes = m_store->getSkippedKey(sessionId, QByteArray(reinterpret_cast<const char*>(drHeader.dh.data.data()), drHeader.dh.data.size()), drHeader.n);
        if (skippedRes.success) {
            DoubleRatchet::SkippedKeyId id{drHeader.dh.data, drHeader.n};
            DoubleRatchet::MessageKey mk;
            mk.data.resize(32);
            std::copy(skippedRes.data.value().message_key.constData(), skippedRes.data.value().message_key.constData() + 32, mk.data.data());
            state.MKSKIPPED.emplace(id, std::move(mk));
        }

        auto receiveRes = m_ratchet->Receive(state, drHeader);
        if (!receiveRes) return VoidResult::fail("DoubleRatchet Receive failed");

        QByteArray rAd = Wire::WireCodec::getRatchetMessageAAD(ratchetEnvOpt->header);
        ByteView msgKeyView{receiveRes->second.data.data(), static_cast<size_t>(receiveRes->second.data.size())};
        ByteView rCtView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->ciphertext.constData()), static_cast<size_t>(ratchetEnvOpt->ciphertext.size())};
        ByteView rTagView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->tag.data.constData()), static_cast<size_t>(ratchetEnvOpt->tag.data.size())};
        ByteView rAdView{reinterpret_cast<const uint8_t*>(rAd.constData()), static_cast<size_t>(rAd.size())};

        auto decPtOpt = m_aead->decrypt(msgKeyView, rCtView, rTagView, rAdView);
        if (!decPtOpt) return VoidResult::fail("Ratchet payload decryption failed");

        Storage::SessionUpdateTx tx;
        tx.session = stateToSessionRecord(receiveRes->first, sessionId, sessionRes.data.value().remote_identity_key, sessionRes.data.value().local_identity_id);
        
        // Compute diff for skipped keys
        for (const auto& [id, mk] : receiveRes->first.MKSKIPPED) {
            if (state.MKSKIPPED.find(id) == state.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                skey.message_key = QByteArray(reinterpret_cast<const char*>(mk.data.data()), mk.data.size());
                skey.created_at = QDateTime::currentMSecsSinceEpoch();
                tx.new_skipped_keys.push_back(skey);
            }
        }
        for (const auto& [id, mk] : state.MKSKIPPED) {
            if (receiveRes->first.MKSKIPPED.find(id) == receiveRes->first.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                tx.deleted_skipped_keys.push_back(skey);
            }
        }

        auto saveRes = m_store->updateSessionState(tx);
        if (!saveRes.success) return VoidResult::fail(saveRes.message);

        if (m_receiveCb) {
            m_receiveCb(sessionId, decPtOpt.value());
        }

        return VoidResult::ok(std::monostate{});
    }

    return VoidResult::fail("Unhandled envelope");
}

} // namespace Session
} // namespace Crypto
} // namespace NeoNect
