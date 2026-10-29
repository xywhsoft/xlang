#ifndef BLAKE2_H
#define BLAKE2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BLAKE2b: 64-bit words, up to 64-byte output */
typedef struct {
	uint64_t h[8];
	uint64_t t[2];
	uint64_t f[2];
	uint8_t buf[128];
	size_t buflen;
	size_t outlen;
} blake2b_state;

void blake2b_init(blake2b_state* S, size_t outlen);
void blake2b_init_key(blake2b_state* S, size_t outlen, const void* key, size_t keylen);
void blake2b_update(blake2b_state* S, const void* in, size_t inlen);
void blake2b_final(blake2b_state* S, void* out, size_t outlen);

/* BLAKE2s: 32-bit words, up to 32-byte output */
typedef struct {
	uint32_t h[8];
	uint32_t t[2];
	uint32_t f[2];
	uint8_t buf[64];
	size_t buflen;
	size_t outlen;
} blake2s_state;

void blake2s_init(blake2s_state* S, size_t outlen);
void blake2s_init_key(blake2s_state* S, size_t outlen, const void* key, size_t keylen);
void blake2s_update(blake2s_state* S, const void* in, size_t inlen);
void blake2s_final(blake2s_state* S, void* out, size_t outlen);

#ifdef __cplusplus
}
#endif

#endif
