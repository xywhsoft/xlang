#include "xl_host.h"

#include "xl_host_tcc.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

static void __xlCopyText(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}
	if ( iLen != 0 ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
}

static void __xlStripFileName(char* sPath)
{
	size_t iLen;

	if ( sPath == NULL ) {
		return;
	}

	iLen = strlen(sPath);
	while ( iLen > 0 ) {
		if ( xlPathIsSep((unsigned char)sPath[iLen - 1]) ) {
			sPath[iLen - 1] = '\0';
			return;
		}
		--iLen;
	}
}

static void __xlAppendPathPart(char* sOut, size_t iOutSize, const char* sPart)
{
	size_t iLen;
	size_t iPartLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( sPart == NULL ) {
		sPart = "";
	}

	iLen = strlen(sOut);
	if ( (iLen > 0) && !xlPathIsSep((unsigned char)sOut[iLen - 1]) ) {
		if ( (iLen + 1) >= iOutSize ) {
			return;
		}
		sOut[iLen] = '\\';
		++iLen;
		sOut[iLen] = '\0';
	}

	iPartLen = strlen(sPart);
	if ( iPartLen >= (iOutSize - iLen) ) {
		iPartLen = (iOutSize - iLen) - 1;
	}
	if ( iPartLen != 0 ) {
		memcpy(sOut + iLen, sPart, iPartLen);
		iLen += iPartLen;
	}
	sOut[iLen] = '\0';
}

static void __xlBuildPath2(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1)
{
	__xlCopyText(sOut, iOutSize, sBase);
	__xlAppendPathPart(sOut, iOutSize, sPart1);
}

static void __xlBuildPath3(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1, const char* sPart2)
{
	__xlCopyText(sOut, iOutSize, sBase);
	__xlAppendPathPart(sOut, iOutSize, sPart1);
	__xlAppendPathPart(sOut, iOutSize, sPart2);
}

static void __xlNormalizeRootPath(char* sOut, size_t iOutSize, const char* sPath)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	if ( sPath == NULL ) {
		sPath = "release";
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( GetFullPathNameA(sPath, (DWORD)iOutSize, sOut, NULL) == 0 ) {
		__xlCopyText(sOut, iOutSize, sPath);
	}
#else
	__xlCopyText(sOut, iOutSize, sPath);
#endif
}

static int __xlDetectProjectRoot(char* sOut, size_t iOutSize)
{
#if defined(_WIN32) || defined(_WIN64)
	DWORD iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return XL_RC_BAD_ARG;
	}

	iLen = GetModuleFileNameA(NULL, sOut, (DWORD)iOutSize);
	if ( (iLen == 0) || (iLen >= iOutSize) ) {
		sOut[0] = '\0';
		return XL_RC_FAIL;
	}

	__xlStripFileName(sOut);
	__xlStripFileName(sOut);
	__xlStripFileName(sOut);
	return (sOut[0] != '\0') ? XL_RC_OK : XL_RC_FAIL;
#else
	XL_UNUSED(sOut);
	XL_UNUSED(iOutSize);
	return XL_RC_NOT_IMPLEMENTED;
#endif
}

void xlHostContextInit(XL_HostContext* pCtx, const char* sInputFile, const char* sReleaseRoot)
{
	if ( pCtx == NULL ) {
		return;
	}

	memset(pCtx, 0, sizeof(*pCtx));
	__xlCopyText(pCtx->sInputFile, sizeof(pCtx->sInputFile), sInputFile);
	if ( __xlDetectProjectRoot(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot)) == XL_RC_OK ) {
		__xlBuildPath2(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot), pCtx->sProjectRoot, "release");
	} else {
		__xlNormalizeRootPath(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot), sReleaseRoot);
		__xlCopyText(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot), pCtx->sReleaseRoot);
		__xlStripFileName(pCtx->sProjectRoot);
	}
	__xlBuildPath3(pCtx->sTccRoot, sizeof(pCtx->sTccRoot), pCtx->sProjectRoot, "release", "tcc");
}

int xlHostRunCText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	char** psTraceText,
	XL_HostRunResult* pResult
)
{
	return xlHostTccRunText(pCtx, pDiag, sVirtualPath, sCode, sEntrySymbol, psTraceText, pResult);
}

int xlHostRunCFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	char** psTraceText,
	XL_HostRunResult* pResult
)
{
	return xlHostTccRunFile(pCtx, pDiag, sSourceFile, psTraceText, pResult);
}

int xlHostBuildCText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
)
{
	return xlHostTccBuildText(pCtx, pDiag, sVirtualPath, sCode, sEntrySymbol, sOutputFile, psTraceText, pResult);
}

int xlHostBuildCFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
)
{
	return xlHostTccBuildFile(pCtx, pDiag, sSourceFile, sOutputFile, psTraceText, pResult);
}
