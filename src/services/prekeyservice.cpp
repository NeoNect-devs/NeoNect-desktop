#include "prekeyservice.h"
#include "../common/constants.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace NeoNect {
namespace Services {

PreKeyService::PreKeyService(std::shared_ptr<Transport::IHttpTransport> transport,
                             std::shared_ptr<Crypto::IPreKeyStore> preKeyStore,
                             QObject *parent)
    : QObject(parent), m_transport(transport), m_preKeyStore(preKeyStore) {}

void PreKeyService::uploadPreKeys(const QString& deviceId) {
    if (deviceId.isEmpty()) return;

    auto spkOpt = m_preKeyStore->signedPreKey();
    if (!spkOpt.has_value()) {
        emit preKeysUploaded(false, "No signed prekey available");
        return;
    }

    QJsonObject req;
    req["device_id"] = deviceId;

    QJsonObject spkJson;
    spkJson["key_id"] = static_cast<qint64>(spkOpt->id);
    spkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(spkOpt->publicKey.data.data()), spkOpt->publicKey.data.size()).toBase64());
    spkJson["signature"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(spkOpt->signature.data.data()), spkOpt->signature.data.size()).toBase64());
    
    req["signed_curve_prekey"] = spkJson;

    QJsonArray opkArr;
    auto opks = m_preKeyStore->availableOneTimePreKeys();
    // Only upload up to 200 OPKs per server requirement
    int count = 0;
    for (const auto& opk : opks) {
        if (count >= 200) break;
        QJsonObject opkJson;
        opkJson["key_id"] = static_cast<qint64>(opk.id);
        opkJson["public_key"] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(opk.publicKey.data.data()), opk.publicKey.data.size()).toBase64());
        opkArr.append(opkJson);
        count++;
    }
    
    if (count > 0) {
        req["one_time_curve_prekeys"] = opkArr;
    }

    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_PREKEY_UPLOAD, payload, this, [this](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        Q_UNUSED(data);
        if (error == QNetworkReply::NoError && statusCode == 200) {
            emit preKeysUploaded(true, "");
        } else {
            emit preKeysUploaded(false, errStr);
        }
    });
}

void PreKeyService::claimPreKeys(const QString& targetUser, const QString& targetDevice) {
    if (targetUser.isEmpty() || targetDevice.isEmpty()) return;

    QJsonObject req;
    req["target_user"] = targetUser;
    req["target_device"] = targetDevice;
    QByteArray payload = QJsonDocument(req).toJson(QJsonDocument::Compact);

    m_transport->post(Constants::EP_PREKEY_CLAIM, payload, this, [this, targetUser, targetDevice](int statusCode, const QByteArray &data, QNetworkReply::NetworkError error, const QString &errStr) {
        if (error != QNetworkReply::NoError || statusCode != 200) {
            emit preKeyBundleClaimed(targetUser, targetDevice, std::nullopt);
            return;
        }

        auto doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            emit preKeyBundleClaimed(targetUser, targetDevice, std::nullopt);
            return;
        }
        auto obj = doc.object();
        
        if (!obj.contains("identity_key") || !obj.contains("signed_curve_prekey")) {
            emit preKeyBundleClaimed(targetUser, targetDevice, std::nullopt);
            return;
        }

        QByteArray ikBytes = QByteArray::fromBase64(obj["identity_key"].toString().toLatin1());
        if (ikBytes.size() != 32) {
            emit preKeyBundleClaimed(targetUser, targetDevice, std::nullopt);
            return;
        }

        auto spkObj = obj["signed_curve_prekey"].toObject();
        QByteArray spkPubBytes = QByteArray::fromBase64(spkObj["public_key"].toString().toLatin1());
        QByteArray spkSigBytes = QByteArray::fromBase64(spkObj["signature"].toString().toLatin1());
        if (spkPubBytes.size() != 32 || spkSigBytes.size() != 64) {
            emit preKeyBundleClaimed(targetUser, targetDevice, std::nullopt);
            return;
        }

        Crypto::X3DH::BobPreKeyBundle bundle;
        bundle.identityKey.data.resize(32);
        std::copy(ikBytes.begin(), ikBytes.end(), bundle.identityKey.data.data());

        bundle.signedPreKey.data.resize(32);
        std::copy(spkPubBytes.begin(), spkPubBytes.end(), bundle.signedPreKey.data.data());
        
        bundle.signedPreKeyId = static_cast<Crypto::KeyId>(spkObj["key_id"].toInt());

        bundle.signedPreKeySignature.data.resize(64);
        std::copy(spkSigBytes.begin(), spkSigBytes.end(), bundle.signedPreKeySignature.data.data());

        if (obj.contains("one_time_curve_prekey")) {
            auto opkObj = obj["one_time_curve_prekey"].toObject();
            QByteArray opkPubBytes = QByteArray::fromBase64(opkObj["public_key"].toString().toLatin1());
            if (opkPubBytes.size() == 32) {
                Crypto::X25519PublicKey opkPub;
                opkPub.data.resize(32);
                std::copy(opkPubBytes.begin(), opkPubBytes.end(), opkPub.data.data());
                bundle.oneTimePreKey = opkPub;
                bundle.oneTimePreKeyId = static_cast<Crypto::KeyId>(opkObj["key_id"].toInt());
            }
        }

        emit preKeyBundleClaimed(targetUser, targetDevice, bundle);
    });
}

} // namespace Services
} // namespace NeoNect
