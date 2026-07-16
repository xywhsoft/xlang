#include "xrt_ini_internal.h"

#include <string.h>



static void __xrtIniTrim(const unsigned char* pData, size_t* pStart, size_t* pEnd)
{
	while ( *pStart < *pEnd && (pData[*pStart] == ' ' || pData[*pStart] == '\t') ) {
		*pStart += 1u;
	}
	while ( *pEnd > *pStart && (pData[*pEnd - 1u] == ' ' || pData[*pEnd - 1u] == '\t') ) {
		*pEnd -= 1u;
	}
}

static str __xrtIniCopyRange(const unsigned char* pData, size_t iStart, size_t iEnd)
{
	return iEnd > iStart ? xrtCopyStr((str)(pData + iStart), iEnd - iStart) :
		xrtCopyStr((str)"", 0u);
}

static bool __xrtIniAppendPending(xrtiniconfig* pConfig, xbuffer pPending,
	const unsigned char* pData, size_t iStart, size_t iEnd)
{
	uint64 iAdded = (uint64)(iEnd - iStart) + 1u;
	if ( pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
		(iAdded > pConfig->Options.tLimits.iMaxOutputBytes ||
		 pPending->Length > pConfig->Options.tLimits.iMaxOutputBytes - iAdded) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, iStart,
			"INI preserved comment data exceeds the configured limit");
		return FALSE;
	}
	if ( iEnd > iStart && !xrtBufferAppend(pPending, (ptr)(pData + iStart),
		(uint32)(iEnd - iStart), XBUF_BINARY) ) { return FALSE; }
	return xrtBufferAppend(pPending, "\n", 1u, XBUF_BINARY);
}

static str __xrtIniTakePending(xbuffer pPending)
{
	str sResult = pPending->Length != 0u ?
		xrtCopyStr((str)pPending->Buffer, pPending->Length) : xrtCopyStr((str)"", 0u);
	pPending->Length = 0u;
	return sResult;
}

static bool __xrtIniAppendContinuation(xrtiniconfig* pConfig, xrt_ini_entry* pEntry,
	const unsigned char* pData, size_t iStart, size_t iEnd, uint64 iLine)
{
	size_t iOld = strlen((const char*)pEntry->Value);
	size_t iPart = iEnd - iStart;
	str sValue;
	if ( pConfig->Options.tLimits.iMaxItemBytes != 0u &&
		((uint64)iOld + 1u + (uint64)iPart > pConfig->Options.tLimits.iMaxItemBytes) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, iStart,
			"INI multiline value exceeds the configured item limit");
		return FALSE;
	}
	sValue = (str)xrtMalloc(iOld + 1u + iPart + 1u);
	if ( sValue == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iStart,
			"cannot grow INI multiline value");
		return FALSE;
	}
	memcpy(sValue, pEntry->Value, iOld);
	sValue[iOld] = '\n';
	if ( iPart != 0u ) { memcpy(sValue + iOld + 1u, pData + iStart, iPart); }
	sValue[iOld + 1u + iPart] = 0;
	xrtFree(pEntry->Value);
	pEntry->Value = sValue;
	return TRUE;
}

static bool __xrtIniParseBuffer(xrtiniconfig* pConfig, const unsigned char* pData,
	size_t iSize)
{
	xbuffer pPending = xrtBufferCreate(256u);
	xrt_ini_section* pSection = NULL;
	xrt_ini_entry* pLastEntry = NULL;
	size_t iOffset = 0u;
	uint64 iLine = 1u;
	if ( pPending == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI parser state");
		return FALSE;
	}
	if ( iSize >= 3u && pData[0] == 0xefu && pData[1] == 0xbbu && pData[2] == 0xbfu ) {
		iOffset = 3u;
	}
	while ( iOffset < iSize ) {
		size_t iLineStart = iOffset;
		size_t iLineEnd;
		size_t iStart;
		size_t iEnd;
		bool bIndented;
		while ( iOffset < iSize && pData[iOffset] != '\r' && pData[iOffset] != '\n' ) {
			iOffset += 1u;
		}
		iLineEnd = iOffset;
		if ( iOffset < iSize && pData[iOffset] == '\r' ) { iOffset += 1u; }
		if ( iOffset < iSize && pData[iOffset] == '\n' ) { iOffset += 1u; }
		iStart = iLineStart;
		iEnd = iLineEnd;
		bIndented = iStart < iEnd && (pData[iStart] == ' ' || pData[iStart] == '\t');
		__xrtIniTrim(pData, &iStart, &iEnd);

		if ( iStart == iEnd || pData[iStart] == ';' || pData[iStart] == '#' ) {
			if ( !__xrtIniAppendPending(pConfig, pPending, pData, iLineStart, iLineEnd) ) {
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			pLastEntry = NULL;
			iLine += 1u;
			continue;
		}

		if ( bIndented && pLastEntry != NULL && pPending->Length == 0u ) {
			if ( !__xrtIniAppendContinuation(pConfig, pLastEntry, pData,
				iStart, iEnd, iLine) ) {
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			iLine += 1u;
			continue;
		}

		if ( pData[iStart] == '[' ) {
			str sName;
			str sLeading;
			size_t iClose = iEnd;
			if ( iEnd <= iStart + 2u || pData[iEnd - 1u] != ']' ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, iLine, 1u, iLineStart,
					"invalid INI section header");
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			iStart += 1u;
			iClose -= 1u;
			__xrtIniTrim(pData, &iStart, &iClose);
			if ( iStart == iClose ||
				(pConfig->Options.tLimits.iMaxItemBytes != 0u &&
				 (uint64)(iClose - iStart) > pConfig->Options.tLimits.iMaxItemBytes) ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, iLineStart,
					"empty or oversized INI section name");
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			sName = __xrtIniCopyRange(pData, iStart, iClose);
			sLeading = __xrtIniTakePending(pPending);
			if ( sName == NULL || sLeading == NULL ) {
				xrtFree(sName);
				xrtFree(sLeading);
				__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iLineStart,
					"cannot copy INI section header");
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			pSection = __xrtIniAddSection(pConfig, (const char*)sName,
				(const char*)sLeading, iLine, TRUE);
			xrtFree(sName);
			xrtFree(sLeading);
			if ( pSection == NULL ) { xrtBufferDestroy(pPending); return FALSE; }
			pLastEntry = NULL;
			iLine += 1u;
			continue;
		}

		{
			size_t iDelimiter = iStart;
			size_t iKeyStart = iStart;
			size_t iKeyEnd;
			size_t iValueStart;
			size_t iValueEnd = iEnd;
			str sKey;
			str sValue;
			str sLeading;
			while ( iDelimiter < iEnd && pData[iDelimiter] != '=' && pData[iDelimiter] != ':' ) {
				iDelimiter += 1u;
			}
			if ( iDelimiter == iEnd ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, iLine, 1u, iLineStart,
					"INI entry must contain = or :");
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			iKeyEnd = iDelimiter;
			iValueStart = iDelimiter + 1u;
			__xrtIniTrim(pData, &iKeyStart, &iKeyEnd);
			__xrtIniTrim(pData, &iValueStart, &iValueEnd);
			if ( iKeyStart == iKeyEnd ||
				(pConfig->Options.tLimits.iMaxItemBytes != 0u &&
				 ((uint64)(iKeyEnd - iKeyStart) > pConfig->Options.tLimits.iMaxItemBytes ||
				  (uint64)(iValueEnd - iValueStart) > pConfig->Options.tLimits.iMaxItemBytes)) ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, iLine, 1u, iLineStart,
					"empty or oversized INI key/value");
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			if ( pSection == NULL ) {
				sLeading = __xrtIniTakePending(pPending);
				pSection = __xrtIniAddSection(pConfig, "", (const char*)sLeading,
					iLine, FALSE);
				xrtFree(sLeading);
				if ( pSection == NULL ) { xrtBufferDestroy(pPending); return FALSE; }
			}
			sKey = __xrtIniCopyRange(pData, iKeyStart, iKeyEnd);
			sValue = __xrtIniCopyRange(pData, iValueStart, iValueEnd);
			sLeading = __xrtIniTakePending(pPending);
			if ( sKey == NULL || sValue == NULL || sLeading == NULL ||
				!__xrtIniAddEntry(pConfig, pSection, (const char*)sKey,
					(const char*)sValue, (const char*)sLeading, iLine, TRUE) ) {
				xrtFree(sKey);
				xrtFree(sValue);
				xrtFree(sLeading);
				if ( !pConfig->Failed ) {
					__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iLineStart,
						"cannot copy INI entry");
				}
				xrtBufferDestroy(pPending);
				return FALSE;
			}
			xrtFree(sKey);
			xrtFree(sValue);
			xrtFree(sLeading);
			pLastEntry = &pSection->Entries[pSection->Count - 1u];
		}
		iLine += 1u;
	}
	if ( pPending->Length != 0u ) {
		pConfig->Trailing = __xrtIniTakePending(pPending);
		if ( pConfig->Trailing == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iSize,
				"cannot copy trailing INI comments");
			xrtBufferDestroy(pPending);
			return FALSE;
		}
	}
	xrtBufferDestroy(pPending);
	return TRUE;
}



XXAPI xrtiniconfig* xrtIniParse(xreader pReader, bool bTakeOwnership,
	const xrtinioptions* pOptions)
{
	xrtiniconfig* pConfig = xrtIniCreate(pOptions);
	xbuffer pData = NULL;
	uint64 iSize = 0u;
	uint64 iLimit;
	if ( pConfig == NULL ) {
		if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
		return NULL;
	}
	if ( pConfig->Failed || pReader == NULL ) {
		if ( !pConfig->Failed ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_ARGUMENT, 0u, 0u, 0u,
				"invalid INI reader");
		}
		if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
		return pConfig;
	}
	iLimit = pConfig->Options.tLimits.iMaxInputBytes;
	if ( xrtReaderSize(pReader, &iSize) && iLimit != 0u && iSize > iLimit ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"INI input exceeds the configured limit");
	} else {
		pData = xrtReaderReadAll(pReader, iLimit);
		if ( pData == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_IO, 0u, 0u, 0u,
				"cannot read INI stream or input limit was exceeded");
		} else {
			pConfig->InputBytes = pData->Length;
			if ( !xrtIsUTF8((str)pData->Buffer, pData->Length) ) {
				__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, 1u, 1u, 0u,
					"INI input is not valid UTF-8");
			} else {
				__xrtIniParseBuffer(pConfig, (const unsigned char*)pData->Buffer,
					pData->Length);
			}
		}
	}
	xrtBufferDestroy(pData);
	if ( bTakeOwnership ) { xrtReaderDestroy(pReader); }
	return pConfig;
}

XXAPI xrtiniconfig* xrtIniParseMemory(const void* pData, uint64 iSize,
	const xrtinioptions* pOptions)
{
	xreader pReader = pData != NULL || iSize == 0u ? xrtReaderFromMemory(pData, iSize) : NULL;
	return xrtIniParse(pReader, TRUE, pOptions);
}

XXAPI xrtiniconfig* xrtIniLoad(str sPath, const xrtinioptions* pOptions)
{
	xreader pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtIniParse(pReader, TRUE, pOptions);
}
