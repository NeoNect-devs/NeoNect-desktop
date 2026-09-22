/**
 * @file openssl_raii.h
 * @brief RAII memory-safety wrappers and zero-memory secure buffers for OpenSSL.
 * @details Encapsulates C-style OpenSSL heap resources (`EVP_CIPHER_CTX`, `EVP_PKEY`, `BIO`)
 * within modern C++ `std::unique_ptr` wrappers and provides an automatic memory-cleansing
 * secure buffer to prevent heap-inspection attacks on cryptographic secrets.
 * 
 * @par Design Pattern:
 * Resource Acquisition Is Initialization (RAII)
 * @author NeoNect Development Team
 * @version 1.0.0
 */

#pragma once
#include <memory>
#include <vector>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>

namespace NeoNect {
namespace Crypto {

/**
 * @brief Custom deleter functor for OpenSSL `EVP_CIPHER_CTX` structures.
 * @details Invokes `EVP_CIPHER_CTX_free` on non-null context pointers.
 */
struct EvpCipherCtxDeleter {
    /**
     * @brief Frees the cipher context.
     * @param ctx Pointer to the `EVP_CIPHER_CTX` instance.
     */
    void operator()(EVP_CIPHER_CTX *ctx) const noexcept {
        if (ctx) {
            EVP_CIPHER_CTX_free(ctx);
        }
    }
};

/**
 * @brief Custom deleter functor for OpenSSL `EVP_PKEY` public/private key handles.
 * @details Invokes `EVP_PKEY_free` on non-null key handles.
 */
struct EvpPkeyDeleter {
    /**
     * @brief Frees the EVP_PKEY structure.
     * @param key Pointer to the `EVP_PKEY` instance.
     */
    void operator()(EVP_PKEY *key) const noexcept {
        if (key) {
            EVP_PKEY_free(key);
        }
    }
};

/**
 * @brief Custom deleter functor for OpenSSL `BIO` I/O streams.
 * @details Invokes `BIO_free_all` to cleanly unwind and deallocate the entire BIO chain.
 */
struct BioDeleter {
    /**
     * @brief Frees the BIO stream chain.
     * @param bio Pointer to the root `BIO` instance.
     */
    void operator()(BIO *bio) const noexcept {
        if (bio) {
            BIO_free_all(bio);
        }
    }
};

/**
 * @brief Unique ownership smart pointer for `EVP_CIPHER_CTX`.
 * @details Ensures cipher contexts are automatically released upon scope exit.
 */
using EvpCipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter>;

/**
 * @brief Unique ownership smart pointer for `EVP_PKEY`.
 * @details Ensures asymmetric key objects are freed when leaving scope.
 */
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

/**
 * @brief Unique ownership smart pointer for `BIO` streams.
 * @details Automatically cleans up BIO chains on destruction.
 */
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

/**
 * @brief Factory helper constructing a newly allocated, RAII-managed `EVP_CIPHER_CTX`.
 * @return `EvpCipherCtxPtr` managing the newly allocated OpenSSL context.
 */
inline EvpCipherCtxPtr makeEvpCipherCtx() {
    return EvpCipherCtxPtr(EVP_CIPHER_CTX_new());
}

/**
 * @class SecureBuffer
 * @brief Memory buffer that securely wipes sensitive key material from RAM on destruction.
 * @details Implements secure memory sanitization via OpenSSL's `OPENSSL_cleanse`. Prevents
 * sensitive plaintext passwords, symmetric keys, and intermediate hash digests from lingering
 * in heap memory or swap space.
 * 
 * Non-copyable to prevent inadvertent memory duplication; movable to allow efficient transfers.
 * 
 * @pattern RAII (Resource Acquisition Is Initialization)
 */
class SecureBuffer {
public:
    /**
     * @brief Constructs a zero-initialized SecureBuffer of specified byte size.
     * @param size Initial capacity and size in bytes.
     */
    explicit SecureBuffer(std::size_t size = 0) : m_data(size, 0) {}

    /**
     * @brief Destructor that securely overwrites buffer contents with zeroes.
     * @details Calls `OPENSSL_cleanse` before releasing vector memory back to the heap.
     */
    ~SecureBuffer() {
        cleanse();
    }

    SecureBuffer(const SecureBuffer &other) = delete;
    SecureBuffer &operator=(const SecureBuffer &other) = delete;

    /**
     * @brief Move constructor transferring buffer ownership without copying secret data.
     * @param other Rvalue reference to existing SecureBuffer.
     */
    SecureBuffer(SecureBuffer &&other) noexcept : m_data(std::move(other.m_data)) {}

    /**
     * @brief Move assignment operator cleanly cleansing target before taking ownership.
     * @param other Rvalue reference to existing SecureBuffer.
     * @return Reference to this instance.
     */
    SecureBuffer &operator=(SecureBuffer &&other) noexcept {
        if (this != &other) {
            cleanse();
            m_data = std::move(other.m_data);
        }
        return *this;
    }

    /**
     * @brief Resizes buffer capacity, zero-initializing any new byte positions.
     * @param size New desired size in bytes.
     */
    void resize(std::size_t size) {
        m_data.resize(size, 0);
    }

    /**
     * @brief Explicitly overwrites buffer memory with zeroes using `OPENSSL_cleanse`.
     */
    void cleanse() noexcept {
        if (!m_data.empty()) {
            OPENSSL_cleanse(m_data.data(), m_data.size());
        }
    }

    /**
     * @brief Returns a mutable pointer to the raw byte buffer.
     * @return Pointer to unsigned char byte array.
     */
    unsigned char *data() noexcept { return m_data.data(); }

    /**
     * @brief Returns an immutable pointer to the raw byte buffer.
     * @return Const pointer to unsigned char byte array.
     */
    const unsigned char *data() const noexcept { return m_data.data(); }

    /**
     * @brief Returns the buffer size in bytes.
     * @return Number of allocated bytes.
     */
    std::size_t size() const noexcept { return m_data.size(); }

    /**
     * @brief Checks if buffer contains zero bytes.
     * @return True if empty, false otherwise.
     */
    bool empty() const noexcept { return m_data.empty(); }

private:
    std::vector<unsigned char> m_data; ///< Underlying contiguous memory vector.
};

} // namespace Crypto
} // namespace NeoNect
