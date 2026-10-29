#include "xrt_tar_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



XRT_API void xrtTarOptionsInit(xrttaroptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_TAR_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxDepth = 0u;
	pOptions->tLimits.iMaxCompressionRatio = 0u;
	pOptions->iFileModeMask = 0777u;
}

XRT_API void xrtTarErrorInit(xrttarerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iEntryIndex = UINT64_MAX;
}

XRT_API void xrtTarEntryInit(xrttarentry* pEntry)
{
	if ( pEntry == NULL ) { return; }
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->iSize = sizeof(*pEntry);
	pEntry->iVersion = XRT_TAR_ENTRY_VERSION;
}

bool __xrtTarCopyOptions(xrttaroptions* pOutput, const xrttaroptions* pInput)
{
	xrtTarOptionsInit(pOutput);
	if ( pInput == NULL ) { return true; }
	if ( pInput->iVersion != XRT_TAR_OPTIONS_VERSION ||
		pInput->iSize < offsetof(xrttaroptions, iReserved) ||
		pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pInput->tLimits.iSize < sizeof(xrtresourcelimits) ) { return false; }
	memcpy(pOutput, pInput, pInput->iSize < sizeof(*pOutput) ? pInput->iSize : sizeof(*pOutput));
	if ( pOutput->tLimits.iReserved != 0u || pOutput->iReserved != 0u ||
		pOutput->tLimits.iFlags != 0u || pOutput->tLimits.iMaxDepth != 0u ||
		pOutput->tLimits.iMaxCompressionRatio != 0u ) { return false; }
	pOutput->iSize = sizeof(*pOutput);
	pOutput->iVersion = XRT_TAR_OPTIONS_VERSION;
	return true;
}

static void __xrtTarSetErrorValue(xrttarerror* pError, int iCode, uint64 iEntry,
	uint64 iOffset, const char* sFormat, va_list tArgs)
{
	xerrkind Kind;

	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iEntryIndex = iEntry;
	pError->iOffset = iOffset;
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1u] = 0;
	switch ( iCode ) {
		case XRT_TAR_ERROR_ARGUMENT: Kind = XERR_ARGUMENT; break;
		case XRT_TAR_ERROR_MEMORY: Kind = XERR_MEMORY; break;
		case XRT_TAR_ERROR_FORMAT: Kind = XERR_PROTOCOL; break;
		case XRT_TAR_ERROR_IO: Kind = XERR_IO; break;
		case XRT_TAR_ERROR_LIMIT: Kind = XERR_RANGE; break;
		case XRT_TAR_ERROR_UNSAFE_PATH: Kind = XERR_PERMISSION; break;
		case XRT_TAR_ERROR_EXISTS: Kind = XERR_EXISTS; break;
		case XRT_TAR_ERROR_STATE: Kind = XERR_STATE; break;
		case XRT_TAR_ERROR_UNSUPPORTED: Kind = XERR_UNSUPPORTED; break;
		case XRT_TAR_ERROR_CANCELLED: Kind = XERR_CANCELLED; break;
		default: Kind = XERR_STATE; break;
	}
	xrtSetErrorInfo(Kind, "xlang.extlib.tar", iCode, pError->sMessage);
}

void __xrtTarSetReaderError(xrttarreader* pReader, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pReader == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pReader->Error, iCode, pReader->EntryIndex,
		pReader->Offset, sFormat, tArgs);
	va_end(tArgs);
}

void __xrtTarSetWriterError(xrttarwriter* pWriter, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pWriter == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pWriter->Error, iCode, pWriter->EntryCount,
		pWriter->OutputBytes, sFormat, tArgs);
	va_end(tArgs);
}





/* 记录不可恢复的 Reader 错误并锁定流状态。 */
void __xrtTarFailReader(xrttarreader* pReader, int iCode, const char* sFormat, ...)
{
	va_list tArgs;

	if ( pReader == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pReader->Error, iCode, pReader->EntryIndex,
		pReader->Offset, sFormat, tArgs);
	va_end(tArgs);
	pReader->Failed = true;
}





/* 记录不可恢复的 Writer 错误并锁定输出状态。 */
void __xrtTarFailWriter(xrttarwriter* pWriter, int iCode, const char* sFormat, ...)
{
	va_list tArgs;

	if ( pWriter == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pWriter->Error, iCode, pWriter->EntryCount,
		pWriter->OutputBytes, sFormat, tArgs);
	va_end(tArgs);
	pWriter->Failed = true;
}

/* 流回调的内存故障保留类别；其他失败仍按普通 I/O 处理。 */
int __xrtTarIoFailureCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_TAR_ERROR_MEMORY : XRT_TAR_ERROR_IO;
}

bool __xrtTarReadExact(xrttarreader* pReader, void* pData, uint64 iSize)
{
	bytes pOutput = (bytes)pData;
	uint64 iRemaining = iSize;

	if ( pReader == NULL || pReader->Reader == NULL || pReader->Failed ) { return false; }
	if ( (pData == NULL) && (iSize != 0u) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_ARGUMENT,
			"TAR input destination is null");
		return false;
	}
	if ( (pReader->Options.tLimits.iMaxInputBytes != 0u) &&
		(iSize > pReader->Options.tLimits.iMaxInputBytes ||
		 pReader->Offset > pReader->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return false;
	}
	while ( iRemaining != 0u ) {
		size_t iChunk = iRemaining > SIZE_MAX ? SIZE_MAX : (size_t)iRemaining;
		size_t iRead = 0u;

		if ( !xrtReaderReadFull(pReader->Reader, pOutput, iChunk, &iRead) ) {
			const xerror* pCause = xrtGetError();
			pReader->Offset += iRead;
			if ( pCause != NULL && xrtErrorKind(pCause) == XERR_IO &&
				xrtErrorCode(pCause) == XIO_ERROR_EOF ) {
				__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
					"truncated TAR stream at byte %llu",
					(unsigned long long)pReader->Offset);
			} else {
				__xrtTarFailReader(pReader, __xrtTarIoFailureCode(),
					"cannot read TAR stream at byte %llu",
					(unsigned long long)pReader->Offset);
			}
			return false;
		}
		pOutput += iRead;
		pReader->Offset += iRead;
		iRemaining -= iRead;
	}
	return true;
}

bool __xrtTarSkipBytes(xrttarreader* pReader, uint64 iSize)
{
	unsigned char sDiscard[65536];
	if ( pReader == NULL || pReader->Failed ) { return false; }
	if ( iSize == 0u ) { return true; }
	if ( (pReader->Options.tLimits.iMaxInputBytes != 0u) &&
		(iSize > pReader->Options.tLimits.iMaxInputBytes ||
		 pReader->Offset > pReader->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return false;
	}
	if ( xrtReaderCanSeek(pReader->Reader) && (iSize <= INT64_MAX) ) {
		if ( !xrtReaderSeek(pReader->Reader, (int64)iSize,
			XSEEK_CURRENT, NULL) ) {
			__xrtTarFailReader(pReader, __xrtTarIoFailureCode(),
				"cannot seek over TAR input");
			return false;
		}
		pReader->Offset += iSize;
		return true;
	}
	while ( iSize != 0u ) {
		uint64 iChunk = iSize < sizeof(sDiscard) ? iSize : sizeof(sDiscard);
		if ( !__xrtTarReadExact(pReader, sDiscard, iChunk) ) { return false; }
		iSize -= iChunk;
	}
	return true;
}

bool __xrtTarWriteAll(xrttarwriter* pWriter, const void* pData, uint64 iSize)
{
	cbytes pInput = (cbytes)pData;
	uint64 iRemaining = iSize;

	if ( pWriter == NULL || pWriter->Writer == NULL || pWriter->Failed || pWriter->Closed ) {
		return false;
	}
	if ( (pData == NULL) && (iSize != 0u) ) {
		__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR output source is null");
		return false;
	}
	if ( (pWriter->Options.tLimits.iMaxOutputBytes != 0u) &&
		(iSize > pWriter->Options.tLimits.iMaxOutputBytes ||
		 pWriter->OutputBytes > pWriter->Options.tLimits.iMaxOutputBytes - iSize) ) {
		__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR output exceeds the configured limit");
		return false;
	}
	while ( iRemaining != 0u ) {
		size_t iChunk = iRemaining > SIZE_MAX ? SIZE_MAX : (size_t)iRemaining;
		size_t iWritten = 0u;

		if ( !xrtWriterWriteFull(pWriter->Writer,
			pInput, iChunk, &iWritten) ) {
			pWriter->OutputBytes += iWritten;
			__xrtTarFailWriter(pWriter, __xrtTarIoFailureCode(),
				"cannot write TAR stream");
			return false;
		}
		pInput += iWritten;
		pWriter->OutputBytes += iWritten;
		iRemaining -= iWritten;
	}
	return true;
}

bool __xrtTarEntryPathSafe(const char* sName, bool bDirectory, xrttarerror* pError,
	uint64 iEntryIndex)
{
	if ( xrtPathIsSafeEntry(xrtStrView(sName), bDirectory) ) { return true; }
	if ( pError != NULL ) {
		xrtTarErrorInit(pError);
		pError->iCode = XRT_TAR_ERROR_UNSAFE_PATH;
		pError->iEntryIndex = iEntryIndex;
		snprintf(pError->sMessage, sizeof(pError->sMessage),
			"unsafe TAR entry path: %s", sName != NULL ? sName : "");
	}
	return false;
}

void __xrtTarPaxClear(xrttarpax* pPax)
{
	if ( pPax == NULL ) { return; }
	xrtFree(pPax->sPath);
	xrtFree(pPax->sLinkPath);
	xrtFree(pPax->sUserName);
	xrtFree(pPax->sGroupName);
	memset(pPax, 0, sizeof(*pPax));
}
