#include "siphash.h"
#include <string.h>

/* SipHash-2-4 reference C implementation (portable) */

static uint64_t sip_load64(const uint8_t* p)
{
	return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24)
		| ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

static void sip_store64(uint8_t* p, uint64_t v)
{
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
	p[4] = (uint8_t)(v >> 32); p[5] = (uint8_t)(v >> 40); p[6] = (uint8_t)(v >> 48); p[7] = (uint8_t)(v >> 56);
}

static uint64_t sip_rotl(uint64_t x, unsigned b)
{
	return (x << b) | (x >> (64 - b));
}

#define SIPROUND do { \
	v0 += v1; v1 = sip_rotl(v1, 13); v1 ^= v0; v0 = sip_rotl(v0, 32); \
	v2 += v3; v3 = sip_rotl(v3, 16); v3 ^= v2; \
	v0 += v3; v3 = sip_rotl(v3, 21); v3 ^= v0; \
	v2 += v1; v1 = sip_rotl(v1, 17); v1 ^= v2; v2 = sip_rotl(v2, 32); \
} while(0)

uint64_t siphash_2_4(const void* in, size_t inlen, const uint8_t key[16])
{
	const uint8_t* p = (const uint8_t*)in;
	uint64_t k0 = sip_load64(key);
	uint64_t k1 = sip_load64(key + 8);
	uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
	uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
	uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
	uint64_t v3 = k1 ^ 0x7465646279746573ULL;
	uint64_t b = ((uint64_t)inlen) << 56;
	size_t left = inlen;

	while (left >= 8) {
		uint64_t m = sip_load64(p);
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
		p += 8;
		left -= 8;
	}

	/* remaining bytes + length */
	switch (left) {
	case 7: b |= ((uint64_t)p[6]) << 48; /* fall through */
	case 6: b |= ((uint64_t)p[5]) << 40; /* fall through */
	case 5: b |= ((uint64_t)p[4]) << 32; /* fall through */
	case 4: b |= ((uint64_t)p[3]) << 24; /* fall through */
	case 3: b |= ((uint64_t)p[2]) << 16; /* fall through */
	case 2: b |= ((uint64_t)p[1]) << 8;  /* fall through */
	case 1: b |= ((uint64_t)p[0]);        break;
	case 0: break;
	}

	v3 ^= b;
	SIPROUND;
	SIPROUND;
	v0 ^= b;
	v2 ^= 0xff;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	return v0 ^ v1 ^ v2 ^ v3;
}

void siphash_2_4_128(const void* in, size_t inlen, const uint8_t key[16], uint8_t out[16])
{
	const uint8_t* p = (const uint8_t*)in;
	uint64_t k0 = sip_load64(key);
	uint64_t k1 = sip_load64(key + 8);
	uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
	uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
	uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
	uint64_t v3 = k1 ^ 0x7465646279746573ULL;
	uint64_t b = ((uint64_t)inlen) << 56;
	size_t left = inlen;

	v1 ^= 0xee;

	while (left >= 8) {
		uint64_t m = sip_load64(p);
		v3 ^= m;
		SIPROUND;
		SIPROUND;
		v0 ^= m;
		p += 8;
		left -= 8;
	}

	switch (left) {
	case 7: b |= ((uint64_t)p[6]) << 48; /* fall through */
	case 6: b |= ((uint64_t)p[5]) << 40; /* fall through */
	case 5: b |= ((uint64_t)p[4]) << 32; /* fall through */
	case 4: b |= ((uint64_t)p[3]) << 24; /* fall through */
	case 3: b |= ((uint64_t)p[2]) << 16; /* fall through */
	case 2: b |= ((uint64_t)p[1]) << 8;  /* fall through */
	case 1: b |= ((uint64_t)p[0]);        break;
	case 0: break;
	}

	v3 ^= b;
	SIPROUND;
	SIPROUND;
	v0 ^= b;
	v2 ^= 0xee;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	sip_store64(out, v0 ^ v1 ^ v2 ^ v3);

	v1 ^= 0xdd;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	sip_store64(out + 8, v0 ^ v1 ^ v2 ^ v3);
}


/* ---- Streaming API ---- */

void siphash_init(siphash_state* S, const uint8_t key[16])
{
	uint64_t k0 = sip_load64(key);
	uint64_t k1 = sip_load64(key + 8);
	memset(S, 0, sizeof(*S));
	memcpy(S->key, key, 16);
	S->v0 = k0 ^ 0x736f6d6570736575ULL;
	S->v1 = k1 ^ 0x646f72616e646f6dULL;
	S->v2 = k0 ^ 0x6c7967656e657261ULL;
	S->v3 = k1 ^ 0x7465646279746573ULL;
}

void siphash_update(siphash_state* S, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	uint64_t v0 = S->v0, v1 = S->v1, v2 = S->v2, v3 = S->v3;
	S->total += inlen;
	while (inlen > 0) {
		size_t fill = 8 - S->buflen;
		if (inlen >= fill) {
			memcpy(S->buf + S->buflen, p, fill);
			{
				uint64_t m = sip_load64(S->buf);
				v3 ^= m;
				SIPROUND;
				SIPROUND;
				v0 ^= m;
			}
			S->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(S->buf + S->buflen, p, inlen);
			S->buflen += inlen;
			inlen = 0;
		}
	}
	S->v0 = v0; S->v1 = v1; S->v2 = v2; S->v3 = v3;
}

uint64_t siphash_final(siphash_state* S)
{
	uint64_t v0 = S->v0, v1 = S->v1, v2 = S->v2, v3 = S->v3;
	uint64_t b = (S->total << 56);
	size_t i;
	for (i = 0; i < S->buflen; ++i) {
		b |= ((uint64_t)S->buf[i]) << (i * 8);
	}
	v3 ^= b;
	SIPROUND;
	SIPROUND;
	v0 ^= b;
	v2 ^= 0xff;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	return v0 ^ v1 ^ v2 ^ v3;
}

void siphash_final128(siphash_state* S, uint8_t out[16])
{
	uint64_t v0 = S->v0, v1 = S->v1, v2 = S->v2, v3 = S->v3;
	uint64_t b = (S->total << 56);
	size_t i;
	for (i = 0; i < S->buflen; ++i) {
		b |= ((uint64_t)S->buf[i]) << (i * 8);
	}
	v3 ^= b;
	SIPROUND;
	SIPROUND;
	v0 ^= b;
	v2 ^= 0xee;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	sip_store64(out, v0 ^ v1 ^ v2 ^ v3);
	v1 ^= 0xdd;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	SIPROUND;
	sip_store64(out + 8, v0 ^ v1 ^ v2 ^ v3);
}
