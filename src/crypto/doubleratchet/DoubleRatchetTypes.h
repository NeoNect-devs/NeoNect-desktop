#pragma once

#include <QByteArray>
#include <map>
#include <optional>
#include <cstdint>
#include "../CryptoTypes.h"

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

struct Header {
    X25519PublicKey dh;
    uint32_t pn = 0;
    uint32_t n = 0;
};

struct MessageKey {
    SecureBuffer data;
    
    MessageKey() : data(32) {}
    MessageKey(const MessageKey&) = delete;
    MessageKey& operator=(const MessageKey&) = delete;
    MessageKey(MessageKey&&) = default;
    MessageKey& operator=(MessageKey&&) = default;
};

struct ChainKey {
    SecureBuffer data;
    
    ChainKey() : data(32) {}
    ChainKey(const ChainKey&) = delete;
    ChainKey& operator=(const ChainKey&) = delete;
    ChainKey(ChainKey&&) = default;
    ChainKey& operator=(ChainKey&&) = default;
};

struct RootKey {
    SecureBuffer data;
    
    RootKey() : data(32) {}
    RootKey(const RootKey&) = delete;
    RootKey& operator=(const RootKey&) = delete;
    RootKey(RootKey&&) = default;
    RootKey& operator=(RootKey&&) = default;
};

struct SkippedKeyId {
    QByteArray dh;
    uint32_t n;

    bool operator<(const SkippedKeyId& other) const {
        if (dh != other.dh) {
            return dh < other.dh;
        }
        return n < other.n;
    }
};

struct State {
    std::optional<std::pair<X25519PrivateKey, X25519PublicKey>> DHs;
    std::optional<X25519PublicKey> DHr;
    std::optional<RootKey> RK;
    std::optional<ChainKey> CKs;
    std::optional<ChainKey> CKr;
    uint32_t Ns = 0;
    uint32_t Nr = 0;
    uint32_t PN = 0;
    std::map<SkippedKeyId, MessageKey> MKSKIPPED;
    
    // Disable copy to prevent accidental secret duplication.
    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = default;
    State& operator=(State&&) = default;
    
    // Deep copy helper for atomic state transitions
    State Clone() const {
        State cloned;
        if (DHs) {
            cloned.DHs.emplace();
            cloned.DHs->first.data.resize(DHs->first.data.size());
            std::copy(DHs->first.data.data(), DHs->first.data.data() + DHs->first.data.size(), cloned.DHs->first.data.data());
            cloned.DHs->second.data = DHs->second.data;
        }
        if (DHr) {
            cloned.DHr = *DHr;
        }
        if (RK) {
            cloned.RK.emplace();
            cloned.RK->data.resize(RK->data.size());
            std::copy(RK->data.data(), RK->data.data() + RK->data.size(), cloned.RK->data.data());
        }
        if (CKs) {
            cloned.CKs.emplace();
            cloned.CKs->data.resize(CKs->data.size());
            std::copy(CKs->data.data(), CKs->data.data() + CKs->data.size(), cloned.CKs->data.data());
        }
        if (CKr) {
            cloned.CKr.emplace();
            cloned.CKr->data.resize(CKr->data.size());
            std::copy(CKr->data.data(), CKr->data.data() + CKr->data.size(), cloned.CKr->data.data());
        }
        cloned.Ns = Ns;
        cloned.Nr = Nr;
        cloned.PN = PN;
        for (const auto& [id, mk] : MKSKIPPED) {
            MessageKey newMk;
            newMk.data.resize(mk.data.size());
            std::copy(mk.data.data(), mk.data.data() + mk.data.size(), newMk.data.data());
            cloned.MKSKIPPED.emplace(id, std::move(newMk));
        }
        return cloned;
    }
};

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
