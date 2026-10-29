#ifndef SHA3_H
#define SHA3_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SHA-3 context (works for all variants) */
typedef struct {
	uint64_t st[25];
	uint8_t buf[200];
	size_t buflen;
	size_t rate;      /* rate in bytes */
	uint8_t pad;      /* domain separation byte: 0x06=SHA3, 0x1f=SHAKE */
} sha3_ctx;

/* Initialize for SHA3-{224,256,384,512} */
void sha3_init(sha3_ctx* ctx, int bits);

/* Initialize for SHAKE128 (outlen=variable) or SHAKE256 */
void shake_init(sha3_ctx* ctx, int strength);

void sha3_update(sha3_ctx* ctx, const void* in, size_t inlen);

/* Finalize SHA3 (fixed output) */
void sha3_final(sha3_ctx* ctx, uint8_t* out, size_t outlen);

/* Finalize once. Snapshot callers copy the state before this operation. */
void shake_final(sha3_ctx* ctx, uint8_t* out, size_t outlen);

/* One-shot convenience */
void sha3_hash(const void* in, size_t inlen, int bits, uint8_t* out, size_t outlen);
void shake_hash(const void* in, size_t inlen, int strength, uint8_t* out, size_t outlen);

#ifdef __cplusplus
}
#endif

#endif
