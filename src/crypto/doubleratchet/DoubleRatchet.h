#pragma once

#include <memory>
#include <optional>
#include <utility>
#include "DoubleRatchetTypes.h"
#include "../ICryptoBackend.h"

namespace NeoNect {
namespace Crypto {
namespace DoubleRatchet {

class Engine {
public:
    static constexpr uint32_t MAX_SKIP = 1000;

    explicit Engine(std::shared_ptr<ICryptoBackend> crypto);

    void RatchetInitAlice(State& state, const SecureBuffer& SK, const X25519PublicKey& bob_dh_public_key);
    void RatchetInitBob(State& state, const SecureBuffer& SK, const std::pair<X25519PrivateKey, X25519PublicKey>& bob_dh_key_pair);

    std::optional<std::pair<uint32_t, MessageKey>> RatchetSendKey(State& state);

    // Returns a cloned, advanced state and the derived MessageKey if successful.
    // Does not mutate the input state.
    std::optional<std::pair<State, MessageKey>> Receive(const State& state, const Header& header);

private:
    std::shared_ptr<ICryptoBackend> m_crypto;

    std::pair<RootKey, ChainKey> KdfRk(const SecureBuffer& rk, const SecureBuffer& dh_out);
    std::pair<ChainKey, MessageKey> KdfCk(const SecureBuffer& ck);

    std::optional<MessageKey> TrySkippedMessageKeys(State& state, const Header& header);
    bool SkipMessageKeys(State& state, uint32_t until);
    bool DHRatchet(State& state, const Header& header);
};

} // namespace DoubleRatchet
} // namespace Crypto
} // namespace NeoNect
