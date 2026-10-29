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
	if ( pReader->InputIndex == pReader->InputSize ) {
		size_t iRequest = sizeof(pReader->Input);
		uint64 iLimit = pReader->Dialect.tLimits.iMaxInputBytes;
		/* 多读一个探测字节，区分恰好达到上限和真正超限。 */
		if ( iLimit != 0u && iLimit - pReader->InputBytes < iRequest ) {
			iRequest = (size_t)(iLimit - pReader->InputBytes) + 1u;
		}
		pReader->InputIndex = 0u;
		if ( !xrtReaderRead(pReader->Reader, pReader->Input, iRequest, &pReader->InputSize) ) {
			__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_IO, "cannot read CSV stream");
			return -2;
		}
		if ( pReader->Failed ) { (void)__xrtCsvFailed(pReader->Failure); return -2; }
		if ( pReader->InputSize == 0u ) { return -1; }
	}
	if ( pReader->Dialect.tLimits.iMaxInputBytes != 0u &&
		pReader->InputBytes >= pReader->Dialect.tLimits.iMaxInputBytes ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV input exceeds the configured limit");
		return -2;
	}
	pReader->InputBytes += 1u;
	return pReader->Input[pReader->InputIndex++];
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
		if ( (pReader->PrefixCount == 1u && c != 0xef) ||
			(pReader->PrefixCount == 2u && c != 0xbb) ) { break; }
	}
	if ( pReader->PrefixCount == 3u && pReader->Prefix[0] == 0xefu &&
		pReader->Prefix[1] == 0xbbu && pReader->Prefix[2] == 0xbfu ) {
		pReader->PrefixIndex = 3u;
		pReader->Offset = 3u;
	}
	pReader->PrefixReady = true;
	return true;
}

static int __xrtCsvPeek(xrtcsvreader* pReader)
{
	int c;
	if ( pReader->Pending != -3 ) { return pReader->Pending; }
	if ( !__xrtCsvPreparePrefix(pReader) ) { return -2; }
	if ( pReader->PrefixIndex < pReader->PrefixCount ) {
		c = pReader->Prefix[pReader->PrefixIndex++];
	}
	else { c = __xrtCsvRawGet(pReader); }
	pReader->Pending = c;
	return c;
}

/* 游标只在消费字节时前进；CR 的预读不能提前消费下一行首字节。 */
static int __xrtCsvGet(xrtcsvreader* pReader)
{
	int c;
	pReader->LastOffset = pReader->Offset;
	pReader->LastLine = pReader->Line;
	pReader->LastColumn = pReader->Column;
	c = __xrtCsvPeek(pReader);
	if ( c >= 0 ) {
		pReader->LastOffset = pReader->Offset;
		pReader->Pending = -3;
		pReader->Offset += 1u;
		if ( c == '\r' || c == '\n' ) {
			if ( c != '\n' || !pReader->PreviousCR ) { pReader->Line += 1u; }
			pReader->Column = 1u;
		} else { pReader->Column += 1u; }
		pReader->PreviousCR = c == '\r';
	}
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
	uint64 iOutputLimit = pReader->Dialect.tLimits.iMaxOutputBytes;
	if ( c == 0u ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_FORMAT,
			"CSV string fields cannot contain a zero byte");
		return false;
	}
	/* 解码配额必须在字段增长前检查，不能等到分隔符才限制内存消耗。 */
	if ( iOutputLimit != 0u && (pReader->OutputBytes >= iOutputLimit ||
		pReader->Field->Size >= iOutputLimit - pReader->OutputBytes) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
			"CSV decoded output exceeds the configured limit");
		return false;
	}
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
	/* 空字段无需 UTF-8 解码；非空字段按明确长度验证。 */
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
		uint64 iCapacity = pReader->FieldCapacity != 0u ? (uint64)pReader->FieldCapacity * 2u : 16u;
		if ( iCapacity > UINT32_MAX || iCapacity > SIZE_MAX / sizeof(str) ) {
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
		pReader->FieldCapacity = (uint32)iCapacity;
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
	iNext = __xrtCsvPeek(pReader);
	if ( iNext == -2 ) { return -1; }
	if ( iNext == '\n' ) { *pSecond = __xrtCsvGet(pReader); }
	return 1;
}

XRT_API xrtcsvreader* xrtCsvReaderCreate(xreader* pInput, bool bTakeOwnership,
	const xrtcsvdialect* pDialect)
{
	xrtcsvreader* pReader;
	uint64 iSize = 0u, iPosition = 0u;
	xrtClearError();
	pReader = (xrtcsvreader*)xrtCalloc(1u, sizeof(*pReader));
	if ( pReader == NULL ) {
		xerror* pSaved = xrtTakeError();
		if ( bTakeOwnership ) { (void)xrtReaderDestroy(pInput); }
		xrtSetErrorTake(pSaved);
		return NULL;
	}
	xrtCsvErrorInit(&pReader->Error);
	pReader->Reader = pInput;
	pReader->TakeReader = bTakeOwnership;
	pReader->Line = 1u;
	pReader->Column = 1u;
	pReader->LastLine = 1u;
	pReader->LastColumn = 1u;
	pReader->Pending = -3;
	if ( pInput == NULL ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_ARGUMENT, "CSV input stream is required");
		return pReader;
	}
	xrtClearError();
	if ( !__xrtCsvCopyDialect(&pReader->Dialect, pDialect) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_ARGUMENT, "invalid CSV dialect");
		return pReader;
	}
	pReader->Field = xrtBufferCreate();
	if ( pReader->Field == NULL || !xrtBufferReserve(pReader->Field, 256u) ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_MEMORY, "cannot allocate CSV field buffer");
		return pReader;
	}
	/* 只按本次将消费的剩余视图预检；无法定位的流在读取时执行同一配额。 */
	if ( xrtReaderCanSize(pInput) && xrtReaderCanSeek(pInput) ) {
		if ( !xrtReaderSize(pInput, &iSize) || !xrtReaderTell(pInput, &iPosition) ) {
			__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_IO, "cannot query CSV input size or position");
		} else {
			iSize = iPosition <= iSize ? iSize - iPosition : 0u;
			if ( pReader->Dialect.tLimits.iMaxInputBytes != 0u &&
				iSize > pReader->Dialect.tLimits.iMaxInputBytes ) {
				__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
					"CSV input exceeds the configured limit");
			}
		}
	}
	return pReader;
}

XRT_API xrtcsvreader* xrtCsvReaderOpenMemory(const void* pData, uint64 iSize,
	const xrtcsvdialect* pDialect)
{
	xreader* pInput;
	xrtClearError();
	if ( iSize > SIZE_MAX || (pData == NULL && iSize != 0u) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.csv", XRT_CSV_ERROR_ARGUMENT, "invalid CSV memory input");
		return NULL;
	}
	pInput = xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize });
	if ( pInput == NULL ) { return NULL; }
	return xrtCsvReaderCreate(pInput, true, pDialect);
}

XRT_API xrtcsvreader* xrtCsvReaderOpenFile(cstr sPath, const xrtcsvdialect* pDialect)
{
	xreader* pInput;
	xrtClearError();
	pInput = xrtReaderOpen(sPath);
	if ( pInput == NULL ) { return NULL; }
	return xrtCsvReaderCreate(pInput, true, pDialect);
}

XRT_API const xrtcsverror* xrtCsvReaderError(const xrtcsvreader* pReader)
{
	return pReader != NULL ? &pReader->Error : NULL;
}

static bool __xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow)
{
	int iState = XRT_CSV_FIELD_START;
	bool bSawInput = false;
	xrtClearError();
	if ( pRow != NULL ) { memset(pRow, 0, sizeof(*pRow)); }
	if ( pReader == NULL || pRow == NULL ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.csv", XRT_CSV_ERROR_ARGUMENT, "CSV reader and row are required");
		return false;
	}
	if ( pReader->Failed ) { return __xrtCsvFailed(pReader->Failure); }
	if ( pReader->Closed ) {
		xrtSetErrorInfo(XERR_STATE, "xrt.csv", XRT_CSV_ERROR_STATE, "CSV reader is closed");
		return false;
	}
	__xrtCsvClearFields(pReader);
	if ( pReader->EOFReached ) { return false; }
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
		if ( !bSawInput && pReader->Dialect.tLimits.iMaxEntries != 0u &&
			pReader->RowNumber >= pReader->Dialect.tLimits.iMaxEntries ) {
			__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_LIMIT,
				"CSV row count exceeds the configured limit");
			return false;
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
			}
			continue;
		}
		if ( iNewline ) {
			if ( iState == XRT_CSV_FIELD_AFTER_QUOTE || iState == XRT_CSV_FIELD_UNQUOTED ||
				pReader->FieldCount != 0u ) {
				if ( !__xrtCsvFinishField(pReader) ) { return false; }
			}
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

static bool __xrtCsvReaderClose(xrtcsvreader* pReader)
{
	bool bOk = true;
	xrtClearError();
	if ( pReader == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !pReader->Closed ) {
		pReader->Closed = true;
		if ( pReader->TakeReader ) { bOk = xrtReaderDestroy(pReader->Reader); }
		pReader->Reader = NULL;
		pReader->TakeReader = false;
		__xrtCsvClearFields(pReader);
		if ( !bOk ) { __xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_IO, "cannot close CSV stream"); }
	}
	return pReader->Failed ? __xrtCsvFailed(pReader->Failure) : bOk;
}

/* IO 回调可使用其他对象，但不能重新推进、关闭或释放当前解析器。 */
XRT_API bool xrtCsvReaderNext(xrtcsvreader* pReader, xrtcsvrow* pRow)
{
	bool bOk;
	if ( pReader != NULL && pReader->Busy ) {
		if ( pRow != NULL ) { memset(pRow, 0, sizeof(*pRow)); }
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_STATE, "CSV reader callback cannot reenter its reader");
		return false;
	}
	if ( pReader != NULL ) { pReader->Busy = true; }
	bOk = __xrtCsvReaderNext(pReader, pRow);
	if ( pReader != NULL ) { pReader->Busy = false; }
	return bOk;
}

XRT_API bool xrtCsvReaderClose(xrtcsvreader* pReader)
{
	bool bOk;
	if ( pReader != NULL && pReader->Busy ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_STATE, "CSV reader callback cannot close its reader");
		return false;
	}
	if ( pReader != NULL ) { pReader->Busy = true; }
	bOk = __xrtCsvReaderClose(pReader);
	if ( pReader != NULL ) { pReader->Busy = false; }
	return bOk;
}

XRT_API void xrtCsvReaderDestroy(xrtcsvreader* pReader)
{
	xerror* pSaved;
	if ( pReader == NULL ) { return; }
	if ( pReader->Busy ) {
		__xrtCsvSetReaderError(pReader, XRT_CSV_ERROR_STATE, "CSV reader callback cannot destroy its reader");
		return;
	}
	pReader->Busy = true;
	pSaved = xrtTakeError();
	if ( pReader->TakeReader && pReader->Reader != NULL ) { xrtReaderDestroy(pReader->Reader); }
	__xrtCsvClearFields(pReader);
	xrtFree(pReader->Fields);
	xrtBufferDestroy(pReader->Field);
	xrtErrorFree(pReader->Failure);
	xrtFree(pReader);
	xrtSetErrorTake(pSaved);
}
