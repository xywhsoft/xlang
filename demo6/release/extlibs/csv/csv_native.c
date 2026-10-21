#include "xrt_csv_internal.h"
#include "include/csv_native.h"

#include <string.h>




typedef struct {
	xrtcsvreader* Reader;
	xrtcsvrow Row;
	bool HasRow;
	void* Input;
} xcsv_reader_handle;

typedef struct {
	xrtcsvwriter* Writer;
	str* Fields;
	uint32 Count;
	uint32 Capacity;
	bool RowOpen;
	bool Memory;
	uint64 RowBytes;
} xcsv_writer_handle;

static void xcsv_error(xerrkind Kind, const char* sMessage)
{
	int iCode = Kind == XERR_MEMORY ? XRT_CSV_ERROR_MEMORY :
		Kind == XERR_RANGE ? XRT_CSV_ERROR_LIMIT :
		Kind == XERR_ARGUMENT ? XRT_CSV_ERROR_ARGUMENT : XRT_CSV_ERROR_STATE;
	xrtSetErrorInfo(Kind, "xrt.csv", iCode,
		sMessage != NULL ? sMessage : "CSV operation failed");
}



/* 仅语言声明层附加 CsvError；C 引擎保持通用 XRT 错误，并作为原因完整保留。 */
static void xcsv_publish_kind(cstr sKind)
{
	const xerror* pCause = xrtGetError();
	xerrordesc tDesc = {0};
	xerror* pError;
	tDesc.Kind = xrtErrorKind(pCause);
	tDesc.Code = xrtErrorCode(pCause);
	tDesc.SystemCode = xrtErrorSystemCode(pCause);
	tDesc.Domain = "xlang";
	tDesc.Operation = xrtErrorOperation(pCause);
	tDesc.Message = xrtErrorMessage(pCause);
	tDesc.Data = sKind;
	tDesc.Cause = pCause;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) { xrtSetErrorTake(pError); }
}

static void xcsv_publish_error(void)
{
	const xerror* pError = xrtGetError();
	if ( pError == NULL ) { xcsv_error(XERR_STATE, "CSV operation failed without a diagnostic"); return; }
	if ( xrtErrorKind(pError) == XERR_VALUE && xrtErrorCode(pError) == XRT_CSV_ERROR_FORMAT &&
		strcmp(xrtErrorDomain(pError), "xrt.csv") == 0 ) { xcsv_publish_kind("CsvError"); }
}

/* 引号为可打印 ASCII；分隔符另外允许 TSV 常用的 TAB。 */
static bool xcsv_set_character(char* pOutput, cstr sValue, bool bDelimiter)
{
	if ( sValue == NULL || sValue[0] == '\0' || sValue[1] != '\0' ||
		(! (bDelimiter && sValue[0] == '\t') &&
		 ((unsigned char)sValue[0] < 0x20u || (unsigned char)sValue[0] > 0x7eu)) ) {
		xcsv_error(XERR_ARGUMENT, "CSV dialect requires printable ASCII; only delimiter permits TAB");
		return false;
	}
	*pOutput = (char)sValue[0];
	return true;
}

static bool xcsv_reader_dialect(xrtcsvdialect* pDialect, cstr sDelimiter, cstr sQuote,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxRows, uint64 iMaxFields)
{
	xrtCsvDialectInit(pDialect);
	if ( !xcsv_set_character(&pDialect->iDelimiter, sDelimiter, true) ||
		!xcsv_set_character(&pDialect->iQuote, sQuote, false) ) { return false; }
	pDialect->bStrict = bStrict;
	pDialect->tLimits.iMaxInputBytes = iMaxInput;
	pDialect->tLimits.iMaxOutputBytes = iMaxOutput;
	pDialect->tLimits.iMaxItemBytes = iMaxItem;
	pDialect->tLimits.iMaxEntries = iMaxRows;
	pDialect->tLimits.iMaxNodes = iMaxFields;
	return true;
}

static bool xcsv_writer_dialect(xrtcsvdialect* pDialect, cstr sDelimiter, cstr sQuote,
	int iNewline, bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtCsvDialectInit(pDialect);
	if ( !xcsv_set_character(&pDialect->iDelimiter, sDelimiter, true) ||
		!xcsv_set_character(&pDialect->iQuote, sQuote, false) ||
		(iNewline != XRT_CSV_NEWLINE_LF && iNewline != XRT_CSV_NEWLINE_CRLF) ) {
		if ( iNewline != XRT_CSV_NEWLINE_LF && iNewline != XRT_CSV_NEWLINE_CRLF ) {
			xcsv_error(XERR_ARGUMENT, "CSV newline must be LF or CRLF");
		}
		return false;
	}
	pDialect->iNewline = iNewline;
	pDialect->bQuoteAll = bQuoteAll;
	pDialect->bWriteBOM = bWriteBOM;
	pDialect->tLimits.iMaxInputBytes = iMaxInput;
	pDialect->tLimits.iMaxOutputBytes = iMaxOutput;
	pDialect->tLimits.iMaxItemBytes = iMaxItem;
	pDialect->tLimits.iMaxEntries = iMaxRows;
	pDialect->tLimits.iMaxNodes = iMaxFields;
	return true;
}



static void* xcsv_validate_reader(xrtcsvreader* pReader)
{
	xcsv_reader_handle* pHandle;
	const xrtcsverror* pError = xrtCsvReaderError(pReader);
	if ( pReader == NULL || (pError != NULL && pError->iCode != XRT_CSV_OK) ) {
		xcsv_publish_error();
		xrtCsvReaderDestroy(pReader);
		return NULL;
	}
	pHandle = (xcsv_reader_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtCsvReaderDestroy(pReader);
		xcsv_error(XERR_MEMORY, "cannot allocate CSV reader handle");
		return NULL;
	}
	pHandle->Reader = pReader;
	return pHandle;
}

/* XL 读取器拥有输入快照；Buffer 可在返回后修改或释放。C 核心仍借用显式内存视图。 */
void* xcsv_reader_open_memory(void* pData, uint64 iSize,
	cstr sDelimiter, cstr sQuote, bool bStrict, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	void* pCopy = NULL;
	xcsv_reader_handle* pHandle;
	xrtClearError();
	if ( !xcsv_reader_dialect(&tDialect, sDelimiter, sQuote, bStrict,
		 iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	if ( iSize > SIZE_MAX || (pData == NULL && iSize != 0u) ) {
		xcsv_error(XERR_ARGUMENT, "invalid CSV input view"); return NULL;
	}
	if ( iMaxInput != 0u && iSize > iMaxInput ) {
		xcsv_error(XERR_RANGE, "CSV input exceeds the configured limit"); return NULL;
	}
	if ( iSize != 0u ) {
		pCopy = xrtMemDup(pData, (size_t)iSize);
		if ( pCopy == NULL ) { return NULL; }
	}
	pHandle = (xcsv_reader_handle*)xcsv_validate_reader(xrtCsvReaderOpenMemory(pCopy, iSize, &tDialect));
	if ( pHandle == NULL ) { xrtFree(pCopy); }
	else { pHandle->Input = pCopy; }
	return pHandle;
}

void* xcsv_reader_open_file(cstr sPath, cstr sDelimiter, cstr sQuote,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_reader_dialect(&tDialect, sDelimiter, sQuote, bStrict,
		iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_reader(xrtCsvReaderOpenFile(sPath, &tDialect));
}

void xcsv_reader_destroy(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	if ( pHandle == NULL ) { return; }
	xrtCsvReaderDestroy(pHandle->Reader);
	xrtFree(pHandle->Input);
	xrtFree(pHandle);
}

bool xcsv_reader_next(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	if ( pHandle == NULL ) { xcsv_error(XERR_ARGUMENT, "CSV reader is required"); return false; }
	pHandle->HasRow = xrtCsvReaderNext(pHandle->Reader, &pHandle->Row);
	if ( !pHandle->HasRow && !xrtCsvReaderEOF(pHandle->Reader) ) {
		xcsv_publish_error();
	}
	return pHandle->HasRow;
}

bool xcsv_reader_eof(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && xrtCsvReaderEOF(pHandle->Reader);
}

int64 xcsv_reader_row(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasRow ? (int64)pHandle->Row.iRow : -1;
}

int64 xcsv_reader_count(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasRow ? (int64)pHandle->Row.iCount : -1;
}

bool xcsv_reader_close(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	bool bOk;
	if ( pHandle == NULL ) { xcsv_error(XERR_ARGUMENT, "CSV reader is required"); return false; }
	bOk = xrtCsvReaderClose(pHandle->Reader);
	pHandle->HasRow = false;
	xrtFree(pHandle->Input);
	pHandle->Input = NULL;
	if ( !bOk ) { xcsv_publish_error(); }
	return bOk;
}

void* xcsv_reader_field(void* pValue, int64 iIndex)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	const char* sValue;
	xrtClearError();
	if ( pHandle == NULL || !pHandle->HasRow ) {
		xcsv_error(XERR_STATE, "CSV reader has no current row");
		return NULL;
	}
	if ( iIndex < 0 || (uint64)iIndex >= pHandle->Row.iCount ) {
		xcsv_error(XERR_RANGE, "CSV field index is out of range");
		xcsv_publish_kind("IndexError");
		return NULL;
	}
	sValue = pHandle->Row.pFields[iIndex];
	return xrtStrDup(sValue != NULL ? sValue : "");
}



static void xcsv_writer_clear_fields(xcsv_writer_handle* pHandle)
{
	uint32 i;
	if ( pHandle == NULL ) { return; }
	for ( i = 0u; i < pHandle->Count; ++i ) { xrtFree(pHandle->Fields[i]); }
	pHandle->Count = 0u;
	pHandle->RowBytes = 0u;
}

/* 暂存行失败也使写入器失败，不能忽略某次 field 失败后提交残缺行。 */
static bool xcsv_writer_failure(xcsv_writer_handle* pHandle, int iCode, cstr sMessage)
{
	__xrtCsvSetWriterError(pHandle->Writer, iCode, "%s", sMessage);
	xcsv_writer_clear_fields(pHandle);
	pHandle->RowOpen = false;
	return false;
}

static void* xcsv_validate_writer(xrtcsvwriter* pWriter, bool bMemory)
{
	xcsv_writer_handle* pHandle;
	const xrtcsverror* pError = xrtCsvWriterError(pWriter);
	if ( pWriter == NULL || (pError != NULL && pError->iCode != XRT_CSV_OK) ) {
		xcsv_publish_error();
		xrtCsvWriterDestroy(pWriter);
		return NULL;
	}
	pHandle = (xcsv_writer_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtCsvWriterDestroy(pWriter);
		xcsv_error(XERR_MEMORY, "cannot allocate CSV writer handle");
		return NULL;
	}
	pHandle->Writer = pWriter;
	pHandle->Memory = bMemory;
	return pHandle;
}

void* xcsv_writer_create_memory(cstr sDelimiter, cstr sQuote, int iNewline,
	bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_writer_dialect(&tDialect, sDelimiter, sQuote, iNewline, bQuoteAll,
		bWriteBOM, iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_writer(xrtCsvWriterCreateMemory(&tDialect), true);
}

void* xcsv_writer_open_file(cstr sPath, cstr sDelimiter, cstr sQuote,
	int iNewline, bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_writer_dialect(&tDialect, sDelimiter, sQuote, iNewline, bQuoteAll,
		bWriteBOM, iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_writer(xrtCsvWriterOpenFile(sPath, &tDialect), false);
}

void xcsv_writer_destroy(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	if ( pHandle == NULL ) { return; }
	xcsv_writer_clear_fields(pHandle);
	xrtFree(pHandle->Fields);
	xrtCsvWriterDestroy(pHandle->Writer);
	xrtFree(pHandle);
}

bool xcsv_writer_begin(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	xrtClearError();
	if ( pHandle == NULL || pHandle->Writer->Closed ) {
		xcsv_error(XERR_STATE, "CSV writer is not open"); return false;
	}
	if ( pHandle->Writer->Failed ) {
		(void)__xrtCsvFailed(pHandle->Writer->Failure); xcsv_publish_error(); return false;
	}
	/* 调用顺序错误不丢弃已收集的字段，调用者仍可结束当前行。 */
	if ( pHandle->RowOpen ) {
		xcsv_error(XERR_STATE, "CSV writer row is already open"); return false;
	}
	xcsv_writer_clear_fields(pHandle);
	pHandle->RowOpen = true;
	return true;
}

bool xcsv_writer_field(void* pValue, cstr sValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	str sCopy;
	size_t iLength = sValue != NULL ? strlen(sValue) : 0u;
	const xrtresourcelimits* pLimits;
	xrtClearError();
	if ( pHandle != NULL && pHandle->Writer->Failed ) {
		return __xrtCsvFailed(pHandle->Writer->Failure);
	}
	if ( pHandle == NULL || !pHandle->RowOpen ) {
		xcsv_error(XERR_STATE, "CSV writer row is not open");
		return false;
	}
	pLimits = &pHandle->Writer->Dialect.tLimits;
	if ( (pLimits->iMaxItemBytes != 0u && iLength > pLimits->iMaxItemBytes) ||
		(pLimits->iMaxNodes != 0u && pHandle->Writer->TotalFields + pHandle->Count >= pLimits->iMaxNodes) ||
		pHandle->RowBytes > UINT64_MAX - iLength ||
		(pLimits->iMaxInputBytes != 0u &&
		 ((uint64)iLength > pLimits->iMaxInputBytes ||
		  pHandle->RowBytes > pLimits->iMaxInputBytes - iLength ||
		  pHandle->Writer->InputBytes > pLimits->iMaxInputBytes - iLength - pHandle->RowBytes)) ) {
		return xcsv_writer_failure(pHandle, XRT_CSV_ERROR_LIMIT, "CSV row exceeds the configured limit");
	}
	if ( pHandle->Count == pHandle->Capacity ) {
		uint64 iCapacity = pHandle->Capacity != 0u ? (uint64)pHandle->Capacity * 2u : 16u;
		str* pFields;
		if ( iCapacity > UINT32_MAX || iCapacity > SIZE_MAX / sizeof(str) ) {
			return xcsv_writer_failure(pHandle, XRT_CSV_ERROR_LIMIT, "too many CSV fields");
		}
		pFields = (str*)xrtRealloc(pHandle->Fields, (size_t)iCapacity * sizeof(str));
		if ( pFields == NULL ) {
			return xcsv_writer_failure(pHandle, XRT_CSV_ERROR_MEMORY, "cannot grow CSV writer row");
		}
		pHandle->Fields = pFields;
		pHandle->Capacity = (uint32)iCapacity;
	}
	sCopy = xrtStrDup(sValue != NULL ? sValue : "");
	if ( sCopy == NULL ) {
		return xcsv_writer_failure(pHandle, XRT_CSV_ERROR_MEMORY, "cannot copy CSV field");
	}
	pHandle->Fields[pHandle->Count++] = sCopy;
	pHandle->RowBytes += iLength;
	return true;
}

bool xcsv_writer_end(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	bool bResult;
	if ( pHandle != NULL && pHandle->Writer->Failed ) {
		(void)__xrtCsvFailed(pHandle->Writer->Failure); xcsv_publish_error(); return false;
	}
	if ( pHandle == NULL || !pHandle->RowOpen ) {
		xcsv_error(XERR_STATE, "CSV writer row is not open");
		return false;
	}
	bResult = xrtCsvWriterWrite(pHandle->Writer, pHandle->Count,
		(const str*)pHandle->Fields);
	pHandle->RowOpen = false;
	xcsv_writer_clear_fields(pHandle);
	if ( !bResult ) { xcsv_publish_error(); }
	return bResult;
}

bool xcsv_writer_close(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	if ( pHandle != NULL && pHandle->Writer->Failed ) {
		(void)__xrtCsvFailed(pHandle->Writer->Failure); xcsv_publish_error(); return false;
	}
	if ( pHandle == NULL || pHandle->RowOpen ) {
		xcsv_error(XERR_STATE, "cannot close CSV writer while a row is open");
		return false;
	}
	if ( !xrtCsvWriterClose(pHandle->Writer) ) {
		xcsv_publish_error();
		return false;
	}
	return true;
}

void* xcsv_writer_take_buffer(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	xbuffer* pResult;
	if ( pHandle != NULL && pHandle->Writer->Failed ) {
		(void)__xrtCsvFailed(pHandle->Writer->Failure); xcsv_publish_error(); return NULL;
	}
	if ( pHandle == NULL || !pHandle->Memory || pHandle->RowOpen ) {
		xcsv_error(XERR_STATE, "CSV writer has no available memory result");
		return NULL;
	}
	if ( !xrtCsvWriterClose(pHandle->Writer) ) {
		xcsv_publish_error();
		return NULL;
	}
	pResult = xrtCsvWriterTakeBuffer(pHandle->Writer);
	if ( pResult == NULL ) { xcsv_publish_error(); }
	return pResult;
}
