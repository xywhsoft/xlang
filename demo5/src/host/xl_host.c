#include "xl_host.h"

#include "xl_host_tcc.h"
#include "../common/xl_app_path.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

int xlHostContextAddLibraryPath(XL_HostContext* pCtx, const char* sPath);
int xlHostContextAddIncludePath(XL_HostContext* pCtx, const char* sPath);

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

static void __xlInitProjectRoot(char* sOut, size_t iOutSize, const char* sInputFile, const char* sReleaseRoot)
{
	char sResolved[XL_PATH_MAX_LEN];
	char sReleaseResolved[XL_PATH_MAX_LEN];
	size_t iReleaseLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	if ( (sInputFile != NULL) && (sInputFile[0] != '\0') ) {
		sResolved[0] = '\0';
#if defined(_WIN32) || defined(_WIN64)
		if ( GetFullPathNameA(sInputFile, (DWORD)sizeof(sResolved), sResolved, NULL) != 0 ) {
			__xlCopyText(sOut, iOutSize, sResolved);
		} else {
			__xlCopyText(sOut, iOutSize, sInputFile);
		}
#else
		__xlCopyText(sOut, iOutSize, sInputFile);
#endif
		sReleaseResolved[0] = '\0';
		__xlNormalizeRootPath(sReleaseResolved, sizeof(sReleaseResolved), sReleaseRoot);
		iReleaseLen = strlen(sReleaseResolved);
		while ( (iReleaseLen > 0) && xlPathIsSep((unsigned char)sReleaseResolved[iReleaseLen - 1]) ) {
			sReleaseResolved[--iReleaseLen] = '\0';
		}
		if ( (iReleaseLen != 0) &&
#if defined(_WIN32) || defined(_WIN64)
			(_strnicmp(sOut, sReleaseResolved, iReleaseLen) == 0) &&
#else
			(strncmp(sOut, sReleaseResolved, iReleaseLen) == 0) &&
#endif
			((sOut[iReleaseLen] == '\0') || xlPathIsSep((unsigned char)sOut[iReleaseLen])) ) {
			__xlCopyText(sOut, iOutSize, sReleaseResolved);
			__xlStripFileName(sOut);
			if ( sOut[0] != '\0' ) {
				return;
			}
		}
		__xlStripFileName(sOut);
		if ( sOut[0] != '\0' ) {
			return;
		}
	}

	__xlCopyText(sOut, iOutSize, sReleaseRoot);
}

static void __xlHostContextAddDefaultSearchPaths(XL_HostContext* pCtx)
{
	char sAppLibs[XL_PATH_MAX_LEN];
	char sAppLibDir[XL_PATH_MAX_LEN];
	char sAppIncDir[XL_PATH_MAX_LEN];
	char sLocalLibs[XL_PATH_MAX_LEN];
	char sLocalLibDir[XL_PATH_MAX_LEN];
	char sLocalIncDir[XL_PATH_MAX_LEN];

	if ( pCtx == NULL ) {
		return;
	}

	__xlBuildPath2(sAppLibs, sizeof(sAppLibs), pCtx->sReleaseRoot, "libs");
	__xlBuildPath2(sAppLibDir, sizeof(sAppLibDir), sAppLibs, "lib");
	__xlBuildPath2(sAppIncDir, sizeof(sAppIncDir), sAppLibs, "include");

	(void)xlHostContextAddLibraryPath(pCtx, sAppLibs);
	(void)xlHostContextAddLibraryPath(pCtx, sAppLibDir);
	(void)xlHostContextAddIncludePath(pCtx, sAppLibs);
	(void)xlHostContextAddIncludePath(pCtx, sAppIncDir);

	if ( pCtx->sInputFile[0] == '\0' ) {
		return;
	}

	__xlBuildPath2(sLocalLibs, sizeof(sLocalLibs), pCtx->sProjectRoot, "libs");
	__xlBuildPath2(sLocalLibDir, sizeof(sLocalLibDir), sLocalLibs, "lib");
	__xlBuildPath2(sLocalIncDir, sizeof(sLocalIncDir), sLocalLibs, "include");

	(void)xlHostContextAddLibraryPath(pCtx, sLocalLibs);
	(void)xlHostContextAddLibraryPath(pCtx, sLocalLibDir);
	(void)xlHostContextAddIncludePath(pCtx, sLocalLibs);
	(void)xlHostContextAddIncludePath(pCtx, sLocalIncDir);
}

void xlHostContextInit(XL_HostContext* pCtx, const char* sInputFile, const char* sReleaseRoot)
{
	if ( pCtx == NULL ) {
		return;
	}

	memset(pCtx, 0, sizeof(*pCtx));
#if XLANG_DEBUGMODE_ENABLED
	pCtx->bDebugMode = 1;
#else
	pCtx->bDebugMode = 0;
#endif
	__xlCopyText(pCtx->sInputFile, sizeof(pCtx->sInputFile), sInputFile);
	if ( xlAppPathGetDirectory(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot)) != XL_RC_OK ) {
		__xlNormalizeRootPath(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot), sReleaseRoot);
	}
	__xlInitProjectRoot(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot), pCtx->sInputFile, pCtx->sReleaseRoot);
	__xlBuildPath2(pCtx->sTccRoot, sizeof(pCtx->sTccRoot), pCtx->sReleaseRoot, "tcc");
	__xlHostContextAddDefaultSearchPaths(pCtx);
}

int xlHostContextAddArtifact(XL_HostContext* pCtx, const char* sKind, const char* sFile)
{
	size_t i;

	if ( (pCtx == NULL) || (sKind == NULL) || (sFile == NULL) || (sKind[0] == '\0') || (sFile[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		if ( (strcmp(pCtx->arrArtifacts[i].sKind, sKind) == 0) &&
			(strcmp(pCtx->arrArtifacts[i].sFile, sFile) == 0) ) {
			return XL_RC_OK;
		}
	}

	if ( pCtx->iArtifactCount >= XL_HOST_ARTIFACT_MAX ) {
		return XL_RC_FAIL;
	}

	__xlCopyText(pCtx->arrArtifacts[pCtx->iArtifactCount].sKind, sizeof(pCtx->arrArtifacts[pCtx->iArtifactCount].sKind), sKind);
	__xlCopyText(pCtx->arrArtifacts[pCtx->iArtifactCount].sFile, sizeof(pCtx->arrArtifacts[pCtx->iArtifactCount].sFile), sFile);
	pCtx->iArtifactCount += 1;
	return XL_RC_OK;
}

int xlHostContextAddExternSymbol(XL_HostContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iExternSymbolCount; ++i ) {
		if ( strcmp(pCtx->arrExternSymbols[i].sName, sName) == 0 ) {
			return XL_RC_OK;
		}
	}

	if ( pCtx->iExternSymbolCount >= XL_HOST_EXTERN_SYMBOL_MAX ) {
		return XL_RC_FAIL;
	}

	__xlCopyText(pCtx->arrExternSymbols[pCtx->iExternSymbolCount].sName, sizeof(pCtx->arrExternSymbols[pCtx->iExternSymbolCount].sName), sName);
	pCtx->iExternSymbolCount += 1;
	return XL_RC_OK;
}

int xlHostContextAddLinkLibrary(XL_HostContext* pCtx, const char* sName)
{
	size_t i;

	if ( (pCtx == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iLinkLibraryCount; ++i ) {
		if ( strcmp(pCtx->arrLinkLibraries[i].sName, sName) == 0 ) {
			return XL_RC_OK;
		}
	}

	if ( pCtx->iLinkLibraryCount >= XL_HOST_LINK_LIBRARY_MAX ) {
		return XL_RC_FAIL;
	}

	__xlCopyText(
		pCtx->arrLinkLibraries[pCtx->iLinkLibraryCount].sName,
		sizeof(pCtx->arrLinkLibraries[pCtx->iLinkLibraryCount].sName),
		sName
	);
	pCtx->iLinkLibraryCount += 1;
	return XL_RC_OK;
}

int xlHostContextAddLibraryPath(XL_HostContext* pCtx, const char* sPath)
{
	size_t i;

	if ( (pCtx == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pCtx->iLibraryPathCount; ++i ) {
		if ( strcmp(pCtx->arrLibraryPaths[i].sPath, sPath) == 0 ) {
			return XL_RC_OK;
		}
	}
	if ( pCtx->iLibraryPathCount >= XL_HOST_LIBRARY_PATH_MAX ) {
		return XL_RC_FAIL;
	}
	__xlCopyText(pCtx->arrLibraryPaths[pCtx->iLibraryPathCount].sPath, sizeof(pCtx->arrLibraryPaths[pCtx->iLibraryPathCount].sPath), sPath);
	pCtx->iLibraryPathCount += 1;
	return XL_RC_OK;
}

int xlHostContextAddIncludePath(XL_HostContext* pCtx, const char* sPath)
{
	size_t i;

	if ( (pCtx == NULL) || (sPath == NULL) || (sPath[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pCtx->iIncludePathCount; ++i ) {
		if ( strcmp(pCtx->arrIncludePaths[i].sPath, sPath) == 0 ) {
			return XL_RC_OK;
		}
	}
	if ( pCtx->iIncludePathCount >= XL_HOST_INCLUDE_PATH_MAX ) {
		return XL_RC_FAIL;
	}
	__xlCopyText(pCtx->arrIncludePaths[pCtx->iIncludePathCount].sPath, sizeof(pCtx->arrIncludePaths[pCtx->iIncludePathCount].sPath), sPath);
	pCtx->iIncludePathCount += 1;
	return XL_RC_OK;
}

int xlHostContextAddLoadLibrary(XL_HostContext* pCtx, const char* sFile)
{
	size_t i;

	if ( (pCtx == NULL) || (sFile == NULL) || (sFile[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pCtx->iLoadLibraryCount; ++i ) {
		if ( strcmp(pCtx->arrLoadLibraries[i].sFile, sFile) == 0 ) {
			return XL_RC_OK;
		}
	}
	if ( pCtx->iLoadLibraryCount >= XL_HOST_LOAD_LIBRARY_MAX ) {
		return XL_RC_FAIL;
	}
	__xlCopyText(pCtx->arrLoadLibraries[pCtx->iLoadLibraryCount].sFile, sizeof(pCtx->arrLoadLibraries[pCtx->iLoadLibraryCount].sFile), sFile);
	pCtx->iLoadLibraryCount += 1;
	return XL_RC_OK;
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
