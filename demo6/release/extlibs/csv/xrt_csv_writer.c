#include "xrt_csv_internal.h"

#include <string.h>



static bool __xrtCsvFieldNeedsQuote(const xrtcsvwriter* pWriter, const char* sField)
{
	const unsigned char* p = (const unsigned char*)sField;
	if ( pWriter->Dialect.bQuoteAll ) { return TRUE; }
	while ( *p != 0u ) {
		if ( *p == (unsigned char)pWriter->Dialect.iDelimiter ||
			*p == (unsigned char)pWriter->Dialect.iQuote || *p == '\r' || *p == '\n' ) {
			return TRUE;
		}
		++p;
	}
	return FALSE;
}

static bool __xrtCsvWriteField(xrtcsvwriter* pWriter, const char* sField)
{
	const unsigned char* p = (const unsigned char*)(sField != NULL ? sField : "");
	bool bQuote = __xrtCsvFieldNeedsQuote(pWriter, (const char*)p);
	unsigned char iQuote = (unsigned char)pWriter->Dialect.iQuote;
	if ( bQuote && !__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ) { return FALSE; }
	while ( *p != 0u ) {
		const unsigned char* pStart = p;
		while ( *p != 0u && (!bQuote || *p != iQuote) ) { ++p; }
		if ( p != pStart && !__xrtCsvWriterWriteAll(pWriter, pStart, (uint64)(p - pStart)) ) {
			return FALSE;
		}
		if ( *p == iQuote ) {
			if ( !__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ||
				!__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ) { return FALSE; }
			++p;
		}
	}
	return !bQuote || __xrtCsvWriterWriteAll(pWriter, &iQuote, 1u);
}

XXAPI xrtcsvwriter* xrtCsvWriterCreate(xwriter pOutput, bool bTakeOwnership,
	const xrtcsvdialect* pDialect)
{
	static const unsigned char sBOM[] = { 0xefu, 0xbbu, 0xbfu };
	xrtcsvwriter* pWriter = (xrtcsvwriter*)xrtCalloc(1u, sizeof(*pWriter));
	if ( pWriter == NULL ) { return NULL; }
	xrtCsvErrorInit(&pWriter->Error);
	pWriter->Writer = pOutput;
	pWriter->TakeWriter = bTakeOwnership;
	if ( pOutput == NULL || !__xrtCsvCopyDialect(&pWriter->Dialect, pDialect) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_ARGUMENT,
			"invalid CSV writer or dialect");
		return pWriter;
	}
	if ( pWriter->Dialect.bWriteBOM &&
		!__xrtCsvWriterWriteAll(pWriter, sBOM, sizeof(sBOM)) ) { return pWriter; }
	return pWriter;
}

XXAPI xrtcsvwriter* xrtCsvWriterCreateMemory(const xrtcsvdialect* pDialect)
{
	xbuffer pBuffer = xrtBufferCreate(4096u);
	xwriter pOutput = pBuffer != NULL ? xrtWriterFromBuffer(pBuffer, FALSE) : NULL;
	xrtcsvwriter* pWriter = xrtCsvWriterCreate(pOutput, TRUE, pDialect);
	if ( pWriter != NULL ) { pWriter->Memory = pBuffer; }
	else {
		xrtWriterDestroy(pOutput);
		xrtBufferDestroy(pBuffer);
	}
	return pWriter;
}

XXAPI xrtcsvwriter* xrtCsvWriterOpenFile(str sPath, const xrtcsvdialect* pDialect)
{
	xwriter pOutput = sPath != NULL ? xrtWriterOpen(sPath, TRUE) : NULL;
	return xrtCsvWriterCreate(pOutput, TRUE, pDialect);
}

XXAPI const xrtcsverror* xrtCsvWriterError(const xrtcsvwriter* pWriter)
{
	return pWriter != NULL ? &pWriter->Error : NULL;
}

XXAPI bool xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields)
{
	uint64 iInput = 0u;
	uint32 i;
	const char* sNewline;
	size_t iNewline;
	if ( pWriter == NULL || pWriter->Failed || pWriter->Closed ||
		(pFields == NULL && iCount != 0u) ) { return FALSE; }
	if ( pWriter->Dialect.tLimits.iMaxEntries != 0u &&
		pWriter->RowNumber >= pWriter->Dialect.tLimits.iMaxEntries ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV row count exceeds the configured limit");
		return FALSE;
	}
	if ( pWriter->Dialect.tLimits.iMaxNodes != 0u &&
		((uint64)iCount > pWriter->Dialect.tLimits.iMaxNodes ||
		 pWriter->TotalFields > pWriter->Dialect.tLimits.iMaxNodes - (uint64)iCount) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV field count exceeds the configured limit");
		return FALSE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		size_t iLength = pFields[i] != NULL ? strlen((const char*)pFields[i]) : 0u;
		if ( (pFields[i] != NULL && !xrtIsUTF8(pFields[i], iLength)) ||
			(pWriter->Dialect.tLimits.iMaxItemBytes != 0u &&
			 iLength > pWriter->Dialect.tLimits.iMaxItemBytes) ||
			iInput > UINT64_MAX - iLength ) {
			__xrtCsvSetWriterError(pWriter,
				pFields[i] != NULL && !xrtIsUTF8(pFields[i], iLength) ?
				XRT_CSV_ERROR_FORMAT : XRT_CSV_ERROR_LIMIT,
				"invalid or oversized CSV field");
			return FALSE;
		}
		iInput += iLength;
	}
	if ( pWriter->Dialect.tLimits.iMaxInputBytes != 0u &&
		(iInput > pWriter->Dialect.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Dialect.tLimits.iMaxInputBytes - iInput) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV input exceeds the configured limit");
		return FALSE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		unsigned char iDelimiter = (unsigned char)pWriter->Dialect.iDelimiter;
		if ( i != 0u && !__xrtCsvWriterWriteAll(pWriter, &iDelimiter, 1u) ) { return FALSE; }
		if ( !__xrtCsvWriteField(pWriter, (const char*)pFields[i]) ) { return FALSE; }
	}
	sNewline = pWriter->Dialect.iNewline == XRT_CSV_NEWLINE_CRLF ? "\r\n" : "\n";
	iNewline = pWriter->Dialect.iNewline == XRT_CSV_NEWLINE_CRLF ? 2u : 1u;
	if ( !__xrtCsvWriterWriteAll(pWriter, sNewline, iNewline) ) { return FALSE; }
	pWriter->InputBytes += iInput;
	pWriter->TotalFields += iCount;
	pWriter->RowNumber += 1u;
	return TRUE;
}

XXAPI bool xrtCsvWriterClose(xrtcsvwriter* pWriter)
{
	if ( pWriter == NULL || pWriter->Failed ) { return FALSE; }
	if ( pWriter->Closed ) { return TRUE; }
	if ( !xrtWriterFlush(pWriter->Writer) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_IO,
			"cannot flush CSV stream");
		return FALSE;
	}
	pWriter->Closed = TRUE;
	return TRUE;
}

XXAPI xbuffer xrtCsvWriterTakeBuffer(xrtcsvwriter* pWriter)
{
	xbuffer pResult;
	if ( pWriter == NULL || !pWriter->Closed || pWriter->Memory == NULL ) { return NULL; }
	pResult = pWriter->Memory;
	pWriter->Memory = NULL;
	return pResult;
}

XXAPI void xrtCsvWriterDestroy(xrtcsvwriter* pWriter)
{
	if ( pWriter == NULL ) { return; }
	if ( !pWriter->Closed && !pWriter->Failed ) { xrtCsvWriterClose(pWriter); }
	if ( pWriter->TakeWriter && pWriter->Writer != NULL ) { xrtWriterDestroy(pWriter->Writer); }
	xrtBufferDestroy(pWriter->Memory);
	xrtFree(pWriter);
}
