#include "sodium.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int sodium_init(void) {
    return 0;
}

void sodium_memzero(void *pnt, const size_t len) {
    OPENSSL_cleanse(pnt, len);
}

void sodium_stackzero(const size_t len) {
    volatile char buf[256];
    if (len <= sizeof(buf)) {
        OPENSSL_cleanse((void*)buf, len);
    }
}

int crypto_hash_sha512_init(crypto_hash_sha512_state *state) {
    state->ctx = EVP_MD_CTX_new();
    if (!state->ctx) return -1;
    if (EVP_DigestInit_ex((EVP_MD_CTX*)state->ctx, EVP_sha512(), NULL) != 1) return -1;
    return 0;
}

int crypto_hash_sha512_update(crypto_hash_sha512_state *state, const unsigned char *in, unsigned long long inlen) {
    if (EVP_DigestUpdate((EVP_MD_CTX*)state->ctx, in, inlen) != 1) return -1;
    return 0;
}

int crypto_hash_sha512_final(crypto_hash_sha512_state *state, unsigned char *out) {
    if (EVP_DigestFinal_ex((EVP_MD_CTX*)state->ctx, out, NULL) != 1) return -1;
    EVP_MD_CTX_free((EVP_MD_CTX*)state->ctx);
    state->ctx = NULL;
    return 0;
}

int crypto_sign_ed25519_detached(unsigned char *sig, unsigned long long *siglen_p, const unsigned char *m, unsigned long long mlen, const unsigned char *sk) {
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, sk, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    
    int ret = -1;
    size_t siglen = 64;
    if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) == 1) {
        if (EVP_DigestSign(ctx, sig, &siglen, m, mlen) == 1) {
            if (siglen_p) *siglen_p = siglen;
            ret = 0;
        }
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

int crypto_sign_verify_detached(const unsigned char *sig, const unsigned char *m, unsigned long long mlen, const unsigned char *pk) {
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pk, 32);
    if (!pkey) return -1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { EVP_PKEY_free(pkey); return -1; }
    
    int ret = -1;
    if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey) == 1) {
        if (EVP_DigestVerify(ctx, sig, 64, m, mlen) == 1) {
            ret = 0;
        }
    }
    
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

void randombytes_buf(void * const buf, const size_t size) {
    RAND_bytes((unsigned char*)buf, size);
}

int crypto_sign_ed25519_pk_to_curve25519(unsigned char *curve25519_pk, const unsigned char *ed25519_pk) { return -1; }
int crypto_scalarmult_curve25519_base(unsigned char *q, const unsigned char *n) { return -1; }
int crypto_sign_ed25519_sk_to_curve25519(unsigned char *curve25519_sk, const unsigned char *ed25519_sk) { return -1; }
int crypto_scalarmult_curve25519(unsigned char *q, const unsigned char *n, const unsigned char *p) { return -1; }

#ifdef __cplusplus
}
#endif
