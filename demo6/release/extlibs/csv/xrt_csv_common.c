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
	pDialect->tLimits.iMaxDepth = 0u;
	pDialect->tLimits.iMaxCompressionRatio = 0u;
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
		if ( pInput->iVersion != XRT_CSV_DIALECT_VERSION || pInput->iSize != sizeof(*pInput) ||
			pInput->bReserved || pInput->tLimits.iSize != sizeof(pInput->tLimits) ||
			pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
			pInput->tLimits.iMaxDepth != 0u ||
			pInput->tLimits.iMaxCompressionRatio != 0u ||
			pInput->tLimits.iFlags != 0u || pInput->tLimits.iReserved != 0u ) {
			return false;
		}
		*pOutput = *pInput;
	}
	return (pOutput->iDelimiter == '\t' ||
		(pOutput->iDelimiter >= 0x20 && pOutput->iDelimiter <= 0x7e)) &&
		pOutput->iQuote >= 0x20 && pOutput->iQuote <= 0x7e &&
		pOutput->iDelimiter != pOutput->iQuote &&
		pOutput->iDelimiter != '\r' && pOutput->iDelimiter != '\n' &&
		pOutput->iQuote != '\r' && pOutput->iQuote != '\n' &&
		(pOutput->iNewline == XRT_CSV_NEWLINE_LF ||
		 pOutput->iNewline == XRT_CSV_NEWLINE_CRLF);
}

static xerrkind __xrtCsvErrorKind(int iCode)
{
	switch ( iCode ) {
	case XRT_CSV_ERROR_ARGUMENT: return XERR_ARGUMENT;
	case XRT_CSV_ERROR_MEMORY: return XERR_MEMORY;
	case XRT_CSV_ERROR_FORMAT: return XERR_VALUE;
	case XRT_CSV_ERROR_IO: return XERR_IO;
	case XRT_CSV_ERROR_LIMIT: return XERR_RANGE;
	default: return XERR_STATE;
	}
}

/* 保留流/分配器的第一处原因；CSV 位置是独立的输入位置，不是 C 源码位置。 */
static void __xrtCsvSetError(xrtcsverror* pError, int iCode, uint64 iRow,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sOperation,
	const char* sFormat, va_list tArgs)
{
	char sData[192];
	xerrordesc tDesc = {0};
	xerror* pCause;
	xerror* pPublished;
	if ( pError == NULL ) { return; }
	pCause = xrtTakeError();
	if ( iCode == XRT_CSV_ERROR_IO && xrtErrorKind(pCause) == XERR_MEMORY ) {
		iCode = XRT_CSV_ERROR_MEMORY;
	}
	pError->iCode = iCode;
	pError->iRow = iRow;
	pError->iLine = iLine;
	pError->iColumn = iColumn;
	pError->iOffset = iOffset;
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1u] = 0;
	(void)snprintf(sData, sizeof(sData),
		"{\"row\":%llu,\"line\":%llu,\"column\":%llu,\"offset\":%llu}",
		(unsigned long long)iRow, (unsigned long long)iLine,
		(unsigned long long)iColumn, (unsigned long long)iOffset);
	tDesc.Kind = __xrtCsvErrorKind(iCode);
	tDesc.Code = iCode;
	tDesc.Domain = "xrt.csv";
	tDesc.Operation = sOperation;
	tDesc.Message = pError->sMessage;
	tDesc.Data = sData;
	if ( iCode == XRT_CSV_ERROR_IO || iCode == XRT_CSV_ERROR_MEMORY ) {
		tDesc.Cause = pCause;
		tDesc.SystemCode = xrtErrorSystemCode(pCause);
	}
	pPublished = xrtErrorBuild(&tDesc);
	if ( pPublished != NULL ) { xrtSetErrorTake(pPublished); }
	xrtErrorFree(pCause);
}

bool __xrtCsvFailed(const xerror* pError)
{
	xrtSetError(pError);
	return false;
}

void __xrtCsvSetReaderError(xrtcsvreader* pReader, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pReader == NULL ) { return; }
	if ( pReader->Failed ) { (void)__xrtCsvFailed(pReader->Failure); return; }
	va_start(tArgs, sFormat);
	__xrtCsvSetError(&pReader->Error, iCode, pReader->RowNumber,
		pReader->LastLine, pReader->LastColumn, pReader->LastOffset,
		"read", sFormat, tArgs);
	va_end(tArgs);
	pReader->Failed = true;
	pReader->Failure = xrtErrorRef(xrtGetError());
}

void __xrtCsvSetWriterError(xrtcsvwriter* pWriter, int iCode, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pWriter == NULL ) { return; }
	if ( pWriter->Failed ) { (void)__xrtCsvFailed(pWriter->Failure); return; }
	va_start(tArgs, sFormat);
	__xrtCsvSetError(&pWriter->Error, iCode, pWriter->RowNumber,
		0u, 0u, pWriter->OutputBytes, "write", sFormat, tArgs);
	va_end(tArgs);
	pWriter->Failed = true;
	pWriter->Failure = xrtErrorRef(xrtGetError());
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
	if ( pWriter->Failed ) { return __xrtCsvFailed(pWriter->Failure); }
	pWriter->OutputBytes += iSize;
	return true;
}
