#include "xl_exec_ctx.h"

#include <ctype.h>

#include "../../lib/xrt/xrt.h"

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

static char* __xlDupText(const char* sText)
{
	size_t iLen;
	char* sOut;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}

	if ( iLen != 0u ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static int __xlAppendText(char* sBuf, size_t iBufSize, const char* sText)
{
	size_t iLen;
	size_t iTextLen;

	if ( (sBuf == NULL) || (iBufSize == 0u) ) {
		return 0;
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sBuf);
	iTextLen = strlen(sText);
	if ( (iLen + iTextLen) >= iBufSize ) {
		return 0;
	}

	memcpy(sBuf + iLen, sText, iTextLen + 1u);
	return 1;
}

static void __xlStripFileName(char* sPath)
{
	size_t iLen;

	if ( sPath == NULL ) {
		return;
	}

	iLen = strlen(sPath);
	while ( iLen > 0u ) {
		if ( (sPath[iLen - 1] == '\\') || (sPath[iLen - 1] == '/') ) {
			sPath[iLen - 1] = '\0';
			return;
		}
		--iLen;
	}
}

static char* __xlConcatText3(const char* sA, const char* sB, const char* sC)
{
	size_t iLenA;
	size_t iLenB;
	size_t iLenC;
	size_t iLen;
	char* sOut;

	if ( sA == NULL ) {
		sA = "";
	}
	if ( sB == NULL ) {
		sB = "";
	}
	if ( sC == NULL ) {
		sC = "";
	}

	iLenA = strlen(sA);
	iLenB = strlen(sB);
	iLenC = strlen(sC);
	iLen = iLenA + iLenB + iLenC;
	sOut = (char*)malloc(iLen + 1u);
	if ( sOut == NULL ) {
		return NULL;
	}

	if ( iLenA != 0u ) {
		memcpy(sOut, sA, iLenA);
	}
	if ( iLenB != 0u ) {
		memcpy(sOut + iLenA, sB, iLenB);
	}
	if ( iLenC != 0u ) {
		memcpy(sOut + iLenA + iLenB, sC, iLenC);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static char* __xlReadTextFile(const char* sFile)
{
	FILE* pFile;
	long iSize;
	size_t iRead;
	char* sText;

	if ( (sFile == NULL) || (sFile[0] == '\0') ) {
		return NULL;
	}

	pFile = fopen(sFile, "rb");
	if ( pFile == NULL ) {
		return NULL;
	}
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return NULL;
	}
	iSize = ftell(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return NULL;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return NULL;
	}

	sText = (char*)malloc((size_t)iSize + 1u);
	if ( sText == NULL ) {
		fclose(pFile);
		return NULL;
	}

	iRead = (iSize > 0) ? fread(sText, 1u, (size_t)iSize, pFile) : 0u;
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(sText);
		return NULL;
	}

	sText[iRead] = '\0';
	return sText;
}

static void __xlRemoveFileBestEffort(const char* sFile)
{
	int iAttempt;

	if ( (sFile == NULL) || (sFile[0] == '\0') ) {
		return;
	}

	for ( iAttempt = 0; iAttempt < 20; ++iAttempt ) {
		if ( remove(sFile) == 0 ) {
			return;
		}
		xrtSleep(10u);
	}
}

static void __xlBuildFallbackExePath(XL_ExecCtx* pCtx, char* sOut, size_t iOutSize)
{
	const char* sInput;
	const char* sDot;
	const char* sBase;
	const char* sSuffix;
	static unsigned long sFallbackCounter = 0ul;
	unsigned long iCounter;
	size_t iBaseLen;
	size_t i;
	char sStem[128];
	char sHead[XL_PATH_MAX_LEN];
	char sTag[48];

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}

	sInput = ((pCtx != NULL) && (pCtx->sInputFile[0] != '\0')) ? pCtx->sInputFile : "xl_exec";
	sBase = strrchr(sInput, '\\');
	if ( sBase == NULL ) {
		sBase = strrchr(sInput, '/');
	}
	sBase = (sBase != NULL) ? (sBase + 1) : sInput;
	sDot = strrchr(sBase, '.');
	iBaseLen = (sDot != NULL) ? (size_t)(sDot - sBase) : strlen(sBase);
	if ( iBaseLen >= sizeof(sStem) ) {
		iBaseLen = sizeof(sStem) - 1u;
	}

	for ( i = 0u; i < iBaseLen; ++i ) {
		char c = sBase[i];
		sStem[i] = (isalnum((unsigned char)c) || (c == '_') || (c == '-')) ? c : '_';
	}
	sStem[iBaseLen] = '\0';

#if defined(_WIN32) || defined(_WIN64)
	sSuffix = ".fallback.exe";
#else
	sSuffix = ".fallback";
#endif
	__xlCopyText(sHead, sizeof(sHead), ((pCtx != NULL) ? pCtx->sReleaseRoot : "."));
#if defined(_WIN32) || defined(_WIN64)
	if ( !__xlAppendText(sHead, sizeof(sHead), "\\tmp\\") || !__xlAppendText(sHead, sizeof(sHead), sStem) || !__xlAppendText(sHead, sizeof(sHead), ".") ) {
		__xlCopyText(sOut, iOutSize, "");
		return;
	}
#else
	if ( !__xlAppendText(sHead, sizeof(sHead), "/tmp/") || !__xlAppendText(sHead, sizeof(sHead), sStem) || !__xlAppendText(sHead, sizeof(sHead), ".") ) {
		__xlCopyText(sOut, iOutSize, "");
		return;
	}
#endif

	iCounter = ++sFallbackCounter;
#if defined(_WIN32) || defined(_WIN64)
	snprintf(
		sTag,
		sizeof(sTag),
		"%08llx_%08lx_%04lx",
		(unsigned long long)GetTickCount64(),
		(unsigned long)GetCurrentProcessId(),
		iCounter & 0xFFFFul
	);
#else
	snprintf(
		sTag,
		sizeof(sTag),
		"%08llx_%04lx",
		(unsigned long long)xrtNow(),
		iCounter & 0xFFFFul
	);
#endif
	__xlCopyText(sOut, iOutSize, sHead);
	if ( !__xlAppendText(sOut, iOutSize, sTag) || !__xlAppendText(sOut, iOutSize, sSuffix) ) {
		__xlCopyText(sOut, iOutSize, "");
	}
}

static char* __xlBuildFallbackPathEnv(const XL_ExecCtx* pCtx)
{
#if defined(_WIN32) || defined(_WIN64)
	char sPathValue[8192];
	char sDir[XL_PATH_MAX_LEN];
	char* sEnvText;
	const char* sCurrentPath;
	size_t i;

	if ( pCtx == NULL ) {
		return NULL;
	}

	sPathValue[0] = '\0';
	for ( i = 0u; i < pCtx->iArtifactCount; ++i ) {
		if ( strcmp(pCtx->arrArtifacts[i].sKind, "shared") != 0 ) {
			continue;
		}
		__xlCopyText(sDir, sizeof(sDir), pCtx->arrArtifacts[i].sFile);
		__xlStripFileName(sDir);
		if ( sDir[0] == '\0' ) {
			continue;
		}
		if ( sPathValue[0] != '\0' && !__xlAppendText(sPathValue, sizeof(sPathValue), ";") ) {
			return NULL;
		}
		if ( !__xlAppendText(sPathValue, sizeof(sPathValue), sDir) ) {
			return NULL;
		}
	}

	if ( sPathValue[0] == '\0' ) {
		return NULL;
	}

	sCurrentPath = getenv("PATH");
	if ( (sCurrentPath != NULL) && (sCurrentPath[0] != '\0') ) {
		if ( !__xlAppendText(sPathValue, sizeof(sPathValue), ";") ||
			!__xlAppendText(sPathValue, sizeof(sPathValue), sCurrentPath) ) {
			return NULL;
		}
	}

	sEnvText = __xlConcatText3("PATH=", sPathValue, "");
	return sEnvText;
#else
	XL_UNUSED(pCtx);
	return NULL;
#endif
}

static char* __xlBuildFallbackAsyncTraceEnv(const char* sTraceFile)
{
	if ( (sTraceFile == NULL) || (sTraceFile[0] == '\0') ) {
		return NULL;
	}

	return __xlConcatText3("XL_GENERATED_ASYNC_TRACE_FILE=", sTraceFile, "");
}

static void __xlFreeExecFallbackConfig(xprocessconfig* pConfig)
{
	size_t i;

	if ( (pConfig == NULL) || (pConfig->arrEnv == NULL) ) {
		return;
	}

	for ( i = 0u; i < pConfig->iEnvCount; ++i ) {
		if ( pConfig->arrEnv[i] != NULL ) {
			free(pConfig->arrEnv[i]);
		}
	}
	free(pConfig->arrEnv);
	pConfig->arrEnv = NULL;
	pConfig->iEnvCount = 0u;
}

static void __xlInitExecFallbackConfig(
	xprocessconfig* pConfig,
	const XL_ExecCtx* pCtx,
	const char* sExePath,
	const char* sWorkDir,
	const char* sTraceFile
)
{
	char* sEnvPath;
	char* sEnvTrace;
	str* arrEnv;
	size_t iEnvCount;

	if ( pConfig == NULL ) {
		return;
	}

	xrtProcessConfigInit(pConfig);
	pConfig->iTargetKind = XPROC_TARGET_EXEC;
	pConfig->sProgram = (str)sExePath;
	pConfig->sWorkDir = (str)sWorkDir;
	pConfig->bHideWindow = true;
	pConfig->bMergeStderr = true;
	pConfig->iMaxCaptureBytes = 65536u;

	sEnvPath = __xlBuildFallbackPathEnv(pCtx);
	sEnvTrace = __xlBuildFallbackAsyncTraceEnv(sTraceFile);
	iEnvCount = ((sEnvPath != NULL) ? 1u : 0u) + ((sEnvTrace != NULL) ? 1u : 0u);
	if ( iEnvCount == 0u ) {
		return;
	}

	arrEnv = (str*)calloc(iEnvCount, sizeof(str));
	if ( arrEnv == NULL ) {
		free(sEnvPath);
		free(sEnvTrace);
		return;
	}

	iEnvCount = 0u;
	if ( sEnvPath != NULL ) {
		arrEnv[iEnvCount++] = (str)sEnvPath;
	}
	if ( sEnvTrace != NULL ) {
		arrEnv[iEnvCount++] = (str)sEnvTrace;
	}

	pConfig->arrEnv = arrEnv;
	pConfig->iEnvCount = iEnvCount;
	pConfig->bInheritEnv = true;
}

static char* __xlBuildFallbackTrace(
	const char* sRunTrace,
	const char* sBuildTrace,
	const xprocessresult* pResult,
	const char* sAsyncTrace
)
{
	const char* sStdout;
	char* sWithBuild;
	char* sWithHeader;
	char* sOut;
	char* sWithAsync;

	sStdout = ((pResult != NULL) && (pResult->pStdout != NULL)) ? (const char*)pResult->pStdout : "";
	sWithBuild = __xlConcatText3(
		(sRunTrace != NULL) ? sRunTrace : "",
		((sBuildTrace != NULL) && (sBuildTrace[0] != '\0')) ? "\n[fallback.build]\n" : "",
		(sBuildTrace != NULL) ? sBuildTrace : ""
	);
	if ( sWithBuild == NULL ) {
		return __xlDupText(sRunTrace);
	}

	sWithHeader = __xlConcatText3(
		sWithBuild,
		"\n[fallback.exec]\n",
		sStdout
	);
	free(sWithBuild);
	if ( sWithHeader == NULL ) {
		return __xlDupText(sRunTrace);
	}

	if ( (sAsyncTrace != NULL) && (sAsyncTrace[0] != '\0') ) {
		sWithAsync = __xlConcatText3(
			sWithHeader,
			"\n[fallback.async_trace]\n",
			sAsyncTrace
		);
		free(sWithHeader);
		if ( sWithAsync == NULL ) {
			return __xlDupText(sRunTrace);
		}
		sOut = sWithAsync;
	} else {
		sOut = sWithHeader;
	}
	return sOut;
}

void xlExecCtxInit(XL_ExecCtx* pCtx, const char* sInputFile, const char* sReleaseRoot)
{
	XL_HostContext hostCtx;
	size_t i;

	if ( pCtx == NULL ) {
		return;
	}

	memset(pCtx, 0, sizeof(*pCtx));
	__xlCopyText(pCtx->sEntrySymbol, sizeof(pCtx->sEntrySymbol), "xl_generated_entry");
	xlHostContextInit(&hostCtx, sInputFile, sReleaseRoot);
	__xlCopyText(pCtx->sInputFile, sizeof(pCtx->sInputFile), hostCtx.sInputFile);
	__xlCopyText(pCtx->sReleaseRoot, sizeof(pCtx->sReleaseRoot), hostCtx.sReleaseRoot);
	__xlCopyText(pCtx->sProjectRoot, sizeof(pCtx->sProjectRoot), hostCtx.sProjectRoot);
	__xlCopyText(pCtx->sTccRoot, sizeof(pCtx->sTccRoot), hostCtx.sTccRoot);
	pCtx->iArtifactCount = hostCtx.iArtifactCount;
	pCtx->iExternSymbolCount = hostCtx.iExternSymbolCount;
	pCtx->iLinkLibraryCount = hostCtx.iLinkLibraryCount;
	pCtx->iLibraryPathCount = hostCtx.iLibraryPathCount;
	pCtx->iIncludePathCount = hostCtx.iIncludePathCount;
	pCtx->iLoadLibraryCount = hostCtx.iLoadLibraryCount;
	for ( i = 0; i < XL_HOST_ARTIFACT_MAX; ++i ) {
		__xlCopyText(pCtx->arrArtifacts[i].sKind, sizeof(pCtx->arrArtifacts[i].sKind), hostCtx.arrArtifacts[i].sKind);
		__xlCopyText(pCtx->arrArtifacts[i].sFile, sizeof(pCtx->arrArtifacts[i].sFile), hostCtx.arrArtifacts[i].sFile);
	}
	for ( i = 0; i < XL_HOST_EXTERN_SYMBOL_MAX; ++i ) {
		__xlCopyText(pCtx->arrExternSymbols[i].sName, sizeof(pCtx->arrExternSymbols[i].sName), hostCtx.arrExternSymbols[i].sName);
	}
	for ( i = 0; i < XL_HOST_LINK_LIBRARY_MAX; ++i ) {
		__xlCopyText(pCtx->arrLinkLibraries[i].sName, sizeof(pCtx->arrLinkLibraries[i].sName), hostCtx.arrLinkLibraries[i].sName);
	}
	for ( i = 0; i < XL_HOST_LIBRARY_PATH_MAX; ++i ) {
		__xlCopyText(pCtx->arrLibraryPaths[i].sPath, sizeof(pCtx->arrLibraryPaths[i].sPath), hostCtx.arrLibraryPaths[i].sPath);
	}
	for ( i = 0; i < XL_HOST_INCLUDE_PATH_MAX; ++i ) {
		__xlCopyText(pCtx->arrIncludePaths[i].sPath, sizeof(pCtx->arrIncludePaths[i].sPath), hostCtx.arrIncludePaths[i].sPath);
	}
	for ( i = 0; i < XL_HOST_LOAD_LIBRARY_MAX; ++i ) {
		__xlCopyText(pCtx->arrLoadLibraries[i].sFile, sizeof(pCtx->arrLoadLibraries[i].sFile), hostCtx.arrLoadLibraries[i].sFile);
	}
}

int xlExecCtxAddArtifact(XL_ExecCtx* pCtx, const char* sKind, const char* sFile)
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

int xlExecCtxAddExternSymbol(XL_ExecCtx* pCtx, const char* sName)
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

int xlExecCtxAddLinkLibrary(XL_ExecCtx* pCtx, const char* sName)
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

int xlExecCtxAddLibraryPath(XL_ExecCtx* pCtx, const char* sPath)
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

int xlExecCtxAddIncludePath(XL_ExecCtx* pCtx, const char* sPath)
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

int xlExecCtxAddLoadLibrary(XL_ExecCtx* pCtx, const char* sFile)
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

int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText)
{
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	XL_HostBuildResult buildResult;
	xprocessconfig procConfig;
	xprocessresult procResult;
	char sFallbackExe[XL_PATH_MAX_LEN];
	char sFallbackTraceFile[XL_PATH_MAX_LEN];
	char* sRunTraceText;
	char* sBuildTraceText;
	char* sFallbackTrace;
	char* sFallbackAsyncTrace;
	int bExecOk;
	int iRc;
	size_t i;

	if ( (pCtx == NULL) || (sCode == NULL) || (psTraceText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&hostCtx, 0, sizeof(hostCtx));
	memset(&result, 0, sizeof(result));
	memset(&buildResult, 0, sizeof(buildResult));
	memset(&procConfig, 0, sizeof(procConfig));
	memset(&procResult, 0, sizeof(procResult));
	sRunTraceText = NULL;
	sBuildTraceText = NULL;
	sFallbackTrace = NULL;
	sFallbackAsyncTrace = NULL;
	__xlCopyText(hostCtx.sInputFile, sizeof(hostCtx.sInputFile), pCtx->sInputFile);
	__xlCopyText(hostCtx.sReleaseRoot, sizeof(hostCtx.sReleaseRoot), pCtx->sReleaseRoot);
	__xlCopyText(hostCtx.sProjectRoot, sizeof(hostCtx.sProjectRoot), pCtx->sProjectRoot);
	__xlCopyText(hostCtx.sTccRoot, sizeof(hostCtx.sTccRoot), pCtx->sTccRoot);
	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		iRc = xlHostContextAddArtifact(&hostCtx, pCtx->arrArtifacts[i].sKind, pCtx->arrArtifacts[i].sFile);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iExternSymbolCount; ++i ) {
		iRc = xlHostContextAddExternSymbol(&hostCtx, pCtx->arrExternSymbols[i].sName);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iLinkLibraryCount; ++i ) {
		iRc = xlHostContextAddLinkLibrary(&hostCtx, pCtx->arrLinkLibraries[i].sName);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iLibraryPathCount; ++i ) {
		iRc = xlHostContextAddLibraryPath(&hostCtx, pCtx->arrLibraryPaths[i].sPath);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iIncludePathCount; ++i ) {
		iRc = xlHostContextAddIncludePath(&hostCtx, pCtx->arrIncludePaths[i].sPath);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	for ( i = 0; i < pCtx->iLoadLibraryCount; ++i ) {
		iRc = xlHostContextAddLoadLibrary(&hostCtx, pCtx->arrLoadLibraries[i].sFile);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	iRc = XL_RC_FAIL;
	iRc = xlHostRunCText(
		&hostCtx,
		pDiag,
		pCtx->sInputFile,
		sCode,
		pCtx->sEntrySymbol,
		&sRunTraceText,
		&result
	);

	if ( psTraceText != NULL ) {
		*psTraceText = sRunTraceText;
	}
	pCtx->iExitCode = result.iExitCode;
	pCtx->bHadRuntimeError = result.bHadRuntimeError;
	pCtx->bHadException = result.bHadException;
	pCtx->iExceptionKind = result.iExceptionKind;
	__xlCopyText(pCtx->sExceptionText, sizeof(pCtx->sExceptionText), result.sExceptionText);
	__xlCopyText(pCtx->sExceptionFile, sizeof(pCtx->sExceptionFile), result.sExceptionFile);
	pCtx->iExceptionLine = result.iExceptionLine;
	pCtx->iExceptionColumn = result.iExceptionColumn;
	pCtx->iExceptionInt = result.iExceptionInt;
	pCtx->fExceptionFloat = result.fExceptionFloat;
	if ( result.sEntrySymbol[0] != '\0' ) {
		__xlCopyText(pCtx->sEntrySymbol, sizeof(pCtx->sEntrySymbol), result.sEntrySymbol);
	}
	if ( iRc == XL_RC_OK ) {
		return iRc;
	}
	if ( result.bHadException ) {
		/*
		 * Preserve the embedded-run failure for language-level exceptions.
		 * The host trace already carries exception.kind / exception.text, and
		 * falling back to an external executable would erase that outcome.
		 */
		return iRc;
	}

	__xlBuildFallbackExePath(pCtx, sFallbackExe, sizeof(sFallbackExe));
	iRc = xlHostBuildCText(
		&hostCtx,
		pDiag,
		pCtx->sInputFile,
		sCode,
		pCtx->sEntrySymbol,
		sFallbackExe,
		&sBuildTraceText,
		&buildResult
	);
	if ( iRc != XL_RC_OK ) {
		sFallbackTrace = __xlConcatText3(
			(sRunTraceText != NULL) ? sRunTraceText : "",
			((sBuildTraceText != NULL) && (sBuildTraceText[0] != '\0')) ? "\n[fallback.build]\n" : "",
			(sBuildTraceText != NULL) ? sBuildTraceText : ""
		);
		if ( sFallbackTrace != NULL ) {
			free(sRunTraceText);
			sRunTraceText = sFallbackTrace;
			if ( psTraceText != NULL ) {
				*psTraceText = sRunTraceText;
			}
		}
		free(sBuildTraceText);
		return iRc;
	}

	__xlCopyText(sFallbackTraceFile, sizeof(sFallbackTraceFile), buildResult.sOutputFile);
	if ( !__xlAppendText(sFallbackTraceFile, sizeof(sFallbackTraceFile), ".trace.txt") ) {
		sFallbackTraceFile[0] = '\0';
	}
	if ( sFallbackTraceFile[0] != '\0' ) {
		__xlRemoveFileBestEffort(sFallbackTraceFile);
	}

	__xlInitExecFallbackConfig(&procConfig, pCtx, buildResult.sOutputFile, pCtx->sProjectRoot, sFallbackTraceFile);
	bExecOk = xrtExecCapture(&procConfig, &procResult, 0u) ? 1 : 0;
	__xlFreeExecFallbackConfig(&procConfig);
	pCtx->iExitCode = procResult.iExitCode;
	pCtx->bHadRuntimeError = bExecOk ? 0 : 1;
	pCtx->bHadException = 0;
	pCtx->iExceptionKind = 0;
	pCtx->sExceptionText[0] = '\0';
	pCtx->iExceptionInt = 0;
	pCtx->fExceptionFloat = 0.0;

	if ( sFallbackTraceFile[0] != '\0' ) {
		sFallbackAsyncTrace = __xlReadTextFile(sFallbackTraceFile);
		__xlRemoveFileBestEffort(sFallbackTraceFile);
	}

	sFallbackTrace = __xlBuildFallbackTrace(sRunTraceText, sBuildTraceText, &procResult, sFallbackAsyncTrace);
	if ( sFallbackTrace != NULL ) {
		free(sRunTraceText);
		sRunTraceText = sFallbackTrace;
		if ( psTraceText != NULL ) {
			*psTraceText = sRunTraceText;
		}
	}

	free(sBuildTraceText);
	free(sFallbackAsyncTrace);
	xrtProcessResultUnit(&procResult);
	if ( buildResult.sOutputFile[0] != '\0' ) {
		__xlRemoveFileBestEffort(buildResult.sOutputFile);
	}
	if ( !bExecOk && (pCtx->iExitCode == 0) ) {
		return XL_RC_FAIL;
	}
	return XL_RC_OK;
}
