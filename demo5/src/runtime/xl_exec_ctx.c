#include "xl_exec_ctx.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#include <windows.h>
#endif

typedef struct XL_RunTraceBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_RunTraceBuffer;

typedef struct XL_TccApi {
#if defined(_WIN32) || defined(_WIN64)
	HMODULE hModule;
#else
	void* hModule;
#endif
	TCCState* (*fn_tcc_new)(void);
	void (*fn_tcc_delete)(TCCState* s);
	void (*fn_tcc_set_lib_path)(TCCState* s, const char* path);
	void (*fn_tcc_set_error_func)(TCCState* s, void* opaque, TCCErrorFunc* error_func);
	int (*fn_tcc_set_output_type)(TCCState* s, int output_type);
	int (*fn_tcc_add_include_path)(TCCState* s, const char* pathname);
	int (*fn_tcc_add_sysinclude_path)(TCCState* s, const char* pathname);
	int (*fn_tcc_add_library_path)(TCCState* s, const char* pathname);
	int (*fn_tcc_add_symbol)(TCCState* s, const char* name, const void* val);
	int (*fn_tcc_compile_string)(TCCState* s, const char* buf);
	int (*fn_tcc_relocate)(TCCState* s);
	void* (*fn_tcc_get_symbol)(TCCState* s, const char* name);
} XL_TccApi;

#if defined(_WIN32) || defined(_WIN64)
static const char* __xlHostName(void)
{
	static char sName[256];
	DWORD iSize;

	memset(sName, 0, sizeof(sName));
	iSize = (DWORD)sizeof(sName);
	if ( GetComputerNameA(sName, &iSize) == 0 ) {
		sName[0] = '\0';
	} else {
		sName[sizeof(sName) - 1] = '\0';
	}
	return sName;
}

static const char* __xlHostIp(void)
{
	static char sIp[64];

	/* Keep std.host non-blocking in the demo runtime. */
	snprintf(sIp, sizeof(sIp), "%s", "127.0.0.1");
	return sIp;
}

static const char* __xlEnvGet(const char* sName)
{
	const char* sValue;

	if ( sName == NULL ) {
		return "";
	}
	sValue = getenv(sName);
	return (sValue != NULL) ? sValue : "";
}

static int __xlEnvExists(const char* sName)
{
	const char* sValue;

	if ( sName == NULL ) {
		return 0;
	}
	sValue = getenv(sName);
	return (sValue != NULL) ? 1 : 0;
}

static const char* __xlEnvCwd(void)
{
	static char sPath[XL_PATH_MAX_LEN];
	DWORD iLen;

	memset(sPath, 0, sizeof(sPath));
	iLen = GetCurrentDirectoryA((DWORD)sizeof(sPath), sPath);
	if ( (iLen == 0u) || (iLen >= (DWORD)sizeof(sPath)) ) {
		sPath[0] = '\0';
	}
	return sPath;
}
#else
static const char* __xlHostName(void)
{
	return "";
}

static const char* __xlHostIp(void)
{
	return "";
}

static const char* __xlEnvGet(const char* sName)
{
	const char* sValue;

	if ( sName == NULL ) {
		return "";
	}
	sValue = getenv(sName);
	return (sValue != NULL) ? sValue : "";
}

static int __xlEnvExists(const char* sName)
{
	const char* sValue;

	if ( sName == NULL ) {
		return 0;
	}
	sValue = getenv(sName);
	return (sValue != NULL) ? 1 : 0;
}

static const char* __xlEnvCwd(void)
{
	static char sPath[XL_PATH_MAX_LEN];

	memset(sPath, 0, sizeof(sPath));
	if ( getcwd(sPath, sizeof(sPath)) == NULL ) {
		sPath[0] = '\0';
	}
	return sPath;
}
#endif

static int __xlRunTraceGrow(XL_RunTraceBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 512 : pBuf->iCap;
	while ( iNewCap < (pBuf->iLen + iNeed + 1) ) {
		iNewCap *= 2;
	}

	sNewData = (char*)realloc(pBuf->sData, iNewCap);
	if ( sNewData == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pBuf->sData = sNewData;
	pBuf->iCap = iNewCap;
	return XL_RC_OK;
}

static void __xlRunTraceAppend(XL_RunTraceBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlRunTraceGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlRunTraceAppendFormat(XL_RunTraceBuffer* pBuf, const char* sFormat, ...)
{
	va_list args;
	va_list args2;
	int iNeed;

	va_start(args, sFormat);
	va_copy(args2, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(args2);
		return;
	}

	if ( __xlRunTraceGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlTccErrorThunk(void* pOpaque, const char* sMsg)
{
	XL_RunTraceBuffer* pTrace;

	pTrace = (XL_RunTraceBuffer*)pOpaque;
	if ( (pTrace != NULL) && (sMsg != NULL) ) {
		__xlRunTraceAppendFormat(pTrace, "[tcc] %s\n", sMsg);
	}
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

	if ( iLen > 0 ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
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

	if ( iPartLen > 0 ) {
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

static void __xlBuildPath4(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1, const char* sPart2, const char* sPart3)
{
	__xlCopyText(sOut, iOutSize, sBase);
	__xlAppendPathPart(sOut, iOutSize, sPart1);
	__xlAppendPathPart(sOut, iOutSize, sPart2);
	__xlAppendPathPart(sOut, iOutSize, sPart3);
}

static void __xlFormatExceptionText(
	char* sOut,
	size_t iOutSize,
	int iKind,
	const char* sText,
	int64_t iIntValue,
	double fFloatValue
)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	switch ( iKind ) {
		case 1:
			snprintf(sOut, iOutSize, "%lld", (long long)iIntValue);
			break;
		case 2:
			snprintf(sOut, iOutSize, "%g", fFloatValue);
			break;
		case 3:
			__xlCopyText(sOut, iOutSize, (sText != NULL) ? sText : "");
			break;
		default:
			__xlCopyText(sOut, iOutSize, (sText != NULL) ? sText : "throw");
			break;
	}
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

static int __xlLoadProc(XL_TccApi* pApi, const char* sName, void** ppFn)
{
#if defined(_WIN32) || defined(_WIN64)
	*ppFn = (void*)GetProcAddress(pApi->hModule, sName);
#else
	XL_UNUSED(pApi);
	XL_UNUSED(sName);
	*ppFn = NULL;
#endif
	return (*ppFn != NULL) ? XL_RC_OK : XL_RC_FAIL;
}

static int __xlLoadTccApi(XL_TccApi* pApi, const char* sDllPath)
{
	if ( (pApi == NULL) || (sDllPath == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(pApi, 0, sizeof(*pApi));
#if defined(_WIN32) || defined(_WIN64)
	pApi->hModule = LoadLibraryA(sDllPath);
	if ( pApi->hModule == NULL ) {
		return XL_RC_FAIL;
	}
	if ( (__xlLoadProc(pApi, "tcc_new", (void**)&pApi->fn_tcc_new) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_delete", (void**)&pApi->fn_tcc_delete) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_set_lib_path", (void**)&pApi->fn_tcc_set_lib_path) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_set_error_func", (void**)&pApi->fn_tcc_set_error_func) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_set_output_type", (void**)&pApi->fn_tcc_set_output_type) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_add_include_path", (void**)&pApi->fn_tcc_add_include_path) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_add_sysinclude_path", (void**)&pApi->fn_tcc_add_sysinclude_path) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_add_library_path", (void**)&pApi->fn_tcc_add_library_path) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_add_symbol", (void**)&pApi->fn_tcc_add_symbol) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_compile_string", (void**)&pApi->fn_tcc_compile_string) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_relocate", (void**)&pApi->fn_tcc_relocate) != XL_RC_OK) ||
		(__xlLoadProc(pApi, "tcc_get_symbol", (void**)&pApi->fn_tcc_get_symbol) != XL_RC_OK) ) {
		FreeLibrary(pApi->hModule);
		memset(pApi, 0, sizeof(*pApi));
		return XL_RC_FAIL;
	}
	return XL_RC_OK;
#else
	return XL_RC_NOT_IMPLEMENTED;
#endif
}

static void __xlUnloadTccApi(XL_TccApi* pApi)
{
	if ( pApi == NULL ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( pApi->hModule != NULL ) {
		FreeLibrary(pApi->hModule);
	}
#endif
	memset(pApi, 0, sizeof(*pApi));
}

void xlExecCtxInit(XL_ExecCtx* pCtx, const char* sInputFile, const char* sReleaseRoot)
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
	__xlBuildPath4(pCtx->sLibTccDll, sizeof(pCtx->sLibTccDll), pCtx->sProjectRoot, "lib", "tcc", "libtcc.dll");
	__xlBuildPath3(pCtx->sTccRoot, sizeof(pCtx->sTccRoot), pCtx->sProjectRoot, "release", "tcc");
	__xlCopyText(pCtx->sEntrySymbol, sizeof(pCtx->sEntrySymbol), "xl_generated_entry");
}

int xlExecRunCode(XL_ExecCtx* pCtx, XL_DiagContext* pDiag, const char* sCode, char** psTraceText)
{
	XL_TccApi api;
	XL_RunTraceBuffer trace;
	TCCState* pTcc;
	char sIncludeWin[XL_PATH_MAX_LEN];
	char sIncludeWinApi[XL_PATH_MAX_LEN];
	char sProjectRuntimeInc[XL_PATH_MAX_LEN];
	char sProjectXrtInc[XL_PATH_MAX_LEN];
	char sLibDir[XL_PATH_MAX_LEN];
	int* piHadException;
	int* piExceptionKind;
	const char** psExceptionText;
	int64_t* piExceptionInt;
	double* pfExceptionFloat;
	const char* sAsyncTrace;
	int* piAsyncTraceLen;
	int (*pfnEntry)(void);

	if ( (pCtx == NULL) || (sCode == NULL) || (psTraceText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&trace, 0, sizeof(trace));
	*psTraceText = NULL;
	pCtx->iExitCode = 0;
	pCtx->bHadRuntimeError = 0;
	pCtx->bHadException = 0;
	pCtx->iExceptionKind = 0;
	pCtx->sExceptionText[0] = '\0';
	pCtx->iExceptionInt = 0;
	pCtx->fExceptionFloat = 0.0;

	__xlRunTraceAppendFormat(&trace, "input=%s\n", pCtx->sInputFile);
	__xlRunTraceAppendFormat(&trace, "libtcc=%s\n", pCtx->sLibTccDll);
	__xlRunTraceAppendFormat(&trace, "tcc_root=%s\n", pCtx->sTccRoot);

	if ( __xlLoadTccApi(&api, pCtx->sLibTccDll) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "runtime", "failed to load libtcc dll: %s", pCtx->sLibTccDll);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	pTcc = api.fn_tcc_new();
	if ( pTcc == NULL ) {
		XL_DIAG_ERROR(pDiag, "runtime", "failed to create TCC state");
		__xlUnloadTccApi(&api);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	api.fn_tcc_set_error_func(pTcc, &trace, __xlTccErrorThunk);
	api.fn_tcc_set_lib_path(pTcc, pCtx->sTccRoot);
	api.fn_tcc_set_output_type(pTcc, TCC_OUTPUT_MEMORY);

	__xlBuildPath2(sIncludeWin, sizeof(sIncludeWin), pCtx->sTccRoot, "include_win");
	__xlBuildPath3(sIncludeWinApi, sizeof(sIncludeWinApi), pCtx->sTccRoot, "include_win", "winapi");
	__xlBuildPath3(sProjectRuntimeInc, sizeof(sProjectRuntimeInc), pCtx->sProjectRoot, "src", "runtime");
	__xlBuildPath3(sProjectXrtInc, sizeof(sProjectXrtInc), pCtx->sProjectRoot, "lib", "xrt");
	__xlBuildPath2(sLibDir, sizeof(sLibDir), pCtx->sTccRoot, "lib");
	api.fn_tcc_add_include_path(pTcc, sIncludeWin);
	api.fn_tcc_add_include_path(pTcc, sIncludeWinApi);
	api.fn_tcc_add_include_path(pTcc, sProjectRuntimeInc);
	api.fn_tcc_add_include_path(pTcc, sProjectXrtInc);
	api.fn_tcc_add_sysinclude_path(pTcc, sIncludeWin);
	api.fn_tcc_add_library_path(pTcc, sLibDir);
	api.fn_tcc_add_symbol(pTcc, "xrtThreadIsAttached", xrtThreadIsAttached);
	api.fn_tcc_add_symbol(pTcc, "xrtThreadAttachCurrent", xrtThreadAttachCurrent);
	api.fn_tcc_add_symbol(pTcc, "xrtThreadDetachCurrent", xrtThreadDetachCurrent);
	api.fn_tcc_add_symbol(pTcc, "xrtNow", xrtNow);
	api.fn_tcc_add_symbol(pTcc, "xrtTimeToStr", xrtTimeToStr);
	api.fn_tcc_add_symbol(pTcc, "xrtNowStr", xrtNowStr);
	api.fn_tcc_add_symbol(pTcc, "xrtDateStr", xrtDateStr);
	api.fn_tcc_add_symbol(pTcc, "xrtTimeStr", xrtTimeStr);
	api.fn_tcc_add_symbol(pTcc, "xrtRelativeTime", xrtRelativeTime);
	api.fn_tcc_add_symbol(pTcc, "xrtStrToTime", xrtStrToTime);
	api.fn_tcc_add_symbol(pTcc, "xrtPathJoin", xrtPathJoin);
	api.fn_tcc_add_symbol(pTcc, "xrtPathGetNameExt", xrtPathGetNameExt);
	api.fn_tcc_add_symbol(pTcc, "xrtPathGetName", xrtPathGetName);
	api.fn_tcc_add_symbol(pTcc, "xrtPathGetExt", xrtPathGetExt);
	api.fn_tcc_add_symbol(pTcc, "xrtPathGetDir", xrtPathGetDir);
	api.fn_tcc_add_symbol(pTcc, "xrtPathIsAbs", xrtPathIsAbs);
	api.fn_tcc_add_symbol(pTcc, "xrtPathRandom", xrtPathRandom);
	api.fn_tcc_add_symbol(pTcc, "xrtPathExists", xrtPathExists);
	api.fn_tcc_add_symbol(pTcc, "xrtFileExists", xrtFileExists);
	api.fn_tcc_add_symbol(pTcc, "xrtDirExists", xrtDirExists);
	api.fn_tcc_add_symbol(pTcc, "xrtFileGetSize", xrtFileGetSize);
	api.fn_tcc_add_symbol(pTcc, "xrtFileAppend", xrtFileAppend);
	api.fn_tcc_add_symbol(pTcc, "xrtFileWriteAll", xrtFileWriteAll);
	api.fn_tcc_add_symbol(pTcc, "xrtFileReadAll", xrtFileReadAll);
	api.fn_tcc_add_symbol(pTcc, "xrtFileCopy", xrtFileCopy);
	api.fn_tcc_add_symbol(pTcc, "xrtFileMove", xrtFileMove);
	api.fn_tcc_add_symbol(pTcc, "xrtFileDelete", xrtFileDelete);
	api.fn_tcc_add_symbol(pTcc, "xrtDirCreateAll", xrtDirCreateAll);
	api.fn_tcc_add_symbol(pTcc, "xrtDirScan", xrtDirScan);
	api.fn_tcc_add_symbol(pTcc, "xrtStrComp", xrtStrComp);
	api.fn_tcc_add_symbol(pTcc, "xrtFindStr", xrtFindStr);
	api.fn_tcc_add_symbol(pTcc, "xrtLCase", xrtLCase);
	api.fn_tcc_add_symbol(pTcc, "xrtUCase", xrtUCase);
	api.fn_tcc_add_symbol(pTcc, "xrtReplace", xrtReplace);
	api.fn_tcc_add_symbol(pTcc, "xrtIntFormat", xrtIntFormat);
	api.fn_tcc_add_symbol(pTcc, "xrtNumFormat", xrtNumFormat);
	api.fn_tcc_add_symbol(pTcc, "xrtRand32", xrtRand32);
	api.fn_tcc_add_symbol(pTcc, "xrtRandRange", xrtRandRange);
	api.fn_tcc_add_symbol(pTcc, "xrtRandStr", xrtRandStr);
	api.fn_tcc_add_symbol(pTcc, "xlrt_host_name", __xlHostName);
	api.fn_tcc_add_symbol(pTcc, "xlrt_host_ip", __xlHostIp);
	api.fn_tcc_add_symbol(pTcc, "xlrt_env_get", __xlEnvGet);
	api.fn_tcc_add_symbol(pTcc, "xlrt_env_exists", __xlEnvExists);
	api.fn_tcc_add_symbol(pTcc, "xlrt_env_cwd", __xlEnvCwd);
	api.fn_tcc_add_symbol(pTcc, "xrtProcessConfigInit", xrtProcessConfigInit);
	api.fn_tcc_add_symbol(pTcc, "xrtExecCapture", xrtExecCapture);
	api.fn_tcc_add_symbol(pTcc, "xrtProcessResultUnit", xrtProcessResultUnit);
	api.fn_tcc_add_symbol(pTcc, "xrtCopyStr", xrtCopyStr);

	__xlRunTraceAppend(&trace, "compile_string\n");
	if ( api.fn_tcc_compile_string(pTcc, sCode) < 0 ) {
		XL_DIAG_ERROR(pDiag, "runtime", "tcc compile failed");
		api.fn_tcc_delete(pTcc);
		__xlUnloadTccApi(&api);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	__xlRunTraceAppend(&trace, "relocate\n");
	if ( api.fn_tcc_relocate(pTcc) < 0 ) {
		XL_DIAG_ERROR(pDiag, "runtime", "tcc relocate failed");
		api.fn_tcc_delete(pTcc);
		__xlUnloadTccApi(&api);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	__xlRunTraceAppendFormat(&trace, "resolve_symbol=%s\n", pCtx->sEntrySymbol);
	pfnEntry = (int (*)(void))api.fn_tcc_get_symbol(pTcc, pCtx->sEntrySymbol);
	if ( pfnEntry == NULL ) {
		XL_DIAG_ERROR(pDiag, "runtime", "entry symbol not found: %s", pCtx->sEntrySymbol);
		api.fn_tcc_delete(pTcc);
		__xlUnloadTccApi(&api);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	__xlRunTraceAppend(&trace, "execute\n");
	pCtx->iExitCode = pfnEntry();
	__xlRunTraceAppendFormat(&trace, "exit_code=%d\n", pCtx->iExitCode);

	piHadException = (int*)api.fn_tcc_get_symbol(pTcc, "xl_generated_had_exception");
	piExceptionKind = (int*)api.fn_tcc_get_symbol(pTcc, "xl_generated_exception_kind");
	psExceptionText = (const char**)api.fn_tcc_get_symbol(pTcc, "xl_generated_exception_text");
	piExceptionInt = (int64_t*)api.fn_tcc_get_symbol(pTcc, "xl_generated_exception_int");
	pfExceptionFloat = (double*)api.fn_tcc_get_symbol(pTcc, "xl_generated_exception_float");
	sAsyncTrace = (const char*)api.fn_tcc_get_symbol(pTcc, "xl_generated_async_trace");
	piAsyncTraceLen = (int*)api.fn_tcc_get_symbol(pTcc, "xl_generated_async_trace_len");
	if ( (sAsyncTrace != NULL) && (piAsyncTraceLen != NULL) && (*piAsyncTraceLen > 0) ) {
		__xlRunTraceAppend(&trace, sAsyncTrace);
	}
	if ( (piHadException != NULL) && (*piHadException != 0) ) {
		pCtx->bHadException = 1;
		pCtx->iExceptionKind = (piExceptionKind != NULL) ? *piExceptionKind : 0;
		pCtx->iExceptionInt = (piExceptionInt != NULL) ? *piExceptionInt : 0;
		pCtx->fExceptionFloat = (pfExceptionFloat != NULL) ? *pfExceptionFloat : 0.0;
		__xlFormatExceptionText(
			pCtx->sExceptionText,
			sizeof(pCtx->sExceptionText),
			pCtx->iExceptionKind,
			((psExceptionText != NULL) && (*psExceptionText != NULL)) ? *psExceptionText : "throw",
			pCtx->iExceptionInt,
			pCtx->fExceptionFloat
		);
		__xlRunTraceAppendFormat(&trace, "exception.kind=%d\n", pCtx->iExceptionKind);
		__xlRunTraceAppendFormat(&trace, "exception.text=%s\n", pCtx->sExceptionText);
		XL_DIAG_ERROR(pDiag, "runtime", "uncaught exception: %s", pCtx->sExceptionText);
		api.fn_tcc_delete(pTcc);
		__xlUnloadTccApi(&api);
		pCtx->bHadRuntimeError = 1;
		*psTraceText = trace.sData;
		return XL_RC_FAIL;
	}

	api.fn_tcc_delete(pTcc);
	__xlUnloadTccApi(&api);
	*psTraceText = trace.sData;
	return XL_RC_OK;
}
