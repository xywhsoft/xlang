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
		return true;
	}
	for ( i = iLength; i-- > 0u; ) {
		if ( sName[i] == '/' && i <= 155u && iLength - i - 1u <= 100u ) {
			memcpy(sPrefix, sName, i);
			memcpy(sHeaderName, sName + i + 1u, iLength - i - 1u);
			return true;
		}
	}
	return false;
}

static bool __xrtTarOctalFits(uint64 iValue, size_t iLength)
{
	uint64 iMax = 0u;
	size_t i;
	if ( iLength < 2u ) { return false; }
	for ( i = 0u; i < iLength - 1u; ++i ) {
		if ( iMax > (UINT64_MAX - 7u) / 8u ) { return true; }
		iMax = iMax * 8u + 7u;
	}
	return iValue <= iMax;
}

static bool __xrtTarWriteOctal(unsigned char* pField, size_t iLength, uint64 iValue)
{
	char sValue[32];
	int iWritten;
	if ( !__xrtTarOctalFits(iValue, iLength) || iLength > sizeof(sValue) ) { return false; }
	iWritten = snprintf(sValue, sizeof(sValue), "%0*llo", (int)iLength - 1,
		(unsigned long long)iValue);
	if ( iWritten != (int)iLength - 1 ) { return false; }
	memcpy(pField, sValue, iLength - 1u);
	pField[iLength - 1u] = 0;
	return true;
}

static size_t __xrtTarDecimalDigits(size_t iValue)
{
	size_t iDigits = 1u;
	while ( iValue >= 10u ) { iValue /= 10u; ++iDigits; }
	return iDigits;
}

static bool __xrtTarPaxAppend(xbuffer* pPax, const char* sKey, const char* sValue)
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
	return (iPrefix > 0) && xrtBufferAppend(pPax,
		(xbytesview){ (const uint8*)sPrefix, (size_t)iPrefix }) &&
		xrtBufferAppend(pPax, (xbytesview){ (const uint8*)sKey, strlen(sKey) }) &&
		xrtBufferAppend(pPax, XRT_BYTES_LITERAL("=")) &&
		xrtBufferAppend(pPax, (xbytesview){ (const uint8*)sValue, strlen(sValue) }) &&
		xrtBufferAppend(pPax, XRT_BYTES_LITERAL("\n"));
}

static bool __xrtTarPaxAppendUInt(xbuffer* pPax, const char* sKey, uint64 iValue)
{
	char sValue[32];
	snprintf(sValue, sizeof(sValue), "%llu", (unsigned long long)iValue);
	return __xrtTarPaxAppend(pPax, sKey, sValue);
}

static bool __xrtTarPaxAppendInt(xbuffer* pPax, const char* sKey, int64 iValue)
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
		!__xrtTarWriteOctal(pHeader + 337u, 8u, pEntry->iDeviceMinor) ) { return false; }
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
	return true;
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




/* 计算一个条目完整帧的字节数，确保任何输出前都已通过上限检查。 */
static bool __xrtTarEntryFrameFits(xrttarwriter* pWriter,
	size_t iPaxSize, uint64 iDataSize)
{
	uint64 iFrameSize = XRT_TAR_BLOCK_SIZE;
	uint64 iPadding;

	if ( iPaxSize != 0u ) {
		iPadding = (XRT_TAR_BLOCK_SIZE -
			((uint64)iPaxSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
		if ( ((uint64)iPaxSize > UINT64_MAX - XRT_TAR_BLOCK_SIZE) ||
			(iPadding > UINT64_MAX - XRT_TAR_BLOCK_SIZE - (uint64)iPaxSize) ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
				"TAR PAX metadata is too large");
			return false;
		}
		iFrameSize += XRT_TAR_BLOCK_SIZE + (uint64)iPaxSize + iPadding;
	}
	iPadding = (XRT_TAR_BLOCK_SIZE -
		(iDataSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
	if ( (iDataSize > UINT64_MAX - iFrameSize) ||
		(iPadding > UINT64_MAX - iFrameSize - iDataSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR entry frame is too large");
		return false;
	}
	iFrameSize += iDataSize + iPadding;
	if ( (pWriter->Options.tLimits.iMaxOutputBytes != 0u) &&
		(iFrameSize > pWriter->Options.tLimits.iMaxOutputBytes ||
		 pWriter->OutputBytes >
			pWriter->Options.tLimits.iMaxOutputBytes - iFrameSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR output cannot contain the complete entry frame");
		return false;
	}
	return true;
}

static bool __xrtTarWritePaxHeader(xrttarwriter* pWriter, const xrttarentry* pEntry,
	xbuffer* pPax)
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
	if ( !__xrtTarBuildHeader(sHeader, &tPaxEntry, sPaxName, NULL, pPax->Size, 'x') ||
		!__xrtTarWriteAll(pWriter, sHeader, sizeof(sHeader)) ||
		!__xrtTarWriteAll(pWriter, pPax->Data, pPax->Size) ||
		!__xrtTarWritePadding(pWriter, pPax->Size) ) { return false; }
	return true;
}

static bool __xrtTarWriteEntryHeader(xrttarwriter* pWriter, const xrttarentry* pEntry,
	uint64 iDataSize)
{
	unsigned char sHeader[XRT_TAR_BLOCK_SIZE];
	xbuffer* pPax;
	char sNameField[100];
	char sPrefix[155];
	unsigned char iType = __xrtTarTypeFlag(pEntry->iType);
	bool bNeedPax;
	if ( pEntry == NULL || pEntry->sName == NULL || pEntry->sName[0] == 0 || iType == 0u ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT, "invalid TAR entry metadata");
		return false;
	}
	if ( !xrtUtf8Valid(xrtStrView(pEntry->sName), NULL) ||
		(pEntry->sLinkName != NULL &&
		 !xrtUtf8Valid(xrtStrView(pEntry->sLinkName), NULL)) ||
		(pEntry->sUserName != NULL &&
		 !xrtUtf8Valid(xrtStrView(pEntry->sUserName), NULL)) ||
		(pEntry->sGroupName != NULL &&
		 !xrtUtf8Valid(xrtStrView(pEntry->sGroupName), NULL)) ||
		!__xrtTarEntryPathSafe(pEntry->sName,
		 pEntry->iType == XRT_TAR_ENTRY_DIRECTORY, &pWriter->Error, pWriter->EntryCount) ) {
		if ( pWriter->Error.iCode == XRT_TAR_OK ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
				"TAR entry metadata is not valid UTF-8");
		} else {
			xrtSetErrorInfo(XERR_PERMISSION, "xlang.extlib.tar",
				pWriter->Error.iCode, pWriter->Error.sMessage);
		}
		return false;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_SYMLINK && !pWriter->Options.bAllowSymlinks ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR symbolic links are disabled");
		return false;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_HARDLINK && !pWriter->Options.bAllowHardlinks ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR hard links are disabled");
		return false;
	}
	if ( (pEntry->iType == XRT_TAR_ENTRY_SYMLINK ||
		pEntry->iType == XRT_TAR_ENTRY_HARDLINK) &&
		(pEntry->sLinkName == NULL ||
		 !xrtPathIsSafeEntry(xrtStrView(pEntry->sLinkName), false)) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSAFE_PATH,
			"unsafe TAR link target: %s",
			pEntry->sLinkName != NULL ? pEntry->sLinkName : "");
		return false;
	}
	if ( pEntry->iType >= XRT_TAR_ENTRY_FIFO && !pWriter->Options.bAllowSpecialFiles ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"TAR special files are disabled");
		return false;
	}
	if ( pWriter->Options.tLimits.iMaxEntries != 0u &&
		pWriter->EntryCount >= pWriter->Options.tLimits.iMaxEntries ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR entry limit reached");
		return false;
	}
	if ( pWriter->Options.tLimits.iMaxItemBytes != 0u &&
		iDataSize > pWriter->Options.tLimits.iMaxItemBytes ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR entry exceeds the configured item limit");
		return false;
	}
	pPax = xrtBufferCreate();
	if ( (pPax == NULL) || !xrtBufferReserve(pPax, 256u) ) {
		xrtBufferDestroy(pPax);
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR PAX metadata");
		return false;
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
	if ( !__xrtTarEntryFrameFits(pWriter, pPax->Size, iDataSize) ) {
		xrtBufferDestroy(pPax);
		return false;
	}
	if ( !__xrtTarBuildHeader(sHeader, pEntry, pEntry->sName,
		pEntry->sLinkName, iDataSize, iType) ) {
		xrtBufferDestroy(pPax);
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_FORMAT,
			"cannot encode TAR entry header");
		return false;
	}
	if ( (pPax->Size != 0u && !__xrtTarWritePaxHeader(pWriter, pEntry, pPax)) ||
		!__xrtTarWriteAll(pWriter, sHeader, sizeof(sHeader)) ) {
		xrtBufferDestroy(pPax);
		return false;
	}
	xrtBufferDestroy(pPax);
	pWriter->EntryCount += 1u;
	return true;

memory_error:
	xrtBufferDestroy(pPax);
	__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
		"cannot encode TAR PAX metadata");
	return false;
}

/* 分配 Writer 并验证公共选项；只有分配失败时才返回空指针。 */
static xrttarwriter* __xrtTarWriterAllocate(const xrttaroptions* pOptions)
{
	xrttarwriter* pWriter = (xrttarwriter*)xrtCalloc(1u, sizeof(*pWriter));

	if ( pWriter == NULL ) { return NULL; }
	xrtTarErrorInit(&pWriter->Error);
	if ( !__xrtTarCopyOptions(&pWriter->Options, pOptions) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR writer options");
		pWriter->Failed = true;
	}
	return pWriter;
}






/* 使用调用方 Writer 创建流式 TAR；所有权只在参数有效时接管。 */
XRT_API xrttarwriter* xrtTarWriterCreate(xwriter* pOutput, bool bTakeOwnership,
	const xrttaroptions* pOptions)
{
	xrttarwriter* pWriter = __xrtTarWriterAllocate(pOptions);

	if ( pWriter == NULL ) { return NULL; }
	if ( pWriter->Failed ) { return pWriter; }
	if ( pOutput == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR output writer is required");
		pWriter->Failed = true;
		return pWriter;
	}
	pWriter->Writer = pOutput;
	pWriter->TakeWriter = bTakeOwnership;
	return pWriter;
}





/* 创建由 TAR Writer 持有的内存 Buffer。 */
XRT_API xrttarwriter* xrtTarWriterCreateMemory(const xrttaroptions* pOptions)
{
	xrttarwriter* pWriter = __xrtTarWriterAllocate(pOptions);

	if ( (pWriter == NULL) || pWriter->Failed ) { return pWriter; }
	pWriter->Memory = xrtBufferCreate();
	if ( (pWriter->Memory != NULL) &&
		!xrtBufferReserve(pWriter->Memory, 65536u) ) {
		xrtBufferDestroy(pWriter->Memory);
		pWriter->Memory = NULL;
	}
	if ( pWriter->Memory != NULL ) {
		pWriter->Writer = xrtWriterFromBuffer(pWriter->Memory);
	}
	if ( pWriter->Writer == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR memory output");
		pWriter->Failed = true;
	}
	pWriter->TakeWriter = true;
	return pWriter;
}






/* 在目标同目录创建事务文件，成功关闭归档时才替换目标。 */
XRT_API xrttarwriter* xrtTarWriterOpenFile(cstr sPath, const xrttaroptions* pOptions)
{
	str sDirectory = NULL;
	xfile File = NULL;
	xrttarwriter* pWriter = __xrtTarWriterAllocate(pOptions);

	if ( (pWriter == NULL) || pWriter->Failed ) { return pWriter; }
	if ( (sPath == NULL) || (sPath[0] == 0) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR output path is required");
		pWriter->Failed = true;
		return pWriter;
	}
	pWriter->Path = xrtStrDup(sPath);
	sDirectory = xrtPathParent(sPath);
	if ( (pWriter->Path == NULL) || (sDirectory == NULL) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR file transaction");
		pWriter->Failed = true;
		goto cleanup;
	}
	File = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-tar-", ".tmp", &pWriter->Temporary);
	if ( File == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
			"cannot create TAR transaction file: %s", sPath);
		pWriter->Failed = true;
		goto cleanup;
	}
	pWriter->Writer = xrtWriterTakeFile(&File);
	if ( pWriter->Writer == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR transaction writer");
		pWriter->Failed = true;
		goto cleanup;
	}
	pWriter->TakeWriter = true;

cleanup:
	if ( File != NULL ) { (void)xrtClose(File); }
	xrtFree(sDirectory);
	return pWriter;
}

XRT_API const xrttarerror* xrtTarWriterError(const xrttarwriter* pWriter)
{
	return pWriter != NULL ? &pWriter->Error : NULL;
}

XRT_API bool xrtTarWriterAdd(xrttarwriter* pWriter, const xrttarentry* pEntry,
	const void* pData, uint64 iSize)
{
	uint64 iDataSize;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( pWriter->Closed ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR writer is closed");
		return false;
	}
	if ( (pEntry == NULL) || ((pData == NULL) && (iSize != 0u)) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR entry input");
		return false;
	}
	iDataSize = pEntry->iType == XRT_TAR_ENTRY_FILE ? iSize : 0u;
	if ( pWriter->Options.tLimits.iMaxInputBytes != 0u &&
		(iDataSize > pWriter->Options.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Options.tLimits.iMaxInputBytes - iDataSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return false;
	}
	if ( pEntry->iType == XRT_TAR_ENTRY_FILE && pEntry->iSizeBytes != 0u &&
		pEntry->iSizeBytes != iSize ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry size does not match its data");
		return false;
	}
	if ( !__xrtTarWriteEntryHeader(pWriter, pEntry, iDataSize) ||
		(iDataSize != 0u && !__xrtTarWriteAll(pWriter, pData, iDataSize)) ||
		!__xrtTarWritePadding(pWriter, iDataSize) ) { return false; }
	pWriter->InputBytes += iDataSize;
	return true;
}






/* 从调用方 Reader 精确复制一个普通文件条目。 */
XRT_API bool xrtTarWriterAddFrom(xrttarwriter* pWriter,
	const xrttarentry* pEntry, xreader* pReader, uint64 iSize)
{
	uint64 iCopied = 0u;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( pWriter->Closed ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR writer is closed");
		return false;
	}
	if ( (pEntry == NULL) || (pReader == NULL) ||
		(pEntry->iType != XRT_TAR_ENTRY_FILE) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR streaming entry input");
		return false;
	}
	if ( (pEntry->iSizeBytes != 0u) &&
		(pEntry->iSizeBytes != iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry size does not match its input stream");
		return false;
	}
	if ( (pWriter->Options.tLimits.iMaxInputBytes != 0u) &&
		(iSize > pWriter->Options.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
		return false;
	}
	if ( (pWriter->Options.tLimits.iMaxOutputBytes != 0u) &&
		(iSize > pWriter->Options.tLimits.iMaxOutputBytes ||
		 pWriter->OutputBytes > pWriter->Options.tLimits.iMaxOutputBytes - iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR output cannot contain the streaming entry");
		return false;
	}
	if ( !__xrtTarWriteEntryHeader(pWriter, pEntry, iSize) ) { return false; }
	if ( !xrtReaderCopyN(pReader, pWriter->Writer, iSize, &iCopied) ||
		(iCopied != iSize) ) {
		pWriter->OutputBytes += iCopied;
		__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_IO,
			"cannot copy TAR entry input stream");
		return false;
	}
	pWriter->OutputBytes += iCopied;
	if ( !__xrtTarWritePadding(pWriter, iSize) ) { return false; }
	pWriter->InputBytes += iSize;
	return true;
}






/* 流式复制 Reader 当前条目并消费其数据，所有元数据均由同一核心编码器保留。 */
XRT_API bool xrtTarWriterCopyCurrent(xrttarwriter* pWriter,
	xrttarreader* pReader)
{
	const xrttarentry* pEntry;
	uint64 iSize;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( pWriter->Closed ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR writer is closed");
		return false;
	}
	if ( pReader == NULL || pReader->Failed || !pReader->HasCurrent ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR reader has no current entry to copy");
		return false;
	}
	pEntry = &pReader->Entry;
	if ( pEntry->iType != XRT_TAR_ENTRY_FILE ) {
		if ( !xrtTarWriterAdd(pWriter, pEntry, NULL, 0u) ) { return false; }
		if ( !xrtTarReaderSkip(pReader) ) {
			__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_IO,
				"cannot consume copied TAR metadata entry");
			return false;
		}
		return true;
	}
	iSize = pReader->DataRemaining;
	if ( iSize != pEntry->iSizeBytes ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR current file entry was already partially consumed");
		return false;
	}
	if ( (pWriter->Options.tLimits.iMaxInputBytes != 0u) &&
		(iSize > pWriter->Options.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes >
			pWriter->Options.tLimits.iMaxInputBytes - iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR copied input exceeds the configured limit");
		return false;
	}
	if ( (pReader->Options.tLimits.iMaxOutputBytes != 0u) &&
		(iSize > pReader->Options.tLimits.iMaxOutputBytes ||
		 pReader->TotalOutput >
			pReader->Options.tLimits.iMaxOutputBytes - iSize) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_LIMIT,
			"TAR copied entry exceeds the reader output limit");
		return false;
	}
	if ( !__xrtTarWriteEntryHeader(pWriter, pEntry, iSize) ) { return false; }
	if ( !xrtTarReaderReadTo(pReader, pWriter->Writer) ) {
		__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_IO,
			"cannot stream the current TAR entry into the writer");
		return false;
	}
	pWriter->OutputBytes += iSize;
	if ( !__xrtTarWritePadding(pWriter, iSize) ) { return false; }
	pWriter->InputBytes += iSize;
	return true;
}





XRT_API bool xrtTarWriterAddFile(xrttarwriter* pWriter, cstr sName, cstr sSourcePath,
	uint32 iMode, int64 iMTime)
{
	xreader* pInput;
	xrttarentry tEntry;
	uint64 iSize;
	bool bResult;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( (sName == NULL) || (sSourcePath == NULL) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry and source paths are required");
		return false;
	}
	pInput = xrtReaderOpen(sSourcePath);
	if ( pInput == NULL || !xrtReaderSize(pInput, &iSize) ) {
		(void)xrtReaderDestroy(pInput);
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
			"cannot open TAR source file: %s", sSourcePath);
		return false;
	}
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.iType = XRT_TAR_ENTRY_FILE;
	tEntry.iSizeBytes = iSize;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	bResult = xrtTarWriterAddFrom(pWriter, &tEntry, pInput, iSize);
	if ( !xrtReaderDestroy(pInput) && bResult ) {
		__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_IO,
			"cannot close TAR source file: %s", sSourcePath);
		bResult = false;
	}
	return bResult;
}

XRT_API bool xrtTarWriterAddDirectory(xrttarwriter* pWriter, cstr sName,
	uint32 iMode, int64 iMTime)
{
	xrttarentry tEntry;
	str sDirectory;
	size_t iLength;
	bool bResult;
	if ( sName == NULL || sName[0] == 0 ) {
		if ( pWriter != NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
				"TAR directory name is required");
		}
		return false;
	}
	iLength = strlen((const char*)sName);
	sDirectory = sName[iLength - 1u] == '/' ?
		xrtStrDupN(sName, iLength) : xrtFormat("%s/", sName);
	if ( sDirectory == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot copy TAR directory name");
		return false;
	}
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sDirectory;
	tEntry.iType = XRT_TAR_ENTRY_DIRECTORY;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	bResult = xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u);
	xrtFree(sDirectory);
	return bResult;
}

XRT_API bool xrtTarWriterAddSymlink(xrttarwriter* pWriter, cstr sName, cstr sTarget,
	uint32 iMode, int64 iMTime)
{
	xrttarentry tEntry;
	if ( sName == NULL || sTarget == NULL ||
		!xrtPathIsSafeEntry(xrtStrView(sTarget), false) ) {
		if ( pWriter != NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
				"invalid TAR symbolic-link input");
		}
		return false;
	}
	xrtTarEntryInit(&tEntry);
	tEntry.sName = (const char*)sName;
	tEntry.sLinkName = (const char*)sTarget;
	tEntry.iType = XRT_TAR_ENTRY_SYMLINK;
	tEntry.iMode = iMode;
	tEntry.iMTime = iMTime;
	return xrtTarWriterAdd(pWriter, &tEntry, NULL, 0u);
}






typedef struct {
	xrttarwriter* Writer;
	cstr Source;
	cstr ArchiveRoot;
} xrttartreecontext;





/* 把本机遍历路径转换为 TAR 使用的 POSIX 相对路径。 */
static str __xrtTarTreeName(const xrttartreecontext* pContext, cstr sPath)
{
	str sRelative = xrtPathRel(pContext->Source, sPath);
	str sResult;
	char* p;

	if ( sRelative == NULL ) { return NULL; }
	for ( p = sRelative; *p != 0; ++p ) {
		if ( *p == '\\' ) { *p = '/'; }
	}
	if ( strcmp(sRelative, ".") == 0 ) { sRelative[0] = 0; }
	if ( (pContext->ArchiveRoot == NULL) ||
		(pContext->ArchiveRoot[0] == 0) ) {
		return sRelative;
	}
	if ( sRelative[0] == 0 ) {
		xrtFree(sRelative);
		return xrtStrDup(pContext->ArchiveRoot);
	}
	sResult = xrtFormat("%s/%s", pContext->ArchiveRoot, sRelative);
	xrtFree(sRelative);
	return sResult;
}





/* 把目录遍历事件写入同一个 TAR Writer。 */
static xwalkcontrol __xrtTarTreeProc(const xwalkentry* pWalk, ptr pUserData)
{
	xrttartreecontext* pContext = (xrttartreecontext*)pUserData;
	xrttarwriter* pWriter = pContext->Writer;
	str sName;
	uint32 iMode;
	int64 iMTime;
	bool bResult = true;

	if ( pWalk->Event == XWALK_LEAVE ) { return XWALK_CONTINUE; }
	if ( (pWalk->Flags & XWALK_ENTRY_UTF8) == 0u ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR source tree contains a non-UTF-8 path");
		return XWALK_ERROR;
	}
	sName = __xrtTarTreeName(pContext, pWalk->Path);
	if ( sName == NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
			"cannot build TAR tree entry name");
		return XWALK_ERROR;
	}
	iMode = (pWalk->Info.Available & XFILE_INFO_MODE) != 0u ?
		pWalk->Info.Mode : (pWalk->Info.Type == XFILE_TYPE_DIRECTORY ? 0755u : 0644u);
	iMTime = (pWalk->Info.Available & XFILE_INFO_MODIFY_TIME) != 0u ?
		(int64)(pWalk->Info.Modified / 1000000) : 0;

	if ( pWalk->Event == XWALK_ENTER ) {
		if ( sName[0] != 0 ) {
			bResult = xrtTarWriterAddDirectory(pWriter, sName, iMode, iMTime);
		}
	} else if ( pWalk->Info.Type == XFILE_TYPE_FILE ) {
		bResult = xrtTarWriterAddFile(pWriter, sName,
			pWalk->Path, iMode, iMTime);
	} else if ( pWalk->Info.Type == XFILE_TYPE_LINK ) {
		str sTarget = xrtLinkRead(pWalk->Path);

		if ( sTarget == NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
				"cannot read TAR source link: %s", pWalk->Path);
			bResult = false;
		} else {
			bResult = xrtTarWriterAddSymlink(pWriter, sName,
				sTarget, iMode, iMTime);
			xrtFree(sTarget);
		}
	} else {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_UNSUPPORTED,
			"unsupported TAR source tree object: %s", pWalk->Path);
		bResult = false;
	}
	xrtFree(sName);
	return bResult ? XWALK_CONTINUE : XWALK_ERROR;
}





/* 添加目录树；遍历默认不跟随链接，链接条目按 Writer 策略处理。 */
XRT_API bool xrtTarWriterAddTree(xrttarwriter* pWriter, cstr sArchiveRoot,
	cstr sSourceDirectory, bool bRecursive)
{
	xfileinfo Info;
	xwalkoptions Options;
	xrttartreecontext Context;
	str sRoot = NULL;
	size_t iLength;
	bool bResult;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( (sSourceDirectory == NULL) || (sSourceDirectory[0] == 0) ||
		!xrtPathStat(sSourceDirectory, true, &Info) ||
		(Info.Type != XFILE_TYPE_DIRECTORY) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
			"TAR source directory does not exist");
		return false;
	}
	if ( (sArchiveRoot != NULL) && (sArchiveRoot[0] != 0) ) {
		sRoot = xrtStrDup(sArchiveRoot);
		if ( sRoot == NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_MEMORY,
				"cannot copy TAR tree root");
			return false;
		}
		iLength = strlen(sRoot);
		while ( (iLength != 0u) && (sRoot[iLength - 1u] == '/') ) {
			sRoot[--iLength] = 0;
		}
		if ( (sRoot[0] == 0) ||
			!xrtPathIsSafeEntry(xrtStrView(sRoot), false) ) {
			xrtFree(sRoot);
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
				"invalid TAR tree root");
			return false;
		}
	}
	xrtWalkOptionsInit(&Options);
	Options.MaxDepth = bRecursive ? (size_t)-1 : 1u;
	Context.Writer = pWriter;
	Context.Source = sSourceDirectory;
	Context.ArchiveRoot = sRoot;
	bResult = xrtFileWalk(sSourceDirectory, &Options,
		__xrtTarTreeProc, &Context, NULL);
	if ( !bResult && (pWriter->Error.iCode == XRT_TAR_OK) ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
			"cannot walk TAR source directory: %s", sSourceDirectory);
	}
	xrtFree(sRoot);
	return bResult;
}






/* 事务性重建已有归档；源归档保持只读，显式关闭后才原子替换。 */
XRT_API xrttarwriter* xrtTarWriterOpenAppend(cstr sPath,
	const xrttaroptions* pOptions)
{
	xfileinfo Info;
	const xerror* pSystemError;
	xrttarreader* pReader = NULL;
	xrttarwriter* pWriter;
	const xrttarerror* pReaderError;
	xrttarentry Entry;

	if ( (sPath == NULL) || (sPath[0] == 0) ) {
		return xrtTarWriterOpenFile(sPath, pOptions);
	}
	if ( !xrtPathStat(sPath, false, &Info) ) {
		pSystemError = xrtGetError();
		if ( (pSystemError != NULL) &&
			(xrtErrorKind(pSystemError) == XERR_NOT_FOUND) ) {
			xrtClearError();
			return xrtTarWriterOpenFile(sPath, pOptions);
		}
		pWriter = __xrtTarWriterAllocate(pOptions);
		if ( pWriter != NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
				"cannot inspect TAR append target: %s", sPath);
			pWriter->Failed = true;
		}
		return pWriter;
	}
	if ( Info.Type != XFILE_TYPE_FILE ) {
		pWriter = __xrtTarWriterAllocate(pOptions);
		if ( pWriter != NULL ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_ARGUMENT,
				"TAR append target is not a regular file: %s", sPath);
			pWriter->Failed = true;
		}
		return pWriter;
	}
	pReader = xrtTarReaderOpenFile(sPath, pOptions);
	pReaderError = xrtTarReaderError(pReader);
	if ( (pReader == NULL) ||
		(pReaderError != NULL && pReaderError->iCode != XRT_TAR_OK) ) {
		pWriter = __xrtTarWriterAllocate(pOptions);
		if ( pWriter != NULL ) {
			__xrtTarSetWriterError(pWriter,
				pReaderError != NULL ? pReaderError->iCode : XRT_TAR_ERROR_IO,
				"cannot open TAR append source: %s",
				pReaderError != NULL ? pReaderError->sMessage : sPath);
			pWriter->Failed = true;
		}
		xrtTarReaderDestroy(pReader);
		return pWriter;
	}
	pWriter = xrtTarWriterOpenFile(sPath, pOptions);
	if ( pWriter == NULL || pWriter->Failed ) {
		xrtTarReaderDestroy(pReader);
		return pWriter;
	}
	while ( xrtTarReaderNext(pReader, &Entry) ) {
		if ( !xrtTarWriterCopyCurrent(pWriter, pReader) ) {
			pWriter->Failed = true;
			break;
		}
	}
	if ( !pWriter->Failed && !xrtTarReaderEOF(pReader) ) {
		pReaderError = xrtTarReaderError(pReader);
		__xrtTarFailWriter(pWriter,
			pReaderError != NULL ? pReaderError->iCode : XRT_TAR_ERROR_FORMAT,
			"cannot read TAR append source: %s",
			pReaderError != NULL ? pReaderError->sMessage : sPath);
	}
	xrtTarReaderDestroy(pReader);
	return pWriter;
}





/* 完成流并提交磁盘事务；原子替换失败时允许调用方重试提交。 */
XRT_API bool xrtTarWriterClose(xrttarwriter* pWriter)
{
	static const unsigned char sEnd[XRT_TAR_BLOCK_SIZE * 2u] = { 0 };
	bool bClose;

	if ( pWriter == NULL || pWriter->Failed ) { return false; }
	if ( pWriter->Closed ) { return true; }
	if ( !pWriter->Finalized ) {
		if ( !__xrtTarWriteAll(pWriter, sEnd, sizeof(sEnd)) ||
			!xrtWriterFlush(pWriter->Writer) ) {
			if ( !pWriter->Failed ) {
				__xrtTarFailWriter(pWriter, __xrtTarIoFailureCode(),
					"cannot finish TAR stream");
			}
			return false;
		}
		if ( pWriter->Temporary != NULL ) {
			bClose = xrtWriterDestroy(pWriter->Writer);
			pWriter->Writer = NULL;
			if ( !bClose ) {
				__xrtTarFailWriter(pWriter, XRT_TAR_ERROR_IO,
					"cannot close TAR transaction file");
				return false;
			}
			pWriter->TakeWriter = false;
		}
		pWriter->Finalized = true;
	}
	if ( pWriter->Temporary != NULL ) {
		if ( !xrtPathRename(pWriter->Temporary, pWriter->Path, true) ) {
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
				"cannot atomically save TAR file: %s", pWriter->Path);
			return false;
		}
		xrtFree(pWriter->Temporary);
		pWriter->Temporary = NULL;
	}
	pWriter->Closed = true;
	return true;
}






/* 仅在内存归档成功关闭后移交 Buffer。 */
XRT_API xbuffer* xrtTarWriterTakeBuffer(xrttarwriter* pWriter)
{
	xbuffer* pResult;
	if ( pWriter == NULL ) { return NULL; }
	if ( !pWriter->Closed || pWriter->Memory == NULL || pWriter->Path != NULL ) {
		__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_STATE,
			"TAR Buffer is only available after closing a memory archive");
		return NULL;
	}
	if ( pWriter->TakeWriter && pWriter->Writer != NULL ) {
		if ( !xrtWriterDestroy(pWriter->Writer) ) {
			pWriter->Writer = NULL;
			pWriter->TakeWriter = false;
			__xrtTarSetWriterError(pWriter, XRT_TAR_ERROR_IO,
				"cannot close TAR memory writer");
			return NULL;
		}
		pWriter->Writer = NULL;
		pWriter->TakeWriter = false;
	}
	pResult = pWriter->Memory;
	pWriter->Memory = NULL;
	return pResult;
}






/* 销毁 Writer；未提交事务始终删除临时文件，不触碰原目标。 */
XRT_API void xrtTarWriterDestroy(xrttarwriter* pWriter)
{
	if ( pWriter == NULL ) { return; }
	if ( pWriter->TakeWriter && pWriter->Writer != NULL ) {
		(void)xrtWriterDestroy(pWriter->Writer);
	}
	if ( (pWriter->Temporary != NULL) && xrtPathExists(pWriter->Temporary) ) {
		(void)xrtFileDelete(pWriter->Temporary);
	}
	xrtBufferDestroy(pWriter->Memory);
	xrtFree(pWriter->Path);
	xrtFree(pWriter->Temporary);
	xrtFree(pWriter);
}
