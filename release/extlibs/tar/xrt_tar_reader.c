#include "xrt_tar_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>



static bool __xrtTarBlockZero(const unsigned char* pBlock)
{
	size_t i;
	for ( i = 0u; i < XRT_TAR_BLOCK_SIZE; ++i ) {
		if ( pBlock[i] != 0u ) { return false; }
	}
	return true;
}

static str __xrtTarFieldString(const unsigned char* pField, size_t iLength)
{
	size_t iSize = 0u;
	while ( iSize < iLength && pField[iSize] != 0u ) { ++iSize; }
	while ( iSize != 0u && pField[iSize - 1u] == ' ' ) { --iSize; }
	return xrtStrDupN((const char*)pField, iSize);
}

static bool __xrtTarParseBinary(const unsigned char* pField, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	bool bNegative = (pField[0] & 0x40u) != 0u;
	size_t iPrefix = iLength > 8u ? iLength - 8u : 0u;
	uint64 iValue = 0u;
	size_t i;
	if ( bNegative && !bSigned ) { return false; }
	for ( i = 0u; i < iPrefix; ++i ) {
		unsigned char iExpected = bNegative ? 0xffu : 0u;
		unsigned char iByte = i == 0u ? (unsigned char)(pField[i] & 0x7fu) : pField[i];
		if ( i == 0u && bNegative ) { iExpected = 0x7fu; }
		if ( iByte != iExpected ) { return false; }
	}
	for ( i = iPrefix; i < iLength; ++i ) {
		unsigned char iByte = i == 0u ? (unsigned char)(pField[i] & 0x7fu) : pField[i];
		iValue = (iValue << 8u) | iByte;
	}
	if ( bNegative ) {
		if ( iLength <= 8u ) {
			uint32 iBits = (uint32)(iLength * 8u - 1u);
			iValue |= UINT64_MAX << iBits;
		}
		if ( pSigned != NULL ) { *pSigned = (int64)iValue; }
	}
	else {
		if ( pUnsigned != NULL ) { *pUnsigned = iValue; }
		if ( pSigned != NULL ) {
			if ( iValue > INT64_MAX ) { return false; }
			*pSigned = (int64)iValue;
		}
	}
	return true;
}

static bool __xrtTarParseNumber(const unsigned char* pField, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	uint64 iValue = 0u;
	size_t i = 0u;
	bool bDigit = false;
	if ( iLength == 0u ) { return false; }
	if ( (pField[0] & 0x80u) != 0u ) {
		return __xrtTarParseBinary(pField, iLength, bSigned, pSigned, pUnsigned);
	}
	while ( i < iLength && (pField[i] == ' ' || pField[i] == 0u) ) { ++i; }
	for ( ; i < iLength; ++i ) {
		unsigned char c = pField[i];
		if ( c == 0u || c == ' ' ) { break; }
		if ( c < '0' || c > '7' || iValue > (UINT64_MAX - (c - '0')) / 8u ) { return false; }
		iValue = iValue * 8u + (uint64)(c - '0');
		bDigit = true;
	}
	if ( !bDigit ) { iValue = 0u; }
	while ( i < iLength ) {
		if ( pField[i] != 0u && pField[i] != ' ' ) { return false; }
		++i;
	}
	if ( pUnsigned != NULL ) { *pUnsigned = iValue; }
	if ( pSigned != NULL ) {
		if ( iValue > INT64_MAX ) { return false; }
		*pSigned = (int64)iValue;
	}
	return true;
}

static bool __xrtTarChecksumValid(const unsigned char* pBlock)
{
	uint64 iStored = 0u;
	uint64 iUnsigned = 0u;
	int64 iSigned = 0;
	size_t i;
	if ( !__xrtTarParseNumber(pBlock + 148u, 8u, false, NULL, &iStored) ) { return false; }
	for ( i = 0u; i < XRT_TAR_BLOCK_SIZE; ++i ) {
		unsigned char c = i >= 148u && i < 156u ? (unsigned char)' ' : pBlock[i];
		iUnsigned += c;
		iSigned += (signed char)c;
	}
	return iStored == iUnsigned || (iSigned >= 0 && iStored == (uint64)iSigned);
}

static int __xrtTarType(unsigned char iType)
{
	switch ( iType ) {
		case 0:
		case '0': return XRT_TAR_ENTRY_FILE;
		case '5': return XRT_TAR_ENTRY_DIRECTORY;
		case '2': return XRT_TAR_ENTRY_SYMLINK;
		case '1': return XRT_TAR_ENTRY_HARDLINK;
		case '6': return XRT_TAR_ENTRY_FIFO;
		case '3': return XRT_TAR_ENTRY_CHARACTER;
		case '4': return XRT_TAR_ENTRY_BLOCK;
		default: return XRT_TAR_ENTRY_OTHER;
	}
}

static bool __xrtTarSetString(str* pTarget, const unsigned char* pValue, size_t iLength)
{
	str sValue;
	if ( memchr(pValue, 0, iLength) != NULL ||
		!xrtUtf8Valid((xstrview){ (const char*)pValue, iLength }, NULL) ) { return false; }
	sValue = xrtStrDupN((const char*)pValue, iLength);
	if ( sValue == NULL ) { return false; }
	xrtFree(*pTarget);
	*pTarget = sValue;
	return true;
}

/* GNU 长名称以 NUL 或换行结尾；PAX 值则由记录长度精确界定。 */
static bool __xrtTarSetGnuString(str* pTarget, const unsigned char* pValue, size_t iLength)
{
	while ( iLength != 0u && (pValue[iLength - 1u] == 0u || pValue[iLength - 1u] == '\n') ) {
		--iLength;
	}
	return __xrtTarSetString(pTarget, pValue, iLength);
}

static bool __xrtTarPaxNumber(const unsigned char* pValue, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	if ( iLength == 0u || iLength >= 96u ) { return false; }
	if ( bSigned ) {
		size_t i = 0u;
		int64 iValue;
		while ( i < iLength && pValue[i] != '.' ) { ++i; }
		if ( i < iLength ) {
			size_t j = i + 1u;
			if ( j == iLength ) { return false; }
			for ( ; j < iLength; ++j ) {
				if ( pValue[j] < '0' || pValue[j] > '9' ) { return false; }
			}
		}
		if ( !xrtIntParse((xstrview){ (const char*)pValue, i },
			10u, 0u, &iValue) ) { return false; }
		if ( pSigned != NULL ) { *pSigned = iValue; }
		return true;
	}
	else {
		uint64 iValue;
		if ( !xrtUIntParse((xstrview){ (const char*)pValue, iLength },
			10u, 0u, &iValue) ) { return false; }
		if ( pUnsigned != NULL ) { *pUnsigned = iValue; }
		return true;
	}
}

static bool __xrtTarParsePax(xrttarreader* pReader, xrttarpax* pPax,
	const unsigned char* pData, size_t iSize)
{
	size_t iOffset = 0u;
	uint64 iRecords = 0u;
	if ( iSize != 0u && memchr(pData, 0, iSize) != NULL ) { return false; }
	while ( iOffset < iSize ) {
		size_t i = iOffset;
		uint64 iRecordLength = 0u;
		const unsigned char* pKey;
		const unsigned char* pValue;
		size_t iKeyLength;
		size_t iValueLength;
		if ( pReader->Options.tLimits.iMaxNodes != 0u &&
			iRecords >= pReader->Options.tLimits.iMaxNodes ) { return false; }
		iRecords += 1u;
		while ( i < iSize && pData[i] >= '0' && pData[i] <= '9' ) {
			if ( iRecordLength > (UINT64_MAX - (pData[i] - '0')) / 10u ) { return false; }
			iRecordLength = iRecordLength * 10u + (uint64)(pData[i] - '0');
			++i;
		}
		if ( i == iOffset || i >= iSize || pData[i] != ' ' ||
			iRecordLength > iSize - iOffset || iRecordLength < 4u ) { return false; }
		if ( pData[iOffset + (size_t)iRecordLength - 1u] != '\n' ) { return false; }
		pKey = pData + ++i;
		while ( i < iOffset + (size_t)iRecordLength - 1u && pData[i] != '=' ) { ++i; }
		if ( i >= iOffset + (size_t)iRecordLength - 1u ) { return false; }
		iKeyLength = (size_t)(pData + i - pKey);
		pValue = pData + i + 1u;
		iValueLength = iOffset + (size_t)iRecordLength - 1u - (i + 1u);
		if ( iKeyLength == 4u && memcmp(pKey, "path", 4u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sPath, pValue, iValueLength) ) { return false; }
		}
		else if ( iKeyLength == 8u && memcmp(pKey, "linkpath", 8u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sLinkPath, pValue, iValueLength) ) { return false; }
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "uname", 5u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sUserName, pValue, iValueLength) ) { return false; }
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "gname", 5u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sGroupName, pValue, iValueLength) ) { return false; }
		}
		else if ( iKeyLength == 4u && memcmp(pKey, "size", 4u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, false, NULL, &pPax->iSize) ) { return false; }
			pPax->bHasSize = true;
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "mtime", 5u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, true, &pPax->iMTime, NULL) ) { return false; }
			pPax->bHasMTime = true;
		}
		else if ( iKeyLength == 3u && memcmp(pKey, "uid", 3u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, false, NULL, &pPax->iUID) ) { return false; }
			pPax->bHasUID = true;
		}
		else if ( iKeyLength == 3u && memcmp(pKey, "gid", 3u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, false, NULL, &pPax->iGID) ) { return false; }
			pPax->bHasGID = true;
		}
		iOffset += (size_t)iRecordLength;
	}
	(void)pReader;
	return true;
}

static xbuffer* __xrtTarReadExtension(xrttarreader* pReader, uint64 iSize)
{
	xbuffer* pData;
	uint64 iPadding = (XRT_TAR_BLOCK_SIZE - (iSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
	if ( iSize > SIZE_MAX ||
		(pReader->Options.tLimits.iMaxItemBytes != 0u &&
		 iSize > pReader->Options.tLimits.iMaxItemBytes) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR extension record exceeds the configured limit");
		return NULL;
	}
	pData = xrtBufferCreate();
	if ( (pData == NULL) || !xrtBufferResize(pData, (size_t)iSize) ) {
		xrtBufferDestroy(pData);
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR extension record");
		return NULL;
	}
	if ( (iSize != 0u && !__xrtTarReadExact(pReader, pData->Data, iSize)) ||
		!__xrtTarSkipBytes(pReader, iPadding) ) {
		xrtBufferDestroy(pData);
		return NULL;
	}
	return pData;
}

static void __xrtTarReaderClearEntry(xrttarreader* pReader)
{
	xrtFree(pReader->Name);
	xrtFree(pReader->LinkName);
	xrtFree(pReader->UserName);
	xrtFree(pReader->GroupName);
	pReader->Name = NULL;
	pReader->LinkName = NULL;
	pReader->UserName = NULL;
	pReader->GroupName = NULL;
	xrtTarEntryInit(&pReader->Entry);
}

static bool __xrtTarApplyPaxString(str* pTarget, str sValue)
{
	str sCopy;
	if ( sValue == NULL ) { return true; }
	sCopy = xrtStrDup(sValue);
	if ( sCopy == NULL ) { return false; }
	xrtFree(*pTarget);
	*pTarget = sCopy;
	return true;
}

static bool __xrtTarHeaderEntry(xrttarreader* pReader, const unsigned char* pBlock,
	xrttarpax* pLocal, str sLongName, str sLongLink)
{
	bool bUstar = memcmp(pBlock + 257u, "ustar", 5u) == 0;
	bool bV7 = pBlock[257u] == 0u && pBlock[258u] == 0u && pBlock[259u] == 0u &&
		pBlock[260u] == 0u && pBlock[261u] == 0u && pBlock[262u] == 0u;
	str sName = __xrtTarFieldString(pBlock, 100u);
	str sPrefix = bUstar ? __xrtTarFieldString(pBlock + 345u, 155u) : NULL;
	str sJoined = NULL;
	uint64 iSize = 0u;
	uint64 iMode = 0u;
	uint64 iUID = 0u;
	uint64 iGID = 0u;
	uint64 iDeviceMajor = 0u;
	uint64 iDeviceMinor = 0u;
	int64 iMTime = 0;
	int iType = __xrtTarType(pBlock[156u]);
	bool bOk;
	if ( !bUstar && !bV7 ) {
		xrtFree(sName);
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
			"unsupported TAR header format");
		return false;
	}
	if ( sPrefix != NULL && sPrefix[0] != 0 ) {
		sJoined = xrtFormat("%s/%s", sPrefix, sName != NULL ? sName : "");
	}
	else { sJoined = xrtStrDup(sName); }
	xrtFree(sName);
	xrtFree(sPrefix);
	bOk = sJoined != NULL &&
		__xrtTarParseNumber(pBlock + 100u, 8u, false, NULL, &iMode) &&
		__xrtTarParseNumber(pBlock + 108u, 8u, false, NULL, &iUID) &&
		__xrtTarParseNumber(pBlock + 116u, 8u, false, NULL, &iGID) &&
		__xrtTarParseNumber(pBlock + 124u, 12u, false, NULL, &iSize) &&
		__xrtTarParseNumber(pBlock + 136u, 12u, true, &iMTime, NULL) &&
		__xrtTarParseNumber(pBlock + 329u, 8u, false, NULL, &iDeviceMajor) &&
		__xrtTarParseNumber(pBlock + 337u, 8u, false, NULL, &iDeviceMinor) &&
		iDeviceMajor <= UINT32_MAX && iDeviceMinor <= UINT32_MAX;
	if ( !bOk ) {
		xrtFree(sJoined);
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
			"invalid TAR header fields");
		return false;
	}
	pReader->Name = sJoined;
	pReader->LinkName = __xrtTarFieldString(pBlock + 157u, 100u);
	pReader->UserName = __xrtTarFieldString(pBlock + 265u, 32u);
	pReader->GroupName = __xrtTarFieldString(pBlock + 297u, 32u);
	if ( !__xrtTarApplyPaxString(&pReader->Name, pReader->GlobalPax.sPath) ||
		!__xrtTarApplyPaxString(&pReader->LinkName, pReader->GlobalPax.sLinkPath) ||
		!__xrtTarApplyPaxString(&pReader->UserName, pReader->GlobalPax.sUserName) ||
		!__xrtTarApplyPaxString(&pReader->GroupName, pReader->GlobalPax.sGroupName) ||
		!__xrtTarApplyPaxString(&pReader->Name, pLocal->sPath) ||
		!__xrtTarApplyPaxString(&pReader->LinkName, pLocal->sLinkPath) ||
		!__xrtTarApplyPaxString(&pReader->UserName, pLocal->sUserName) ||
		!__xrtTarApplyPaxString(&pReader->GroupName, pLocal->sGroupName) ||
		!__xrtTarApplyPaxString(&pReader->Name, sLongName) ||
		!__xrtTarApplyPaxString(&pReader->LinkName, sLongLink) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot copy TAR entry metadata");
		return false;
	}
	if ( pReader->GlobalPax.bHasSize ) { iSize = pReader->GlobalPax.iSize; }
	if ( pReader->GlobalPax.bHasMTime ) { iMTime = pReader->GlobalPax.iMTime; }
	if ( pReader->GlobalPax.bHasUID ) { iUID = pReader->GlobalPax.iUID; }
	if ( pReader->GlobalPax.bHasGID ) { iGID = pReader->GlobalPax.iGID; }
	if ( pLocal->bHasSize ) { iSize = pLocal->iSize; }
	if ( pLocal->bHasMTime ) { iMTime = pLocal->iMTime; }
	if ( pLocal->bHasUID ) { iUID = pLocal->iUID; }
	if ( pLocal->bHasGID ) { iGID = pLocal->iGID; }
	if ( pReader->Name == NULL || pReader->Name[0] == 0 ||
		!xrtUtf8Valid(xrtStrView(pReader->Name), NULL) ||
		(pReader->LinkName != NULL &&
		 !xrtUtf8Valid(xrtStrView(pReader->LinkName), NULL)) ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
			"TAR entry metadata is not valid UTF-8");
		return false;
	}
	if ( pReader->Options.tLimits.iMaxItemBytes != 0u &&
		iSize > pReader->Options.tLimits.iMaxItemBytes ) {
		__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR entry exceeds the configured item limit: %s", pReader->Name);
		return false;
	}
	pReader->Entry.iIndex = pReader->EntryIndex;
	pReader->Entry.iSizeBytes = iSize;
	pReader->Entry.iMTime = iMTime;
	pReader->Entry.iUID = iUID;
	pReader->Entry.iGID = iGID;
	pReader->Entry.iMode = (uint32)iMode;
	pReader->Entry.iDeviceMajor = (uint32)iDeviceMajor;
	pReader->Entry.iDeviceMinor = (uint32)iDeviceMinor;
	pReader->Entry.iType = iType;
	pReader->Entry.sName = (const char*)pReader->Name;
	pReader->Entry.sLinkName = (const char*)pReader->LinkName;
	pReader->Entry.sUserName = (const char*)pReader->UserName;
	pReader->Entry.sGroupName = (const char*)pReader->GroupName;
	pReader->DataRemaining = iSize;
	pReader->DataPadding = (XRT_TAR_BLOCK_SIZE - (iSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
	pReader->HasCurrent = true;
	return true;
}

XRT_API xrttarreader* xrtTarReaderCreate(xreader* pInput, bool bTakeOwnership,
	const xrttaroptions* pOptions)
{
	xrttarreader* pReader = (xrttarreader*)xrtCalloc(1u, sizeof(*pReader));
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	if ( pReader == NULL ) { return NULL; }
	xrtTarErrorInit(&pReader->Error);
	xrtTarEntryInit(&pReader->Entry);
	pReader->EntryIndex = 0u;
	if ( pInput == NULL || !__xrtTarCopyOptions(&pReader->Options, pOptions) ) {
		pReader->Reader = pInput;
		pReader->TakeReader = bTakeOwnership;
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR reader or options");
		pReader->Failed = true;
		return pReader;
	}
	pReader->Reader = pInput;
	pReader->TakeReader = bTakeOwnership;
	if ( pReader->Options.tLimits.iMaxInputBytes != 0u && xrtReaderCanSize(pInput) ) {
		if ( xrtReaderSize(pInput, &iSize) &&
			xrtReaderTell(pInput, &iPosition) && iPosition <= iSize ) {
			if ( iSize - iPosition > pReader->Options.tLimits.iMaxInputBytes ) {
				__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
					"TAR input exceeds the configured limit");
			}
		} else {
			/* 长度或当前位置不可得时，仍由流式读取逐字节执行上限。 */
			xrtClearError();
		}
	}
	return pReader;
}

XRT_API xrttarreader* xrtTarReaderOpenMemory(const void* pData, uint64 iSize,
	const xrttaroptions* pOptions)
{
	xreader* pInput;
	if ( pData == NULL && iSize != 0u ) { return xrtTarReaderCreate(NULL, false, pOptions); }
	if ( iSize > SIZE_MAX ) { return xrtTarReaderCreate(NULL, false, pOptions); }
	pInput = xrtReaderFromMemory((xbytesview){ (const uint8*)pData, (size_t)iSize });
	return xrtTarReaderCreate(pInput, true, pOptions);
}

XRT_API xrttarreader* xrtTarReaderOpenFile(cstr sPath, const xrttaroptions* pOptions)
{
	xreader* pInput = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtTarReaderCreate(pInput, true, pOptions);
}

XRT_API const xrttarerror* xrtTarReaderError(const xrttarreader* pReader)
{
	return pReader != NULL ? &pReader->Error : NULL;
}

XRT_API bool xrtTarReaderSkip(xrttarreader* pReader)
{
	if ( pReader == NULL || pReader->Failed ) { return false; }
	if ( !pReader->HasCurrent ) { return true; }
	if ( !__xrtTarSkipBytes(pReader, pReader->DataRemaining) ||
		!__xrtTarSkipBytes(pReader, pReader->DataPadding) ) { return false; }
	pReader->DataRemaining = 0u;
	pReader->DataPadding = 0u;
	pReader->HasCurrent = false;
	return true;
}

XRT_API bool xrtTarReaderNext(xrttarreader* pReader, xrttarentry* pEntry)
{
	unsigned char sHeader[XRT_TAR_BLOCK_SIZE];
	xrttarpax tLocal;
	str sLongName = NULL;
	str sLongLink = NULL;
	bool bResult = false;
	memset(&tLocal, 0, sizeof(tLocal));
	if ( pReader == NULL || pReader->Failed || pReader->EOFReached ) { return false; }
	if ( pEntry == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry output is null");
		return false;
	}
	if ( !xrtTarReaderSkip(pReader) ) { return false; }
	__xrtTarReaderClearEntry(pReader);
	for ( ;; ) {
		xbuffer* pExtension;
		uint64 iSize = 0u;
		if ( !__xrtTarReadExact(pReader, sHeader, sizeof(sHeader)) ) { break; }
		if ( __xrtTarBlockZero(sHeader) ) {
			if ( !__xrtTarReadExact(pReader, sHeader, sizeof(sHeader)) ||
				!__xrtTarBlockZero(sHeader) ) {
				if ( !pReader->Failed ) {
					__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
						"TAR end marker is incomplete");
				}
				break;
			}
			pReader->EOFReached = true;
			break;
		}
		if ( !__xrtTarChecksumValid(sHeader) ||
			!__xrtTarParseNumber(sHeader + 124u, 12u, false, NULL, &iSize) ) {
			__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
				"invalid TAR header checksum or size");
			break;
		}
		if ( sHeader[156u] == 'x' || sHeader[156u] == 'g' ||
			sHeader[156u] == 'L' || sHeader[156u] == 'K' ) {
			pExtension = __xrtTarReadExtension(pReader, iSize);
			if ( pExtension == NULL ) { break; }
			if ( sHeader[156u] == 'x' ) {
				__xrtTarPaxClear(&tLocal);
				bResult = __xrtTarParsePax(pReader, &tLocal,
					(const unsigned char*)pExtension->Data, pExtension->Size);
			}
			else if ( sHeader[156u] == 'g' ) {
				bResult = __xrtTarParsePax(pReader, &pReader->GlobalPax,
					(const unsigned char*)pExtension->Data, pExtension->Size);
			}
			else if ( sHeader[156u] == 'L' ) {
				bResult = __xrtTarSetGnuString(&sLongName,
					(const unsigned char*)pExtension->Data, pExtension->Size);
			}
			else {
				bResult = __xrtTarSetGnuString(&sLongLink,
					(const unsigned char*)pExtension->Data, pExtension->Size);
			}
			xrtBufferDestroy(pExtension);
			if ( !bResult ) {
				__xrtTarFailReader(pReader, XRT_TAR_ERROR_FORMAT,
					"invalid TAR PAX or GNU extension record");
				break;
			}
			continue;
		}
		if ( pReader->Options.tLimits.iMaxEntries != 0u &&
			pReader->EntryIndex >= pReader->Options.tLimits.iMaxEntries ) {
			__xrtTarFailReader(pReader, XRT_TAR_ERROR_LIMIT,
				"TAR archive contains too many entries");
			break;
		}
		bResult = __xrtTarHeaderEntry(pReader, sHeader, &tLocal, sLongName, sLongLink);
		if ( bResult ) {
			*pEntry = pReader->Entry;
			pReader->EntryIndex += 1u;
		}
		break;
	}
	__xrtTarPaxClear(&tLocal);
	xrtFree(sLongName);
	xrtFree(sLongLink);
	return bResult;
}

XRT_API bool xrtTarReaderEOF(const xrttarreader* pReader)
{
	return pReader != NULL && pReader->EOFReached && !pReader->Failed;
}

XRT_API xbuffer* xrtTarReaderRead(xrttarreader* pReader)
{
	xbuffer* pOutput;
	uint64 iSize;
	if ( pReader == NULL || pReader->Failed ) { return NULL; }
	if ( !pReader->HasCurrent ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_STATE,
			"TAR reader has no current entry");
		return NULL;
	}
	iSize = pReader->DataRemaining;
	if ( pReader->Entry.iType != XRT_TAR_ENTRY_FILE ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_STATE,
			"current TAR entry is not a regular file");
		return NULL;
	}
	if ( iSize > SIZE_MAX ||
		(pReader->Options.tLimits.iMaxOutputBytes != 0u &&
		 (iSize > pReader->Options.tLimits.iMaxOutputBytes ||
		  pReader->TotalOutput > pReader->Options.tLimits.iMaxOutputBytes - iSize)) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR entry output exceeds the configured limit");
		return NULL;
	}
	pOutput = xrtBufferCreate();
	if ( (pOutput == NULL) || !xrtBufferResize(pOutput, (size_t)iSize) ) {
		xrtBufferDestroy(pOutput);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR entry output");
		return NULL;
	}
	if ( (iSize != 0u && !__xrtTarReadExact(pReader, pOutput->Data, iSize)) ||
		!__xrtTarSkipBytes(pReader, pReader->DataPadding) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	pReader->DataRemaining = 0u;
	pReader->DataPadding = 0u;
	pReader->HasCurrent = false;
	pReader->TotalOutput += iSize;
	return pOutput;
}

XRT_API bool xrtTarReaderReadTo(xrttarreader* pReader, xwriter* pWriter)
{
	unsigned char sChunk[65536];
	uint64 iTotal;
	if ( pReader == NULL || pReader->Failed ) { return false; }
	if ( pWriter == NULL ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"TAR entry output Writer is null");
		return false;
	}
	if ( !pReader->HasCurrent ||
		(pReader->Entry.iType != XRT_TAR_ENTRY_FILE) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_STATE,
			"current TAR entry is not a regular file");
		return false;
	}
	iTotal = pReader->DataRemaining;
	if ( pReader->Options.tLimits.iMaxOutputBytes != 0u &&
		(iTotal > pReader->Options.tLimits.iMaxOutputBytes ||
		 pReader->TotalOutput > pReader->Options.tLimits.iMaxOutputBytes - iTotal) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR output exceeds the configured limit");
		return false;
	}
	while ( pReader->DataRemaining != 0u ) {
		uint64 iChunk = pReader->DataRemaining < sizeof(sChunk) ?
			pReader->DataRemaining : sizeof(sChunk);
		if ( !__xrtTarReadExact(pReader, sChunk, iChunk) ||
			!xrtWriterWriteFull(pWriter, sChunk, (size_t)iChunk, NULL) ) {
			if ( !pReader->Failed ) {
				__xrtTarFailReader(pReader, __xrtTarIoFailureCode(),
					"cannot copy TAR entry data");
			}
			return false;
		}
		pReader->DataRemaining -= iChunk;
	}
	if ( !__xrtTarSkipBytes(pReader, pReader->DataPadding) ) { return false; }
	pReader->DataPadding = 0u;
	pReader->HasCurrent = false;
	pReader->TotalOutput += iTotal;
	return true;
}

XRT_API void xrtTarReaderDestroy(xrttarreader* pReader)
{
	if ( pReader == NULL ) { return; }
	if ( pReader->TakeReader && pReader->Reader != NULL ) { xrtReaderDestroy(pReader->Reader); }
	__xrtTarReaderClearEntry(pReader);
	__xrtTarPaxClear(&pReader->GlobalPax);
	xrtFree(pReader);
}
