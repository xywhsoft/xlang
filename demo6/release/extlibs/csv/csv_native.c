#include "include/xrt_csv.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XCSV_EXPORT __declspec(dllexport)
#else
#define XCSV_EXPORT
#endif



typedef struct {
	xrtcsvreader* Reader;
	xrtcsvrow Row;
	bool HasRow;
} xcsv_reader_handle;

typedef struct {
	xrtcsvwriter* Writer;
	str* Fields;
	uint32 Count;
	uint32 Capacity;
	bool RowOpen;
	bool Memory;
} xcsv_writer_handle;



static void xcsv_publish_reader_error(const xrtcsvreader* pReader)
{
	const xrtcsverror* pError = xrtCsvReaderError(pReader);
	xrtSetError(xrtFormat("%s", pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "CSV reader operation failed"), TRUE);
}

static void xcsv_publish_writer_error(const xrtcsvwriter* pWriter)
{
	const xrtcsverror* pError = xrtCsvWriterError(pWriter);
	xrtSetError(xrtFormat("%s", pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "CSV writer operation failed"), TRUE);
}

static bool xcsv_set_character(char* pOutput, str sValue, const char* sName)
{
	if ( sValue == NULL || strlen((const char*)sValue) != 1u ||
		(unsigned char)sValue[0] < 0x20u || (unsigned char)sValue[0] > 0x7eu ) {
		xrtSetError(xrtFormat("CSV %s must be one printable ASCII character", sName), TRUE);
		return FALSE;
	}
	*pOutput = (char)sValue[0];
	return TRUE;
}

static bool xcsv_reader_dialect(xrtcsvdialect* pDialect, str sDelimiter, str sQuote,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxRows, uint64 iMaxFields)
{
	xrtCsvDialectInit(pDialect);
	if ( !xcsv_set_character(&pDialect->iDelimiter, sDelimiter, "delimiter") ||
		!xcsv_set_character(&pDialect->iQuote, sQuote, "quote") ) { return FALSE; }
	pDialect->bStrict = bStrict;
	if ( iMaxInput != 0u ) { pDialect->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pDialect->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pDialect->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxRows != 0u ) { pDialect->tLimits.iMaxEntries = iMaxRows; }
	if ( iMaxFields != 0u ) { pDialect->tLimits.iMaxNodes = iMaxFields; }
	return TRUE;
}

static bool xcsv_writer_dialect(xrtcsvdialect* pDialect, str sDelimiter, str sQuote,
	int iNewline, bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtCsvDialectInit(pDialect);
	if ( !xcsv_set_character(&pDialect->iDelimiter, sDelimiter, "delimiter") ||
		!xcsv_set_character(&pDialect->iQuote, sQuote, "quote") ||
		(iNewline != XRT_CSV_NEWLINE_LF && iNewline != XRT_CSV_NEWLINE_CRLF) ) {
		if ( iNewline != XRT_CSV_NEWLINE_LF && iNewline != XRT_CSV_NEWLINE_CRLF ) {
			xrtSetError(xrtFormat("CSV newline must be LF or CRLF"), TRUE);
		}
		return FALSE;
	}
	pDialect->iNewline = iNewline;
	pDialect->bQuoteAll = bQuoteAll;
	pDialect->bWriteBOM = bWriteBOM;
	if ( iMaxInput != 0u ) { pDialect->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pDialect->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxItem != 0u ) { pDialect->tLimits.iMaxItemBytes = iMaxItem; }
	if ( iMaxRows != 0u ) { pDialect->tLimits.iMaxEntries = iMaxRows; }
	if ( iMaxFields != 0u ) { pDialect->tLimits.iMaxNodes = iMaxFields; }
	return TRUE;
}



static void* xcsv_validate_reader(xrtcsvreader* pReader)
{
	xcsv_reader_handle* pHandle;
	const xrtcsverror* pError = xrtCsvReaderError(pReader);
	if ( pReader == NULL || (pError != NULL && pError->iCode != XRT_CSV_OK) ) {
		xcsv_publish_reader_error(pReader);
		xrtCsvReaderDestroy(pReader);
		return NULL;
	}
	pHandle = (xcsv_reader_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtCsvReaderDestroy(pReader);
		xrtSetError(xrtFormat("cannot allocate CSV reader handle"), TRUE);
		return NULL;
	}
	pHandle->Reader = pReader;
	return pHandle;
}

XCSV_EXPORT void* xcsv_reader_open_memory(const void* pData, uint64 iSize,
	str sDelimiter, str sQuote, bool bStrict, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_reader_dialect(&tDialect, sDelimiter, sQuote, bStrict,
		iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_reader(xrtCsvReaderOpenMemory(pData, iSize, &tDialect));
}

XCSV_EXPORT void* xcsv_reader_open_file(str sPath, str sDelimiter, str sQuote,
	bool bStrict, uint64 iMaxInput, uint64 iMaxOutput, uint64 iMaxItem,
	uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_reader_dialect(&tDialect, sDelimiter, sQuote, bStrict,
		iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_reader(xrtCsvReaderOpenFile(sPath, &tDialect));
}

XCSV_EXPORT void xcsv_reader_destroy(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	if ( pHandle == NULL ) { return; }
	xrtCsvReaderDestroy(pHandle->Reader);
	xrtFree(pHandle);
}

XCSV_EXPORT bool xcsv_reader_next(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	if ( pHandle == NULL ) { return FALSE; }
	pHandle->HasRow = xrtCsvReaderNext(pHandle->Reader, &pHandle->Row);
	if ( !pHandle->HasRow && !xrtCsvReaderEOF(pHandle->Reader) ) {
		xcsv_publish_reader_error(pHandle->Reader);
	}
	return pHandle->HasRow;
}

XCSV_EXPORT bool xcsv_reader_eof(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && xrtCsvReaderEOF(pHandle->Reader);
}

XCSV_EXPORT int64 xcsv_reader_row(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasRow ? (int64)pHandle->Row.iRow : -1;
}

XCSV_EXPORT int xcsv_reader_count(void* pValue)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	return pHandle != NULL && pHandle->HasRow ? (int)pHandle->Row.iCount : -1;
}

XCSV_EXPORT void* xcsv_reader_field(void* pValue, int iIndex)
{
	xcsv_reader_handle* pHandle = (xcsv_reader_handle*)pValue;
	const char* sValue;
	if ( pHandle == NULL || !pHandle->HasRow || iIndex < 0 ||
		(uint32)iIndex >= pHandle->Row.iCount ) {
		xrtSetError(xrtFormat("CSV field index is out of range"), TRUE);
		return NULL;
	}
	sValue = pHandle->Row.pFields[iIndex];
	return xrtCopyStr((str)(sValue != NULL ? sValue : ""), 0u);
}



static void xcsv_writer_clear_fields(xcsv_writer_handle* pHandle)
{
	uint32 i;
	if ( pHandle == NULL ) { return; }
	for ( i = 0u; i < pHandle->Count; ++i ) { xrtFree(pHandle->Fields[i]); }
	pHandle->Count = 0u;
}

static void* xcsv_validate_writer(xrtcsvwriter* pWriter, bool bMemory)
{
	xcsv_writer_handle* pHandle;
	const xrtcsverror* pError = xrtCsvWriterError(pWriter);
	if ( pWriter == NULL || (pError != NULL && pError->iCode != XRT_CSV_OK) ) {
		xcsv_publish_writer_error(pWriter);
		xrtCsvWriterDestroy(pWriter);
		return NULL;
	}
	pHandle = (xcsv_writer_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		xrtCsvWriterDestroy(pWriter);
		xrtSetError(xrtFormat("cannot allocate CSV writer handle"), TRUE);
		return NULL;
	}
	pHandle->Writer = pWriter;
	pHandle->Memory = bMemory;
	return pHandle;
}

XCSV_EXPORT void* xcsv_writer_create_memory(str sDelimiter, str sQuote, int iNewline,
	bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput, uint64 iMaxOutput,
	uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_writer_dialect(&tDialect, sDelimiter, sQuote, iNewline, bQuoteAll,
		bWriteBOM, iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_writer(xrtCsvWriterCreateMemory(&tDialect), TRUE);
}

XCSV_EXPORT void* xcsv_writer_open_file(str sPath, str sDelimiter, str sQuote,
	int iNewline, bool bQuoteAll, bool bWriteBOM, uint64 iMaxInput,
	uint64 iMaxOutput, uint64 iMaxItem, uint64 iMaxRows, uint64 iMaxFields)
{
	xrtcsvdialect tDialect;
	xrtClearError();
	if ( !xcsv_writer_dialect(&tDialect, sDelimiter, sQuote, iNewline, bQuoteAll,
		bWriteBOM, iMaxInput, iMaxOutput, iMaxItem, iMaxRows, iMaxFields) ) { return NULL; }
	return xcsv_validate_writer(xrtCsvWriterOpenFile(sPath, &tDialect), FALSE);
}

XCSV_EXPORT void xcsv_writer_destroy(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	if ( pHandle == NULL ) { return; }
	xcsv_writer_clear_fields(pHandle);
	xrtFree(pHandle->Fields);
	xrtCsvWriterDestroy(pHandle->Writer);
	xrtFree(pHandle);
}

XCSV_EXPORT bool xcsv_writer_begin(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	if ( pHandle == NULL ) { return FALSE; }
	xcsv_writer_clear_fields(pHandle);
	pHandle->RowOpen = TRUE;
	return TRUE;
}

XCSV_EXPORT bool xcsv_writer_field(void* pValue, str sValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	str sCopy;
	if ( pHandle == NULL || !pHandle->RowOpen ) {
		xrtSetError(xrtFormat("CSV writer row is not open"), TRUE);
		return FALSE;
	}
	if ( pHandle->Count == pHandle->Capacity ) {
		uint32 iCapacity = pHandle->Capacity != 0u ? pHandle->Capacity * 2u : 16u;
		str* pFields;
		if ( iCapacity < pHandle->Capacity ) {
			xrtSetError(xrtFormat("too many CSV fields"), TRUE);
			return FALSE;
		}
		pFields = (str*)xrtRealloc(pHandle->Fields, (size_t)iCapacity * sizeof(str));
		if ( pFields == NULL ) {
			xrtSetError(xrtFormat("cannot grow CSV writer row"), TRUE);
			return FALSE;
		}
		pHandle->Fields = pFields;
		pHandle->Capacity = iCapacity;
	}
	sCopy = xrtCopyStr(sValue != NULL ? sValue : (str)"", 0u);
	if ( sCopy == NULL ) {
		xrtSetError(xrtFormat("cannot copy CSV field"), TRUE);
		return FALSE;
	}
	pHandle->Fields[pHandle->Count++] = sCopy;
	return TRUE;
}

XCSV_EXPORT bool xcsv_writer_end(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	bool bResult;
	if ( pHandle == NULL || !pHandle->RowOpen ) {
		xrtSetError(xrtFormat("CSV writer row is not open"), TRUE);
		return FALSE;
	}
	bResult = xrtCsvWriterWrite(pHandle->Writer, pHandle->Count,
		(const str*)pHandle->Fields);
	pHandle->RowOpen = FALSE;
	xcsv_writer_clear_fields(pHandle);
	if ( !bResult ) { xcsv_publish_writer_error(pHandle->Writer); }
	return bResult;
}

XCSV_EXPORT bool xcsv_writer_close(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	if ( pHandle == NULL || pHandle->RowOpen ) {
		xrtSetError(xrtFormat("cannot close CSV writer while a row is open"), TRUE);
		return FALSE;
	}
	if ( !xrtCsvWriterClose(pHandle->Writer) ) {
		xcsv_publish_writer_error(pHandle->Writer);
		return FALSE;
	}
	return TRUE;
}

XCSV_EXPORT void* xcsv_writer_take_buffer(void* pValue)
{
	xcsv_writer_handle* pHandle = (xcsv_writer_handle*)pValue;
	xbuffer pResult;
	if ( pHandle == NULL || !pHandle->Memory || pHandle->RowOpen ) {
		xrtSetError(xrtFormat("CSV writer has no available memory result"), TRUE);
		return NULL;
	}
	if ( !xrtCsvWriterClose(pHandle->Writer) ) {
		xcsv_publish_writer_error(pHandle->Writer);
		return NULL;
	}
	pResult = xrtCsvWriterTakeBuffer(pHandle->Writer);
	if ( pResult == NULL ) { xrtSetError(xrtFormat("CSV writer result was already taken"), TRUE); }
	return pResult;
}
