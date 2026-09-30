#ifndef SODIUM_H
#define SODIUM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int sodium_init(void);
void sodium_memzero(void *pnt, const size_t len);
void sodium_stackzero(const size_t len);

typedef struct {
    void *ctx;
} crypto_hash_sha512_state;

int crypto_hash_sha512_init(crypto_hash_sha512_state *state);
int crypto_hash_sha512_update(crypto_hash_sha512_state *state, const unsigned char *in, unsigned long long inlen);
int crypto_hash_sha512_final(crypto_hash_sha512_state *state, unsigned char *out);

int crypto_sign_ed25519_detached(unsigned char *sig, unsigned long long *siglen_p, const unsigned char *m, unsigned long long mlen, const unsigned char *sk);
int crypto_sign_verify_detached(const unsigned char *sig, const unsigned char *m, unsigned long long mlen, const unsigned char *pk);
int crypto_sign_ed25519_pk_to_curve25519(unsigned char *curve25519_pk, const unsigned char *ed25519_pk);
int crypto_scalarmult_curve25519_base(unsigned char *q, const unsigned char *n);
int crypto_sign_ed25519_sk_to_curve25519(unsigned char *curve25519_sk, const unsigned char *ed25519_sk);
int crypto_scalarmult_curve25519(unsigned char *q, const unsigned char *n, const unsigned char *p);
void randombytes_buf(void * const buf, const size_t size);
#ifdef __cplusplus
}
#endif
#endif
