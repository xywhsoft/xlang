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

static void __xlFreeExecFallbackConfig(xprocessconfig* pConfig)
{
	if ( (pConfig == NULL) || (pConfig->arrEnv == NULL) ) {
		return;
	}

	if ( pConfig->iEnvCount != 0u && pConfig->arrEnv[0] != NULL ) {
		free(pConfig->arrEnv[0]);
	}
	free(pConfig->arrEnv);
	pConfig->arrEnv = NULL;
	pConfig->iEnvCount = 0u;
}

static void __xlInitExecFallbackConfig(xprocessconfig* pConfig, const XL_ExecCtx* pCtx, const char* sExePath, const char* sWorkDir)
{
	char* sEnvText;
	str* arrEnv;

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

	sEnvText = __xlBuildFallbackPathEnv(pCtx);
	if ( sEnvText == NULL ) {
		return;
	}

	arrEnv = (str*)calloc(1u, sizeof(str));
	if ( arrEnv == NULL ) {
		free(sEnvText);
		return;
	}

	arrEnv[0] = (str)sEnvText;
	pConfig->arrEnv = arrEnv;
	pConfig->iEnvCount = 1u;
	pConfig->bInheritEnv = true;
}

static char* __xlBuildFallbackTrace(
	const char* sRunTrace,
	const char* sBuildTrace,
	const xprocessresult* pResult
)
{
	const char* sStdout;
	char* sWithBuild;
	char* sWithHeader;
	char* sOut;

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

	sOut = sWithHeader;
	return sOut;
}

static int __xlShouldPreferFallbackExe(const XL_ExecCtx* pCtx, const char* sCode)
{
	size_t i;

	if ( pCtx != NULL ) {
		for ( i = 0u; i < pCtx->iArtifactCount; ++i ) {
			if ( strcmp(pCtx->arrArtifacts[i].sKind, "shared") == 0 ) {
				/*
				 * Shared xlp artifacts are currently reliable only through the
				 * embedded TCC path. Emitting a fallback executable leaves TCC
				 * with an invalid import descriptor on Windows.
				 */
				return 0;
			}
		}
	}

	if ( sCode == NULL ) {
		return 0;
	}

	/*
	 * Embedded TCC still mis-executes point-related generated code even though
	 * the equivalent built executable returns the correct result.
	 *
	 * Stage-1 generator yield-expression resume code also relies on a control
	 * flow pattern that the embedded in-memory path currently mis-executes,
	 * while the built fallback executable produces the expected behavior.
	 */
	return
		(strstr(sCode, "xvoCreatePoint(") != NULL) ||
		(strstr(sCode, "xvoGetPoint(") != NULL) ||
		(strstr(sCode, "xl_value_to_string(") != NULL) ||
		(strstr(sCode, "xl_point_to_string(") != NULL) ||
		(strstr(sCode, "xl_gen_resume_result_") != NULL);
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
	pCtx->iArtifactCount = 0;
	pCtx->iExternSymbolCount = 0;
	for ( i = 0; i < XL_HOST_ARTIFACT_MAX; ++i ) {
		pCtx->arrArtifacts[i].sKind[0] = '\0';
		pCtx->arrArtifacts[i].sFile[0] = '\0';
	}
	for ( i = 0; i < XL_HOST_EXTERN_SYMBOL_MAX; ++i ) {
		pCtx->arrExternSymbols[i].sName[0] = '\0';
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

int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText)
{
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	XL_HostBuildResult buildResult;
	xprocessconfig procConfig;
	xprocessresult procResult;
	char sFallbackExe[XL_PATH_MAX_LEN];
	char* sRunTraceText;
	char* sBuildTraceText;
	char* sFallbackTrace;
	int bExecOk;
	int iRc;
	size_t i;
	int bPreferFallbackExe;

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
	bPreferFallbackExe = __xlShouldPreferFallbackExe(pCtx, sCode) ? 1 : 0;
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

	iRc = XL_RC_FAIL;
	if ( !bPreferFallbackExe ) {
		iRc = xlHostRunCText(
			&hostCtx,
			pDiag,
			pCtx->sInputFile,
			sCode,
			pCtx->sEntrySymbol,
			&sRunTraceText,
			&result
		);
	} else {
		sRunTraceText = __xlDupText("[runtime] prefer fallback executable\n");
	}

	if ( psTraceText != NULL ) {
		*psTraceText = sRunTraceText;
	}
	pCtx->iExitCode = result.iExitCode;
	pCtx->bHadRuntimeError = result.bHadRuntimeError;
	pCtx->bHadException = result.bHadException;
	pCtx->iExceptionKind = result.iExceptionKind;
	__xlCopyText(pCtx->sExceptionText, sizeof(pCtx->sExceptionText), result.sExceptionText);
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

	__xlInitExecFallbackConfig(&procConfig, pCtx, buildResult.sOutputFile, pCtx->sProjectRoot);
	bExecOk = xrtExecCapture(&procConfig, &procResult, 0u) ? 1 : 0;
	__xlFreeExecFallbackConfig(&procConfig);
	pCtx->iExitCode = procResult.iExitCode;
	pCtx->bHadRuntimeError = bExecOk ? 0 : 1;
	pCtx->bHadException = 0;
	pCtx->iExceptionKind = 0;
	pCtx->sExceptionText[0] = '\0';
	pCtx->iExceptionInt = 0;
	pCtx->fExceptionFloat = 0.0;

	sFallbackTrace = __xlBuildFallbackTrace(sRunTraceText, sBuildTraceText, &procResult);
	if ( sFallbackTrace != NULL ) {
		free(sRunTraceText);
		sRunTraceText = sFallbackTrace;
		if ( psTraceText != NULL ) {
			*psTraceText = sRunTraceText;
		}
	}

	free(sBuildTraceText);
	xrtProcessResultUnit(&procResult);
	if ( !bExecOk && (pCtx->iExitCode == 0) ) {
		return XL_RC_FAIL;
	}
	return XL_RC_OK;
}
