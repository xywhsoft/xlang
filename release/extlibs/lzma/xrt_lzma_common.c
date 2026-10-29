#include "xrt_lzma_internal.h"
#include "third_party/xz/7zCrc.h"
#include "third_party/xz/XzCrc64.h"
#include <stdio.h>
#include <string.h>


// ------------------------------------ SDK 内存与错误 ------------------------------------

static void* xlzmaAlloc(ISzAllocPtr pAllocator, size_t iSize)
{
	(void)pAllocator;
	return iSize != 0u ? xrtMalloc(iSize) : NULL;
}

static void xlzmaFree(ISzAllocPtr pAllocator, void* pData)
{
	(void)pAllocator;
	xrtFree(pData);
}

const ISzAlloc xlzmaAllocator = { xlzmaAlloc, xlzmaFree };

static xonce xlzmaTablesOnce = XRT_ONCE_INIT;

static bool xlzmaGenerateTables(ptr pUnused)
{
	(void)pUnused;
	CrcGenerateTable();
	Crc64GenerateTable();
	return true;
}

bool xlzmaInitXzTables(void)
{
	return xrtOnce(&xlzmaTablesOnce, xlzmaGenerateTables, NULL);
}

void xlzmaSetError(xrtlzmaerror* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sMessage)
{
	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iBackendCode = iBackend;
	pError->iInputOffset = iInput;
	pError->iOutputOffset = iOutput;
	snprintf(pError->sMessage, sizeof(pError->sMessage), "%s", sMessage);
}

int xlzmaStreamErrorCode(void)
{
	const xerror* pCause = xrtGetError();
	if ( pCause != NULL ) {
		switch ( xrtErrorKind(pCause) ) {
		case XERR_MEMORY: return XRT_LZMA_ERROR_MEMORY;
		case XERR_CANCELLED: return XRT_LZMA_ERROR_CANCELLED;
		default: break;
		}
	}
	return XRT_LZMA_ERROR_IO;
}

bool xlzmaAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	if ( (uint64)iAdded > UINT64_MAX - iCurrent ) { return true; }
	return iLimit != 0u &&
		(iCurrent > iLimit || (uint64)iAdded > iLimit - iCurrent);
}

bool xlzmaOverRatio(const xrtresourcelimits* pLimits,
	uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > UINT64_MAX / iRatio ) { return false; }
	return iOutput > iInput * iRatio;
}

const char* xlzmaStatusName(SRes iResult)
{
	switch ( iResult ) {
	case SZ_ERROR_MEM: return "lzma memory error";
	case SZ_ERROR_DATA: return "lzma data error";
	case SZ_ERROR_CRC: return "xz checksum error";
	case SZ_ERROR_NO_ARCHIVE: return "invalid xz stream";
	case SZ_ERROR_UNSUPPORTED: return "lzma unsupported properties";
	case SZ_ERROR_PARAM: return "lzma invalid parameters";
	case SZ_ERROR_READ: return "lzma input read failed";
	case SZ_ERROR_WRITE: return "lzma output write failed";
	case SZ_ERROR_PROGRESS: return "lzma progress callback stopped";
	default: return "lzma operation failed";
	}
}

xbuffer* xlzmaBufferCreate(void)
{
	return xrtBufferCreate();
}


// ------------------------------------ 公开选项 ------------------------------------

XRT_API void xrtLzmaErrorInit(xrtlzmaerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
}

XRT_API void xrtLzmaOptionsInit(xrtlzmaoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_LZMA_OPTIONS_VERSION;
	pOptions->iLevel = 5;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxEntries = 0u;
	pOptions->tLimits.iMaxNodes = 0u;
	pOptions->tLimits.iMaxDepth = 0u;
}

bool xlzmaPrepareOptions(const xrtlzmaoptions* pInput,
	xrtlzmaoptions* pOutput, xrtlzmaerror* pError)
{
	xrtLzmaOptionsInit(pOutput);
	if ( pInput != NULL ) {
		if ( pInput->iSize != sizeof(*pInput) ||
			pInput->iVersion != XRT_LZMA_OPTIONS_VERSION ) {
			xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0,
				"invalid lzma options version or size");
			return false;
		}
		if ( pInput->sReserved[0] != 0u || pInput->sReserved[1] != 0u ||
			pInput->sReserved[2] != 0u || pInput->sReserved[3] != 0u ||
			pInput->tLimits.iSize != sizeof(pInput->tLimits) ||
			pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
			pInput->tLimits.iReserved != 0u || pInput->tLimits.iFlags != 0u ||
			pInput->tLimits.iMaxEntries != 0u || pInput->tLimits.iMaxNodes != 0u ||
			pInput->tLimits.iMaxDepth != 0u ) {
			xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0,
				"invalid lzma resource limits");
			return false;
		}
		*pOutput = *pInput;
	}
	if ( pOutput->iLevel < 0 || pOutput->iLevel > 9 ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0,
			"lzma compression level must be 0..9");
		return false;
	}
	return true;
}
