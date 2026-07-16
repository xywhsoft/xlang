#include "xrt_tar_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>



static bool __xrtTarBlockZero(const unsigned char* pBlock)
{
	size_t i;
	for ( i = 0u; i < XRT_TAR_BLOCK_SIZE; ++i ) {
		if ( pBlock[i] != 0u ) { return FALSE; }
	}
	return TRUE;
}

static str __xrtTarFieldString(const unsigned char* pField, size_t iLength)
{
	size_t iSize = 0u;
	while ( iSize < iLength && pField[iSize] != 0u ) { ++iSize; }
	while ( iSize != 0u && pField[iSize - 1u] == ' ' ) { --iSize; }
	return xrtCopyStr((str)pField, iSize);
}

static bool __xrtTarParseBinary(const unsigned char* pField, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	bool bNegative = (pField[0] & 0x40u) != 0u;
	size_t iPrefix = iLength > 8u ? iLength - 8u : 0u;
	uint64 iValue = 0u;
	size_t i;
	if ( bNegative && !bSigned ) { return FALSE; }
	for ( i = 0u; i < iPrefix; ++i ) {
		unsigned char iExpected = bNegative ? 0xffu : 0u;
		unsigned char iByte = i == 0u ? (unsigned char)(pField[i] & 0x7fu) : pField[i];
		if ( i == 0u && bNegative ) { iExpected = 0x7fu; }
		if ( iByte != iExpected ) { return FALSE; }
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
			if ( iValue > INT64_MAX ) { return FALSE; }
			*pSigned = (int64)iValue;
		}
	}
	return TRUE;
}

static bool __xrtTarParseNumber(const unsigned char* pField, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	uint64 iValue = 0u;
	size_t i = 0u;
	bool bDigit = FALSE;
	if ( iLength == 0u ) { return FALSE; }
	if ( (pField[0] & 0x80u) != 0u ) {
		return __xrtTarParseBinary(pField, iLength, bSigned, pSigned, pUnsigned);
	}
	while ( i < iLength && (pField[i] == ' ' || pField[i] == 0u) ) { ++i; }
	for ( ; i < iLength; ++i ) {
		unsigned char c = pField[i];
		if ( c == 0u || c == ' ' ) { break; }
		if ( c < '0' || c > '7' || iValue > (UINT64_MAX - (c - '0')) / 8u ) { return FALSE; }
		iValue = iValue * 8u + (uint64)(c - '0');
		bDigit = TRUE;
	}
	if ( !bDigit ) { iValue = 0u; }
	while ( i < iLength ) {
		if ( pField[i] != 0u && pField[i] != ' ' ) { return FALSE; }
		++i;
	}
	if ( pUnsigned != NULL ) { *pUnsigned = iValue; }
	if ( pSigned != NULL ) {
		if ( iValue > INT64_MAX ) { return FALSE; }
		*pSigned = (int64)iValue;
	}
	return TRUE;
}

static bool __xrtTarChecksumValid(const unsigned char* pBlock)
{
	uint64 iStored = 0u;
	uint64 iUnsigned = 0u;
	int64 iSigned = 0;
	size_t i;
	if ( !__xrtTarParseNumber(pBlock + 148u, 8u, FALSE, NULL, &iStored) ) { return FALSE; }
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
	while ( iLength != 0u && (pValue[iLength - 1u] == 0u || pValue[iLength - 1u] == '\n') ) {
		--iLength;
	}
	if ( !xrtIsUTF8((str)pValue, iLength) ) { return FALSE; }
	sValue = xrtCopyStr((str)pValue, iLength);
	if ( sValue == NULL ) { return FALSE; }
	xrtFree(*pTarget);
	*pTarget = sValue;
	return TRUE;
}

static bool __xrtTarPaxNumber(const unsigned char* pValue, size_t iLength,
	bool bSigned, int64* pSigned, uint64* pUnsigned)
{
	char sNumber[96];
	char* pEnd;
	if ( iLength == 0u || iLength >= sizeof(sNumber) ) { return FALSE; }
	memcpy(sNumber, pValue, iLength);
	sNumber[iLength] = 0;
	errno = 0;
	if ( bSigned ) {
		long long iValue = strtoll(sNumber, &pEnd, 10);
		if ( errno != 0 || pEnd == sNumber ) { return FALSE; }
		if ( *pEnd == '.' ) {
			++pEnd;
			while ( *pEnd >= '0' && *pEnd <= '9' ) { ++pEnd; }
		}
		if ( *pEnd != 0 ) { return FALSE; }
		if ( pSigned != NULL ) { *pSigned = (int64)iValue; }
		return TRUE;
	}
	else {
		unsigned long long iValue = strtoull(sNumber, &pEnd, 10);
		if ( sNumber[0] == '-' || errno != 0 || pEnd == sNumber || *pEnd != 0 ) { return FALSE; }
		if ( pUnsigned != NULL ) { *pUnsigned = (uint64)iValue; }
		return TRUE;
	}
}

static bool __xrtTarParsePax(xrttarreader* pReader, xrttarpax* pPax,
	const unsigned char* pData, size_t iSize)
{
	size_t iOffset = 0u;
	uint64 iRecords = 0u;
	while ( iOffset < iSize ) {
		size_t i = iOffset;
		uint64 iRecordLength = 0u;
		const unsigned char* pKey;
		const unsigned char* pValue;
		size_t iKeyLength;
		size_t iValueLength;
		if ( pReader->Options.tLimits.iMaxNodes != 0u &&
			iRecords >= pReader->Options.tLimits.iMaxNodes ) { return FALSE; }
		iRecords += 1u;
		while ( i < iSize && pData[i] >= '0' && pData[i] <= '9' ) {
			if ( iRecordLength > (UINT64_MAX - (pData[i] - '0')) / 10u ) { return FALSE; }
			iRecordLength = iRecordLength * 10u + (uint64)(pData[i] - '0');
			++i;
		}
		if ( i == iOffset || i >= iSize || pData[i] != ' ' ||
			iRecordLength > iSize - iOffset || iRecordLength < 4u ) { return FALSE; }
		if ( pData[iOffset + (size_t)iRecordLength - 1u] != '\n' ) { return FALSE; }
		pKey = pData + ++i;
		while ( i < iOffset + (size_t)iRecordLength - 1u && pData[i] != '=' ) { ++i; }
		if ( i >= iOffset + (size_t)iRecordLength - 1u ) { return FALSE; }
		iKeyLength = (size_t)(pData + i - pKey);
		pValue = pData + i + 1u;
		iValueLength = iOffset + (size_t)iRecordLength - 1u - (i + 1u);
		if ( iKeyLength == 4u && memcmp(pKey, "path", 4u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sPath, pValue, iValueLength) ) { return FALSE; }
		}
		else if ( iKeyLength == 8u && memcmp(pKey, "linkpath", 8u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sLinkPath, pValue, iValueLength) ) { return FALSE; }
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "uname", 5u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sUserName, pValue, iValueLength) ) { return FALSE; }
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "gname", 5u) == 0 ) {
			if ( !__xrtTarSetString(&pPax->sGroupName, pValue, iValueLength) ) { return FALSE; }
		}
		else if ( iKeyLength == 4u && memcmp(pKey, "size", 4u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, FALSE, NULL, &pPax->iSize) ) { return FALSE; }
			pPax->bHasSize = TRUE;
		}
		else if ( iKeyLength == 5u && memcmp(pKey, "mtime", 5u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, TRUE, &pPax->iMTime, NULL) ) { return FALSE; }
			pPax->bHasMTime = TRUE;
		}
		else if ( iKeyLength == 3u && memcmp(pKey, "uid", 3u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, FALSE, NULL, &pPax->iUID) ) { return FALSE; }
			pPax->bHasUID = TRUE;
		}
		else if ( iKeyLength == 3u && memcmp(pKey, "gid", 3u) == 0 ) {
			if ( !__xrtTarPaxNumber(pValue, iValueLength, FALSE, NULL, &pPax->iGID) ) { return FALSE; }
			pPax->bHasGID = TRUE;
		}
		iOffset += (size_t)iRecordLength;
	}
	(void)pReader;
	return TRUE;
}

static xbuffer __xrtTarReadExtension(xrttarreader* pReader, uint64 iSize)
{
	xbuffer pData;
	uint64 iPadding = (XRT_TAR_BLOCK_SIZE - (iSize % XRT_TAR_BLOCK_SIZE)) % XRT_TAR_BLOCK_SIZE;
	if ( iSize > UINT32_MAX ||
		(pReader->Options.tLimits.iMaxItemBytes != 0u &&
		 iSize > pReader->Options.tLimits.iMaxItemBytes) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR extension record exceeds the configured limit");
		return NULL;
	}
	pData = xrtBufferCreate(iSize != 0u ? (uint32)iSize : 1u);
	if ( pData == NULL || !xrtBufferMalloc(pData, (uint32)iSize) ) {
		xrtBufferDestroy(pData);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR extension record");
		return NULL;
	}
	pData->Length = (uint32)iSize;
	if ( (iSize != 0u && !__xrtTarReadExact(pReader, pData->Buffer, iSize)) ||
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
	if ( sValue == NULL ) { return TRUE; }
	sCopy = xrtCopyStr(sValue, 0u);
	if ( sCopy == NULL ) { return FALSE; }
	xrtFree(*pTarget);
	*pTarget = sCopy;
	return TRUE;
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
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"unsupported TAR header format");
		return FALSE;
	}
	if ( sPrefix != NULL && sPrefix[0] != 0 ) {
		str sTemp = xrtFormat("%s/%s", sPrefix, sName != NULL ? sName : (str)"");
		sJoined = xrtCopyStr(sTemp, 0u);
	}
	else { sJoined = xrtCopyStr(sName, 0u); }
	xrtFree(sName);
	xrtFree(sPrefix);
	bOk = sJoined != NULL &&
		__xrtTarParseNumber(pBlock + 100u, 8u, FALSE, NULL, &iMode) &&
		__xrtTarParseNumber(pBlock + 108u, 8u, FALSE, NULL, &iUID) &&
		__xrtTarParseNumber(pBlock + 116u, 8u, FALSE, NULL, &iGID) &&
		__xrtTarParseNumber(pBlock + 124u, 12u, FALSE, NULL, &iSize) &&
		__xrtTarParseNumber(pBlock + 136u, 12u, TRUE, &iMTime, NULL) &&
		__xrtTarParseNumber(pBlock + 329u, 8u, FALSE, NULL, &iDeviceMajor) &&
		__xrtTarParseNumber(pBlock + 337u, 8u, FALSE, NULL, &iDeviceMinor) &&
		iDeviceMajor <= UINT32_MAX && iDeviceMinor <= UINT32_MAX;
	if ( !bOk ) {
		xrtFree(sJoined);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"invalid TAR header fields");
		return FALSE;
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
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot copy TAR entry metadata");
		return FALSE;
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
		!xrtIsUTF8(pReader->Name, strlen((const char*)pReader->Name)) ||
		(pReader->LinkName != NULL && !xrtIsUTF8(pReader->LinkName,
		 strlen((const char*)pReader->LinkName))) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
			"TAR entry metadata is not valid UTF-8");
		return FALSE;
	}
	if ( pReader->Options.tLimits.iMaxItemBytes != 0u &&
		iSize > pReader->Options.tLimits.iMaxItemBytes ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR entry exceeds the configured item limit: %s", pReader->Name);
		return FALSE;
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
	pReader->HasCurrent = TRUE;
	return TRUE;
}

XXAPI xrttarreader* xrtTarReaderCreate(xreader pInput, bool bTakeOwnership,
	const xrttaroptions* pOptions)
{
	xrttarreader* pReader = (xrttarreader*)xrtCalloc(1u, sizeof(*pReader));
	uint64 iSize = 0u;
	if ( pReader == NULL ) { return NULL; }
	xrtTarErrorInit(&pReader->Error);
	xrtTarEntryInit(&pReader->Entry);
	pReader->EntryIndex = 0u;
	if ( pInput == NULL || !__xrtTarCopyOptions(&pReader->Options, pOptions) ) {
		pReader->Reader = pInput;
		pReader->TakeReader = bTakeOwnership;
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_ARGUMENT,
			"invalid TAR reader or options");
		return pReader;
	}
	pReader->Reader = pInput;
	pReader->TakeReader = bTakeOwnership;
	if ( xrtReaderSize(pInput, &iSize) && pReader->Options.tLimits.iMaxInputBytes != 0u &&
		iSize > pReader->Options.tLimits.iMaxInputBytes ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR input exceeds the configured limit");
	}
	return pReader;
}

XXAPI xrttarreader* xrtTarReaderOpenMemory(const void* pData, uint64 iSize,
	const xrttaroptions* pOptions)
{
	xreader pInput;
	if ( pData == NULL && iSize != 0u ) { return xrtTarReaderCreate(NULL, FALSE, pOptions); }
	pInput = xrtReaderFromMemory(pData, iSize);
	return xrtTarReaderCreate(pInput, TRUE, pOptions);
}

XXAPI xrttarreader* xrtTarReaderOpenFile(str sPath, const xrttaroptions* pOptions)
{
	xreader pInput = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtTarReaderCreate(pInput, TRUE, pOptions);
}

XXAPI const xrttarerror* xrtTarReaderError(const xrttarreader* pReader)
{
	return pReader != NULL ? &pReader->Error : NULL;
}

XXAPI bool xrtTarReaderSkip(xrttarreader* pReader)
{
	if ( pReader == NULL || pReader->Failed ) { return FALSE; }
	if ( !pReader->HasCurrent ) { return TRUE; }
	if ( !__xrtTarSkipBytes(pReader, pReader->DataRemaining) ||
		!__xrtTarSkipBytes(pReader, pReader->DataPadding) ) { return FALSE; }
	pReader->DataRemaining = 0u;
	pReader->DataPadding = 0u;
	pReader->HasCurrent = FALSE;
	return TRUE;
}

XXAPI bool xrtTarReaderNext(xrttarreader* pReader, xrttarentry* pEntry)
{
	unsigned char sHeader[XRT_TAR_BLOCK_SIZE];
	xrttarpax tLocal;
	str sLongName = NULL;
	str sLongLink = NULL;
	bool bResult = FALSE;
	memset(&tLocal, 0, sizeof(tLocal));
	if ( pReader == NULL || pEntry == NULL || pReader->Failed || pReader->EOFReached ||
		!xrtTarReaderSkip(pReader) ) { return FALSE; }
	__xrtTarReaderClearEntry(pReader);
	for ( ;; ) {
		xbuffer pExtension;
		uint64 iSize = 0u;
		if ( !__xrtTarReadExact(pReader, sHeader, sizeof(sHeader)) ) { break; }
		if ( __xrtTarBlockZero(sHeader) ) {
			if ( !__xrtTarReadExact(pReader, sHeader, sizeof(sHeader)) ||
				!__xrtTarBlockZero(sHeader) ) {
				if ( !pReader->Failed ) {
					__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
						"TAR end marker is incomplete");
				}
				break;
			}
			pReader->EOFReached = TRUE;
			break;
		}
		if ( !__xrtTarChecksumValid(sHeader) ||
			!__xrtTarParseNumber(sHeader + 124u, 12u, FALSE, NULL, &iSize) ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
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
					(const unsigned char*)pExtension->Buffer, pExtension->Length);
			}
			else if ( sHeader[156u] == 'g' ) {
				bResult = __xrtTarParsePax(pReader, &pReader->GlobalPax,
					(const unsigned char*)pExtension->Buffer, pExtension->Length);
			}
			else if ( sHeader[156u] == 'L' ) {
				bResult = __xrtTarSetString(&sLongName,
					(const unsigned char*)pExtension->Buffer, pExtension->Length);
			}
			else {
				bResult = __xrtTarSetString(&sLongLink,
					(const unsigned char*)pExtension->Buffer, pExtension->Length);
			}
			xrtBufferDestroy(pExtension);
			if ( !bResult ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_FORMAT,
					"invalid TAR PAX or GNU extension record");
				break;
			}
			continue;
		}
		if ( pReader->Options.tLimits.iMaxEntries != 0u &&
			pReader->EntryIndex >= pReader->Options.tLimits.iMaxEntries ) {
			__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
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

XXAPI bool xrtTarReaderEOF(const xrttarreader* pReader)
{
	return pReader != NULL && pReader->EOFReached && !pReader->Failed;
}

XXAPI xbuffer xrtTarReaderRead(xrttarreader* pReader)
{
	xbuffer pOutput;
	uint64 iSize;
	if ( pReader == NULL || pReader->Failed || !pReader->HasCurrent ) { return NULL; }
	iSize = pReader->DataRemaining;
	if ( pReader->Entry.iType != XRT_TAR_ENTRY_FILE ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_STATE,
			"current TAR entry is not a regular file");
		return NULL;
	}
	if ( iSize > UINT32_MAX ||
		(pReader->Options.tLimits.iMaxOutputBytes != 0u &&
		 (iSize > pReader->Options.tLimits.iMaxOutputBytes ||
		  pReader->TotalOutput > pReader->Options.tLimits.iMaxOutputBytes - iSize)) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR entry output exceeds the configured limit");
		return NULL;
	}
	pOutput = xrtBufferCreate(iSize != 0u ? (uint32)iSize : 1u);
	if ( pOutput == NULL || !xrtBufferMalloc(pOutput, (uint32)iSize) ) {
		xrtBufferDestroy(pOutput);
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_MEMORY,
			"cannot allocate TAR entry output");
		return NULL;
	}
	pOutput->Length = (uint32)iSize;
	if ( (iSize != 0u && !__xrtTarReadExact(pReader, pOutput->Buffer, iSize)) ||
		!__xrtTarSkipBytes(pReader, pReader->DataPadding) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	pReader->DataRemaining = 0u;
	pReader->DataPadding = 0u;
	pReader->HasCurrent = FALSE;
	pReader->TotalOutput += iSize;
	return pOutput;
}

XXAPI bool xrtTarReaderCopy(xrttarreader* pReader, xwriter pWriter)
{
	unsigned char sChunk[65536];
	uint64 iTotal;
	if ( pReader == NULL || pWriter == NULL || pReader->Failed || !pReader->HasCurrent ||
		pReader->Entry.iType != XRT_TAR_ENTRY_FILE ) { return FALSE; }
	iTotal = pReader->DataRemaining;
	if ( pReader->Options.tLimits.iMaxOutputBytes != 0u &&
		(iTotal > pReader->Options.tLimits.iMaxOutputBytes ||
		 pReader->TotalOutput > pReader->Options.tLimits.iMaxOutputBytes - iTotal) ) {
		__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_LIMIT,
			"TAR output exceeds the configured limit");
		return FALSE;
	}
	while ( pReader->DataRemaining != 0u ) {
		uint64 iChunk = pReader->DataRemaining < sizeof(sChunk) ?
			pReader->DataRemaining : sizeof(sChunk);
		if ( !__xrtTarReadExact(pReader, sChunk, iChunk) ||
			!xrtWriterWriteAll(pWriter, sChunk, iChunk) ) {
			if ( !pReader->Failed ) {
				__xrtTarSetReaderError(pReader, XRT_TAR_ERROR_IO,
					"cannot copy TAR entry data");
			}
			return FALSE;
		}
		pReader->DataRemaining -= iChunk;
	}
	if ( !__xrtTarSkipBytes(pReader, pReader->DataPadding) ) { return FALSE; }
	pReader->DataPadding = 0u;
	pReader->HasCurrent = FALSE;
	pReader->TotalOutput += iTotal;
	return TRUE;
}

XXAPI void xrtTarReaderDestroy(xrttarreader* pReader)
{
	if ( pReader == NULL ) { return; }
	if ( pReader->TakeReader && pReader->Reader != NULL ) { xrtReaderDestroy(pReader->Reader); }
	__xrtTarReaderClearEntry(pReader);
	__xrtTarPaxClear(&pReader->GlobalPax);
	xrtFree(pReader);
}
