#include "xrt_csv_internal.h"

#include <string.h>



static bool __xrtCsvFieldNeedsQuote(const xrtcsvwriter* pWriter, const char* sField)
{
	const unsigned char* p = (const unsigned char*)sField;
	if ( pWriter->Dialect.bQuoteAll ) { return true; }
	while ( *p != 0u ) {
		if ( *p == (unsigned char)pWriter->Dialect.iDelimiter ||
			*p == (unsigned char)pWriter->Dialect.iQuote || *p == '\r' || *p == '\n' ) {
			return true;
		}
		++p;
	}
	return false;
}

static bool __xrtCsvWriteField(xrtcsvwriter* pWriter, const char* sField, bool bForceQuote)
{
	const unsigned char* p = (const unsigned char*)(sField != NULL ? sField : "");
	bool bQuote = bForceQuote || __xrtCsvFieldNeedsQuote(pWriter, (const char*)p);
	unsigned char iQuote = (unsigned char)pWriter->Dialect.iQuote;
	if ( bQuote && !__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ) { return false; }
	while ( *p != 0u ) {
		const unsigned char* pStart = p;
		while ( *p != 0u && (!bQuote || *p != iQuote) ) { ++p; }
		if ( p != pStart && !__xrtCsvWriterWriteAll(pWriter, pStart, (uint64)(p - pStart)) ) {
			return false;
		}
		if ( *p == iQuote ) {
			if ( !__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ||
				!__xrtCsvWriterWriteAll(pWriter, &iQuote, 1u) ) { return false; }
			++p;
		}
	}
	return !bQuote || __xrtCsvWriterWriteAll(pWriter, &iQuote, 1u);
}

XRT_API xrtcsvwriter* xrtCsvWriterCreate(xwriter* pOutput, bool bTakeOwnership,
	const xrtcsvdialect* pDialect)
{
	static const unsigned char sBOM[] = { 0xefu, 0xbbu, 0xbfu };
	xrtcsvwriter* pWriter;
	xrtClearError();
	pWriter = (xrtcsvwriter*)xrtCalloc(1u, sizeof(*pWriter));
	if ( pWriter == NULL ) {
		xerror* pSaved = xrtTakeError();
		if ( bTakeOwnership ) { (void)xrtWriterDestroy(pOutput); }
		xrtSetErrorTake(pSaved);
		return NULL;
	}
	xrtCsvErrorInit(&pWriter->Error);
	pWriter->Writer = pOutput;
	pWriter->TakeWriter = bTakeOwnership;
	if ( pOutput == NULL ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_ARGUMENT, "CSV output stream is required");
		return pWriter;
	}
	xrtClearError();
	if ( !__xrtCsvCopyDialect(&pWriter->Dialect, pDialect) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_ARGUMENT,
			"invalid CSV writer or dialect");
		return pWriter;
	}
	if ( pWriter->Dialect.bWriteBOM &&
		!__xrtCsvWriterWriteAll(pWriter, sBOM, sizeof(sBOM)) ) { return pWriter; }
	return pWriter;
}

XRT_API xrtcsvwriter* xrtCsvWriterCreateMemory(const xrtcsvdialect* pDialect)
{
	xbuffer* pBuffer;
	xwriter* pOutput;
	xrtcsvwriter* pWriter;
	xrtClearError();
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL ) { return NULL; }
	if ( !xrtBufferReserve(pBuffer, 4096u) ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	pOutput = xrtWriterFromBuffer(pBuffer);
	if ( pOutput == NULL ) { xrtBufferDestroy(pBuffer); return NULL; }
	pWriter = xrtCsvWriterCreate(pOutput, true, pDialect);
	if ( pWriter != NULL ) { pWriter->Memory = pBuffer; }
	else {
		xrtBufferDestroy(pBuffer);
	}
	return pWriter;
}

XRT_API xrtcsvwriter* xrtCsvWriterOpenFile(cstr sPath, const xrtcsvdialect* pDialect)
{
	xrtcsvdialect tDialect;
	xrtcsvwriter* pWriter = NULL;
	xwriter* pOutput = NULL;
	xfile pFile = NULL;
	str sDestination = NULL, sParent = NULL, sTemporary = NULL;
	xerror* pSaved;
	xrtClearError();
	if ( sPath == NULL || sPath[0] == 0 || !__xrtCsvCopyDialect(&tDialect, pDialect) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xrt.csv", XRT_CSV_ERROR_ARGUMENT, "invalid CSV destination or dialect");
		return NULL;
	}
	/* 固定绝对目标；成功 close 才替换文件，失败或放弃不破坏旧文件。 */
	sDestination = xrtPathAbs(sPath);
	if ( sDestination == NULL ) { goto cleanup; }
	sParent = xrtPathParent(sDestination);
	if ( sParent == NULL ) { goto cleanup; }
	pFile = xrtFileTemp(sParent, ".xrt-csv-", ".tmp", &sTemporary);
	if ( pFile == NULL ) { goto cleanup; }
	pOutput = xrtWriterTakeFile(&pFile);
	if ( pOutput == NULL ) { goto cleanup; }
	pWriter = xrtCsvWriterCreate(pOutput, true, &tDialect);
	pOutput = NULL;
	if ( pWriter != NULL && !pWriter->Failed ) {
		pWriter->Destination = sDestination;
		pWriter->Temporary = sTemporary;
		xrtFree(sParent);
		return pWriter;
	}
cleanup:
	pSaved = xrtTakeError();
	xrtCsvWriterDestroy(pWriter);
	(void)xrtWriterDestroy(pOutput);
	if ( pFile != NULL ) { (void)xrtClose(pFile); }
	if ( sTemporary != NULL ) { (void)xrtFileDelete(sTemporary); }
	xrtFree(sDestination);
	xrtFree(sParent);
	xrtFree(sTemporary);
	xrtSetErrorTake(pSaved);
	return NULL;
}

XRT_API const xrtcsverror* xrtCsvWriterError(const xrtcsvwriter* pWriter)
{
	return pWriter != NULL ? &pWriter->Error : NULL;
}

static bool __xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields)
{
	uint64 iInput = 0u;
	uint32 i;
	const char* sNewline;
	size_t iNewline;
	xrtClearError();
	if ( pWriter == NULL ) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
	if ( pWriter->Failed ) { return __xrtCsvFailed(pWriter->Failure); }
	if ( pWriter->Closed ) {
		xrtSetErrorInfo(XERR_STATE, "xrt.csv", XRT_CSV_ERROR_STATE, "CSV writer is closed");
		return false;
	}
	if ( pFields == NULL && iCount != 0u ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_ARGUMENT, "CSV fields are required");
		return false;
	}
	if ( pWriter->Dialect.tLimits.iMaxEntries != 0u &&
		pWriter->RowNumber >= pWriter->Dialect.tLimits.iMaxEntries ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV row count exceeds the configured limit");
		return false;
	}
	if ( pWriter->Dialect.tLimits.iMaxNodes != 0u &&
		((uint64)iCount > pWriter->Dialect.tLimits.iMaxNodes ||
		 pWriter->TotalFields > pWriter->Dialect.tLimits.iMaxNodes - (uint64)iCount) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV field count exceeds the configured limit");
		return false;
	}
	for ( i = 0u; i < iCount; ++i ) {
		size_t iLength;
		uint64 iBudget = UINT64_MAX;
		if ( pWriter->Dialect.tLimits.iMaxItemBytes != 0u &&
			pWriter->Dialect.tLimits.iMaxItemBytes < iBudget ) {
			iBudget = pWriter->Dialect.tLimits.iMaxItemBytes;
		}
		if ( pWriter->Dialect.tLimits.iMaxInputBytes != 0u ) {
			uint64 iMaxInput = pWriter->Dialect.tLimits.iMaxInputBytes;
			uint64 iRemaining;
			if ( pWriter->InputBytes > iMaxInput ||
				iInput > iMaxInput - pWriter->InputBytes ) {
				__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
					"CSV input exceeds the configured limit");
				return false;
			}
			iRemaining = iMaxInput - pWriter->InputBytes - iInput;
			if ( iRemaining < iBudget ) { iBudget = iRemaining; }
		}
		if ( !__xrtCsvFieldLength(pFields[i], iBudget, &iLength) ) {
			__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
				"oversized CSV field");
			return false;
		}
		if ( pFields[i] != NULL && !xrtUtf8Valid((xstrview){ pFields[i], iLength }, NULL) ) {
			__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_FORMAT, "CSV field is not valid UTF-8");
			return false;
		}
		if ( (pWriter->Dialect.tLimits.iMaxItemBytes != 0u &&
			 iLength > pWriter->Dialect.tLimits.iMaxItemBytes) ||
			iInput > UINT64_MAX - iLength ) {
			__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT, "oversized CSV field");
			return false;
		}
		iInput += iLength;
	}
	if ( pWriter->Dialect.tLimits.iMaxInputBytes != 0u &&
		(iInput > pWriter->Dialect.tLimits.iMaxInputBytes ||
		 pWriter->InputBytes > pWriter->Dialect.tLimits.iMaxInputBytes - iInput) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_LIMIT,
			"CSV input exceeds the configured limit");
		return false;
	}
	for ( i = 0u; i < iCount; ++i ) {
		unsigned char iDelimiter = (unsigned char)pWriter->Dialect.iDelimiter;
		if ( i != 0u && !__xrtCsvWriterWriteAll(pWriter, &iDelimiter, 1u) ) { return false; }
		/* 单个空字段必须带引号，才能与零字段的空行区分。 */
		if ( !__xrtCsvWriteField(pWriter, (const char*)pFields[i],
			iCount == 1u && (pFields[i] == NULL || pFields[i][0] == 0)) ) { return false; }
	}
	sNewline = pWriter->Dialect.iNewline == XRT_CSV_NEWLINE_CRLF ? "\r\n" : "\n";
	iNewline = pWriter->Dialect.iNewline == XRT_CSV_NEWLINE_CRLF ? 2u : 1u;
	if ( !__xrtCsvWriterWriteAll(pWriter, sNewline, iNewline) ) { return false; }
	pWriter->InputBytes += iInput;
	pWriter->TotalFields += iCount;
	pWriter->RowNumber += 1u;
	return true;
}

static bool __xrtCsvWriterClose(xrtcsvwriter* pWriter)
{
	xrtClearError();
	if ( pWriter == NULL ) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
	if ( pWriter->Failed ) { return __xrtCsvFailed(pWriter->Failure); }
	if ( pWriter->Closed ) { return true; }
	if ( !xrtWriterFlush(pWriter->Writer) ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_IO,
			"cannot flush CSV stream");
		return false;
	}
	if ( pWriter->Failed ) { return __xrtCsvFailed(pWriter->Failure); }
	if ( pWriter->TakeWriter ) {
		xwriter* pOutput = pWriter->Writer;
		pWriter->Writer = NULL;
		pWriter->TakeWriter = false;
		if ( !xrtWriterDestroy(pOutput) ) {
			__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_IO, "cannot close CSV stream");
			return false;
		}
		if ( pWriter->Failed ) { return __xrtCsvFailed(pWriter->Failure); }
	}
	if ( pWriter->Temporary != NULL ) {
		if ( !xrtPathRename(pWriter->Temporary, pWriter->Destination, true) ) {
			__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_IO, "cannot commit CSV file");
			return false;
		}
		xrtFree(pWriter->Temporary);
		pWriter->Temporary = NULL;
	}
	pWriter->Closed = true;
	return true;
}

/* 回调返回成功也不能掩盖重入造成的失败，更不能继续提交文件。 */
XRT_API bool xrtCsvWriterWrite(xrtcsvwriter* pWriter, uint32 iCount, const str* pFields)
{
	bool bOk;
	if ( pWriter != NULL && pWriter->Busy ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_STATE, "CSV writer callback cannot reenter its writer");
		return false;
	}
	if ( pWriter != NULL ) { pWriter->Busy = true; }
	bOk = __xrtCsvWriterWrite(pWriter, iCount, pFields);
	if ( pWriter != NULL ) { pWriter->Busy = false; }
	return bOk;
}

XRT_API bool xrtCsvWriterClose(xrtcsvwriter* pWriter)
{
	bool bOk;
	if ( pWriter != NULL && pWriter->Busy ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_STATE, "CSV writer callback cannot close its writer");
		return false;
	}
	if ( pWriter != NULL ) { pWriter->Busy = true; }
	bOk = __xrtCsvWriterClose(pWriter);
	if ( pWriter != NULL ) { pWriter->Busy = false; }
	return bOk;
}

XRT_API xbuffer* xrtCsvWriterTakeBuffer(xrtcsvwriter* pWriter)
{
	xbuffer* pResult;
	xrtClearError();
	if ( pWriter != NULL && pWriter->Busy ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_STATE, "CSV writer callback cannot take its output");
		return NULL;
	}
	if ( pWriter != NULL && pWriter->Failed ) { (void)__xrtCsvFailed(pWriter->Failure); return NULL; }
	if ( pWriter == NULL || !pWriter->Closed || pWriter->Memory == NULL ) {
		xrtSetErrorInfo(XERR_STATE, "xrt.csv", XRT_CSV_ERROR_STATE, "CSV memory result is not available");
		return NULL;
	}
	pResult = pWriter->Memory;
	pWriter->Memory = NULL;
	return pResult;
}

XRT_API void xrtCsvWriterDestroy(xrtcsvwriter* pWriter)
{
	xerror* pSaved;
	if ( pWriter == NULL ) { return; }
	if ( pWriter->Busy ) {
		__xrtCsvSetWriterError(pWriter, XRT_CSV_ERROR_STATE, "CSV writer callback cannot destroy its writer");
		return;
	}
	pWriter->Busy = true;
	pSaved = xrtTakeError();
	/* 析构只释放资源，不把失败或尚未提交的文件变成成功输出。 */
	if ( pWriter->TakeWriter && pWriter->Writer != NULL ) { xrtWriterDestroy(pWriter->Writer); }
	if ( pWriter->Temporary != NULL ) { (void)xrtFileDelete(pWriter->Temporary); }
	xrtFree(pWriter->Temporary);
	xrtFree(pWriter->Destination);
	xrtBufferDestroy(pWriter->Memory);
	xrtErrorFree(pWriter->Failure);
	xrtFree(pWriter);
	xrtSetErrorTake(pSaved);
}
