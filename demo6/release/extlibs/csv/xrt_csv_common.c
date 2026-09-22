#include "xrt_csv_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



XRT_API void xrtCsvDialectInit(xrtcsvdialect* pDialect)
{
	if ( pDialect == NULL ) { return; }
	memset(pDialect, 0, sizeof(*pDialect));
	pDialect->iSize = sizeof(*pDialect);
	pDialect->iVersion = XRT_CSV_DIALECT_VERSION;
	pDialect->iDelimiter = ',';
	pDialect->iQuote = '"';
	pDialect->iNewline = XRT_CSV_NEWLINE_LF;
	pDialect->bStrict = true;
	xrtResourceLimitsInit(&pDialect->tLimits);
}

XRT_API void xrtCsvErrorInit(xrtcsverror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
}

bool __xrtCsvCopyDialect(xrtcsvdialect* pOutput, const xrtcsvdialect* pInput)
{
	xrtCsvDialectInit(pOutput);
	if ( pInput != NULL ) {
		if ( pInput->iVersion != XRT_CSV_DIALECT_VERSION ||
			pInput->iSize < offsetof(xrtcsvdialect, tLimits) + sizeof(xrtresourcelimits) ) {
			return false;
		}
		memcpy(pOutput, pInput, pInput->iSize < sizeof(*pOutput) ?
			pInput->iSize : sizeof(*pOutput));
		pOutput->iSize = sizeof(*pOutput);
		pOutput->iVersion = XRT_CSV_DIALECT_VERSION;
	}
	return pOutput->iDelimiter >= 0x20 && pOutput->iDelimiter <= 0x7e &&
		pOutput->iQuote >= 0x20 && pOutput->iQuote <= 0x7e &&
		pOutput->iDelimiter != pOutput->iQuote &&
		pOutput->iDelimiter != '\r' && pOutput->iDelimiter != '\n' &&
		pOutput->iQuote != '\r' && pOutput->iQuote != '\n' &&
		(pOutput->iNewline == XRT_CSV_NEWLINE_LF ||
		 pOutput->iNewline == XRT_CSV_NEWLINE_CRLF);
}

static void __xrtCsvSetError(xrtcsverror* pError, int iCode, uint64 iRow,
	uint64 iColumn, uint64 iOffset, const char* sFormat, va_list tArgs)
{
	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iRow = iRow;
	pError->iColumn = iColumn;
	pError->iOffset = iOffset;
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1u] = 0;
	/* 结构化错误保存在 Reader/Writer 上，同时发布到当前 XRT 上下文。 */
	xrtSetErrorInfo(XERR_STATE, "xlang.extlib.csv", iCode,
		pError->sMessage);
}

void __xrtCsvSetReaderError(xrtcsvreader* pReader, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pReader == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtCsvSetError(&pReader->Error, iCode, pReader->RowNumber,
		pReader->Column, pReader->Offset, sFormat, tArgs);
	va_end(tArgs);
	pReader->Failed = true;
}

void __xrtCsvSetWriterError(xrtcsvwriter* pWriter, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pWriter == NULL ) { return; }
	va_start(tArgs, sFormat);
	__xrtCsvSetError(&pWriter->Error, iCode, pWriter->RowNumber,
		0u, pWriter->OutputBytes, sFormat, tArgs);
	va_end(tArgs);
	pWriter->Failed = true;
}

bool __xrtCsvWriterWriteAll(xrtcsvwriter* pWriter, const void* pData, uint64 iSize)
{
	if ( pWriter == NULL || pWriter->Writer == NULL || pWriter->Failed || pWriter->Closed ) {
		return false;
	}
	if ( pWriter->Dialect.tLimits.iMaxOutputBytes != 0u &&
		(iSize > pWriter->Dialect.tLimits.iMaxOutputBytes ||
		 pWriter->OutputBytes > pWriter->Dialect.tLimits.iMaxOutputBytes - iSize) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV output exceeds the configured limit");
		return false;
	}
	if ( !xrtWriterWriteFull(pWriter->Writer, pData, (size_t)iSize, NULL) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_IO, "cannot write CSV stream");
		return false;
	}
	pWriter->OutputBytes += iSize;
	return true;
}
