#include "WireCodec.h"
#include "../KeyEncoding.h"
#include <QtEndian>
#include <cstring>

namespace NeoNect {
namespace Crypto {
namespace Wire {

namespace {

class BinaryWriter {
public:
    void writeUint8(uint8_t val) { buffer.append(static_cast<char>(val)); }
    void writeUint32(uint32_t val) {
        uint32_t be = qToBigEndian(val);
        buffer.append(reinterpret_cast<const char*>(&be), sizeof(be));
    }
    void writeBytes(const QByteArray& data) { buffer.append(data); }
    QByteArray get() const { return buffer; }
private:
    QByteArray buffer;
};

class BinaryReader {
public:
    BinaryReader(const QByteArray& data) : m_data(reinterpret_cast<const uint8_t*>(data.constData())), m_size(data.size()) {}
    
    std::optional<uint8_t> readUint8() {
        if (m_size < 1) return std::nullopt;
        uint8_t val = m_data[0];
        m_data++;
        m_size--;
        return val;
    }
    
    std::optional<uint32_t> readUint32() {
        if (m_size < 4) return std::nullopt;
        uint32_t be;
        std::memcpy(&be, m_data, 4);
        m_data += 4;
        m_size -= 4;
        return qFromBigEndian(be);
    }
    
    std::optional<QByteArray> readBytes(size_t len) {
        if (m_size < len) return std::nullopt;
        QByteArray val(reinterpret_cast<const char*>(m_data), static_cast<int>(len));
        m_data += len;
        m_size -= len;
        return val;
    }
    
    std::optional<X25519PublicKey> readPublicKey() {
        auto bytes = readBytes(33);
        if (!bytes) return std::nullopt;
        if (bytes.value().at(0) != 0x05) return std::nullopt;
        X25519PublicKey pk;
        pk.data = bytes.value().mid(1);
        return pk;
    }

    bool isEmpty() const { return m_size == 0; }
    size_t remaining() const { return m_size; }
private:
    const uint8_t* m_data;
    size_t m_size;
};

} // anonymous namespace

QByteArray WireCodec::encodeRatchetHeader(const RatchetHeader& header) {
    BinaryWriter writer;
    QByteArray dhBytes = KeyEncoding::Encode(header.dh);
    if (dhBytes.size() != 33) return QByteArray(); // Encode invalid key fails safely
    writer.writeBytes(dhBytes);
    writer.writeUint32(header.pn);
    writer.writeUint32(header.n);
    return writer.get();
}

std::optional<RatchetHeader> WireCodec::decodeRatchetHeader(const QByteArray& data) {
    BinaryReader reader(data);
    auto dh = reader.readPublicKey();
    if (!dh) return std::nullopt;
    auto pn = reader.readUint32();
    if (!pn) return std::nullopt;
    auto n = reader.readUint32();
    if (!n) return std::nullopt;
    if (!reader.isEmpty()) return std::nullopt; // Reject trailing garbage
    RatchetHeader header;
    header.dh = dh.value();
    header.pn = pn.value();
    header.n = n.value();
    return header;
}

QByteArray WireCodec::getRatchetMessageAAD(const RatchetHeader& header) {
    BinaryWriter writer;
    writer.writeUint8(CURRENT_VERSION);
    writer.writeUint8(static_cast<uint8_t>(EnvelopeType::DOUBLE_RATCHET_MESSAGE));
    writer.writeBytes(encodeRatchetHeader(header));
    return writer.get();
}

QByteArray WireCodec::encodeInitialEnvelope(const InitialEnvelope& env) {
    if (env.version != CURRENT_VERSION) return QByteArray();
    if (env.senderIdentityKey.data.size() != 32) return QByteArray();
    if (env.senderEphemeralKey.data.size() != 32) return QByteArray();
    if (env.tag.data.size() != 16) return QByteArray();

    BinaryWriter writer;
    writer.writeUint8(env.version);
    writer.writeUint8(static_cast<uint8_t>(EnvelopeType::X3DH_INITIAL));
    writer.writeBytes(KeyEncoding::Encode(env.senderIdentityKey));
    writer.writeBytes(KeyEncoding::Encode(env.senderEphemeralKey));
    writer.writeUint32(env.signedPreKeyId);
    
    if (env.oneTimePreKeyId.has_value()) {
        writer.writeUint8(1);
        writer.writeUint32(env.oneTimePreKeyId.value());
    } else {
        writer.writeUint8(0);
    }
    
    uint32_t ctLen = static_cast<uint32_t>(env.ciphertext.size());
    writer.writeUint32(ctLen);
    writer.writeBytes(env.ciphertext);
    writer.writeBytes(env.tag.data);
    
    QByteArray res = writer.get();
    if (static_cast<size_t>(res.size()) > MAX_ENVELOPE_SIZE) return QByteArray();
    return res;
}

std::optional<InitialEnvelope> WireCodec::decodeInitialEnvelope(const QByteArray& data) {
    if (static_cast<size_t>(data.size()) > MAX_ENVELOPE_SIZE) return std::nullopt;
    
    BinaryReader reader(data);
    auto version = reader.readUint8();
    if (!version || version.value() != CURRENT_VERSION) return std::nullopt;
    
    auto type = reader.readUint8();
    if (!type || type.value() != static_cast<uint8_t>(EnvelopeType::X3DH_INITIAL)) return std::nullopt;
    
    auto ik = reader.readPublicKey();
    if (!ik) return std::nullopt;
    
    auto ek = reader.readPublicKey();
    if (!ek) return std::nullopt;
    
    auto spkId = reader.readUint32();
    if (!spkId) return std::nullopt;
    
    auto opkFlag = reader.readUint8();
    if (!opkFlag) return std::nullopt;
    if (opkFlag.value() != 0 && opkFlag.value() != 1) return std::nullopt;
    
    std::optional<uint32_t> opkId;
    if (opkFlag.value() == 1) {
        auto oid = reader.readUint32();
        if (!oid) return std::nullopt;
        opkId = oid.value();
    }
    
    auto ctLen = reader.readUint32();
    if (!ctLen) return std::nullopt;
    
    if (reader.remaining() < 16) return std::nullopt;
    size_t allowedCt = reader.remaining() - 16;
    if (ctLen.value() != allowedCt) return std::nullopt;
    
    auto ct = reader.readBytes(ctLen.value());
    if (!ct) return std::nullopt;
    
    auto tag = reader.readBytes(16);
    if (!tag) return std::nullopt;
    
    if (!reader.isEmpty()) return std::nullopt;
    
    InitialEnvelope env;
    env.version = version.value();
    env.senderIdentityKey = ik.value();
    env.senderEphemeralKey = ek.value();
    env.signedPreKeyId = spkId.value();
    env.oneTimePreKeyId = opkId;
    env.ciphertext = ct.value();
    env.tag.data = tag.value();
    return env;
}

QByteArray WireCodec::encodeRatchetEnvelope(const RatchetEnvelope& env) {
    if (env.version != CURRENT_VERSION) return QByteArray();
    if (env.header.dh.data.size() != 32) return QByteArray();
    if (env.tag.data.size() != 16) return QByteArray();

    BinaryWriter writer;
    writer.writeUint8(env.version);
    writer.writeUint8(static_cast<uint8_t>(EnvelopeType::DOUBLE_RATCHET_MESSAGE));
    writer.writeBytes(encodeRatchetHeader(env.header));
    
    uint32_t ctLen = static_cast<uint32_t>(env.ciphertext.size());
    writer.writeUint32(ctLen);
    writer.writeBytes(env.ciphertext);
    writer.writeBytes(env.tag.data);
    
    QByteArray res = writer.get();
    if (static_cast<size_t>(res.size()) > MAX_ENVELOPE_SIZE) return QByteArray();
    return res;
}

std::optional<RatchetEnvelope> WireCodec::decodeRatchetEnvelope(const QByteArray& data) {
    if (static_cast<size_t>(data.size()) > MAX_ENVELOPE_SIZE) return std::nullopt;
    
    BinaryReader reader(data);
    auto version = reader.readUint8();
    if (!version || version.value() != CURRENT_VERSION) return std::nullopt;
    
    auto type = reader.readUint8();
    if (!type || type.value() != static_cast<uint8_t>(EnvelopeType::DOUBLE_RATCHET_MESSAGE)) return std::nullopt;
    
    auto dh = reader.readPublicKey();
    if (!dh) return std::nullopt;
    
    auto pn = reader.readUint32();
    if (!pn) return std::nullopt;
    
    auto n = reader.readUint32();
    if (!n) return std::nullopt;
    
    auto ctLen = reader.readUint32();
    if (!ctLen) return std::nullopt;
    
    if (reader.remaining() < 16) return std::nullopt;
    size_t allowedCt = reader.remaining() - 16;
    if (ctLen.value() != allowedCt) return std::nullopt;
    
    auto ct = reader.readBytes(ctLen.value());
    if (!ct) return std::nullopt;
    
    auto tagBytes = reader.readBytes(16);
    if (!tagBytes) return std::nullopt;
    
    if (!reader.isEmpty()) return std::nullopt;
    
    RatchetEnvelope env;
    env.version = version.value();
    env.header.dh = dh.value();
    env.header.pn = pn.value();
    env.header.n = n.value();
    env.ciphertext = ct.value();
    env.tag.data = tagBytes.value();
    return env;
}

} // namespace Wire
} // namespace Crypto
} // namespace NeoNect
