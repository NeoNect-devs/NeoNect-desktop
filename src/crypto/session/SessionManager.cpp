#include "SessionManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace NeoNect {
namespace Crypto {
namespace Session {

SessionManager::SessionManager(
    std::weak_ptr<Storage::ISecureE2EEStore> store,
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

bool SessionManager::hasSession(const QString& sessionId) {
    if (m_sessionsNeedingReinit.contains(sessionId)) return false;
    auto store = m_store.lock();
    if (!store) return false;
    return store->getSession(sessionId).success;
}

ServiceResult<std::monostate> SessionManager::createSession(
    const QString& recipientUsername,
    const QString& recipientDeviceId,
    const X3DH::BobPreKeyBundle& bobBundle,
    const QString& messageId,
    const QByteArray& initialPlaintext)
{
    auto store = m_store.lock();
    if (!store) return ServiceResult<std::monostate>::fail("Storage context unavailable");
    auto identityRes = store->getIdentity();
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

    if (m_sendCb) {
        if (!m_sendCb(recipientUsername, recipientDeviceId, messageId, envBytes)) {
            return ServiceResult<std::monostate>::fail("Failed to enqueue initial envelope");
        }
    }

    Storage::SessionUpdateTx tx;
    tx.session = sessionRecord;
    auto saveRes = store->updateSessionState(tx);
    if (!saveRes.success) return saveRes;

    m_sessionsNeedingReinit.remove(sessionId);
    m_consecutiveDecryptFailures.remove(sessionId);

    return ServiceResult<std::monostate>::ok(std::monostate{});
}

ServiceResult<std::monostate> SessionManager::sendMessage(
    const QString& sessionId,
    const QByteArray& plaintext,
    const QString& messageId,
    const QString& recipientUsername,
    const QString& recipientDeviceId)
{
    auto store = m_store.lock();
    if (!store) return ServiceResult<std::monostate>::fail("Storage context unavailable");
    auto sessionRes = store->getSession(sessionId);
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
    
    if (m_sendCb) {
        if (!m_sendCb(recipientUsername, recipientDeviceId, messageId, envBytes)) {
            return ServiceResult<std::monostate>::fail("Failed to enqueue ratchet envelope");
        }
    }

    Storage::SessionUpdateTx tx;
    tx.session = updatedSession;
    auto saveRes = store->updateSessionState(tx);
    if (!saveRes.success) return saveRes;

    return ServiceResult<std::monostate>::ok(std::monostate{});
}

bool SessionManager::dispatchEnvelope(
    const QString& recipientUsername,
    const QString& recipientDeviceId,
    const QString& messageId,
    const QByteArray& envelopeBytes)
{
    if (!m_sendCb || envelopeBytes.isEmpty()) {
        return false;
    }
    return m_sendCb(recipientUsername, recipientDeviceId, messageId, envelopeBytes);
}

VoidResult SessionManager::handleEnvelope(
    const QByteArray &envelopeBytes,
    const Transport::TransportMetadata &metadata)
{
    auto store = m_store.lock();
    if (!store) return VoidResult::fail("Storage context unavailable");
    QString sessionId = metadata.senderDeviceId;
    if (!metadata.senderUserId.isEmpty()) {
        sessionId = metadata.senderDeviceId.isEmpty()
            ? metadata.senderUserId
            : (metadata.senderUserId + ":" + metadata.senderDeviceId);
    }
    
    auto initialEnvOpt = Wire::WireCodec::decodeInitialEnvelope(envelopeBytes);
    if (initialEnvOpt) {
        auto& env = initialEnvOpt.value();
        
        auto identityRes = store->getIdentity();
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
        
        const QByteArray senderIdentityKeyBytes(
            reinterpret_cast<const char*>(env.senderIdentityKey.data.data()),
            env.senderIdentityKey.data.size());

        DoubleRatchet::Header drHeader;
        drHeader.dh = ratchetEnvOpt->header.dh;
        drHeader.pn = ratchetEnvOpt->header.pn;
        drHeader.n = ratchetEnvOpt->header.n;

        DoubleRatchet::State initState;
        m_ratchet->RatchetInitBob(initState, x3dhRes->sharedSecret, std::make_pair(std::move(bobIdentity.privateKey), bobIdentity.publicKey));
        auto initReceiveRes = m_ratchet->Receive(initState, drHeader);
        if (!initReceiveRes) return VoidResult::fail("DoubleRatchet Receive failed on initial message");

        QByteArray rAd = Wire::WireCodec::getRatchetMessageAAD(ratchetEnvOpt->header);
        ByteView msgKeyView{initReceiveRes->second.data.data(), static_cast<size_t>(initReceiveRes->second.data.size())};
        ByteView rCtView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->ciphertext.constData()), static_cast<size_t>(ratchetEnvOpt->ciphertext.size())};
        ByteView rTagView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->tag.data.constData()), static_cast<size_t>(ratchetEnvOpt->tag.data.size())};
        ByteView rAdView{reinterpret_cast<const uint8_t*>(rAd.constData()), static_cast<size_t>(rAd.size())};

        auto decPtOpt = m_aead->decrypt(msgKeyView, rCtView, rTagView, rAdView);
        if (!decPtOpt) return VoidResult::fail("Ratchet payload decryption failed");

        QJsonParseError jsonErr;
        QJsonDocument ptDoc = QJsonDocument::fromJson(decPtOpt.value(), &jsonErr);
        const QString extractedSender =
            (jsonErr.error == QJsonParseError::NoError && ptDoc.isObject())
                ? ptDoc.object().value("senderId").toString().trimmed()
                : QString();

        if (m_senderVerifier && metadata.senderUserId.isEmpty()) {
            const QString userToVerify = !extractedSender.isEmpty()
                ? extractedSender
                : (sessionId.contains(':') ? sessionId.section(':', 0, 0).trimmed() : QString());
            if (userToVerify.isEmpty() ||
                !m_senderVerifier(userToVerify, metadata.senderDeviceId, senderIdentityKeyBytes) ||
                extractedSender.isEmpty()) {
                return VoidResult::fail("Sender identity verification failed");
            }
            sessionId = metadata.senderDeviceId.isEmpty()
                ? extractedSender
                : (extractedSender + ":" + metadata.senderDeviceId);
        } else if (!metadata.senderUserId.isEmpty()) {
            const QString boundUser = metadata.senderUserId.trimmed();
            if (!boundUser.isEmpty() && !extractedSender.isEmpty() &&
                boundUser.compare(extractedSender, Qt::CaseInsensitive) != 0) {
                return VoidResult::fail("Authenticated session user does not match payload senderId");
            }
        } else if (!extractedSender.isEmpty()) {
            return VoidResult::fail("Sender identity verification failed");
        }

        auto sessionRes = store->getSession(sessionId);
        DoubleRatchet::State baseState;
        DoubleRatchet::State nextState;
        bool shouldConsumeOpk = false;
        if (sessionRes.success) {
            if (!sessionRes.data.value().remote_identity_key.isEmpty() &&
                sessionRes.data.value().remote_identity_key != senderIdentityKeyBytes) {
                return VoidResult::fail("Remote identity key mismatch for existing session");
            }
            sessionId = sessionRes.data.value().session_id;
            baseState = sessionRecordToState(sessionRes.data.value());
            if (baseState.DHr.has_value() && baseState.DHr->data == drHeader.dh.data) {
                auto skippedRes = store->getSkippedKey(sessionId, QByteArray(reinterpret_cast<const char*>(drHeader.dh.data.data()), drHeader.dh.data.size()), drHeader.n);
                if (skippedRes.success) {
                    DoubleRatchet::SkippedKeyId id{drHeader.dh.data, drHeader.n};
                    DoubleRatchet::MessageKey mk;
                    mk.data.resize(32);
                    std::copy(skippedRes.data.value().message_key.constData(), skippedRes.data.value().message_key.constData() + 32, mk.data.data());
                    baseState.MKSKIPPED.emplace(id, std::move(mk));
                }
                auto existingReceiveRes = m_ratchet->Receive(baseState, drHeader);
                if (!existingReceiveRes) return VoidResult::fail("DoubleRatchet Receive failed on initial message");
                nextState = std::move(existingReceiveRes->first);
            } else {
                baseState.MKSKIPPED.clear();
                nextState = std::move(initReceiveRes->first);
                shouldConsumeOpk = env.oneTimePreKeyId.has_value();
            }
        } else {
            nextState = std::move(initReceiveRes->first);
            shouldConsumeOpk = env.oneTimePreKeyId.has_value();
        }

        if (m_commitVerifier && !m_commitVerifier(sessionId, decPtOpt.value())) {
            return VoidResult::fail("Local message persistence failed");
        }

        if (shouldConsumeOpk) {
            if (m_preKeyStore) {
                m_preKeyStore->consumeOneTimePreKey(env.oneTimePreKeyId.value());
            } else {
                store->consumeOneTimePreKeyAtomically(env.oneTimePreKeyId.value());
            }
        }

        Storage::SessionUpdateTx tx;
        tx.session = stateToSessionRecord(nextState, sessionId, senderIdentityKeyBytes, identityRes.data.value().identity_id);
        
        for (const auto& [id, mk] : nextState.MKSKIPPED) {
            if (baseState.MKSKIPPED.find(id) == baseState.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                skey.message_key = QByteArray(reinterpret_cast<const char*>(mk.data.data()), mk.data.size());
                skey.created_at = QDateTime::currentMSecsSinceEpoch();
                tx.new_skipped_keys.push_back(skey);
            }
        }
        for (const auto& [id, mk] : baseState.MKSKIPPED) {
            if (nextState.MKSKIPPED.find(id) == nextState.MKSKIPPED.end()) {
                Storage::E2EESkippedKey skey;
                skey.session_id = sessionId;
                skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                skey.message_number = id.n;
                tx.deleted_skipped_keys.push_back(skey);
            }
        }

        auto saveRes = store->updateSessionState(tx);
        if (!saveRes.success) return VoidResult::fail(saveRes.message);

        m_sessionsNeedingReinit.remove(sessionId);
        m_consecutiveDecryptFailures.remove(sessionId);

        if (m_receiveCb) {
            m_receiveCb(sessionId, decPtOpt.value());
        }

        return VoidResult::ok(std::monostate{});
    }

    auto ratchetEnvOpt = Wire::WireCodec::decodeRatchetEnvelope(envelopeBytes);
    if (ratchetEnvOpt) {
        std::vector<Storage::E2EESession> candidates;
        if (!metadata.senderUserId.isEmpty()) {
            auto sessionRes = store->getSession(sessionId);
            if (!sessionRes.success || !sessionRes.data.has_value()) {
                return VoidResult::fail("Session not found");
            }
            candidates.push_back(sessionRes.data.value());
        } else {
            auto sessionsRes = store->getSessionsForDevice(metadata.senderDeviceId);
            if (!sessionsRes.success || !sessionsRes.data.has_value() || sessionsRes.data->empty()) {
                return VoidResult::fail("Session not found");
            }
            candidates = std::move(sessionsRes.data.value());
        }

        DoubleRatchet::Header drHeader;
        drHeader.dh = ratchetEnvOpt->header.dh;
        drHeader.pn = ratchetEnvOpt->header.pn;
        drHeader.n = ratchetEnvOpt->header.n;

        const QByteArray rAd = Wire::WireCodec::getRatchetMessageAAD(ratchetEnvOpt->header);
        const ByteView rCtView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->ciphertext.constData()), static_cast<size_t>(ratchetEnvOpt->ciphertext.size())};
        const ByteView rTagView{reinterpret_cast<const uint8_t*>(ratchetEnvOpt->tag.data.constData()), static_cast<size_t>(ratchetEnvOpt->tag.data.size())};
        const ByteView rAdView{reinterpret_cast<const uint8_t*>(rAd.constData()), static_cast<size_t>(rAd.size())};

        bool hadReceiveSuccess = false;
        for (const auto& candidate : candidates) {
            const QString candidateSessionId = candidate.session_id;
            DoubleRatchet::State state = sessionRecordToState(candidate);

            auto skippedRes = store->getSkippedKey(candidateSessionId, QByteArray(reinterpret_cast<const char*>(drHeader.dh.data.data()), drHeader.dh.data.size()), drHeader.n);
            if (skippedRes.success) {
                DoubleRatchet::SkippedKeyId id{drHeader.dh.data, drHeader.n};
                DoubleRatchet::MessageKey mk;
                mk.data.resize(32);
                std::copy(skippedRes.data.value().message_key.constData(), skippedRes.data.value().message_key.constData() + 32, mk.data.data());
                state.MKSKIPPED.emplace(id, std::move(mk));
            }

            auto receiveRes = m_ratchet->Receive(state, drHeader);
            if (!receiveRes) {
                if (++m_consecutiveDecryptFailures[candidateSessionId] >= 2) {
                    m_sessionsNeedingReinit.insert(candidateSessionId);
                }
                continue;
            }
            hadReceiveSuccess = true;

            ByteView msgKeyView{receiveRes->second.data.data(), static_cast<size_t>(receiveRes->second.data.size())};
            auto decPtOpt = m_aead->decrypt(msgKeyView, rCtView, rTagView, rAdView);
            if (!decPtOpt) {
                if (++m_consecutiveDecryptFailures[candidateSessionId] >= 2) {
                    m_sessionsNeedingReinit.insert(candidateSessionId);
                }
                continue;
            }

            if (candidateSessionId.contains(':')) {
                const QString boundUser = candidateSessionId.section(':', 0, 0).trimmed();
                QJsonParseError jsonErr;
                QJsonDocument ptDoc = QJsonDocument::fromJson(decPtOpt.value(), &jsonErr);
                if (jsonErr.error == QJsonParseError::NoError && ptDoc.isObject() && ptDoc.object().contains("senderId")) {
                    const QString extractedSender = ptDoc.object().value("senderId").toString().trimmed();
                    if (!boundUser.isEmpty() && !extractedSender.isEmpty() &&
                        boundUser.compare(extractedSender, Qt::CaseInsensitive) != 0) {
                        return VoidResult::fail("Authenticated session user does not match payload senderId");
                    }
                }
            }

            if (m_commitVerifier && !m_commitVerifier(candidateSessionId, decPtOpt.value())) {
                return VoidResult::fail("Local message persistence failed");
            }

            Storage::SessionUpdateTx tx;
            tx.session = stateToSessionRecord(receiveRes->first, candidateSessionId, candidate.remote_identity_key, candidate.local_identity_id);

            for (const auto& [id, mk] : receiveRes->first.MKSKIPPED) {
                if (state.MKSKIPPED.find(id) == state.MKSKIPPED.end()) {
                    Storage::E2EESkippedKey skey;
                    skey.session_id = candidateSessionId;
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
                    skey.session_id = candidateSessionId;
                    skey.remote_ratchet_public_key = QByteArray(reinterpret_cast<const char*>(id.dh.constData()), id.dh.size());
                    skey.message_number = id.n;
                    tx.deleted_skipped_keys.push_back(skey);
                }
            }

            auto saveRes = store->updateSessionState(tx);
            if (!saveRes.success) return VoidResult::fail(saveRes.message);

            m_sessionsNeedingReinit.remove(candidateSessionId);
            m_consecutiveDecryptFailures.remove(candidateSessionId);

            if (m_receiveCb) {
                m_receiveCb(candidateSessionId, decPtOpt.value());
            }

            return VoidResult::ok(std::monostate{});
        }

        return VoidResult::fail(hadReceiveSuccess ? "Ratchet payload decryption failed" : "DoubleRatchet Receive failed");
    }

    return VoidResult::fail("Unhandled envelope");
}

} // namespace Session
} // namespace Crypto
} // namespace NeoNect
