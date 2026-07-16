#include "xrt_tar_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



XXAPI void xrtTarOptionsInit(xrttaroptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_TAR_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->iFileModeMask = 0777u;
}

XXAPI void xrtTarErrorInit(xrttarerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iEntryIndex = UINT64_MAX;
}

XXAPI void xrtTarEntryInit(xrttarentry* pEntry)
{
	if ( pEntry == NULL ) { return; }
	memset(pEntry, 0, sizeof(*pEntry));
	pEntry->iSize = sizeof(*pEntry);
	pEntry->iVersion = XRT_TAR_ENTRY_VERSION;
}

bool __xrtTarCopyOptions(xrttaroptions* pOutput, const xrttaroptions* pInput)
{
	xrtTarOptionsInit(pOutput);
	if ( pInput == NULL ) { return TRUE; }
	if ( pInput->iVersion != XRT_TAR_OPTIONS_VERSION ||
		pInput->iSize < offsetof(xrttaroptions, iReserved) ) { return FALSE; }
	memcpy(pOutput, pInput, pInput->iSize < sizeof(*pOutput) ? pInput->iSize : sizeof(*pOutput));
	pOutput->iSize = sizeof(*pOutput);
	pOutput->iVersion = XRT_TAR_OPTIONS_VERSION;
	return TRUE;
}

static void __xrtTarSetErrorValue(xrttarerror* pError, int iCode, uint64 iEntry,
	uint64 iOffset, const char* sFormat, va_list tArgs)
{
	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iEntryIndex = iEntry;
	pError->iOffset = iOffset;
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1u] = 0;
	xrtSetError(xrtCopyStr((str)pError->sMessage, 0u), TRUE);
}

void __xrtTarSetReaderError(xrttarreader* pReader, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pReader == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pReader->Error, iCode, pReader->EntryIndex,
		pReader->Offset, sFormat, tArgs);
	va_end(tArgs);
	pReader->Failed = TRUE;
}

void __xrtTarSetWriterError(xrttarwriter* pWriter, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pWriter == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtTarSetErrorValue(&pWriter->Error, iCode, pWriter->EntryCount,
		pWriter->OutputBytes, sFormat, tArgs);
	va_end(tArgs);
	pWriter->Failed = TRUE;
}

bool __xrtTarReadExact(xrttarreader* pReader, void* pData, uint64 iSize)
{
	if ( pReader == NULL || pReader->Reader == NULL || pReader->Failed ) { return FALSE; }
	if ( pReader->Options.tLimits.iMaxInputBytes != 0u &&
		(iSize > pReader->Options.tLimits.iMaxInputBytes ||
		 pReader->Offset > pReader->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return FALSE;
	}
	if ( !xrtReaderReadExact(pReader->Reader, pData, iSize) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"truncated TAR stream at byte %llu", (unsigned long long)pReader->Offset);
		return FALSE;
	}
	pReader->Offset += iSize;
	return TRUE;
}

bool __xrtTarSkipBytes(xrttarreader* pReader, uint64 iSize)
{
	unsigned char sDiscard[65536];
	if ( pReader == NULL || pReader->Failed ) { return FALSE; }
	if ( iSize == 0u ) { return TRUE; }
	if ( pReader->Options.tLimits.iMaxInputBytes != 0u &&
		(iSize > pReader->Options.tLimits.iMaxInputBytes ||
		 pReader->Offset > pReader->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return FALSE;
	}
	if ( xrtReaderCanSeek(pReader->Reader) && iSize <= INT64_MAX &&
		xrtReaderSeek(pReader->Reader, (int64)iSize, XRT_SEEK_CUR, NULL) ) {
		pReader->Offset += iSize;
		return TRUE;
	}
	while ( iSize != 0u ) {
		uint64 iChunk = iSize < sizeof(sDiscard) ? iSize : sizeof(sDiscard);
		if ( !__xrtTarReadExact(pReader, sDiscard, iChunk) ) { return FALSE; }
		iSize -= iChunk;
	}
	return TRUE;
}

bool __xrtTarWriteAll(xrttarwriter* pWriter, const void* pData, uint64 iSize)
{
	if ( pWriter == NULL || pWriter->Writer == NULL || pWriter->Failed || pWriter->Closed ) {
		return FALSE;
	}
	if ( pWriter->Options.tLimits.iMaxOutputBytes != 0u &&
		(iSize > pWriter->Options.tLimits.iMaxOutputBytes ||
		 pWriter->OutputBytes > pWriter->Options.tLimits.iMaxOutputBytes - iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR output exceeds the configured limit");
		return FALSE;
	}
	if ( !xrtWriterWriteAll(pWriter->Writer, pData, iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO, "cannot write TAR stream");
		return FALSE;
	}
	pWriter->OutputBytes += iSize;
	return TRUE;
}

bool __xrtTarEntryPathSafe(const char* sName, bool bDirectory, xrttarerror* pError,
	uint64 iEntryIndex)
{
	if ( xrtPathIsSafeArchiveEntry((str)sName, bDirectory) ) { return TRUE; }
	if ( pError != NULL ) {
		xrtTarErrorInit(pError);
		pError->iCode = XRT_TAR_ERROR_UNSAFE_PATH;
		pError->iEntryIndex = iEntryIndex;
		snprintf(pError->sMessage, sizeof(pError->sMessage),
			"unsafe TAR entry path: %s", sName != NULL ? sName : "");
	}
	return FALSE;
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
