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
	return iEnd > iStart ? xrtStrDupN((cstr)(pData + iStart), iEnd - iStart) :
		xrtStrDup("");
}

static bool __xrtIniAppendPending(xrtiniconfig* pConfig, xbuffer* pPending,
	const unsigned char* pData, size_t iStart, size_t iEnd)
{
	uint64 iAdded = (uint64)(iEnd - iStart) + 1u;
	if ( pConfig->Options.tLimits.iMaxOutputBytes != 0u &&
		(iAdded > pConfig->Options.tLimits.iMaxOutputBytes ||
		 pPending->Size > pConfig->Options.tLimits.iMaxOutputBytes - iAdded) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, iStart,
			"INI preserved comment data exceeds the configured limit");
		return false;
	}
	if ( (iEnd > iStart && !xrtBufferAppend(pPending,
		(xbytesview){ pData + iStart, iEnd - iStart })) ||
		!xrtBufferAppendByte(pPending, '\n') ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, iStart,
			"cannot preserve INI comment data");
		return false;
	}
	return true;
}

static str __xrtIniTakePending(xbuffer* pPending)
{
	str sResult = pPending->Size != 0u ?
		xrtStrDupN((cstr)pPending->Data, pPending->Size) : xrtStrDup("");
	pPending->Size = 0u;
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
		return false;
	}
	sValue = (str)xrtMalloc(iOld + 1u + iPart + 1u);
	if ( sValue == NULL ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iStart,
			"cannot grow INI multiline value");
		return false;
	}
	memcpy(sValue, pEntry->Value, iOld);
	sValue[iOld] = '\n';
	if ( iPart != 0u ) { memcpy(sValue + iOld + 1u, pData + iStart, iPart); }
	sValue[iOld + 1u + iPart] = 0;
	xrtFree(pEntry->Value);
	pEntry->Value = sValue;
	return true;
}

static bool __xrtIniParseBuffer(xrtiniconfig* pConfig, const unsigned char* pData,
	size_t iSize)
{
	xbuffer* pPending = xrtBufferCreate();
	xrt_ini_section* pSection = NULL;
	xrt_ini_entry* pLastEntry = NULL;
	size_t iOffset = 0u;
	uint64 iLine = 1u;
	if ( pPending == NULL || !xrtBufferReserve(pPending, 256u) ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
			"cannot allocate INI parser state");
		xrtBufferDestroy(pPending);
		return false;
	}
	if ( iSize >= 3u && pData[0] == 0xefu && pData[1] == 0xbbu && pData[2] == 0xbfu ) {
		iOffset = 3u;
	}
	while ( iOffset < iSize ) {
		size_t iLineStart = iOffset;
		size_t iLineEnd;
		const unsigned char* pNul;
		size_t iStart;
		size_t iEnd;
		bool bIndented;
		while ( iOffset < iSize && pData[iOffset] != '\r' && pData[iOffset] != '\n' ) {
			iOffset += 1u;
		}
		iLineEnd = iOffset;
		pNul = (const unsigned char*)memchr(pData + iLineStart, 0, iLineEnd - iLineStart);
		if ( pNul != NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT, iLine,
				(uint64)(pNul - pData - iLineStart + 1u), (uint64)(pNul - pData),
				"INI input contains an embedded NUL byte");
			xrtBufferDestroy(pPending);
			return false;
		}
		if ( iOffset < iSize && pData[iOffset] == '\r' ) { iOffset += 1u; }
		if ( iOffset < iSize && pData[iOffset] == '\n' ) { iOffset += 1u; }
		iStart = iLineStart;
		iEnd = iLineEnd;
		bIndented = iStart < iEnd && (pData[iStart] == ' ' || pData[iStart] == '\t');
		__xrtIniTrim(pData, &iStart, &iEnd);

		if ( iStart == iEnd || pData[iStart] == ';' || pData[iStart] == '#' ) {
			if ( !__xrtIniAppendPending(pConfig, pPending, pData, iLineStart, iLineEnd) ) {
				xrtBufferDestroy(pPending);
				return false;
			}
			pLastEntry = NULL;
			iLine += 1u;
			continue;
		}

		if ( bIndented && pLastEntry != NULL && pPending->Size == 0u ) {
			if ( !__xrtIniAppendContinuation(pConfig, pLastEntry, pData,
				iStart, iEnd, iLine) ) {
				xrtBufferDestroy(pPending);
				return false;
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
				return false;
			}
			iStart += 1u;
			iClose -= 1u;
			__xrtIniTrim(pData, &iStart, &iClose);
			if ( iStart == iClose ||
				(pConfig->Options.tLimits.iMaxItemBytes != 0u &&
				 (uint64)(iClose - iStart) > pConfig->Options.tLimits.iMaxItemBytes) ) {
				__xrtIniSetError(pConfig, iStart == iClose ? XRT_INI_ERROR_FORMAT :
					XRT_INI_ERROR_LIMIT, iLine, 1u, iLineStart,
					iStart == iClose ? "empty INI section name" :
					"INI section name exceeds the configured item limit");
				xrtBufferDestroy(pPending);
				return false;
			}
			sName = __xrtIniCopyRange(pData, iStart, iClose);
			sLeading = __xrtIniTakePending(pPending);
			if ( sName == NULL || sLeading == NULL ) {
				xrtFree(sName);
				xrtFree(sLeading);
				__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iLineStart,
					"cannot copy INI section header");
				xrtBufferDestroy(pPending);
				return false;
			}
			pSection = __xrtIniAddSection(pConfig, (const char*)sName,
				(const char*)sLeading, iLine, true);
			xrtFree(sName);
			xrtFree(sLeading);
			if ( pSection == NULL ) { xrtBufferDestroy(pPending); return false; }
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
				return false;
			}
			iKeyEnd = iDelimiter;
			iValueStart = iDelimiter + 1u;
			__xrtIniTrim(pData, &iKeyStart, &iKeyEnd);
			__xrtIniTrim(pData, &iValueStart, &iValueEnd);
			if ( iKeyStart == iKeyEnd ||
				(pConfig->Options.tLimits.iMaxItemBytes != 0u &&
				 ((uint64)(iKeyEnd - iKeyStart) > pConfig->Options.tLimits.iMaxItemBytes ||
				  (uint64)(iValueEnd - iValueStart) > pConfig->Options.tLimits.iMaxItemBytes)) ) {
				__xrtIniSetError(pConfig, iKeyStart == iKeyEnd ? XRT_INI_ERROR_FORMAT :
					XRT_INI_ERROR_LIMIT, iLine, 1u, iLineStart,
					iKeyStart == iKeyEnd ? "empty INI key" :
					"INI key/value exceeds the configured item limit");
				xrtBufferDestroy(pPending);
				return false;
			}
			if ( pSection == NULL ) {
				sLeading = __xrtIniTakePending(pPending);
				if ( sLeading == NULL ) {
					__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iLineStart,
						"cannot copy INI leading comments");
					xrtBufferDestroy(pPending);
					return false;
				}
				pSection = __xrtIniAddSection(pConfig, "", (const char*)sLeading,
					iLine, false);
				xrtFree(sLeading);
				if ( pSection == NULL ) { xrtBufferDestroy(pPending); return false; }
			}
			sKey = __xrtIniCopyRange(pData, iKeyStart, iKeyEnd);
			sValue = __xrtIniCopyRange(pData, iValueStart, iValueEnd);
			sLeading = __xrtIniTakePending(pPending);
			/* 覆盖旧键时条目不在表尾，续行必须跟随实际更新的条目。 */
			if ( sKey == NULL || sValue == NULL || sLeading == NULL ||
				(pLastEntry = __xrtIniAddEntry(pConfig, pSection, (const char*)sKey,
					(const char*)sValue, (const char*)sLeading, iLine, true)) == NULL ) {
				xrtFree(sKey);
				xrtFree(sValue);
				xrtFree(sLeading);
				if ( !pConfig->Failed ) {
					__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iLineStart,
						"cannot copy INI entry");
				}
				xrtBufferDestroy(pPending);
				return false;
			}
			xrtFree(sKey);
			xrtFree(sValue);
			xrtFree(sLeading);
		}
		iLine += 1u;
	}
	if ( pPending->Size != 0u ) {
		pConfig->Trailing = __xrtIniTakePending(pPending);
		if ( pConfig->Trailing == NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, iLine, 1u, iSize,
				"cannot copy trailing INI comments");
			xrtBufferDestroy(pPending);
			return false;
		}
	}
	xrtBufferDestroy(pPending);
	return true;
}



XRT_API xrtiniconfig* xrtIniParse(xreader* pReader, bool bTakeOwnership,
	const xrtinioptions* pOptions)
{
	xrtiniconfig* pConfig = xrtIniCreate(pOptions);
	xbuffer* pData = NULL;
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	uint64 iLimit;
	size_t iReadLimit;
	bool bSizeKnown;
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
	iReadLimit = iLimit == 0u || iLimit > SIZE_MAX - 1u ?
		SIZE_MAX - 1u : (size_t)iLimit;
	xrtClearError();
	bSizeKnown = iLimit != 0u && xrtReaderSize(pReader, &iSize) &&
		xrtReaderTell(pReader, &iPosition) && iPosition <= iSize;
	xrtClearError();
	if ( bSizeKnown && iSize - iPosition > iLimit ) {
		__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
			"INI input exceeds the configured limit");
	} else {
		pData = xrtReaderReadAll(pReader, iReadLimit);
		if ( pData == NULL ) {
			const xerror* pReadError = xrtGetError();
			xerrkind eKind = pReadError != NULL ? xrtErrorKind(pReadError) : XERR_IO;
			int iCode = eKind == XERR_RANGE ? XRT_INI_ERROR_LIMIT :
				(eKind == XERR_MEMORY ? XRT_INI_ERROR_MEMORY : XRT_INI_ERROR_IO);
			__xrtIniSetError(pConfig, iCode, 0u, 0u, 0u,
				iCode == XRT_INI_ERROR_LIMIT ? "INI input exceeds the configured limit" :
				(iCode == XRT_INI_ERROR_MEMORY ? "cannot allocate INI input buffer" :
				"cannot read INI stream"));
		} else {
			size_t iBadUtf8 = 0u;
			pConfig->InputBytes = pData->Size;
			if ( !xrtUtf8Valid((xstrview){ (cstr)pData->Data,
				pData->Size }, &iBadUtf8) ) {
				const unsigned char* pText = pData->Data;
				size_t i = pData->Size >= 3u && pText[0] == 0xefu &&
					pText[1] == 0xbbu && pText[2] == 0xbfu ? 3u : 0u;
				uint64 iLine = 1u, iColumn = 1u;
				/* 偏移保留原始 BOM 字节，列号与正文解析一样跳过 BOM。 */
				while ( i < iBadUtf8 ) {
					if ( pText[i] == '\r' || pText[i] == '\n' ) {
						if ( pText[i] == '\r' && i + 1u < iBadUtf8 &&
							pText[i + 1u] == '\n' ) { ++i; }
						++iLine;
						iColumn = 1u;
					} else { ++iColumn; }
					++i;
				}
				__xrtIniSetError(pConfig, XRT_INI_ERROR_FORMAT,
					iLine, iColumn, iBadUtf8,
					"INI input is not valid UTF-8");
			} else {
				__xrtIniParseBuffer(pConfig, (const unsigned char*)pData->Data,
					pData->Size);
			}
		}
	}
	xrtBufferDestroy(pData);
	if ( bTakeOwnership && !xrtReaderDestroy(pReader) && !pConfig->Failed ) {
		/* 正文解析成功后，拥有型 Reader 的关闭仍可能失败。 */
		__xrtIniSetError(pConfig, XRT_INI_ERROR_IO, 0u, 0u, 0u,
			"cannot close INI stream");
	}
	return pConfig;
}

XRT_API xrtiniconfig* xrtIniParseMemory(const void* pData, uint64 iSize,
	const xrtinioptions* pOptions)
{
	if ( iSize > (uint64)SIZE_MAX ) {
		xrtiniconfig* pConfig = xrtIniCreate(pOptions);
		if ( pConfig != NULL ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_LIMIT, 0u, 0u, 0u,
				"INI input size exceeds the addressable range");
		}
		return pConfig;
	}
	xreader* pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize }) : NULL;
	if ( pReader == NULL && (pData != NULL || iSize == 0u) ) {
		xrtiniconfig* pConfig = xrtIniCreate(pOptions);
		if ( pConfig != NULL && !pConfig->Failed ) {
			__xrtIniSetError(pConfig, XRT_INI_ERROR_MEMORY, 0u, 0u, 0u,
				"cannot create INI memory reader");
		}
		return pConfig;
	}
	return xrtIniParse(pReader, true, pOptions);
}

XRT_API xrtiniconfig* xrtIniLoad(str sPath, const xrtinioptions* pOptions)
{
	xreader* pReader;
	if ( sPath == NULL || sPath[0] == 0 ) {
		return xrtIniParse(NULL, false, pOptions);
	}
	pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		const xerror* pError = xrtGetError();
		int iCode = pError != NULL && xrtErrorKind(pError) == XERR_MEMORY ?
			XRT_INI_ERROR_MEMORY : XRT_INI_ERROR_IO;
		const char* sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;
		xrtiniconfig* pConfig = xrtIniCreate(pOptions);
		if ( pConfig != NULL && !pConfig->Failed ) {
			__xrtIniSetError(pConfig, iCode, 0u, 0u, 0u,
				"cannot open INI file: %s",
				sMessage != NULL ? sMessage : "unknown error");
		}
		return pConfig;
	}
	return xrtIniParse(pReader, true, pOptions);
}
