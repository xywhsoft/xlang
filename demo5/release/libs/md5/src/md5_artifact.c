#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XMD5_EXPORT __declspec(dllexport)
#else
#define XMD5_EXPORT
#endif

#if defined(__GNUC__)
#define XMD5_THREAD __thread
#elif defined(_MSC_VER)
#define XMD5_THREAD __declspec(thread)
#else
#define XMD5_THREAD
#endif

typedef struct XMd5Context {
	uint32_t state[4];
	uint64_t bit_count;
	unsigned char buffer[64];
} XMd5Context;

static XMD5_THREAD char g_xmd5_error[512];
static XMD5_THREAD char g_xmd5_hex[33];

static void xmd5_set_error(const char* text)
{
	if ( text == NULL ) text = "";
	strncpy(g_xmd5_error, text, sizeof(g_xmd5_error) - 1u);
	g_xmd5_error[sizeof(g_xmd5_error) - 1u] = '\0';
}

static uint32_t xmd5_load32(const unsigned char* bytes)
{
	return ((uint32_t)bytes[0]) |
		((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) |
		((uint32_t)bytes[3] << 24);
}

static void xmd5_store32(unsigned char* bytes, uint32_t value)
{
	bytes[0] = (unsigned char)(value & 0xffu);
	bytes[1] = (unsigned char)((value >> 8) & 0xffu);
	bytes[2] = (unsigned char)((value >> 16) & 0xffu);
	bytes[3] = (unsigned char)((value >> 24) & 0xffu);
}

static uint32_t xmd5_left_rotate(uint32_t value, uint32_t amount)
{
	return (value << amount) | (value >> (32u - amount));
}

#define XMD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define XMD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define XMD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define XMD5_I(x, y, z) ((y) ^ ((x) | ~(z)))
#define XMD5_STEP(fn, a, b, c, d, x, s, ac) \
	do { \
		(a) += fn((b), (c), (d)) + (x) + (uint32_t)(ac); \
		(a) = xmd5_left_rotate((a), (uint32_t)(s)); \
		(a) += (b); \
	} while ( 0 )

static void xmd5_transform(uint32_t state[4], const unsigned char block[64])
{
	uint32_t a = state[0];
	uint32_t b = state[1];
	uint32_t c = state[2];
	uint32_t d = state[3];
	uint32_t x[16];
	int i;
	for ( i = 0; i < 16; ++i ) x[i] = xmd5_load32(block + i * 4);

	XMD5_STEP(XMD5_F, a, b, c, d, x[0], 7, 0xd76aa478u);
	XMD5_STEP(XMD5_F, d, a, b, c, x[1], 12, 0xe8c7b756u);
	XMD5_STEP(XMD5_F, c, d, a, b, x[2], 17, 0x242070dbu);
	XMD5_STEP(XMD5_F, b, c, d, a, x[3], 22, 0xc1bdceeeu);
	XMD5_STEP(XMD5_F, a, b, c, d, x[4], 7, 0xf57c0fafu);
	XMD5_STEP(XMD5_F, d, a, b, c, x[5], 12, 0x4787c62au);
	XMD5_STEP(XMD5_F, c, d, a, b, x[6], 17, 0xa8304613u);
	XMD5_STEP(XMD5_F, b, c, d, a, x[7], 22, 0xfd469501u);
	XMD5_STEP(XMD5_F, a, b, c, d, x[8], 7, 0x698098d8u);
	XMD5_STEP(XMD5_F, d, a, b, c, x[9], 12, 0x8b44f7afu);
	XMD5_STEP(XMD5_F, c, d, a, b, x[10], 17, 0xffff5bb1u);
	XMD5_STEP(XMD5_F, b, c, d, a, x[11], 22, 0x895cd7beu);
	XMD5_STEP(XMD5_F, a, b, c, d, x[12], 7, 0x6b901122u);
	XMD5_STEP(XMD5_F, d, a, b, c, x[13], 12, 0xfd987193u);
	XMD5_STEP(XMD5_F, c, d, a, b, x[14], 17, 0xa679438eu);
	XMD5_STEP(XMD5_F, b, c, d, a, x[15], 22, 0x49b40821u);

	XMD5_STEP(XMD5_G, a, b, c, d, x[1], 5, 0xf61e2562u);
	XMD5_STEP(XMD5_G, d, a, b, c, x[6], 9, 0xc040b340u);
	XMD5_STEP(XMD5_G, c, d, a, b, x[11], 14, 0x265e5a51u);
	XMD5_STEP(XMD5_G, b, c, d, a, x[0], 20, 0xe9b6c7aau);
	XMD5_STEP(XMD5_G, a, b, c, d, x[5], 5, 0xd62f105du);
	XMD5_STEP(XMD5_G, d, a, b, c, x[10], 9, 0x02441453u);
	XMD5_STEP(XMD5_G, c, d, a, b, x[15], 14, 0xd8a1e681u);
	XMD5_STEP(XMD5_G, b, c, d, a, x[4], 20, 0xe7d3fbc8u);
	XMD5_STEP(XMD5_G, a, b, c, d, x[9], 5, 0x21e1cde6u);
	XMD5_STEP(XMD5_G, d, a, b, c, x[14], 9, 0xc33707d6u);
	XMD5_STEP(XMD5_G, c, d, a, b, x[3], 14, 0xf4d50d87u);
	XMD5_STEP(XMD5_G, b, c, d, a, x[8], 20, 0x455a14edu);
	XMD5_STEP(XMD5_G, a, b, c, d, x[13], 5, 0xa9e3e905u);
	XMD5_STEP(XMD5_G, d, a, b, c, x[2], 9, 0xfcefa3f8u);
	XMD5_STEP(XMD5_G, c, d, a, b, x[7], 14, 0x676f02d9u);
	XMD5_STEP(XMD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8au);

	XMD5_STEP(XMD5_H, a, b, c, d, x[5], 4, 0xfffa3942u);
	XMD5_STEP(XMD5_H, d, a, b, c, x[8], 11, 0x8771f681u);
	XMD5_STEP(XMD5_H, c, d, a, b, x[11], 16, 0x6d9d6122u);
	XMD5_STEP(XMD5_H, b, c, d, a, x[14], 23, 0xfde5380cu);
	XMD5_STEP(XMD5_H, a, b, c, d, x[1], 4, 0xa4beea44u);
	XMD5_STEP(XMD5_H, d, a, b, c, x[4], 11, 0x4bdecfa9u);
	XMD5_STEP(XMD5_H, c, d, a, b, x[7], 16, 0xf6bb4b60u);
	XMD5_STEP(XMD5_H, b, c, d, a, x[10], 23, 0xbebfbc70u);
	XMD5_STEP(XMD5_H, a, b, c, d, x[13], 4, 0x289b7ec6u);
	XMD5_STEP(XMD5_H, d, a, b, c, x[0], 11, 0xeaa127fau);
	XMD5_STEP(XMD5_H, c, d, a, b, x[3], 16, 0xd4ef3085u);
	XMD5_STEP(XMD5_H, b, c, d, a, x[6], 23, 0x04881d05u);
	XMD5_STEP(XMD5_H, a, b, c, d, x[9], 4, 0xd9d4d039u);
	XMD5_STEP(XMD5_H, d, a, b, c, x[12], 11, 0xe6db99e5u);
	XMD5_STEP(XMD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8u);
	XMD5_STEP(XMD5_H, b, c, d, a, x[2], 23, 0xc4ac5665u);

	XMD5_STEP(XMD5_I, a, b, c, d, x[0], 6, 0xf4292244u);
	XMD5_STEP(XMD5_I, d, a, b, c, x[7], 10, 0x432aff97u);
	XMD5_STEP(XMD5_I, c, d, a, b, x[14], 15, 0xab9423a7u);
	XMD5_STEP(XMD5_I, b, c, d, a, x[5], 21, 0xfc93a039u);
	XMD5_STEP(XMD5_I, a, b, c, d, x[12], 6, 0x655b59c3u);
	XMD5_STEP(XMD5_I, d, a, b, c, x[3], 10, 0x8f0ccc92u);
	XMD5_STEP(XMD5_I, c, d, a, b, x[10], 15, 0xffeff47du);
	XMD5_STEP(XMD5_I, b, c, d, a, x[1], 21, 0x85845dd1u);
	XMD5_STEP(XMD5_I, a, b, c, d, x[8], 6, 0x6fa87e4fu);
	XMD5_STEP(XMD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0u);
	XMD5_STEP(XMD5_I, c, d, a, b, x[6], 15, 0xa3014314u);
	XMD5_STEP(XMD5_I, b, c, d, a, x[13], 21, 0x4e0811a1u);
	XMD5_STEP(XMD5_I, a, b, c, d, x[4], 6, 0xf7537e82u);
	XMD5_STEP(XMD5_I, d, a, b, c, x[11], 10, 0xbd3af235u);
	XMD5_STEP(XMD5_I, c, d, a, b, x[2], 15, 0x2ad7d2bbu);
	XMD5_STEP(XMD5_I, b, c, d, a, x[9], 21, 0xeb86d391u);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

static void xmd5_init(XMd5Context* ctx)
{
	ctx->state[0] = 0x67452301u;
	ctx->state[1] = 0xefcdab89u;
	ctx->state[2] = 0x98badcfeu;
	ctx->state[3] = 0x10325476u;
	ctx->bit_count = 0u;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

static void xmd5_update(XMd5Context* ctx, const unsigned char* data, size_t len)
{
	size_t used = (size_t)((ctx->bit_count >> 3) & 63u);
	size_t free_space = 64u - used;
	ctx->bit_count += (uint64_t)len << 3;
	if ( used != 0u && len >= free_space ) {
		memcpy(ctx->buffer + used, data, free_space);
		xmd5_transform(ctx->state, ctx->buffer);
		data += free_space;
		len -= free_space;
		used = 0u;
	}
	while ( len >= 64u ) {
		xmd5_transform(ctx->state, data);
		data += 64u;
		len -= 64u;
	}
	if ( len != 0u ) memcpy(ctx->buffer + used, data, len);
}

static void xmd5_final(XMd5Context* ctx, unsigned char digest[16])
{
	static const unsigned char padding[64] = { 0x80 };
	unsigned char length_bytes[8];
	size_t used = (size_t)((ctx->bit_count >> 3) & 63u);
	size_t pad_len = (used < 56u) ? (56u - used) : (120u - used);
	uint64_t bits = ctx->bit_count;
	int i;
	for ( i = 0; i < 8; ++i ) length_bytes[i] = (unsigned char)((bits >> (8 * i)) & 0xffu);
	xmd5_update(ctx, padding, pad_len);
	xmd5_update(ctx, length_bytes, 8u);
	for ( i = 0; i < 4; ++i ) xmd5_store32(digest + i * 4, ctx->state[i]);
}

static const char* xmd5_format(const unsigned char digest[16])
{
	static const char hex[] = "0123456789abcdef";
	int i;
	for ( i = 0; i < 16; ++i ) {
		g_xmd5_hex[i * 2] = hex[(digest[i] >> 4) & 0x0f];
		g_xmd5_hex[i * 2 + 1] = hex[digest[i] & 0x0f];
	}
	g_xmd5_hex[32] = '\0';
	xmd5_set_error("");
	return g_xmd5_hex;
}

static const char* xmd5_digest_bytes(const unsigned char* data, size_t len)
{
	XMd5Context ctx;
	unsigned char digest[16];
	xmd5_init(&ctx);
	xmd5_update(&ctx, data, len);
	xmd5_final(&ctx, digest);
	return xmd5_format(digest);
}

XMD5_EXPORT const char* xmd5_version(void)
{
	return "md5 0.1.0";
}

XMD5_EXPORT const char* xmd5_last_error(void)
{
	return g_xmd5_error;
}

XMD5_EXPORT const char* xmd5_hex(const char* text)
{
	if ( text == NULL ) text = "";
	return xmd5_digest_bytes((const unsigned char*)text, strlen(text));
}

XMD5_EXPORT const char* xmd5_file(const char* path)
{
	FILE* file;
	XMd5Context ctx;
	unsigned char buffer[8192];
	unsigned char digest[16];
	size_t n;
	if ( path == NULL || path[0] == '\0' ) {
		xmd5_set_error("empty file path");
		return "";
	}
	file = fopen(path, "rb");
	if ( file == NULL ) {
		xmd5_set_error("open file failed");
		return "";
	}
	xmd5_init(&ctx);
	while ( (n = fread(buffer, 1u, sizeof(buffer), file)) != 0u ) {
		xmd5_update(&ctx, buffer, n);
	}
	if ( ferror(file) ) {
		fclose(file);
		xmd5_set_error("read file failed");
		return "";
	}
	fclose(file);
	xmd5_final(&ctx, digest);
	return xmd5_format(digest);
}

XMD5_EXPORT int xmd5_valid(const char* text)
{
	int i;
	if ( text == NULL || strlen(text) != 32u ) return 0;
	for ( i = 0; i < 32; ++i ) {
		if ( !isxdigit((unsigned char)text[i]) ) return 0;
	}
	return 1;
}
