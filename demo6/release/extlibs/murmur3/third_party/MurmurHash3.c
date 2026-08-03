#include "MurmurHash3.h"
#include <string.h>

/* MurmurHash3 - Austin Appleby's public domain implementation */

static uint32_t mm3_rotl32(uint32_t x, int8_t r) { return (x << r) | (x >> (32 - r)); }
static uint64_t mm3_rotl64(uint64_t x, int8_t r) { return (x << r) | (x >> (64 - r)); }

static uint32_t mm3_getblock32(const uint8_t* p, size_t i)
{
	return (uint32_t)p[i*4] | ((uint32_t)p[i*4+1]<<8) | ((uint32_t)p[i*4+2]<<16) | ((uint32_t)p[i*4+3]<<24);
}

static uint64_t mm3_getblock64(const uint8_t* p, size_t i)
{
	return (uint64_t)p[i*8] | ((uint64_t)p[i*8+1]<<8) | ((uint64_t)p[i*8+2]<<16) | ((uint64_t)p[i*8+3]<<24)
		| ((uint64_t)p[i*8+4]<<32) | ((uint64_t)p[i*8+5]<<40) | ((uint64_t)p[i*8+6]<<48) | ((uint64_t)p[i*8+7]<<56);
}

static uint32_t mm3_fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static uint64_t mm3_fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= 0xff51afd7ed558ccdULL;
	k ^= k >> 33;
	k *= 0xc4ceb9fe1a85ec53ULL;
	k ^= k >> 33;
	return k;
}

static void mm3_store64(uint8_t* p, uint64_t v)
{
	p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
	p[4]=(uint8_t)(v>>32); p[5]=(uint8_t)(v>>40); p[6]=(uint8_t)(v>>48); p[7]=(uint8_t)(v>>56);
}


/* ---- MurmurHash3 x86_32 ---- */

uint32_t murmur3_32(const void* key, size_t len, uint32_t seed)
{
	const uint8_t* data = (const uint8_t*)key;
	const size_t nblocks = len / 4;
	uint32_t h1 = seed;
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;
	size_t i;

	for (i = 0; i < nblocks; ++i) {
		uint32_t k1 = mm3_getblock32(data, i);
		k1 *= c1;
		k1 = mm3_rotl32(k1, 15);
		k1 *= c2;
		h1 ^= k1;
		h1 = mm3_rotl32(h1, 13);
		h1 = h1 * 5 + 0xe6546b64;
	}

	{
		const uint8_t* tail = data + nblocks * 4;
		uint32_t k1 = 0;
		switch (len & 3) {
		case 3: k1 ^= (uint32_t)tail[2] << 16; /* fall through */
		case 2: k1 ^= (uint32_t)tail[1] << 8;  /* fall through */
		case 1: k1 ^= (uint32_t)tail[0];
			k1 *= c1; k1 = mm3_rotl32(k1, 15); k1 *= c2; h1 ^= k1;
		}
	}

	h1 ^= (uint32_t)len;
	h1 = mm3_fmix32(h1);
	return h1;
}


/* ---- Streaming MurmurHash3 x86_32 ---- */

void murmur3_32_init(murmur3_32_state* S, uint32_t seed)
{
	memset(S, 0, sizeof(*S));
	S->h1 = seed;
	S->seed = seed;
}

void murmur3_32_update(murmur3_32_state* S, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;
	S->total += inlen;
	while (inlen > 0) {
		size_t fill = 4 - S->buflen;
		if (inlen >= fill) {
			uint32_t k1;
			memcpy(S->buf + S->buflen, p, fill);
			k1 = (uint32_t)S->buf[0] | ((uint32_t)S->buf[1]<<8) | ((uint32_t)S->buf[2]<<16) | ((uint32_t)S->buf[3]<<24);
			k1 *= c1;
			k1 = mm3_rotl32(k1, 15);
			k1 *= c2;
			S->h1 ^= k1;
			S->h1 = mm3_rotl32(S->h1, 13);
			S->h1 = S->h1 * 5 + 0xe6546b64;
			S->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(S->buf + S->buflen, p, inlen);
			S->buflen += inlen;
			inlen = 0;
		}
	}
}

uint32_t murmur3_32_final(murmur3_32_state* S)
{
	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;
	uint32_t h1 = S->h1;
	uint32_t k1 = 0;

	switch (S->buflen) {
	case 3: k1 ^= (uint32_t)S->buf[2] << 16; /* fall through */
	case 2: k1 ^= (uint32_t)S->buf[1] << 8;  /* fall through */
	case 1: k1 ^= (uint32_t)S->buf[0];
		k1 *= c1; k1 = mm3_rotl32(k1, 15); k1 *= c2; h1 ^= k1;
	}

	h1 ^= (uint32_t)S->total;
	h1 = mm3_fmix32(h1);
	return h1;
}


/* ---- MurmurHash3 x86_128 (x64 optimized) ---- */

void murmur3_128(const void* key, size_t len, uint32_t seed, void* out)
{
	const uint8_t* data = (const uint8_t*)key;
	const size_t nblocks = len / 16;
	uint64_t h1 = seed, h2 = seed;
	const uint64_t c1 = 0x87c37b91114253d5ULL;
	const uint64_t c2 = 0x4cf5ad432745937fULL;
	size_t i;

	for (i = 0; i < nblocks; ++i) {
		uint64_t k1 = mm3_getblock64(data, i * 2);
		uint64_t k2 = mm3_getblock64(data, i * 2 + 1);
		k1 *= c1; k1 = mm3_rotl64(k1, 31); k1 *= c2; h1 ^= k1;
		h1 = mm3_rotl64(h1, 27); h1 += h2; h1 = h1 * 5 + 0x52dce729;
		k2 *= c2; k2 = mm3_rotl64(k2, 33); k2 *= c1; h2 ^= k2;
		h2 = mm3_rotl64(h2, 31); h2 += h1; h2 = h2 * 5 + 0x38495ab5;
	}

	{
		const uint8_t* tail = data + nblocks * 16;
		uint64_t k1 = 0, k2 = 0;
		switch (len & 15) {
		case 15: k2 ^= ((uint64_t)tail[14]) << 48; /* fall through */
		case 14: k2 ^= ((uint64_t)tail[13]) << 40; /* fall through */
		case 13: k2 ^= ((uint64_t)tail[12]) << 32; /* fall through */
		case 12: k2 ^= ((uint64_t)tail[11]) << 24; /* fall through */
		case 11: k2 ^= ((uint64_t)tail[10]) << 16; /* fall through */
		case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;  /* fall through */
		case  9: k2 ^= ((uint64_t)tail[ 8]);
			k2 *= c2; k2 = mm3_rotl64(k2, 33); k2 *= c1; h2 ^= k2;
			/* fall through */
		case  8: k1 ^= ((uint64_t)tail[ 7]) << 56; /* fall through */
		case  7: k1 ^= ((uint64_t)tail[ 6]) << 48; /* fall through */
		case  6: k1 ^= ((uint64_t)tail[ 5]) << 40; /* fall through */
		case  5: k1 ^= ((uint64_t)tail[ 4]) << 32; /* fall through */
		case  4: k1 ^= ((uint64_t)tail[ 3]) << 24; /* fall through */
		case  3: k1 ^= ((uint64_t)tail[ 2]) << 16; /* fall through */
		case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;  /* fall through */
		case  1: k1 ^= ((uint64_t)tail[ 0]);
			k1 *= c1; k1 = mm3_rotl64(k1, 31); k1 *= c2; h1 ^= k1;
		}
	}

	h1 ^= (uint64_t)len; h2 ^= (uint64_t)len;
	h1 += h2; h2 += h1;
	h1 = mm3_fmix64(h1); h2 = mm3_fmix64(h2);
	h1 += h2; h2 += h1;
	mm3_store64((uint8_t*)out, h1);
	mm3_store64((uint8_t*)out + 8, h2);
}


/* ---- Streaming MurmurHash3 x86_128 ---- */

void murmur3_128_init(murmur3_128_state* S, uint32_t seed)
{
	memset(S, 0, sizeof(*S));
	S->h1 = seed;
	S->h2 = seed;
	S->seed = seed;
}

void murmur3_128_update(murmur3_128_state* S, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	const uint64_t c1 = 0x87c37b91114253d5ULL;
	const uint64_t c2 = 0x4cf5ad432745937fULL;
	S->total += inlen;
	while (inlen > 0) {
		size_t fill = 16 - S->buflen;
		if (inlen >= fill) {
			uint64_t k1, k2;
			memcpy(S->buf + S->buflen, p, fill);
			k1 = mm3_getblock64(S->buf, 0);
			k2 = mm3_getblock64(S->buf, 1);
			k1 *= c1; k1 = mm3_rotl64(k1, 31); k1 *= c2; S->h1 ^= k1;
			S->h1 = mm3_rotl64(S->h1, 27); S->h1 += S->h2; S->h1 = S->h1 * 5 + 0x52dce729;
			k2 *= c2; k2 = mm3_rotl64(k2, 33); k2 *= c1; S->h2 ^= k2;
			S->h2 = mm3_rotl64(S->h2, 31); S->h2 += S->h1; S->h2 = S->h2 * 5 + 0x38495ab5;
			S->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(S->buf + S->buflen, p, inlen);
			S->buflen += inlen;
			inlen = 0;
		}
	}
}

void murmur3_128_final(murmur3_128_state* S, void* out)
{
	const uint64_t c1 = 0x87c37b91114253d5ULL;
	const uint64_t c2 = 0x4cf5ad432745937fULL;
	uint64_t h1 = S->h1, h2 = S->h2;
	uint64_t k1 = 0, k2 = 0;
	const uint8_t* tail = S->buf;
	size_t len = (size_t)S->total;

	switch (S->buflen) {
	case 15: k2 ^= ((uint64_t)tail[14]) << 48; /* fall through */
	case 14: k2 ^= ((uint64_t)tail[13]) << 40; /* fall through */
	case 13: k2 ^= ((uint64_t)tail[12]) << 32; /* fall through */
	case 12: k2 ^= ((uint64_t)tail[11]) << 24; /* fall through */
	case 11: k2 ^= ((uint64_t)tail[10]) << 16; /* fall through */
	case 10: k2 ^= ((uint64_t)tail[ 9]) << 8;  /* fall through */
	case  9: k2 ^= ((uint64_t)tail[ 8]);
		k2 *= c2; k2 = mm3_rotl64(k2, 33); k2 *= c1; h2 ^= k2;
		/* fall through */
	case  8: k1 ^= ((uint64_t)tail[ 7]) << 56; /* fall through */
	case  7: k1 ^= ((uint64_t)tail[ 6]) << 48; /* fall through */
	case  6: k1 ^= ((uint64_t)tail[ 5]) << 40; /* fall through */
	case  5: k1 ^= ((uint64_t)tail[ 4]) << 32; /* fall through */
	case  4: k1 ^= ((uint64_t)tail[ 3]) << 24; /* fall through */
	case  3: k1 ^= ((uint64_t)tail[ 2]) << 16; /* fall through */
	case  2: k1 ^= ((uint64_t)tail[ 1]) << 8;  /* fall through */
	case  1: k1 ^= ((uint64_t)tail[ 0]);
		k1 *= c1; k1 = mm3_rotl64(k1, 31); k1 *= c2; h1 ^= k1;
	}

	h1 ^= (uint64_t)len; h2 ^= (uint64_t)len;
	h1 += h2; h2 += h1;
	h1 = mm3_fmix64(h1); h2 = mm3_fmix64(h2);
	h1 += h2; h2 += h1;
	mm3_store64((uint8_t*)out, h1);
	mm3_store64((uint8_t*)out + 8, h2);
}
