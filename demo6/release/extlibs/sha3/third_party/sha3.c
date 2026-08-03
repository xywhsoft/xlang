#include "sha3.h"
#include <string.h>

/* SHA-3 / Keccak reference C implementation (portable) */

static const uint64_t keccak_rc[24] = {
	0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
	0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
	0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
	0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
	0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
	0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
	0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
	0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int keccak_rotc[24] = {
	1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
	27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const int keccak_piln[24] = {
	10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
	15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

static uint64_t rotl64(uint64_t x, int y) { return (x << y) | (x >> (64 - y)); }

static void keccakf(uint64_t st[25])
{
	int i, j, r;
	uint64_t t, bc[5];

	for (r = 0; r < 24; ++r) {
		/* Theta */
		for (i = 0; i < 5; ++i)
			bc[i] = st[i] ^ st[i+5] ^ st[i+10] ^ st[i+15] ^ st[i+20];
		for (i = 0; i < 5; ++i) {
			t = bc[(i+4)%5] ^ rotl64(bc[(i+1)%5], 1);
			for (j = 0; j < 25; j += 5)
				st[j+i] ^= t;
		}
		/* Rho Pi */
		t = st[1];
		for (i = 0; i < 24; ++i) {
			j = keccak_piln[i];
			bc[0] = st[j];
			st[j] = rotl64(t, keccak_rotc[i]);
			t = bc[0];
		}
		/* Chi */
		for (j = 0; j < 25; j += 5) {
			for (i = 0; i < 5; ++i)
				bc[i] = st[j+i];
			for (i = 0; i < 5; ++i)
				st[j+i] ^= (~bc[(i+1)%5]) & bc[(i+2)%5];
		}
		/* Iota */
		st[0] ^= keccak_rc[r];
	}
}

static uint64_t load64(const uint8_t* p)
{
	return (uint64_t)p[0] | ((uint64_t)p[1]<<8) | ((uint64_t)p[2]<<16) | ((uint64_t)p[3]<<24)
		| ((uint64_t)p[4]<<32) | ((uint64_t)p[5]<<40) | ((uint64_t)p[6]<<48) | ((uint64_t)p[7]<<56);
}

static void store64(uint8_t* p, uint64_t v)
{
	p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
	p[4]=(uint8_t)(v>>32); p[5]=(uint8_t)(v>>40); p[6]=(uint8_t)(v>>48); p[7]=(uint8_t)(v>>56);
}

static void keccak_absorb(sha3_ctx* ctx)
{
	size_t i;
	size_t lanes = ctx->rate / 8;
	for (i = 0; i < lanes; ++i)
		ctx->st[i] ^= load64(ctx->buf + i * 8);
	keccakf(ctx->st);
}

void sha3_init(sha3_ctx* ctx, int bits)
{
	memset(ctx, 0, sizeof(*ctx));
	switch (bits) {
	case 224: ctx->rate = 144; break;
	case 256: ctx->rate = 136; break;
	case 384: ctx->rate = 104; break;
	case 512: ctx->rate = 72;  break;
	default:  ctx->rate = 136; break;
	}
	ctx->pad = 0x06;
}

void shake_init(sha3_ctx* ctx, int strength)
{
	memset(ctx, 0, sizeof(*ctx));
	if (strength == 128) {
		ctx->rate = 168;
	} else {
		ctx->rate = 136;
	}
	ctx->pad = 0x1f;
}

void sha3_update(sha3_ctx* ctx, const void* in, size_t inlen)
{
	const uint8_t* p = (const uint8_t*)in;
	while (inlen > 0) {
		size_t fill = ctx->rate - ctx->buflen;
		if (inlen >= fill) {
			memcpy(ctx->buf + ctx->buflen, p, fill);
			ctx->buflen = ctx->rate;
			keccak_absorb(ctx);
			ctx->buflen = 0;
			p += fill;
			inlen -= fill;
		} else {
			memcpy(ctx->buf + ctx->buflen, p, inlen);
			ctx->buflen += inlen;
			inlen = 0;
		}
	}
}

void sha3_final(sha3_ctx* ctx, uint8_t* out, size_t outlen)
{
	size_t i;
	/* Pad: pad byte | 0x00...0x00 | 0x80 */
	memset(ctx->buf + ctx->buflen, 0, ctx->rate - ctx->buflen);
	ctx->buf[ctx->buflen] |= ctx->pad;
	ctx->buf[ctx->rate - 1] |= 0x80;
	keccak_absorb(ctx);
	/* Squeeze */
	for (i = 0; i < outlen / 8 && i < ctx->rate / 8; ++i)
		store64(out + i * 8, ctx->st[i]);
	if (outlen % 8 != 0) {
		uint8_t tmp[8];
		store64(tmp, ctx->st[outlen / 8]);
		memcpy(out + (outlen / 8) * 8, tmp, outlen % 8);
	}
}

void shake_final(sha3_ctx* ctx, uint8_t* out, size_t outlen)
{
	size_t offset = 0;
	/* Pad */
	memset(ctx->buf + ctx->buflen, 0, ctx->rate - ctx->buflen);
	ctx->buf[ctx->buflen] |= ctx->pad;
	ctx->buf[ctx->rate - 1] |= 0x80;
	keccak_absorb(ctx);
	/* Squeeze multiple blocks if needed */
	while (offset < outlen) {
		size_t chunk = outlen - offset;
		size_t i;
		if (chunk > ctx->rate) chunk = ctx->rate;
		for (i = 0; i < chunk / 8; ++i)
			store64(out + offset + i * 8, ctx->st[i]);
		if (chunk % 8 != 0) {
			uint8_t tmp[8];
			store64(tmp, ctx->st[chunk / 8]);
			memcpy(out + offset + (chunk / 8) * 8, tmp, chunk % 8);
		}
		offset += chunk;
		if (offset < outlen) keccakf(ctx->st);
	}
}

void sha3_hash(const void* in, size_t inlen, int bits, uint8_t* out, size_t outlen)
{
	sha3_ctx ctx;
	sha3_init(&ctx, bits);
	sha3_update(&ctx, in, inlen);
	sha3_final(&ctx, out, outlen);
}

void shake_hash(const void* in, size_t inlen, int strength, uint8_t* out, size_t outlen)
{
	sha3_ctx ctx;
	shake_init(&ctx, strength);
	sha3_update(&ctx, in, inlen);
	shake_final(&ctx, out, outlen);
}
