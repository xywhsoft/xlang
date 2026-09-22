#include "xrt_csv_internal.h"

#include <string.h>

enum {
	XRT_CSV_FIELD_START = 0,
	XRT_CSV_FIELD_UNQUOTED = 1,
	XRT_CSV_FIELD_QUOTED = 2,
	XRT_CSV_FIELD_AFTER_QUOTE = 3
};



static int __xrtCsvRawGet(xrtcsvreader* pReader)
{
	unsigned char c;
	uint64 iRead = 0u;
	if ( !xrtReaderRead(pReader->Reader, &c, 1u, &iRead) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_IO, "cannot read CSV stream");
		return -2;
	}
	if ( iRead == 0u ) { return -1; }
	if ( pReader->Dialect.tLimits.iMaxInputBytes != 0u &&
		pReader->InputBytes >= pReader->Dialect.tLimits.iMaxInputBytes ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV input exceeds the configured limit");
		return -2;
	}
	pReader->InputBytes += 1u;
	return c;
}

static bool __xrtCsvPreparePrefix(xrtcsvreader* pReader)
{
	int c;
	if ( pReader->PrefixReady ) { return true; }
	while ( pReader->PrefixCount < 3u ) {
		c = __xrtCsvRawGet(pReader);
		if ( c == -2 ) { return false; }
		if ( c == -1 ) { break; }
		pReader->Prefix[pReader->PrefixCount++] = (unsigned char)c;
	}
	if ( pReader->PrefixCount == 3u && pReader->Prefix[0] == 0xefu &&
		pReader->Prefix[1] == 0xbbu && pReader->Prefix[2] == 0xbfu ) {
		pReader->PrefixIndex = 3u;
		pReader->Offset = 3u;
	}
	pReader->PrefixReady = true;
	return true;
}

static int __xrtCsvGet(xrtcsvreader* pReader)
{
	int c;
	if ( pReader->Pending >= 0 ) {
		c = pReader->Pending;
		pReader->Pending = -1;
		return c;
	}
	if ( !__xrtCsvPreparePrefix(pReader) ) { return -2; }
	if ( pReader->PrefixIndex < pReader->PrefixCount ) {
		c = pReader->Prefix[pReader->PrefixIndex++];
	}
	else { c = __xrtCsvRawGet(pReader); }
	if ( c >= 0 ) { pReader->Offset += 1u; }
	return c;
}

static void __xrtCsvClearFields(xrtcsvreader* pReader)
{
	uint32 i;
	for ( i = 0u; i < pReader->FieldCount; ++i ) { xrtFree(pReader->Fields[i]); }
	pReader->FieldCount = 0u;
	if ( pReader->Field != NULL ) { pReader->Field->Size = 0u; }
}

static bool __xrtCsvAppendByte(xrtcsvreader* pReader, unsigned char c)
{
	if ( pReader->Dialect.tLimits.iMaxItemBytes != 0u &&
		pReader->Field->Size >= pReader->Dialect.tLimits.iMaxItemBytes ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV field exceeds the configured item limit");
		return false;
	}
	if ( !xrtBufferAppendByte(pReader->Field, c) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_MEMORY,
			"cannot grow CSV field buffer");
		return false;
	}
	return true;
}

static bool __xrtCsvFinishField(xrtcsvreader* pReader)
{
	str sValue;
	str* pFields;
	if ( pReader->Dialect.tLimits.iMaxNodes != 0u &&
		pReader->TotalFields >= pReader->Dialect.tLimits.iMaxNodes ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV field count exceeds the configured limit");
		return false;
	}
	/* xrtIsUTF8 的长度 0 表示 NUL 结尾输入；空字段无需调用该接口。 */
	if ( pReader->Field->Size != 0u &&
		!xrtUtf8Valid((xstrview){ (cstr)pReader->Field->Data,
			pReader->Field->Size }, NULL) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_FORMAT,
			"CSV field is not valid UTF-8");
		return false;
	}
	if ( pReader->Dialect.tLimits.iMaxOutputBytes != 0u &&
		(pReader->Field->Size > pReader->Dialect.tLimits.iMaxOutputBytes ||
		 pReader->OutputBytes > pReader->Dialect.tLimits.iMaxOutputBytes -
		 pReader->Field->Size) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV decoded output exceeds the configured limit");
		return false;
	}
	sValue = pReader->Field->Size != 0u ?
		xrtStrDupN((cstr)pReader->Field->Data, pReader->Field->Size) :
		xrtStrDup("");
	if ( sValue == NULL ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_MEMORY,
			"cannot copy CSV field");
		return false;
	}
	if ( pReader->FieldCount == pReader->FieldCapacity ) {
		uint32 iCapacity = pReader->FieldCapacity != 0u ? pReader->FieldCapacity * 2u : 16u;
		if ( iCapacity < pReader->FieldCapacity ) {
			xrtFree(sValue);
			__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT, "too many CSV fields");
			return false;
		}
		pFields = (str*)xrtRealloc(pReader->Fields, (size_t)iCapacity * sizeof(str));
		if ( pFields == NULL ) {
			xrtFree(sValue);
			__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_MEMORY,
				"cannot grow CSV row");
			return false;
		}
		pReader->Fields = pFields;
		pReader->FieldCapacity = iCapacity;
	}
	pReader->Fields[pReader->FieldCount++] = sValue;
	pReader->OutputBytes += pReader->Field->Size;
	pReader->TotalFields += 1u;
	pReader->Field->Size = 0u;
	return true;
}

static int __xrtCsvNewline(xrtcsvreader* pReader, int c, int* pSecond)
{
	int iNext;
	*pSecond = -1;
	if ( c != '\r' ) { return c == '\n'; }
	iNext = __xrtCsvGet(pReader);
	if ( iNext == -2 ) { return -1; }
	if ( iNext == '\n' ) { *pSecond = '\n'; }
	else if ( iNext >= 0 ) { pReader->Pending = iNext; }
	return 1;
}

XRT_API xrtcsvreader* xrtCsvReaderCreate(xreader* pInput, bool bTakeOwnership,
	const xrtcsvdialect* pDialect)
{
	xrtcsvreader* pReader = (xrtcsvreader*)xrtCalloc(1u, sizeof(*pReader));
	uint64 iSize = 0u;
	if ( pReader == NULL ) { return NULL; }
	xrtCsvErrorInit(&pReader->Error);
	pReader->Reader = pInput;
	pReader->TakeReader = bTakeOwnership;
	pReader->Line = 1u;
	pReader->Column = 1u;
	pReader->Pending = -1;
	pReader->Field = xrtBufferCreate();
	if ( pInput == NULL || pReader->Field == NULL ||
		!xrtBufferReserve(pReader->Field, 256u) ||
		!__xrtCsvCopyDialect(&pReader->Dialect, pDialect) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_ARGUMENT,
			"invalid CSV reader or dialect");
		return pReader;
	}
	if ( xrtReaderSize(pInput, &iSize) && pReader->Dialect.tLimits.iMaxInputBytes != 0u &&
		iSize > pReader->Dialect.tLimits.iMaxInputBytes ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV input exceeds the configured limit");
	}
	return pReader;
}

XRT_API xrtcsvreader* xrtCsvReaderOpenMemory(const void* pData, uint64 iSize,
	const xrtcsvdialect* pDialect)
{
	xreader* pInput = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize }) : NULL;
	return xrtCsvReaderCreate(pInput, true, pDialect);
}

XRT_API xrtcsvreader* xrtCsvReaderOpenFile(str sPath, const xrtcsvdialect* pDialect)
{
	xreader* pInput = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtCsvReaderCreate(pInput, true, pDialect);
}

XRT_API const xrtcsverror* xrtCsvReaderError(const xrtcsvreader* pReader)
{
	return pReader != NULL ? &pReader->Error : NULL;
}

XRT_API bool xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow)
{
	int iState = XRT_CSV_FIELD_START;
	bool bSawInput = false;
	if ( pReader == NULL || pRow == NULL || pReader->Failed || pReader->EOFReached ) { return false; }
	if ( pReader->Dialect.tLimits.iMaxEntries != 0u &&
		pReader->RowNumber >= pReader->Dialect.tLimits.iMaxEntries ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV row count exceeds the configured limit");
		return false;
	}
	__xrtCsvClearFields(pReader);
	for ( ;; ) {
		int c = __xrtCsvGet(pReader);
		int iSecond = -1;
		int iNewline;
		if ( c == -2 ) { return false; }
		if ( c == -1 ) {
			if ( iState == XRT_CSV_FIELD_QUOTED && pReader->Dialect.bStrict ) {
				__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_FORMAT,
					"unterminated quoted CSV field");
				return false;
			}
			if ( !bSawInput && pReader->FieldCount == 0u && pReader->Field->Size == 0u ) {
				pReader->EOFReached = true;
				return false;
			}
			if ( iState != XRT_CSV_FIELD_START || pReader->FieldCount != 0u ||
				pReader->Field->Size != 0u ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
			}
			pReader->EOFReached = true;
			break;
		}
		bSawInput = true;
		iNewline = c == '\r' || c == '\n' ? __xrtCsvNewline(pReader, c, &iSecond) : 0;
		if ( iNewline < 0 ) { return false; }
		if ( iState == XRT_CSV_FIELD_QUOTED ) {
			if ( c == pReader->Dialect.iQuote ) { iState = XRT_CSV_FIELD_AFTER_QUOTE; }
			else {
				if ( !__xrtCsvAppendByte(pReader, (unsigned char)c) ||
					(iSecond >= 0 && !__xrtCsvAppendByte(pReader, (unsigned char)iSecond)) ) {
					return false;
				}
				if ( iNewline ) { pReader->Line += 1u; pReader->Column = 1u; }
				else { pReader->Column += 1u; }
			}
			continue;
		}
		if ( iNewline ) {
			if ( iState == XRT_CSV_FIELD_AFTER_QUOTE || iState == XRT_CSV_FIELD_UNQUOTED ||
				pReader->FieldCount != 0u ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
			}
			pReader->Line += 1u;
			pReader->Column = 1u;
			break;
		}
		if ( iState == XRT_CSV_FIELD_START ) {
			if ( c == pReader->Dialect.iQuote ) { iState = XRT_CSV_FIELD_QUOTED; }
			else if ( c == pReader->Dialect.iDelimiter ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
			}
			else {
				if ( !__xrtCsvAppendByte(pReader, (unsigned char)c) ) { return false; }
				iState = XRT_CSV_FIELD_UNQUOTED;
			}
		}
		else if ( iState == XRT_CSV_FIELD_UNQUOTED ) {
			if ( c == pReader->Dialect.iDelimiter ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
				iState = XRT_CSV_FIELD_START;
			}
			else if ( c == pReader->Dialect.iQuote && pReader->Dialect.bStrict ) {
				__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_FORMAT,
					"unexpected quote in unquoted CSV field");
				return false;
			}
			else if ( !__xrtCsvAppendByte(pReader, (unsigned char)c) ) { return false; }
		}
		else {
			if ( c == pReader->Dialect.iQuote ) {
				if ( !__xrtCsvAppendByte(pReader, (unsigned char)c) ) { return false; }
				iState = XRT_CSV_FIELD_QUOTED;
			}
			else if ( c == pReader->Dialect.iDelimiter ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
				iState = XRT_CSV_FIELD_START;
			}
			else if ( pReader->Dialect.bStrict ) {
				__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_FORMAT,
					"unexpected character after closing CSV quote");
				return false;
			}
			else {
				if ( !__xrtCsvAppendByte(pReader, (unsigned char)c) ) { return false; }
				iState = XRT_CSV_FIELD_UNQUOTED;
			}
		}
		pReader->Column += 1u;
	}
	pRow->iRow = pReader->RowNumber;
	pRow->iCount = pReader->FieldCount;
	pRow->pFields = (const char* const*)pReader->Fields;
	pReader->RowNumber += 1u;
	return true;
}

XRT_API bool xrtCsvReaderEOF(const xrtcsvreader* pReader)
{
	return pReader != NULL && pReader->EOFReached && !pReader->Failed;
}

XRT_API void xrtCsvReaderDestroy(xrtcsvreader* pReader)
{
	if ( pReader == NULL ) { return; }
	if ( pReader->TakeReader && pReader->Reader != NULL ) { xrtReaderDestroy(pReader->Reader); }
	__xrtCsvClearFields(pReader);
	xrtFree(pReader->Fields);
	xrtBufferDestroy(pReader->Field);
	xrtFree(pReader);
}
