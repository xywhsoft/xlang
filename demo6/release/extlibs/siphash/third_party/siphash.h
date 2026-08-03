#ifndef SIPHASH_H
#define SIPHASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SipHash-2-4: 128-bit key, 64-bit output */
uint64_t siphash_2_4(const void* in, size_t inlen, const uint8_t key[16]);

/* SipHash-2-4 (128-bit output variant) */
void siphash_2_4_128(const void* in, size_t inlen, const uint8_t key[16], uint8_t out[16]);

/* Streaming SipHash-2-4 (64-bit output) */
typedef struct {
	uint64_t v0, v1, v2, v3;
	uint8_t buf[8];
	size_t buflen;
	uint64_t total;
	uint8_t key[16];
} siphash_state;

void siphash_init(siphash_state* S, const uint8_t key[16]);
void siphash_update(siphash_state* S, const void* in, size_t inlen);
uint64_t siphash_final(siphash_state* S);
void siphash_final128(siphash_state* S, uint8_t out[16]);

#ifdef __cplusplus
}
#endif

#endif
