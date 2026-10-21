#include "blake2.h"
#include "xrt.h"
#include <string.h>

/* BLAKE2 reference C implementation (portable, no SIMD) */

static const uint64_t blake2b_IV[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t blake2b_sigma[12][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
	{14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3},
	{11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4},
	{ 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8},
	{ 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13},
	{ 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9},
	{12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11},
	{13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10},
	{ 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5},
	{10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0},
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
	{14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3}
};

static uint64_t blake2b_load64(const uint8_t* p)
{
	return (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24)
		| ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48) | ((uint64_t)p[7]<<56);
}

static void blake2b_store64(uint8_t* p, uint64_t v)
{
	p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
	p[4]=(uint8_t)(v>>32); p[5]=(uint8_t)(v>>40); p[6]=(uint8_t)(v>>48); p[7]=(uint8_t)(v>>56);
}

static uint64_t rotr64(uint64_t w, unsigned c) { return (w >> c) | (w << (64 - c)); }

#define B2B_G(a,b,c,d,x,y) do { \
	v[a] += v[b] + (x); v[d] = rotr64(v[d]^v[a],32); \
	v[c] += v[d];       v[b] = rotr64(v[b]^v[c],24); \
	v[a] += v[b] + (y); v[d] = rotr64(v[d]^v[a],16); \
	v[c] += v[d];       v[b] = rotr64(v[b]^v[c],63); } while(0)

static void blake2b_compress(blake2b_state* S, const uint8_t block[128])
{
	uint64_t m[16], v[16];
	size_t i;
	for (i = 0; i < 16; ++i) m[i] = blake2b_load64(block + i * 8);
	for (i = 0; i < 8; ++i) v[i] = S->h[i];
	v[8] = blake2b_IV[0]; v[9] = blake2b_IV[1]; v[10] = blake2b_IV[2]; v[11] = blake2b_IV[3];
	v[12] = blake2b_IV[4] ^ S->t[0]; v[13] = blake2b_IV[5] ^ S->t[1];
	v[14] = blake2b_IV[6] ^ S->f[0]; v[15] = blake2b_IV[7] ^ S->f[1];
	for (i = 0; i < 12; ++i) {
		const uint8_t* s = blake2b_sigma[i];
		B2B_G(0,4, 8,12, m[s[ 0]], m[s[ 1]]);
		B2B_G(1,5, 9,13, m[s[ 2]], m[s[ 3]]);
		B2B_G(2,6,10,14, m[s[ 4]], m[s[ 5]]);
		B2B_G(3,7,11,15, m[s[ 6]], m[s[ 7]]);
		B2B_G(0,5,10,15, m[s[ 8]], m[s[ 9]]);
		B2B_G(1,6,11,12, m[s[10]], m[s[11]]);
		B2B_G(2,7, 8,13, m[s[12]], m[s[13]]);
		B2B_G(3,4, 9,14, m[s[14]], m[s[15]]);
	}
	for (i = 0; i < 8; ++i) S->h[i] ^= v[i] ^ v[i+8];
}

static void blake2b_increment(blake2b_state* S, uint64_t inc)
{
	S->t[0] += inc;
	if (S->t[0] < inc) S->t[1]++;
}

void blake2b_init(blake2b_state* S, size_t outlen)
{
	size_t i;
	memset(S, 0, sizeof(*S));
	for (i = 0; i < 8; ++i) S->h[i] = blake2b_IV[i];
	S->h[0] ^= 0x01010000UL ^ (uint32_t)outlen;
	S->outlen = outlen;
}

void blake2b_init_key(blake2b_state* S, size_t outlen, const void* key, size_t keylen)
{
	size_t i;
	memset(S, 0, sizeof(*S));
	for (i = 0; i < 8; ++i) S->h[i] = blake2b_IV[i];
	S->h[0] ^= 0x01010000UL ^ ((uint32_t)keylen << 8) ^ (uint32_t)outlen;
	S->outlen = outlen;
	if (keylen > 0) {
		uint8_t block[128];
		memset(block, 0, 128);
		memcpy(block, key, keylen);
		blake2b_update(S, block, 128);
		xrtSecureZero(block, sizeof(block));
	}
}

void blake2b_update(blake2b_state* S, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	while (inlen > 0) {
		size_t left = S->buflen;
		size_t fill = 128 - left;
		if (inlen > fill) {
			memcpy(S->buf + left, p, fill);
			S->buflen = 128;
			blake2b_increment(S, 128);
			blake2b_compress(S, S->buf);
			S->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(S->buf + left, p, inlen);
			S->buflen += inlen;
			inlen = 0;
		}
	}
}

void blake2b_final(blake2b_state* S, void* out, size_t outlen)
{
	uint8_t buffer[64];
	size_t i;
	blake2b_increment(S, (uint64_t)S->buflen);
	S->f[0] = (uint64_t)-1;
	memset(S->buf + S->buflen, 0, 128 - S->buflen);
	blake2b_compress(S, S->buf);
	for (i = 0; i < 8; ++i) blake2b_store64(buffer + i * 8, S->h[i]);
	memcpy(out, buffer, outlen < 64 ? outlen : 64);
	xrtSecureZero(buffer, sizeof(buffer));
}


/* ═══════════════ BLAKE2s ═══════════════ */

static const uint32_t blake2s_IV[8] = {
	0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
	0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t blake2s_sigma[10][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
	{14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3},
	{11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4},
	{ 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8},
	{ 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13},
	{ 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9},
	{12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11},
	{13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10},
	{ 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5},
	{10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0}
};

static uint32_t blake2s_load32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static void blake2s_store32(uint8_t* p, uint32_t v)
{
	p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static uint32_t rotr32s(uint32_t w, unsigned c) { return (w >> c) | (w << (32 - c)); }

#define B2S_G(a,b,c,d,x,y) do { \
	v[a] += v[b] + (x); v[d] = rotr32s(v[d]^v[a],16); \
	v[c] += v[d];       v[b] = rotr32s(v[b]^v[c],12); \
	v[a] += v[b] + (y); v[d] = rotr32s(v[d]^v[a], 8); \
	v[c] += v[d];       v[b] = rotr32s(v[b]^v[c], 7); } while(0)

static void blake2s_compress(blake2s_state* S, const uint8_t block[64])
{
	uint32_t m[16], v[16];
	size_t i;
	for (i = 0; i < 16; ++i) m[i] = blake2s_load32(block + i * 4);
	for (i = 0; i < 8; ++i) v[i] = S->h[i];
	v[8] = blake2s_IV[0]; v[9] = blake2s_IV[1]; v[10] = blake2s_IV[2]; v[11] = blake2s_IV[3];
	v[12] = blake2s_IV[4] ^ S->t[0]; v[13] = blake2s_IV[5] ^ S->t[1];
	v[14] = blake2s_IV[6] ^ S->f[0]; v[15] = blake2s_IV[7] ^ S->f[1];
	for (i = 0; i < 10; ++i) {
		const uint8_t* s = blake2s_sigma[i];
		B2S_G(0,4, 8,12, m[s[ 0]], m[s[ 1]]);
		B2S_G(1,5, 9,13, m[s[ 2]], m[s[ 3]]);
		B2S_G(2,6,10,14, m[s[ 4]], m[s[ 5]]);
		B2S_G(3,7,11,15, m[s[ 6]], m[s[ 7]]);
		B2S_G(0,5,10,15, m[s[ 8]], m[s[ 9]]);
		B2S_G(1,6,11,12, m[s[10]], m[s[11]]);
		B2S_G(2,7, 8,13, m[s[12]], m[s[13]]);
		B2S_G(3,4, 9,14, m[s[14]], m[s[15]]);
	}
	for (i = 0; i < 8; ++i) S->h[i] ^= v[i] ^ v[i+8];
}

static void blake2s_increment(blake2s_state* S, uint32_t inc)
{
	S->t[0] += inc;
	if (S->t[0] < inc) S->t[1]++;
}

void blake2s_init(blake2s_state* S, size_t outlen)
{
	size_t i;
	memset(S, 0, sizeof(*S));
	for (i = 0; i < 8; ++i) S->h[i] = blake2s_IV[i];
	S->h[0] ^= 0x01010000UL ^ (uint32_t)outlen;
	S->outlen = outlen;
}

void blake2s_init_key(blake2s_state* S, size_t outlen, const void* key, size_t keylen)
{
	size_t i;
	memset(S, 0, sizeof(*S));
	for (i = 0; i < 8; ++i) S->h[i] = blake2s_IV[i];
	S->h[0] ^= 0x01010000UL ^ ((uint32_t)keylen << 8) ^ (uint32_t)outlen;
	S->outlen = outlen;
	if (keylen > 0) {
		uint8_t block[64];
		memset(block, 0, 64);
		memcpy(block, key, keylen);
		blake2s_update(S, block, 64);
	xrtSecureZero(block, sizeof(block));
	}
}

void blake2s_update(blake2s_state* S, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	while (inlen > 0) {
		size_t left = S->buflen;
		size_t fill = 64 - left;
		if (inlen > fill) {
			memcpy(S->buf + left, p, fill);
			S->buflen = 64;
			blake2s_increment(S, 64);
			blake2s_compress(S, S->buf);
			S->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(S->buf + left, p, inlen);
			S->buflen += inlen;
			inlen = 0;
		}
	}
}

void blake2s_final(blake2s_state* S, void* out, size_t outlen)
{
	uint8_t buffer[32];
	size_t i;
	blake2s_increment(S, (uint32_t)S->buflen);
	S->f[0] = (uint32_t)-1;
	memset(S->buf + S->buflen, 0, 64 - S->buflen);
	blake2s_compress(S, S->buf);
	for (i = 0; i < 8; ++i) blake2s_store32(buffer + i * 4, S->h[i]);
	memcpy(out, buffer, outlen < 32 ? outlen : 32);
	xrtSecureZero(buffer, sizeof(buffer));
}
