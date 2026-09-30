#include "DoubleRatchet.h"
#include <cstring>
#include <stdexcept>

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

static bool IsAllZero(const SecureBuffer& buf) {
    for (size_t i = 0; i < buf.size(); ++i) {
        if (buf.data()[i] != 0) return false;
    }
    return true;
}

Engine::Engine(std::shared_ptr<ICryptoBackend> crypto) 
    : m_crypto(std::move(crypto)) {
    if (!m_crypto) {
        throw std::invalid_argument("ICryptoBackend cannot be null");
    }
}

std::pair<RootKey, ChainKey> Engine::KdfRk(const SecureBuffer& rk, const SecureBuffer& dh_out) {
    QByteArray ikm(reinterpret_cast<const char*>(dh_out.data()), dh_out.size());
    QByteArray salt(reinterpret_cast<const char*>(rk.data()), rk.size());
    QByteArray info("NeoNectDoubleRatchetRKv1");
    
    QByteArray out = m_crypto->HkdfSha256(ikm, salt, info, 64);
    if (out.size() != 64) {
        throw std::runtime_error("KDF_RK failed to produce 64 bytes");
    }
    
    RootKey newRk;
    std::memcpy(newRk.data.data(), out.constData(), 32);
    ChainKey newCk;
    std::memcpy(newCk.data.data(), out.constData() + 32, 32);
    return {std::move(newRk), std::move(newCk)};
}

std::pair<ChainKey, MessageKey> Engine::KdfCk(const SecureBuffer& ck) {
    QByteArray key(reinterpret_cast<const char*>(ck.data()), ck.size());
    QByteArray dataMk(1, 0x01);
    QByteArray outMk = m_crypto->HmacSha256(key, dataMk);
    if (outMk.size() != 32) {
        throw std::runtime_error("KDF_CK MK failed");
    }
    
    QByteArray dataCk(1, 0x02);
    QByteArray outCk = m_crypto->HmacSha256(key, dataCk);
    if (outCk.size() != 32) {
        throw std::runtime_error("KDF_CK CK failed");
    }
    
    MessageKey mk;
    std::memcpy(mk.data.data(), outMk.constData(), 32);
    ChainKey nextCk;
    std::memcpy(nextCk.data.data(), outCk.constData(), 32);
    return {std::move(nextCk), std::move(mk)};
}

void Engine::RatchetInitAlice(State& state, const SecureBuffer& SK, const X25519PublicKey& bob_dh_public_key) {
    state.DHs = m_crypto->GenerateX25519KeyPair();
    state.DHr = bob_dh_public_key;
    auto dh_out = m_crypto->X25519(state.DHs->first, bob_dh_public_key);
    if (dh_out.size() == 0 || IsAllZero(dh_out)) {
        throw std::runtime_error("Invalid DH output during Alice init");
    }
    auto [rk, ck] = KdfRk(SK, dh_out);
    state.RK = std::move(rk);
    state.CKs = std::move(ck);
    state.CKr = std::nullopt;
    state.Ns = 0;
    state.Nr = 0;
    state.PN = 0;
    state.MKSKIPPED.clear();
}

void Engine::RatchetInitBob(State& state, const SecureBuffer& SK, const std::pair<X25519PrivateKey, X25519PublicKey>& bob_dh_key_pair) {
    state.DHs.emplace();
    state.DHs->first.data.resize(bob_dh_key_pair.first.data.size());
    std::copy(bob_dh_key_pair.first.data.data(), bob_dh_key_pair.first.data.data() + bob_dh_key_pair.first.data.size(), state.DHs->first.data.data());
    state.DHs->second = bob_dh_key_pair.second;
    state.DHr = std::nullopt;
    
    RootKey rk;
    if (SK.size() != 32) {
        throw std::runtime_error("Invalid SK size");
    }
    std::copy(SK.data(), SK.data() + 32, rk.data.data());
    state.RK = std::move(rk);
    state.CKs = std::nullopt;
    state.CKr = std::nullopt;
    state.Ns = 0;
    state.Nr = 0;
    state.PN = 0;
    state.MKSKIPPED.clear();
}

std::optional<std::pair<uint32_t, MessageKey>> Engine::RatchetSendKey(State& state) {
    if (!state.CKs) {
        return std::nullopt;
    }
    if (state.Ns == 0xFFFFFFFF) {
        return std::nullopt;
    }
    
    auto [nextCk, mk] = KdfCk(state.CKs->data);
    state.CKs = std::move(nextCk);
    uint32_t n = state.Ns;
    state.Ns++;
    return std::make_pair(n, std::move(mk));
}

std::optional<std::pair<State, MessageKey>> Engine::Receive(const State& in_state, const Header& header) {
    State state = in_state.Clone();
    auto mk_opt = TrySkippedMessageKeys(state, header);
    if (mk_opt) {
        return std::make_pair(std::move(state), std::move(*mk_opt));
    }

    if (!state.DHr || header.dh.data != state.DHr->data) {
        if (!SkipMessageKeys(state, header.pn)) {
            return std::nullopt;
        }
        if (!DHRatchet(state, header)) {
            return std::nullopt;
        }
    }

    if (!SkipMessageKeys(state, header.n)) {
        return std::nullopt;
    }

    if (!state.CKr) {
        return std::nullopt;
    }

    if (state.Nr == 0xFFFFFFFF) {
        return std::nullopt;
    }

    auto [nextCk, mk] = KdfCk(state.CKr->data);
    state.CKr = std::move(nextCk);
    state.Nr++;
    
    return std::make_pair(std::move(state), std::move(mk));
}

std::optional<MessageKey> Engine::TrySkippedMessageKeys(State& state, const Header& header) {
    SkippedKeyId id{header.dh.data, header.n};
    auto it = state.MKSKIPPED.find(id);
    if (it != state.MKSKIPPED.end()) {
        MessageKey mk;
        std::copy(it->second.data.data(), it->second.data.data() + 32, mk.data.data());
        state.MKSKIPPED.erase(it);
        return mk;
    }
    return std::nullopt;
}

bool Engine::SkipMessageKeys(State& state, uint32_t until) {
    if (state.Nr == until) {
        return true;
    }
    if (state.Nr > until) {
        return false;
    }
    uint32_t diff = until - state.Nr;
    if (diff > MAX_SKIP) {
        return false;
    }
    if (!state.CKr || !state.DHr) {
        return false;
    }
    
    while (state.Nr < until) {
        if (state.MKSKIPPED.size() >= MAX_SKIP) {
            return false;
        }
        SkippedKeyId id{state.DHr->data, state.Nr};
        if (state.MKSKIPPED.find(id) != state.MKSKIPPED.end()) {
            return false;
        }

        auto [nextCk, mk] = KdfCk(state.CKr->data);
        state.CKr = std::move(nextCk);
        state.MKSKIPPED[id] = std::move(mk);
        state.Nr++;
    }
    return true;
}

bool Engine::DHRatchet(State& state, const Header& header) {
    state.PN = state.Ns;
    state.Ns = 0;
    state.Nr = 0;
    state.DHr = header.dh;
    
    if (!state.RK) {
        return false;
    }
    
    auto dh_out1 = m_crypto->X25519(state.DHs->first, state.DHr.value());
    if (dh_out1.size() == 0 || IsAllZero(dh_out1)) {
        return false;
    }
    auto [rk1, ck1] = KdfRk(state.RK->data, dh_out1);
    state.RK = std::move(rk1);
    state.CKr = std::move(ck1);
    
    state.DHs = m_crypto->GenerateX25519KeyPair();
    
    auto dh_out2 = m_crypto->X25519(state.DHs->first, state.DHr.value());
    if (dh_out2.size() == 0 || IsAllZero(dh_out2)) {
        return false;
    }
    auto [rk2, ck2] = KdfRk(state.RK->data, dh_out2);
    state.RK = std::move(rk2);
    state.CKs = std::move(ck2);
    
    return true;
}

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
