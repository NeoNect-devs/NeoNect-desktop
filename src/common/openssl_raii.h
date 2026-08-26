// src/common/openssl_raii.h
#pragma once
#include <memory>
#include <vector>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>

namespace Avila {
namespace Crypto {

// Custom deleters for OpenSSL C-style handles
struct EvpCipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX *ctx) const noexcept {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }
};

struct EvpPkeyDeleter {
    void operator()(EVP_PKEY *key) const noexcept {
        if (key) {
            EVP_PKEY_free(key);
        }
    }
};

struct BioDeleter {
    void operator()(BIO *bio) const noexcept {
        if (bio) {
            BIO_free_all(bio);
        }
    }
};

// RAII Smart Pointer Aliases
using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

inline EvpCipherCtxPtr makeEvpCipherCtx() {
    return EvpCipherCtxPtr(EVP_CIPHER_CTX_new());
}

/**
 * @brief RAII buffer that securely zeroes out sensitive key material on destruction.
 */
class SecureBuffer {
public:
    explicit SecureBuffer(std::size_t size = 0) : m_data(size, 0) {}

    ~SecureBuffer() {
        cleanse();
    }

    SecureBuffer(const SecureBuffer &other) : m_data(other.m_data) {}

    SecureBuffer &operator=(const SecureBuffer &other) {
        if (this != &other) {
            cleanse();
            m_data = other.m_data;
        }
        return *this;
    }

    SecureBuffer(SecureBuffer &&other) noexcept : m_data(std::move(other.m_data)) {}

    SecureBuffer &operator=(SecureBuffer &&other) noexcept {
        if (this != &other) {
            cleanse();
            m_data = std::move(other.m_data);
        }
        return *this;
    }

    void resize(std::size_t size) {
        m_data.resize(size, 0);
    }

    void cleanse() noexcept {
        if (!m_data.empty()) {
            OPENSSL_cleanse(m_data.data(), m_data.size());
        }
    }

    unsigned char *data() noexcept { return m_data.data(); }
    const unsigned char *data() const noexcept { return m_data.data(); }
    std::size_t size() const noexcept { return m_data.size(); }
    bool empty() const noexcept { return m_data.empty(); }

private:
    std::vector<unsigned char> m_data;
};

} // namespace Crypto
} // namespace Avila
