#include "include/xrt_md5.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_MD5_CHUNK 65536u

/* ═══════════════════════════════════════════════════════════
 * MD5 算法核心（RFC 1321）
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
	uint32_t state[4];
	uint64_t bit_count;
	unsigned char buffer[64];
} xrtmd5ctx;

static uint32_t md5_load32(const unsigned char* p)
{
	return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void md5_store32(unsigned char* p, uint32_t v)
{
	p[0] = (unsigned char)(v & 0xffu);
	p[1] = (unsigned char)((v >> 8) & 0xffu);
	p[2] = (unsigned char)((v >> 16) & 0xffu);
	p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static uint32_t md5_rotl(uint32_t v, uint32_t n)
{
	return (v << n) | (v >> (32u - n));
}

#define MD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))
#define MD5_STEP(fn, a, b, c, d, x, s, ac) \
	do { (a) += fn((b),(c),(d)) + (x) + (uint32_t)(ac); \
	     (a) = md5_rotl((a),(uint32_t)(s)); (a) += (b); } while(0)

static void md5_transform(uint32_t state[4], const unsigned char block[64])
{
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
	uint32_t x[16];
	int i;
	for (i = 0; i < 16; ++i) x[i] = md5_load32(block + i * 4);

	MD5_STEP(MD5_F,a,b,c,d,x[ 0], 7,0xd76aa478u); MD5_STEP(MD5_F,d,a,b,c,x[ 1],12,0xe8c7b756u);
	MD5_STEP(MD5_F,c,d,a,b,x[ 2],17,0x242070dbu); MD5_STEP(MD5_F,b,c,d,a,x[ 3],22,0xc1bdceeeu);
	MD5_STEP(MD5_F,a,b,c,d,x[ 4], 7,0xf57c0fafu); MD5_STEP(MD5_F,d,a,b,c,x[ 5],12,0x4787c62au);
	MD5_STEP(MD5_F,c,d,a,b,x[ 6],17,0xa8304613u); MD5_STEP(MD5_F,b,c,d,a,x[ 7],22,0xfd469501u);
	MD5_STEP(MD5_F,a,b,c,d,x[ 8], 7,0x698098d8u); MD5_STEP(MD5_F,d,a,b,c,x[ 9],12,0x8b44f7afu);
	MD5_STEP(MD5_F,c,d,a,b,x[10],17,0xffff5bb1u); MD5_STEP(MD5_F,b,c,d,a,x[11],22,0x895cd7beu);
	MD5_STEP(MD5_F,a,b,c,d,x[12], 7,0x6b901122u); MD5_STEP(MD5_F,d,a,b,c,x[13],12,0xfd987193u);
	MD5_STEP(MD5_F,c,d,a,b,x[14],17,0xa679438eu); MD5_STEP(MD5_F,b,c,d,a,x[15],22,0x49b40821u);

	MD5_STEP(MD5_G,a,b,c,d,x[ 1], 5,0xf61e2562u); MD5_STEP(MD5_G,d,a,b,c,x[ 6], 9,0xc040b340u);
	MD5_STEP(MD5_G,c,d,a,b,x[11],14,0x265e5a51u); MD5_STEP(MD5_G,b,c,d,a,x[ 0],20,0xe9b6c7aau);
	MD5_STEP(MD5_G,a,b,c,d,x[ 5], 5,0xd62f105du); MD5_STEP(MD5_G,d,a,b,c,x[10], 9,0x02441453u);
	MD5_STEP(MD5_G,c,d,a,b,x[15],14,0xd8a1e681u); MD5_STEP(MD5_G,b,c,d,a,x[ 4],20,0xe7d3fbc8u);
	MD5_STEP(MD5_G,a,b,c,d,x[ 9], 5,0x21e1cde6u); MD5_STEP(MD5_G,d,a,b,c,x[14], 9,0xc33707d6u);
	MD5_STEP(MD5_G,c,d,a,b,x[ 3],14,0xf4d50d87u); MD5_STEP(MD5_G,b,c,d,a,x[ 8],20,0x455a14edu);
	MD5_STEP(MD5_G,a,b,c,d,x[13], 5,0xa9e3e905u); MD5_STEP(MD5_G,d,a,b,c,x[ 2], 9,0xfcefa3f8u);
	MD5_STEP(MD5_G,c,d,a,b,x[ 7],14,0x676f02d9u); MD5_STEP(MD5_G,b,c,d,a,x[12],20,0x8d2a4c8au);

	MD5_STEP(MD5_H,a,b,c,d,x[ 5], 4,0xfffa3942u); MD5_STEP(MD5_H,d,a,b,c,x[ 8],11,0x8771f681u);
	MD5_STEP(MD5_H,c,d,a,b,x[11],16,0x6d9d6122u); MD5_STEP(MD5_H,b,c,d,a,x[14],23,0xfde5380cu);
	MD5_STEP(MD5_H,a,b,c,d,x[ 1], 4,0xa4beea44u); MD5_STEP(MD5_H,d,a,b,c,x[ 4],11,0x4bdecfa9u);
	MD5_STEP(MD5_H,c,d,a,b,x[ 7],16,0xf6bb4b60u); MD5_STEP(MD5_H,b,c,d,a,x[10],23,0xbebfbc70u);
	MD5_STEP(MD5_H,a,b,c,d,x[13], 4,0x289b7ec6u); MD5_STEP(MD5_H,d,a,b,c,x[ 0],11,0xeaa127fau);
	MD5_STEP(MD5_H,c,d,a,b,x[ 3],16,0xd4ef3085u); MD5_STEP(MD5_H,b,c,d,a,x[ 6],23,0x04881d05u);
	MD5_STEP(MD5_H,a,b,c,d,x[ 9], 4,0xd9d4d039u); MD5_STEP(MD5_H,d,a,b,c,x[12],11,0xe6db99e5u);
	MD5_STEP(MD5_H,c,d,a,b,x[15],16,0x1fa27cf8u); MD5_STEP(MD5_H,b,c,d,a,x[ 2],23,0xc4ac5665u);

	MD5_STEP(MD5_I,a,b,c,d,x[ 0], 6,0xf4292244u); MD5_STEP(MD5_I,d,a,b,c,x[ 7],10,0x432aff97u);
	MD5_STEP(MD5_I,c,d,a,b,x[14],15,0xab9423a7u); MD5_STEP(MD5_I,b,c,d,a,x[ 5],21,0xfc93a039u);
	MD5_STEP(MD5_I,a,b,c,d,x[12], 6,0x655b59c3u); MD5_STEP(MD5_I,d,a,b,c,x[ 3],10,0x8f0ccc92u);
	MD5_STEP(MD5_I,c,d,a,b,x[10],15,0xffeff47du); MD5_STEP(MD5_I,b,c,d,a,x[ 1],21,0x85845dd1u);
	MD5_STEP(MD5_I,a,b,c,d,x[ 8], 6,0x6fa87e4fu); MD5_STEP(MD5_I,d,a,b,c,x[15],10,0xfe2ce6e0u);
	MD5_STEP(MD5_I,c,d,a,b,x[ 6],15,0xa3014314u); MD5_STEP(MD5_I,b,c,d,a,x[13],21,0x4e0811a1u);
	MD5_STEP(MD5_I,a,b,c,d,x[ 4], 6,0xf7537e82u); MD5_STEP(MD5_I,d,a,b,c,x[11],10,0xbd3af235u);
	MD5_STEP(MD5_I,c,d,a,b,x[ 2],15,0x2ad7d2bbu); MD5_STEP(MD5_I,b,c,d,a,x[ 9],21,0xeb86d391u);

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_init(xrtmd5ctx* ctx)
{
	ctx->state[0] = 0x67452301u;
	ctx->state[1] = 0xefcdab89u;
	ctx->state[2] = 0x98badcfeu;
	ctx->state[3] = 0x10325476u;
	ctx->bit_count = 0u;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

static void md5_update(xrtmd5ctx* ctx, const unsigned char* data, size_t len)
{
	size_t used = (size_t)((ctx->bit_count >> 3) & 63u);
	size_t free_space = 64u - used;
	ctx->bit_count += (uint64_t)len << 3;
	if (used != 0u && len >= free_space) {
		memcpy(ctx->buffer + used, data, free_space);
		md5_transform(ctx->state, ctx->buffer);
		data += free_space;
		len -= free_space;
		used = 0u;
	}
	while (len >= 64u) {
		md5_transform(ctx->state, data);
		data += 64u;
		len -= 64u;
	}
	if (len != 0u) memcpy(ctx->buffer + used, data, len);
}

static void md5_final(xrtmd5ctx* ctx, unsigned char digest[16])
{
	static const unsigned char padding[64] = { 0x80 };
	unsigned char length_bytes[8];
	size_t used = (size_t)((ctx->bit_count >> 3) & 63u);
	size_t pad_len = (used < 56u) ? (56u - used) : (120u - used);
	uint64_t bits = ctx->bit_count;
	int i;
	for (i = 0; i < 8; ++i) length_bytes[i] = (unsigned char)((bits >> (8 * i)) & 0xffu);
	md5_update(ctx, padding, pad_len);
	md5_update(ctx, length_bytes, 8u);
	for (i = 0; i < 4; ++i) md5_store32(digest + i * 4, ctx->state[i]);
}


/* ═══════════════════════════════════════════════════════════
 * 容器层：流式哈希器 + 一次性 + 文件流
 * ═══════════════════════════════════════════════════════════ */

struct xrtmd5hasher {
	xrtmd5ctx Ctx;
	xrtmd5options Options;
	xrtmd5error Error;
	uint64 InputBytes;
	bool Initialized;
	bool Failed;
};

static void __xrtMd5SetError(xrtmd5error* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sFormat, ...)
{
	va_list pArgs;
	if (pError == NULL) { return; }
	pError->iCode = iCode;
	pError->iBackendCode = iBackend;
	pError->iInputOffset = iInput;
	pError->iOutputOffset = iOutput;
	va_start(pArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, pArgs);
	va_end(pArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1] = '\0';
}

static bool __xrtMd5OverInputLimit(const xrtresourcelimits* pLimits, uint64 iInput)
{
	return pLimits->iMaxInputBytes != 0u && iInput > pLimits->iMaxInputBytes;
}

XXAPI void xrtMd5ErrorInit(xrtmd5error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_MD5_OK;
}

XXAPI void xrtMd5OptionsInit(xrtmd5options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_MD5_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}


/* ---- 流式哈希器 ---- */

XXAPI xrtmd5hasher* xrtMd5HasherCreate(const xrtmd5options* pOptions)
{
	xrtmd5hasher* pHasher;
	pHasher = (xrtmd5hasher*)xrtMalloc(sizeof(xrtmd5hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtMd5ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtMd5OptionsInit(&pHasher->Options);
	}
	md5_init(&pHasher->Ctx);
	pHasher->Initialized = TRUE;
	return pHasher;
}

XXAPI bool xrtMd5HasherUpdate(xrtmd5hasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	if (iSize != 0u && pData == NULL) {
		__xrtMd5SetError(&pHasher->Error, XRT_MD5_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid md5 input");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtMd5SetError(&pHasher->Error, XRT_MD5_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "md5 input exceeds maxInput limit");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (iSize != 0u) {
		md5_update(&pHasher->Ctx, (const unsigned char*)pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return TRUE;
}

XXAPI bool xrtMd5HasherDigest(const xrtmd5hasher* pHasher, uint8 pDigest[XRT_MD5_DIGEST_SIZE])
{
	xrtmd5ctx tmpCtx;
	if (pHasher == NULL || pHasher->Failed || pDigest == NULL) { return FALSE; }
	/* 复制上下文以允许 digest 不终结状态 */
	memcpy(&tmpCtx, &pHasher->Ctx, sizeof(tmpCtx));
	md5_final(&tmpCtx, pDigest);
	return TRUE;
}

XXAPI bool xrtMd5HasherReset(xrtmd5hasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	md5_init(&pHasher->Ctx);
	pHasher->InputBytes = 0u;
	return TRUE;
}

XXAPI uint64 xrtMd5HasherInputBytes(const xrtmd5hasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XXAPI const xrtmd5error* xrtMd5HasherError(const xrtmd5hasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XXAPI void xrtMd5HasherDestroy(xrtmd5hasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XXAPI bool xrtMd5Hash(const void* pData, size_t iSize, uint8 pDigest[XRT_MD5_DIGEST_SIZE],
	xrtmd5error* pError)
{
	xrtmd5ctx ctx;
	xrtMd5ErrorInit(pError);
	if (pDigest == NULL || (pData == NULL && iSize != 0u)) {
		__xrtMd5SetError(pError, XRT_MD5_ERROR_ARGUMENT, 0, 0, 0, "invalid md5 input");
		return FALSE;
	}
	md5_init(&ctx);
	if (iSize != 0u) {
		md5_update(&ctx, (const unsigned char*)pData, iSize);
	}
	md5_final(&ctx, pDigest);
	return TRUE;
}


/* ---- 文件流式哈希 ---- */

XXAPI bool xrtMd5File(xreader pReader, const xrtresourcelimits* pLimits,
	uint8 pDigest[XRT_MD5_DIGEST_SIZE], xrtmd5error* pError)
{
	uint8 sInput[XRT_MD5_CHUNK];
	xrtmd5ctx ctx;
	uint64 iTotal = 0u;
	xrtMd5ErrorInit(pError);
	if (pReader == NULL || pDigest == NULL) {
		__xrtMd5SetError(pError, XRT_MD5_ERROR_ARGUMENT, 0, 0, 0, "invalid md5 stream");
		return FALSE;
	}
	md5_init(&ctx);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtMd5SetError(pError, XRT_MD5_ERROR_IO, 0, iTotal, 0,
				"cannot read md5 input stream");
			return FALSE;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (__xrtMd5OverInputLimit(pLimits, iTotal)) {
			__xrtMd5SetError(pError, XRT_MD5_ERROR_LIMIT, 0, iTotal, 0,
				"md5 input exceeds maxInput limit");
			return FALSE;
		}
		md5_update(&ctx, sInput, (size_t)iRead);
	}
	md5_final(&ctx, pDigest);
	return TRUE;
}
