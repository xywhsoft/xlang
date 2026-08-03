#ifndef MURMURHASH3_H
#define MURMURHASH3_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MurmurHash3 x86_32: 32-bit output */
uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

/* MurmurHash3 x86_128: 128-bit output (optimized for x64) */
void murmur3_128(const void* key, size_t len, uint32_t seed, void* out);

/* Streaming MurmurHash3 x86_32 */
typedef struct {
	uint32_t h1;
	uint8_t buf[4];
	size_t buflen;
	uint64_t total;
	uint32_t seed;
} murmur3_32_state;

void murmur3_32_init(murmur3_32_state* S, uint32_t seed);
void murmur3_32_update(murmur3_32_state* S, const void* in, size_t inlen);
uint32_t murmur3_32_final(murmur3_32_state* S);

/* Streaming MurmurHash3 x86_128 */
typedef struct {
	uint64_t h1, h2;
	uint8_t buf[16];
	size_t buflen;
	uint64_t total;
	uint32_t seed;
} murmur3_128_state;

void murmur3_128_init(murmur3_128_state* S, uint32_t seed);
void murmur3_128_update(murmur3_128_state* S, const void* in, size_t inlen);
void murmur3_128_final(murmur3_128_state* S, void* out);

#ifdef __cplusplus
}
#endif

#endif
