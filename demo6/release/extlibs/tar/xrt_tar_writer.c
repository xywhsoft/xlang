#include "xrt_tar_internal.h"

#include <stdio.h>
#include <string.h>



static bool __xrtTarNameSplit(const char* sName, char* sHeaderName, char* sPrefix)
{
	size_t iLength = strlen(sName);
	size_t i;
	memset(sHeaderName, 0, 100u);
	memset(sPrefix, 0, 155u);
	if ( iLength <= 100u ) {
		memcpy(sHeaderName, sName, iLength);
		return TRUE;
	}
	for ( i = iLength; i-- > 0u; ) {
		if ( sName[i] == '/' && i <= 155u && iLength - i - 1u <= 100u ) {
			memcpy(sPrefix, sName, i);
			memcpy(sHeaderName, sName + i + 1u, iLength - i - 1u);
			return TRUE;
		}
	}
	return FALSE;
}

static bool __xrtTarOctalFits(uint64 iValue, size_t iLength)
{
	uint64 iMax = 0u;
	size_t i;
	if ( iLength < 2u ) { return FALSE; }
	for ( i = 0u; i < iLength - 1u; ++i ) {
		if ( iMax > (UINT64_MAX - 7u) / 8u ) { return TRUE; }
		iMax = iMax * 8u + 7u;
	}
	return iValue <= iMax;
}

static bool __xrtTarWriteOctal(unsigned char* pField, size_t iLength, uint64 iValue)
{
	char sValue[32];
	int iWritten;
	if ( !__xrtTarOctalFits(iValue, iLength) || iLength > sizeof(sValue) ) { return FALSE; }
	iWritten = snprintf(sValue, sizeof(sValue), "%0*llo", (int)iLength - 1,
		(unsigned long long)iValue);
	if ( iWritten != (int)iLength - 1 ) { return FALSE; }
	memcpy(pField, sValue, iLength - 1u);
	pField[iLength - 1u] = 0;
	return TRUE;
}

static size_t __xrtTarDecimalDigits(size_t iValue)
{
	size_t iDigits = 1u;
	while ( iValue >= 10u ) { iValue /= 10u; ++iDigits; }
	return iDigits;
}

static bool __xrtTarPaxAppend(xbuffer pPax, const char* sKey, const char* sValue)
{
	size_t iBody = strlen(sKey) + 1u + strlen(sValue) + 1u;
	size_t iLength = iBody + 2u;
	char sPrefix[32];
	int iPrefix;
	for ( ;; ) {
		size_t iNew = iBody + __xrtTarDecimalDigits(iLength) + 1u;
		if ( iNew == iLength ) { break; }
		iLength = iNew;
	}
	iPrefix = snprintf(sPrefix, sizeof(sPrefix), "%llu ", (unsigned long long)iLength);
	return iPrefix > 0 && xrtBufferAppend(pPax, sPrefix, (uint32)iPrefix, XBUF_BINARY) &&
		xrtBufferAppend(pPax, (ptr)sKey, (uint32)strlen(sKey), XBUF_BINARY) &&
		xrtBufferAppend(pPax, "=", 1u, XBUF_BINARY) &&
		xrtBufferAppend(pPax, (ptr)sValue, (uint32)strlen(sValue), XBUF_BINARY) &&
		xrtBufferAppend(pPax, "\n", 1u, XBUF_BINARY);
}

static bool __xrtTarPaxAppendUInt(xbuffer pPax, const char* sKey, uint64 iValue)
{
	char sValue[32];
	snprintf(sValue, sizeof(sValue), "%llu", (unsigned long long)iValue);
	return __xrtTarPaxAppend(pPax, sKey, sValue);
}

static bool __xrtTarPaxAppendInt(xbuffer pPax, const char* sKey, int64 iValue)
{
	char sValue[32];
	snprintf(sValue, sizeof(sValue), "%lld", (long long)iValue);
	return __xrtTarPaxAppend(pPax, sKey, sValue);
}

static void __xrtTarHeaderChecksum(unsigned char* pHeader)
{
	uint64 iSum = 0u;
	size_t i;
	memset(pHeader + 148u, ' ', 8u);
	for ( i = 0u; i < XRT_TAR_BLOCK_SIZE; ++i ) { iSum += pHeader[i]; }
	snprintf((char*)pHeader + 148u, 8u, "%06llo", (unsigned long long)iSum);
	pHeader[154u] = 0;
	pHeader[155u] = ' ';
}

static bool __xrtTarBuildHeader(unsigned char* pHeader, const xrttarentry* pEntry,
	const char* sName, const char* sLinkName, uint64 iDataSize, unsigned char iType)
{
	char sHeaderName[100];
	char sPrefix[155];
	memset(pHeader, 0, XRT_TAR_BLOCK_SIZE);
	if ( !__xrtTarNameSplit(sName, sHeaderName, sPrefix) ) {
		memcpy(sHeaderName, "PaxEntry", 8u);
	}
	memcpy(pHeader, sHeaderName, sizeof(sHeaderName));
	memcpy(pHeader + 345u, sPrefix, sizeof(sPrefix));
	if ( !__xrtTarWriteOctal(pHeader + 100u, 8u, pEntry->iMode & 07777u) ||
		!__xrtTarWriteOctal(pHeader + 108u, 8u,
		 __xrtTarOctalFits(pEntry->iUID, 8u) ? pEntry->iUID : 0u) ||
		!__xrtTarWriteOctal(pHeader + 116u, 8u,
		 __xrtTarOctalFits(pEntry->iGID, 8u) ? pEntry->iGID : 0u) ||
		!__xrtTarWriteOctal(pHeader + 124u, 12u,
		 __xrtTarOctalFits(iDataSize, 12u) ? iDataSize : 0u) ||
		!__xrtTarWriteOctal(pHeader + 136u, 12u,
		 pEntry->iMTime >= 0 && __xrtTarOctalFits((uint64)pEntry->iMTime, 12u) ?
		 (uint64)pEntry->iMTime : 0u) ||
		!__xrtTarWriteOctal(pHeader + 329u, 8u, pEntry->iDeviceMajor) ||
		!__xrtTarWriteOctal(pHeader + 337u, 8u, pEntry->iDeviceMinor) ) { return FALSE; }
	pHeader[156u] = iType;
	if ( sLinkName != NULL ) {
		size_t iLength = strlen(sLinkName);
		if ( iLength <= 100u ) { memcpy(pHeader + 157u, sLinkName, iLength); }
	}
	memcpy(pHeader + 257u, "ustar", 5u);
	pHeader[262u] = 0;
	memcpy(pHeader + 263u, "00", 2u);
	if ( pEntry->sUserName != NULL ) {
		size_t iLength = strlen(pEntry->sUserName);
		memcpy(pHeader + 265u, pEntry->sUserName, iLength < 32u ? iLength : 32u);
	}
	if ( pEntry->sGroupName != NULL ) {
		size_t iLength = strlen(pEntry->sGroupName);
		memcpy(pHeader + 297u, pEntry->sGroupName, iLength < 32u ? iLength : 32u);
	}
	__xrtTarHeaderChecksum(pHeader);
	return TRUE;
}

static unsigned char __xrtTarTypeFlag(int iType)
{
	switch ( iType ) {
		case XRT_TAR_ENTRY_FILE: return '0';
		case XRT_TAR_ENTRY_DIRECTORY: return '5';
		case XRT_TAR_ENTRY_SYMLINK: return '2';
		case XRT_TAR_ENTRY_HARDLINK: return '1';
		case XRT_TAR_ENTRY_FIFO: return '6';
		case XRT_TAR_ENTRY_CHARACTER: return '3';
		case XRT_TAR_ENTRY_BLOCK: return '4';
		default: return 0;
	}
}

static bool __xrtTarWritePadding(xrttarwriter* pWriter, uint64 iSize)
{
	static const unsigned char sZero[XRT_TAR_BLOCK_SIZE] = { 0 };
	uint64 iPadding = (XRT_TAR_BLOCK_SIZE - (iSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
	return iPadding == 0u || __xrtTarWriteAll(pWriter, sZero, iPadding);
}

static bool __xrtTarWritePaxHeader(xrttarwriter* pWriter, const xrttarentry* pEntry,
	xbuffer pPax)
{
	unsigned char sHeader[XRT_TAR_BLOCK_SIZE];
	xrttarentry tPaxEntry = *pEntry;
	const char* sName = pEntry->sName != NULL ? pEntry->sName : "entry";
	const char* sBase = strrchr(sName, '/');
	char sPaxName[101];
	if ( sBase != NULL ) { ++sBase; }
	else { sBase = sName; }
	snprintf(sPaxName, sizeof(sPaxName), "PaxHeaders.X/%.80s", sBase);
	tPaxEntry.iMode = 0644u;
	tPaxEntry.iUID = 0u;
	tPaxEntry.iGID = 0u;
	tPaxEntry.sUserName = "";
	tPaxEntry.sGroupName = "";
	if ( !__xrtTarBuildHeader(sHeader, &tPaxEntry, sPaxName, NULL, pPax->Length, 'x') ||
		!__xrtTarWriteAll(pWriter, sHeader, sizeof(sHeader)) ||
		!__xrtTarWriteAll(pWriter, pPax->Buffer, pPax->Length) ||
		!__xrtTarWritePadding(pWriter, pPax->Length) ) { return FALSE; }
	return TRUE;
}

static bool __xrtTarWriteEntryHeader(xrttarwriter* pWriter, const xrttarentry* pEntry,
	uint64 iDataSize)
{
	unsigned char sHeader[XRT_TAR_BLOCK_SIZE];
	xbuffer pPax;
	char sNameField[100];
	char sPrefix[155];
	unsigned char iType = __xrtTarTypeFlag(pEntry->iType);
	bool bNeedPax;
	if ( pEntry == NULL || pEntry->sName == NULL || pEntry->sName[0] == 0 || iType == 0u ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT, "invalid TAR entry metadata");
		return FALSE;
	}
	if ( !xrtIsUTF8((str)pEntry->sName, strlen(pEntry->sName)) ||
		(pEntry->sLinkName != NULL &&
		 !xrtIsUTF8((str)pEntry->sLinkName, strlen(pEntry->sLinkName))) ||
		(pEntry->sUserName != NULL &&
		 !xrtIsUTF8((str)pEntry->sUserName, strlen(pEntry->sUserName))) ||
		(pEntry->sGroupName != NULL &&
		 !xrtIsUTF8((str)pEntry->sGroupName, strlen(pEntry->sGroupName))) ||
		!__xrtTarEntryPathSafe(pEntry->sName,
		 pEntry->iType == XRT_TAR_ENTRY_DIRECTORY, &pWriter->Error, pWriter->EntryCount) ) {
		pWriter->Failed = TRUE;
		xrtSetError(xrtCopyStr((str)pWriter->Error.sMessage, 0u), TRUE);
		return FALSE;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_SYMLINK && !pWriter->Options.bAllowSymlinks ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR symbolic links are disabled");
		return FALSE;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_HARDLINK && !pWriter->Options.bAllowHardlinks ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR hard links are disabled");
		return FALSE;
	}
	if ( (pEntry->iType == XRT_TAR_ENTRY_SYMLINK ||
		pEntry->iType == XRT_TAR_ENTRY_HARDLINK) &&
		(pEntry->sLinkName == NULL ||
		 !xrtPathIsSafeArchiveEntry((str)pEntry->sLinkName, FALSE)) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR link target: %s",
			pEntry->sLinkName != NULL ? pEntry->sLinkName : "");
		return FALSE;
	}
	if ( pEntry->iType >= XRT_TAR_ENTRY_FIFO && !pWriter->Options.bAllowSpecialFiles ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR special files are disabled");
		return FALSE;
	}
	if ( pWriter->Options.tLimits.iMaxEntries != 0u &&
		pWriter->EntryCount >= pWriter->Options.tLimits.iMaxEntries ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR entry limit reached");
		return FALSE;
	}
	if ( pWriter->Options.tLimits.iMaxItemBytes != 0u &&
		iDataSize > pWriter->Options.tLimits.iMaxItemBytes ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR entry exceeds the configured item limit");
		return FALSE;
	}
	pPax = xrtBufferCreate(256u);
	if ( pPax == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR PAX metadata");
		return FALSE;
	}
	bNeedPax = !__xrtTarNameSplit(pEntry->sName, sNameField, sPrefix);
	if ( bNeedPax && !__xrtTarPaxAppend(pPax, "path", pEntry->sName) ) { goto memory_error; }
	if ( pEntry->sLinkName != NULL && strlen(pEntry->sLinkName) > 100u &&
		!__xrtTarPaxAppend(pPax, "linkpath", pEntry->sLinkName) ) { goto memory_error; }
	if ( !__xrtTarOctalFits(iDataSize, 12u) && !__xrtTarPaxAppendUInt(pPax, "size", iDataSize) ) {
		goto memory_error;
	}
	if ( (pEntry->iMTime < 0 || !__xrtTarOctalFits((uint64)pEntry->iMTime, 12u)) &&
		!__xrtTarPaxAppendInt(pPax, "mtime", pEntry->iMTime) ) { goto memory_error; }
	if ( !__xrtTarOctalFits(pEntry->iUID, 8u) && !__xrtTarPaxAppendUInt(pPax, "uid", pEntry->iUID) ) {
		goto memory_error;
	}
	if ( !__xrtTarOctalFits(pEntry->iGID, 8u) && !__xrtTarPaxAppendUInt(pPax, "gid", pEntry->iGID) ) {
		goto memory_error;
	}
	if ( pEntry->sUserName != NULL && strlen(pEntry->sUserName) > 32u &&
		!__xrtTarPaxAppend(pPax, "uname", pEntry->sUserName) ) { goto memory_error; }
	if ( pEntry->sGroupName != NULL && strlen(pEntry->sGroupName) > 32u &&
		!__xrtTarPaxAppend(pPax, "gname", pEntry->sGroupName) ) { goto memory_error; }
	if ( pPax->Length != 0u && !__xrtTarWritePaxHeader(pWriter, pEntry, pPax) ) {
		xrtBufferDestroy(pPax);
		return FALSE;
	}
	xrtBufferDestroy(pPax);
	if ( !__xrtTarBuildHeader(sHeader, pEntry, pEntry->sName,
		pEntry->sLinkName, iDataSize, iType) ||
		!__xrtTarWriteAll(pWriter, sHeader, sizeof(sHeader)) ) {
		if ( !pWriter->Failed ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_FORMAT,
				"cannot encode TAR entry header");
		}
		return FALSE;
	}
	pWriter->EntryCount += 1u;
	return TRUE;

memory_error:
	xrtBufferDestroy(pPax);
	__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
		"cannot encode TAR PAX metadata");
	return FALSE;
}

XXAPI xrttarwriter* xrtTarWriterCreate(xwriter pOutput, bool bTakeOwnership,
	const xrttaroptions* pOptions)
{
	xrttarwriter* pWriter = (xrttarwriter*)xrtCalloc(1u, sizeof(*pWriter));
	if ( pWriter == NULL ) { return NULL; }
	xrtTarErrorInit(&pWriter->Error);
	pWriter->Writer = pOutput;
	pWriter->TakeWriter = bTakeOwnership;
	if ( pOutput == NULL || !__xrtTarCopyOptions(&pWriter->Options, pOptions) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR writer or options");
	}
	return pWriter;
}

XXAPI xrttarwriter* xrtTarWriterCreateMemory(const xrttaroptions* pOptions)
{
	xbuffer pBuffer = xrtBufferCreate(65536u);
	xwriter pOutput = pBuffer != NULL ? xrtWriterFromBuffer(pBuffer, FALSE) : NULL;
	xrttarwriter* pWriter = xrtTarWriterCreate(pOutput, TRUE, pOptions);
	if ( pWriter != NULL ) { pWriter->Memory = pBuffer; }
	else {
		xrtWriterDestroy(pOutput);
		xrtBufferDestroy(pBuffer);
	}
	return pWriter;
}

XXAPI xrttarwriter* xrtTarWriterOpenFile(str sPath, const xrttaroptions* pOptions)
{
	xwriter pOutput = sPath != NULL ? xrtWriterOpen(sPath, TRUE) : NULL;
	return xrtTarWriterCreate(pOutput, TRUE, pOptions);
}

XXAPI const xrttarerror* xrtTarWriterError(const xrttarwriter* pWriter)
{
	return pWriter != NULL ? &pWriter->Error : NULL;
}

XXAPI bool xrtTarWriterAdd(xrttarwriter* pWriter, const xrttarentry* pEntry,
	const void* pData, uint64 iSize)
{
	uint64 iDataSize;
	if ( pWriter == NULL || pEntry == NULL || pWriter->Failed || pWriter->Closed ||
		(pData == NULL && iSize != 0u) ) { return FALSE; }
	iDataSize = pEntry->iType == XRT_TAR_ENTRY_FILE ? iSize : 0u;
	if ( pWriter->Options.tLimits.iMaxInputBytes != 0u &&
		(iDataSize > pWriter->Options.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Options.tLimits.iMaxInputBytes - iDataSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return FALSE;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_FILE && pEntry->iSizeBytes != 0u &&
		pEntry->iSizeBytes != iSize ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry size does not match its data");
		return FALSE;
	}
	if ( !__xrtTarWriteEntryHeader(pWriter, pEntry, iDataSize) ||
		(iDataSize != 0u && !__xrtTarWriteAll(pWriter, pData, iDataSize)) ||
		!__xrtTarWritePadding(pWriter, iDataSize) ) { return FALSE; }
	pWriter->InputBytes += iDataSize;
	return TRUE;
}

XXAPI bool xrtTarWriterAddFile(xrttarwriter* pWriter, str sName, str sSourcePath,
	uint32 iMode, int64 iMTime)
{
	xreader pInput;
	xrttarentry tEntry;
	uint64 iSize;
	uint64 iRemain;
	unsigned char sChunk[65536];
	if ( pWriter == NULL || sName == NULL || sSourcePath == NULL || pWriter->Failed ) { return FALSE; }
	pInput = xrtReaderOpen(sSourcePath);
	if ( pInput == NULL || !xrtReaderSize(pInput, &iSize) ) {
		xrtReaderDestroy(pInput);
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
			"cannot open TAR source file: %s", sSourcePath);
		return FALSE;
	}
	if ( pWriter->Options.tLimits.iMaxInputBytes != 0u &&
		(iSize > pWriter->Options.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Options.tLimits.iMaxInputBytes - iSize) ) {
		xrtReaderDestroy(pInput);
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return FALSE;
	}
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iSizeBytes = iSize;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	if ( !__xrtTarWriteEntryHeader(pWriter, &tEntry, iSize) ) {
		xrtReaderDestroy(pInput);
		return FALSE;
	}
	iRemain = iSize;
	while ( iRemain != 0u ) {
		uint64 iChunk = iRemain < sizeof(sChunk) ? iRemain : sizeof(sChunk);
		if ( !xrtReaderReadExact(pInput, sChunk, iChunk) ||
			!__xrtTarWriteAll(pWriter, sChunk, iChunk) ) {
			xrtReaderDestroy(pInput);
			if ( !pWriter->Failed ) {
				__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
					"cannot read TAR source file: %s", sSourcePath);
			}
			return FALSE;
		}
		iRemain -= iChunk;
	}
	xrtReaderDestroy(pInput);
	if ( !__xrtTarWritePadding(pWriter, iSize) ) { return FALSE; }
	pWriter->InputBytes += iSize;
	return TRUE;
}

XXAPI bool xrtTarWriterAddDirectory(xrttarwriter* pWriter, str sName,
	uint32 iMode, int64 iMTime)
{
	xrttarentry tEntry;
	str sDirectory;
	size_t iLength;
	bool bResult;
	if ( sName == NULL || sName[0] == 0 ) { return FALSE; }
	iLength = strlen((const char*)sName);
	sDirectory = sName[iLength - 1u] == '/' ? xrtCopyStr(sName, iLength) : xrtFormat("%s/", sName);
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sDirectory;
	tEntry.iType = XRT_TAR_ENTRY_DIRECTORY;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	bResult = xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u);
	if ( sDirectory != sName ) { xrtFree(sDirectory); }
	return bResult;
}

XXAPI bool xrtTarWriterAddSymlink(xrttarwriter* pWriter, str sName, str sTarget,
	uint32 iMode, int64 iMTime)
{
	xrttarentry tEntry;
	if ( sName == NULL || sTarget == NULL || !xrtPathIsSafeArchiveEntry(sTarget, FALSE) ) {
		return FALSE;
	}
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.sLinkName = (const char*)sTarget;
	tEntry.iType = XRT_TAR_ENTRY_SYMLINK;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	return xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u);
}

XXAPI bool xrtTarWriterClose(xrttarwriter* pWriter)
{
	static const unsigned char sEnd[XRT_TAR_BLOCK_SIZE * 2u] = { 0 };
	if ( pWriter == NULL || pWriter->Failed ) { return FALSE; }
	if ( pWriter->Closed ) { return TRUE; }
	if ( !__xrtTarWriteAll(pWriter, sEnd, sizeof(sEnd)) ||
		!xrtWriterFlush(pWriter->Writer) ) {
		if ( !pWriter->Failed ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
				"cannot finish TAR stream");
		}
		return FALSE;
	}
	pWriter->Closed = TRUE;
	return TRUE;
}

XXAPI xbuffer xrtTarWriterTakeBuffer(xrttarwriter* pWriter)
{
	xbuffer pResult;
	if ( pWriter == NULL || !pWriter->Closed || pWriter->Memory == NULL ) { return NULL; }
	pResult = pWriter->Memory;
	pWriter->Memory = NULL;
	return pResult;
}

XXAPI void xrtTarWriterDestroy(xrttarwriter* pWriter)
{
	if ( pWriter == NULL ) { return; }
	if ( !pWriter->Closed && !pWriter->Failed ) { xrtTarWriterClose(pWriter); }
	if ( pWriter->TakeWriter && pWriter->Writer != NULL ) { xrtWriterDestroy(pWriter->Writer); }
	xrtBufferDestroy(pWriter->Memory);
	xrtFree(pWriter);
}
