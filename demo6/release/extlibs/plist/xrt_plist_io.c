#include "xrt_plist_internal.h"



XRT_API void xrtPlistOptionsInit(xrtplistoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_PLIST_OPTIONS_VERSION;
	pOptions->iFormat = XRT_PLIST_FORMAT_AUTO;
	pOptions->iIndent = 2u;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XRT_API void xrtPlistErrorInit(xrtplisterror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}

void __xrtPlistSetError(xrtplisterror* pError, int iCode,
	uint64 iLine, uint64 iColumn, uint64 iOffset, const char* sFormat, ...)
{
	va_list tArgs;
	if ( pError == NULL || pError->iCode != XRT_PLIST_OK ) { return; }
	pError->iCode = iCode;
	pError->iLine = iLine;
	pError->iColumn = iColumn;
	pError->iOffset = iOffset;
	va_start(tArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, tArgs);
	va_end(tArgs);
}

bool __xrtPlistResolveOptions(const xrtplistoptions* pOptions,
	xrtplistoptions* pResolved, xrtplisterror* pError)
{
	xrtPlistOptionsInit(pResolved);
	if ( pOptions == NULL ) { return true; }
	if ( pOptions->iSize != sizeof(*pOptions) ||
		pOptions->iVersion != XRT_PLIST_OPTIONS_VERSION ||
		pOptions->tLimits.iSize < sizeof(pOptions->tLimits) ||
		pOptions->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pOptions->tLimits.iReserved != 0u ||
		pOptions->tLimits.iFlags != 0u ) {
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist options version, size or flags");
		return false;
	}
	*pResolved = *pOptions;
	return true;
}

/* Reader 的资源与文件错误不能被统一写成格式或参数错误。 */
static int __xrtPlistReadError(void)
{
	const xerror* pError = xrtGetError();
	xerrkind iKind = pError != NULL ? xrtErrorKind(pError) : XERR_IO;
	return iKind == XERR_MEMORY ? XRT_PLIST_ERROR_MEMORY :
		(iKind == XERR_RANGE ? XRT_PLIST_ERROR_LIMIT : XRT_PLIST_ERROR_IO);
}

bool __xrtPlistLimitNode(xrtplistcontext* pContext, uint32 iDepth,
	uint64 iOffset)
{
	if ( pContext->Options.tLimits.iMaxDepth != 0u &&
		iDepth > pContext->Options.tLimits.iMaxDepth ) {
		__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, iOffset, "plist nesting depth exceeds the configured limit");
		return false;
	}
	pContext->iNodes++;
	if ( pContext->Options.tLimits.iMaxNodes != 0u &&
		pContext->iNodes > pContext->Options.tLimits.iMaxNodes ) {
		__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, iOffset, "plist node count exceeds the configured limit");
		return false;
	}
	return true;
}

bool __xrtPlistLimitItem(xrtplistcontext* pContext, uint64 iSize,
	uint64 iOffset)
{
	if ( pContext->Options.tLimits.iMaxItemBytes != 0u &&
		iSize > pContext->Options.tLimits.iMaxItemBytes ) {
		__xrtPlistSetError(pContext->Error, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, iOffset, "plist item exceeds the configured size limit");
		return false;
	}
	return true;
}

XRT_API xrtplistdocument* xrtPlistParse(xreader* pReader, bool bTakeOwnership,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xrtplistoptions tOptions;
	xbuffer* pInput = NULL;
	xrtplistdocument* pResult = NULL;
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	uint64 iLimit;
	size_t iReadLimit;
	xrtPlistErrorInit(pResolvedError);
	if ( !__xrtPlistResolveOptions(pOptions, &tOptions, pResolvedError) ) {
		goto cleanup;
	}
	iLimit = tOptions.tLimits.iMaxInputBytes;
	if ( pReader == NULL || (tOptions.iFormat < XRT_PLIST_FORMAT_AUTO ||
		tOptions.iFormat > XRT_PLIST_FORMAT_BINARY) ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist reader or format option");
		goto cleanup;
	}
	if ( iLimit != 0u && xrtReaderSize(pReader, &iSize) &&
		xrtReaderTell(pReader, &iPosition) && iPosition <= iSize &&
		iSize - iPosition > iLimit ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "plist input exceeds the configured limit");
		goto cleanup;
	}
	xrtClearError();
	iReadLimit = iLimit == 0u || iLimit > SIZE_MAX - 1u ?
		SIZE_MAX - 1u : (size_t)iLimit;
	pInput = xrtReaderReadAll(pReader, iReadLimit);
	if ( pInput == NULL ) {
		__xrtPlistSetError(pResolvedError, __xrtPlistReadError(),
			0u, 0u, 0u, "could not read plist input");
		goto cleanup;
	}
	if ( tOptions.iFormat == XRT_PLIST_FORMAT_BINARY ||
		(tOptions.iFormat == XRT_PLIST_FORMAT_AUTO && pInput->Size >= 8u &&
		 memcmp(pInput->Data, "bplist00", 8u) == 0) ) {
		pResult = __xrtPlistParseBinary(pInput->Data, pInput->Size,
			&tOptions, pResolvedError);
	} else {
		pResult = __xrtPlistParseXml(pInput->Data, pInput->Size,
			&tOptions, pResolvedError);
	}

cleanup:
	xrtBufferDestroy(pInput);
	if ( bTakeOwnership && pReader != NULL &&
		!xrtReaderDestroy(pReader) && pResult != NULL ) {
		/* 文档读完后关闭失败，不能把未完成的读取报为成功。 */
		xrtPlistDestroy(pResult);
		pResult = NULL;
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_IO,
			0u, 0u, 0u, "could not close plist input");
	}
	return pResult;
}

XRT_API xrtplistdocument* xrtPlistParseMemory(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xreader* pReader;
	if ( pData == NULL && iSize != 0u ) {
		xrtPlistErrorInit(pError);
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist input buffer");
		return NULL;
	}
#if SIZE_MAX < UINT64_MAX
	if ( iSize > SIZE_MAX ) {
		xrtPlistErrorInit(pError);
		__xrtPlistSetError(pError, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "plist input exceeds the platform size limit");
		return NULL;
	}
#endif
	pReader = xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize });
	if ( pReader == NULL ) {
		xrtPlistErrorInit(pError);
		__xrtPlistSetError(pError, __xrtPlistReadError(),
			0u, 0u, 0u, "could not create plist input reader");
		return NULL;
	}
	return xrtPlistParse(pReader, true, pOptions, pError);
}

XRT_API xrtplistdocument* xrtPlistLoad(str sPath,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xreader* pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	if ( pReader == NULL && sPath != NULL ) {
		xrtPlistErrorInit(pError);
		__xrtPlistSetError(pError, __xrtPlistReadError(),
			0u, 0u, 0u, "could not open plist file");
		return NULL;
	}
	return xrtPlistParse(pReader, true, pOptions, pError);
}

XRT_API xbuffer* xrtPlistStringify(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xrtplistoptions tOptions;
	xrtPlistErrorInit(pResolvedError);
	if ( !__xrtPlistResolveOptions(pOptions, &tOptions, pResolvedError) ) {
		return NULL;
	}
	if ( pDocument == NULL || pDocument->pRoot == NULL ||
		(tOptions.iFormat != XRT_PLIST_FORMAT_AUTO &&
		 tOptions.iFormat != XRT_PLIST_FORMAT_XML &&
		 tOptions.iFormat != XRT_PLIST_FORMAT_BINARY) ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist document or format option");
		return NULL;
	}
	xrtClearError();
	return tOptions.iFormat == XRT_PLIST_FORMAT_BINARY ?
		__xrtPlistWriteBinary(pDocument, &tOptions, pResolvedError) :
		__xrtPlistWriteXml(pDocument, &tOptions, pResolvedError);
}

/* 写入或关闭失败时保留底层内存错误，其他失败仍按 IO 报告。 */
static int __xrtPlistOutputErrorCode(void)
{
	const xerror* pCause = xrtGetError();
	return pCause != NULL && xrtErrorKind(pCause) == XERR_MEMORY ?
		XRT_PLIST_ERROR_MEMORY : XRT_PLIST_ERROR_IO;
}

XRT_API bool xrtPlistWrite(const xrtplistdocument* pDocument, xwriter* pWriter,
	bool bTakeOwnership, const xrtplistoptions* pOptions,
	xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtPlistStringify(pDocument, pOptions, pResolvedError);
	bool bResult = false;
	if ( pOutput != NULL && pWriter != NULL ) {
		xrtClearError();
		bResult = xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
			xrtWriterFlush(pWriter);
	}
	if ( !bResult && pResolvedError->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pResolvedError, __xrtPlistOutputErrorCode(),
			0u, 0u, 0u, "could not write plist output");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership && pWriter != NULL &&
		!xrtWriterDestroy(pWriter) && bResult ) {
		/* 写入与刷新成功后，关闭失败仍是写出失败。 */
		bResult = false;
		__xrtPlistSetError(pResolvedError, __xrtPlistOutputErrorCode(),
			0u, 0u, 0u, "could not close plist output");
	}
	return bResult;
}

XRT_API bool xrtPlistSave(str sPath, const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtPlistStringify(pDocument, pOptions, pResolvedError);
	bool bResult = false;
	if ( pOutput != NULL && sPath != NULL ) {
		xrtClearError();
		bResult = xrtFileWriteAtomic(sPath,
			(xbytesview){ pOutput->Data, pOutput->Size });
	}
	if ( !bResult && pResolvedError->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pResolvedError, __xrtPlistOutputErrorCode(),
			0u, 0u, 0u, "could not save plist output file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
