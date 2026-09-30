#include "X3DH.h"
#include "../KeyEncoding.h"
#include <QByteArray>
#include <cstring>

namespace NeoNect {
namespace Crypto {
namespace X3DH {

namespace {

bool IsAllZero(const SecureBuffer& buf) {
    if (buf.empty()) return true;
    for (size_t i = 0; i < buf.size(); ++i) {
        if (buf.data()[i] != 0) return false;
    }
    return true;
}

} // namespace

std::optional<X3DHResult> X3DHImpl::initiate(
    ICryptoBackend& backend,
    IXEdDSA& xeddsa,
    const IdentityKeyPair& aliceIdentity,
    const BobPreKeyBundle& bobBundle)
{
    // Step A - verify Bob's SPK signature
    QByteArray encodedSPK = KeyEncoding::Encode(bobBundle.signedPreKey);
    ByteView spkView{reinterpret_cast<const uint8_t*>(encodedSPK.constData()), static_cast<size_t>(encodedSPK.size())};
    if (!xeddsa.verify(bobBundle.identityKey, spkView, bobBundle.signedPreKeySignature)) {
        return std::nullopt;
    }

    // Step B - generate Alice ephemeral key
    auto [ek_private, ek_public] = backend.GenerateX25519KeyPair();

    // Step C - compute DH values
    SecureBuffer dh1 = backend.X25519(aliceIdentity.privateKey, bobBundle.signedPreKey);
    if (IsAllZero(dh1)) return std::nullopt;
    
    SecureBuffer dh2 = backend.X25519(ek_private, bobBundle.identityKey);
    if (IsAllZero(dh2)) return std::nullopt;
    
    SecureBuffer dh3 = backend.X25519(ek_private, bobBundle.signedPreKey);
    if (IsAllZero(dh3)) return std::nullopt;

    QByteArray km;
    km.append(reinterpret_cast<const char*>(dh1.data()), dh1.size());
    km.append(reinterpret_cast<const char*>(dh2.data()), dh2.size());
    km.append(reinterpret_cast<const char*>(dh3.data()), dh3.size());

    if (bobBundle.oneTimePreKey.has_value()) {
        SecureBuffer dh4 = backend.X25519(ek_private, bobBundle.oneTimePreKey.value());
        if (IsAllZero(dh4)) return std::nullopt;
        km.append(reinterpret_cast<const char*>(dh4.data()), dh4.size());
    }

    QByteArray F(32, '\xFF');
    QByteArray ikm = F + km;

    QByteArray salt(32, '\0');
    QByteArray info = QByteArray("NeoNectX3DHv1");
    
    QByteArray skBytes = backend.HkdfSha256(ikm, salt, info, 32);
    if (skBytes.isEmpty() || skBytes.size() != 32) return std::nullopt;

    SecureBuffer sk;
    sk.resize(32);
    std::memcpy(sk.data(), skBytes.constData(), 32);

    QByteArray ad = KeyEncoding::Encode(aliceIdentity.publicKey) + KeyEncoding::Encode(bobBundle.identityKey);

    X3DHResult result;
    result.sharedSecret = std::move(sk);
    result.associatedData = ad;
    result.localEphemeralPublicKey = ek_public;
    result.signedPreKeyId = bobBundle.signedPreKeyId;
    result.oneTimePreKeyId = bobBundle.oneTimePreKeyId;

    return result;
}

std::optional<X3DHResult> X3DHImpl::respond(
    ICryptoBackend& backend,
    IPreKeyStore& store,
    const InitialMessageMetadata& msg)
{
    auto ik_opt = store.identityKey();
    if (!ik_opt.has_value()) return std::nullopt;
    const auto& bobIdentity = ik_opt.value();

    auto spk_opt = store.signedPreKey();
    if (!spk_opt.has_value() || spk_opt->id != msg.bobSignedPreKeyId) return std::nullopt;
    const auto& bobSPK = spk_opt.value();

    std::optional<OneTimePreKey> bobOPK = std::nullopt;
    if (msg.bobOneTimePreKeyId.has_value()) {
        bobOPK = store.getOneTimePreKey(msg.bobOneTimePreKeyId.value());
        if (!bobOPK.has_value()) {
            return std::nullopt;
        }
    }

    // Compute DH values
    SecureBuffer dh1 = backend.X25519(bobSPK.privateKey, msg.aliceIdentityKey);
    if (IsAllZero(dh1)) return std::nullopt;

    SecureBuffer dh2 = backend.X25519(bobIdentity.privateKey, msg.aliceEphemeralKey);
    if (IsAllZero(dh2)) return std::nullopt;

    SecureBuffer dh3 = backend.X25519(bobSPK.privateKey, msg.aliceEphemeralKey);
    if (IsAllZero(dh3)) return std::nullopt;

    QByteArray km;
    km.append(reinterpret_cast<const char*>(dh1.data()), dh1.size());
    km.append(reinterpret_cast<const char*>(dh2.data()), dh2.size());
    km.append(reinterpret_cast<const char*>(dh3.data()), dh3.size());

    if (bobOPK.has_value()) {
        SecureBuffer dh4 = backend.X25519(bobOPK->privateKey, msg.aliceEphemeralKey);
        if (IsAllZero(dh4)) return std::nullopt;
        km.append(reinterpret_cast<const char*>(dh4.data()), dh4.size());
    }

    QByteArray F(32, '\xFF');
    QByteArray ikm = F + km;

    QByteArray salt(32, '\0');
    QByteArray info = QByteArray("NeoNectX3DHv1");
    
    QByteArray skBytes = backend.HkdfSha256(ikm, salt, info, 32);
    if (skBytes.isEmpty() || skBytes.size() != 32) return std::nullopt;

    SecureBuffer sk;
    sk.resize(32);
    std::memcpy(sk.data(), skBytes.constData(), 32);

    QByteArray ad = KeyEncoding::Encode(msg.aliceIdentityKey) + KeyEncoding::Encode(bobIdentity.publicKey);

    X3DHResult result;
    result.sharedSecret = std::move(sk);
    result.associatedData = ad;
    result.localEphemeralPublicKey = std::nullopt;
    result.signedPreKeyId = msg.bobSignedPreKeyId;
    result.oneTimePreKeyId = msg.bobOneTimePreKeyId;

    // Successful execution! OPK is NOT permanently consumed here.
    return result;
}

} // namespace X3DH
} // namespace Crypto
} // namespace NeoNect
