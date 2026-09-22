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

xrtplistoptions __xrtPlistResolveOptions(const xrtplistoptions* pOptions)
{
	xrtplistoptions tResult;
	xrtPlistOptionsInit(&tResult);
	if ( pOptions != NULL && pOptions->iSize == sizeof(*pOptions) &&
		pOptions->iVersion == XRT_PLIST_OPTIONS_VERSION ) {
		tResult = *pOptions;
	}
	return tResult;
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
	xrtplistoptions tOptions = __xrtPlistResolveOptions(pOptions);
	xbuffer* pInput = NULL;
	xrtplistdocument* pResult = NULL;
	uint64 iSize = 0u;
	uint64 iLimit = tOptions.tLimits.iMaxInputBytes;
	xrtPlistErrorInit(pResolvedError);
	if ( pReader == NULL || (tOptions.iFormat < XRT_PLIST_FORMAT_AUTO ||
		tOptions.iFormat > XRT_PLIST_FORMAT_BINARY) ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist reader or format option");
		goto cleanup;
	}
	if ( xrtReaderSize(pReader, &iSize) && iLimit != 0u && iSize > iLimit ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_LIMIT,
			0u, 0u, 0u, "plist input exceeds the configured limit");
		goto cleanup;
	}
	pInput = xrtReaderReadAll(pReader, iLimit);
	if ( pInput == NULL ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_IO,
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
	if ( bTakeOwnership && pReader != NULL ) { xrtReaderDestroy(pReader); }
	return pResult;
}

XRT_API xrtplistdocument* xrtPlistParseMemory(const void* pData, uint64 iSize,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xreader* pReader = pData != NULL || iSize == 0u ?
		xrtReaderFromMemory((xbytesview){ (cbytes)pData, (size_t)iSize }) : NULL;
	return xrtPlistParse(pReader, true, pOptions, pError);
}

XRT_API xrtplistdocument* xrtPlistLoad(str sPath,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xreader* pReader = sPath != NULL ? xrtReaderOpen(sPath) : NULL;
	return xrtPlistParse(pReader, true, pOptions, pError);
}

XRT_API xbuffer* xrtPlistStringify(const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xrtplistoptions tOptions = __xrtPlistResolveOptions(pOptions);
	xrtPlistErrorInit(pResolvedError);
	if ( pDocument == NULL || pDocument->pRoot == NULL ||
		(tOptions.iFormat != XRT_PLIST_FORMAT_AUTO &&
		 tOptions.iFormat != XRT_PLIST_FORMAT_XML &&
		 tOptions.iFormat != XRT_PLIST_FORMAT_BINARY) ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_ARGUMENT,
			0u, 0u, 0u, "invalid plist document or format option");
		return NULL;
	}
	return tOptions.iFormat == XRT_PLIST_FORMAT_BINARY ?
		__xrtPlistWriteBinary(pDocument, &tOptions, pResolvedError) :
		__xrtPlistWriteXml(pDocument, &tOptions, pResolvedError);
}

XRT_API bool xrtPlistWrite(const xrtplistdocument* pDocument, xwriter* pWriter,
	bool bTakeOwnership, const xrtplistoptions* pOptions,
	xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtPlistStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && pWriter != NULL &&
		xrtWriterWriteFull(pWriter, pOutput->Data, pOutput->Size, NULL) &&
		xrtWriterFlush(pWriter);
	if ( !bResult && pResolvedError->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_IO,
			0u, 0u, 0u, "could not write plist output");
	}
	xrtBufferDestroy(pOutput);
	if ( bTakeOwnership && pWriter != NULL ) { xrtWriterDestroy(pWriter); }
	return bResult;
}

XRT_API bool xrtPlistSave(str sPath, const xrtplistdocument* pDocument,
	const xrtplistoptions* pOptions, xrtplisterror* pError)
{
	xrtplisterror tLocalError;
	xrtplisterror* pResolvedError = pError != NULL ? pError : &tLocalError;
	xbuffer* pOutput = xrtPlistStringify(pDocument, pOptions, pResolvedError);
	bool bResult = pOutput != NULL && sPath != NULL &&
		xrtFileWriteAll(sPath, (xbytesview){ pOutput->Data, pOutput->Size });
	if ( !bResult && pResolvedError->iCode == XRT_PLIST_OK ) {
		__xrtPlistSetError(pResolvedError, XRT_PLIST_ERROR_IO,
			0u, 0u, 0u, "could not save plist output file");
	}
	xrtBufferDestroy(pOutput);
	return bResult;
}
