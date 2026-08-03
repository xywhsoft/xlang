#include "include/xrt_crc32.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_CRC32_CHUNK 65536u

/* ═══════════════════════════════════════════════════════════
 * CRC-32/ISO-HDLC slice-by-8 高性能实现
 * 多项式: 0xEDB88320 (反转表示)
 * 每次处理 8 字节，纯 C 无 SIMD 依赖，TCC 完全兼容
 * ═══════════════════════════════════════════════════════════ */

static uint32_t crc32_table[8][256];
static int crc32_table_ready = 0;

static void crc32_init_tables(void)
{
	uint32_t i, j;
	if (crc32_table_ready) { return; }
	/* 第一张表：标准逐字节 CRC32 */
	for (i = 0; i < 256u; ++i) {
		uint32_t crc = i;
		for (j = 0; j < 8u; ++j) {
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
		}
		crc32_table[0][i] = crc;
	}
	/* 扩展表 1..7：slice-by-8 */
	for (i = 0; i < 256u; ++i) {
		uint32_t crc = crc32_table[0][i];
		for (j = 1u; j < 8u; ++j) {
			crc = crc32_table[0][crc & 0xffu] ^ (crc >> 8);
			crc32_table[j][i] = crc;
		}
	}
	crc32_table_ready = 1;
}

static uint32_t crc32_slice8(uint32_t crc, const unsigned char* data, size_t len)
{
	/* 对齐到 8 字节边界 */
	while (len != 0u && ((size_t)data & 7u) != 0u) {
		crc = crc32_table[0][(crc ^ *data) & 0xffu] ^ (crc >> 8);
		++data;
		--len;
	}
	/* slice-by-8 主循环 */
	while (len >= 8u) {
		uint32_t lo, hi;
		memcpy(&lo, data, 4u);
		memcpy(&hi, data + 4u, 4u);
		lo ^= crc;
		crc = crc32_table[7][ lo        & 0xffu] ^
		      crc32_table[6][(lo >>  8) & 0xffu] ^
		      crc32_table[5][(lo >> 16) & 0xffu] ^
		      crc32_table[4][(lo >> 24) & 0xffu] ^
		      crc32_table[3][ hi        & 0xffu] ^
		      crc32_table[2][(hi >>  8) & 0xffu] ^
		      crc32_table[1][(hi >> 16) & 0xffu] ^
		      crc32_table[0][(hi >> 24) & 0xffu];
		data += 8u;
		len -= 8u;
	}
	/* 尾部逐字节 */
	while (len != 0u) {
		crc = crc32_table[0][(crc ^ *data) & 0xffu] ^ (crc >> 8);
		++data;
		--len;
	}
	return crc;
}

static uint32_t crc32_compute(const unsigned char* data, size_t len)
{
	crc32_init_tables();
	return crc32_slice8(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

static uint32_t crc32_update(uint32_t crc, const unsigned char* data, size_t len)
{
	crc32_init_tables();
	return crc32_slice8(crc ^ 0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}


/* ═══════════════════════════════════════════════════════════
 * 容器层：流式哈希器 + 一次性 + 文件流
 * ═══════════════════════════════════════════════════════════ */

struct xrtcrc32hasher {
	uint32 Crc;
	xrtcrc32options Options;
	xrtcrc32error Error;
	uint64 InputBytes;
	bool Initialized;
	bool Failed;
};

static void __xrtCrc32SetError(xrtcrc32error* pError, int iCode, int iBackend,
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

XXAPI void xrtCrc32ErrorInit(xrtcrc32error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_CRC32_OK;
}

XXAPI void xrtCrc32OptionsInit(xrtcrc32options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_CRC32_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}


/* ---- 流式哈希器 ---- */

XXAPI xrtcrc32hasher* xrtCrc32HasherCreate(const xrtcrc32options* pOptions)
{
	xrtcrc32hasher* pHasher;
	crc32_init_tables();
	pHasher = (xrtcrc32hasher*)xrtMalloc(sizeof(xrtcrc32hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtCrc32ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtCrc32OptionsInit(&pHasher->Options);
	}
	pHasher->Crc = 0u;
	pHasher->Initialized = TRUE;
	return pHasher;
}

XXAPI bool xrtCrc32HasherUpdate(xrtcrc32hasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	if (iSize != 0u && pData == NULL) {
		__xrtCrc32SetError(&pHasher->Error, XRT_CRC32_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid crc32 input");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtCrc32SetError(&pHasher->Error, XRT_CRC32_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "crc32 input exceeds maxInput limit");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (iSize != 0u) {
		pHasher->Crc = crc32_update(pHasher->Crc, (const unsigned char*)pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return TRUE;
}

XXAPI bool xrtCrc32HasherDigest(const xrtcrc32hasher* pHasher, uint32* pCrc)
{
	if (pHasher == NULL || pHasher->Failed || pCrc == NULL) { return FALSE; }
	*pCrc = pHasher->Crc;
	return TRUE;
}

XXAPI bool xrtCrc32HasherReset(xrtcrc32hasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	pHasher->Crc = 0u;
	pHasher->InputBytes = 0u;
	return TRUE;
}

XXAPI uint64 xrtCrc32HasherInputBytes(const xrtcrc32hasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XXAPI const xrtcrc32error* xrtCrc32HasherError(const xrtcrc32hasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XXAPI void xrtCrc32HasherDestroy(xrtcrc32hasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XXAPI bool xrtCrc32Hash(const void* pData, size_t iSize, uint32* pCrc, xrtcrc32error* pError)
{
	xrtCrc32ErrorInit(pError);
	if (pCrc == NULL || (pData == NULL && iSize != 0u)) {
		__xrtCrc32SetError(pError, XRT_CRC32_ERROR_ARGUMENT, 0, 0, 0, "invalid crc32 input");
		return FALSE;
	}
	*pCrc = crc32_compute((const unsigned char*)pData, iSize);
	return TRUE;
}


/* ---- 文件流式哈希 ---- */

XXAPI bool xrtCrc32File(xreader pReader, const xrtresourcelimits* pLimits,
	uint32* pCrc, xrtcrc32error* pError)
{
	uint8 sInput[XRT_CRC32_CHUNK];
	uint32 iCrc = 0u;
	uint64 iTotal = 0u;
	xrtCrc32ErrorInit(pError);
	crc32_init_tables();
	if (pReader == NULL || pCrc == NULL) {
		__xrtCrc32SetError(pError, XRT_CRC32_ERROR_ARGUMENT, 0, 0, 0, "invalid crc32 stream");
		return FALSE;
	}
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtCrc32SetError(pError, XRT_CRC32_ERROR_IO, 0, iTotal, 0,
				"cannot read crc32 input stream");
			return FALSE;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtCrc32SetError(pError, XRT_CRC32_ERROR_LIMIT, 0, iTotal, 0,
				"crc32 input exceeds maxInput limit");
			return FALSE;
		}
		iCrc = crc32_update(iCrc, sInput, (size_t)iRead);
	}
	*pCrc = iCrc;
	return TRUE;
}
