#include "xl_host_tcc.h"

#include "xl_host_symbols.h"

#if XLANG_DEBUGMODE_ENABLED
#include "../debug/xl_debugger_debug.h"
#endif

#include "../../lib/libtcc.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <dlfcn.h>
	#include <unistd.h>
#endif

extern void xrtNetSyncShutdownHiddenEngine(void);

typedef struct XL_RunTraceBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_RunTraceBuffer;

typedef enum XL_HostOutputType {
	XL_HOST_OUTPUT_EXE = TCC_OUTPUT_EXE,
	XL_HOST_OUTPUT_OBJ = TCC_OUTPUT_OBJ,
	XL_HOST_OUTPUT_DLL = TCC_OUTPUT_DLL
} XL_HostOutputType;

typedef struct XL_CwdScope {
	int bActive;
	char sPrev[XL_PATH_MAX_LEN];
} XL_CwdScope;

typedef struct XL_SharedArtifactHandle {
	char sFile[XL_PATH_MAX_LEN];
#if defined(_WIN32) || defined(_WIN64)
	HMODULE hModule;
#else
	void* pHandle;
#endif
} XL_SharedArtifactHandle;

typedef struct XL_StaticExtractedFile {
	char sFile[XL_PATH_MAX_LEN];
} XL_StaticExtractedFile;

#define XL_HOST_STATIC_EXTRACT_MAX 256

#if XLANG_DEBUGMODE_ENABLED
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
#else
#define __xlRunTraceAppend(pBuf, sText) ((void)0)
#define __xlRunTraceAppendFormat(...) ((void)0)
#endif

static void __xlSetTraceText(char** psTraceText, XL_RunTraceBuffer* pTrace)
{
	if ( psTraceText == NULL ) {
		return;
	}
#if XLANG_DEBUGMODE_ENABLED
	*psTraceText = (pTrace != NULL) ? pTrace->sData : NULL;
#else
	(void)pTrace;
	*psTraceText = NULL;
#endif
}

#if XLANG_DEBUGMODE_ENABLED
static void __xlTccErrorThunk(void* pOpaque, const char* sMsg)
{
	XL_RunTraceBuffer* pTrace;

	pTrace = (XL_RunTraceBuffer*)pOpaque;
	if ( (pTrace != NULL) && (sMsg != NULL) ) {
		__xlRunTraceAppendFormat(pTrace, "[tcc] %s\n", sMsg);
	}
}
#endif

static void __xlBuildPath2(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1);

static void __xlAppendHostList(char* sOut, size_t iOutSize, size_t* piUsed, const char* sText)
{
	int iNeed;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0) || (piUsed == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return;
	}

	iUsed = *piUsed;
	if ( iUsed >= iOutSize ) {
		iUsed = iOutSize - 1;
	}
	iNeed = snprintf(sOut + iUsed, iOutSize - iUsed, "%s%s", (iUsed == 0) ? "" : ", ", sText);
	if ( iNeed < 0 ) {
		sOut[iUsed] = '\0';
		*piUsed = iUsed;
		return;
	}
	iUsed += (size_t)iNeed;
	if ( iUsed >= iOutSize ) {
		iUsed = iOutSize - 1;
		sOut[iUsed] = '\0';
	}
	*piUsed = iUsed;
}

static void __xlFormatHostLinkLibraries(const XL_HostContext* pCtx, char* sOut, size_t iOutSize)
{
	static const char* arrDefaultLibs[] = { "shell32", "ws2_32", "iphlpapi", "advapi32" };
	size_t i;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	iUsed = 0;
	for ( i = 0; i < XL_COUNTOF(arrDefaultLibs); ++i ) {
		__xlAppendHostList(sOut, iOutSize, &iUsed, arrDefaultLibs[i]);
	}
	for ( i = 0; (pCtx != NULL) && (i < pCtx->iLinkLibraryCount); ++i ) {
		__xlAppendHostList(sOut, iOutSize, &iUsed, pCtx->arrLinkLibraries[i].sName);
	}
	if ( sOut[0] == '\0' ) {
		snprintf(sOut, iOutSize, "%s", "none");
	}
}

static void __xlFormatHostExternSymbols(const XL_HostContext* pCtx, char* sOut, size_t iOutSize)
{
	size_t i;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	iUsed = 0;
	for ( i = 0; (pCtx != NULL) && (i < pCtx->iExternSymbolCount); ++i ) {
		__xlAppendHostList(sOut, iOutSize, &iUsed, pCtx->arrExternSymbols[i].sName);
	}
	if ( sOut[0] == '\0' ) {
		snprintf(sOut, iOutSize, "%s", "none");
	}
}

static void __xlFormatHostLibraryPaths(const XL_HostContext* pCtx, char* sOut, size_t iOutSize)
{
	char sDefaultLibPath[XL_PATH_MAX_LEN];
	size_t i;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	iUsed = 0;
	if ( (pCtx != NULL) && (pCtx->sTccRoot[0] != '\0') ) {
		__xlBuildPath2(sDefaultLibPath, sizeof(sDefaultLibPath), pCtx->sTccRoot, "lib");
		__xlAppendHostList(sOut, iOutSize, &iUsed, sDefaultLibPath);
	}
	for ( i = 0; (pCtx != NULL) && (i < pCtx->iLibraryPathCount); ++i ) {
		__xlAppendHostList(sOut, iOutSize, &iUsed, pCtx->arrLibraryPaths[i].sPath);
	}
	if ( sOut[0] == '\0' ) {
		snprintf(sOut, iOutSize, "%s", "none");
	}
}

static void __xlEmitHostExternLinkDiag(
	XL_DiagContext* pDiag,
	const XL_HostContext* pCtx,
	const char* sAction,
	const char* sLibraryName,
	const char* sSymbolName
)
{
	char sLibs[XL_DIAG_TEXT_MAX];
	char sSymbols[XL_DIAG_TEXT_MAX];
	char sPaths[XL_DIAG_TEXT_MAX];

	if ( pDiag == NULL ) {
		return;
	}

	__xlFormatHostLinkLibraries(pCtx, sLibs, sizeof(sLibs));
	__xlFormatHostExternSymbols(pCtx, sSymbols, sizeof(sSymbols));
	__xlFormatHostLibraryPaths(pCtx, sPaths, sizeof(sPaths));
	XL_DIAG_ERROR(
		pDiag,
		"host.tcc",
		"extern/link failure: %s; library='%s'; symbol='%s'; link libraries=[%s]; extern symbols=[%s]; library search paths=[%s]",
		((sAction != NULL) && (sAction[0] != '\0')) ? sAction : "unknown",
		((sLibraryName != NULL) && (sLibraryName[0] != '\0')) ? sLibraryName : "?",
		((sSymbolName != NULL) && (sSymbolName[0] != '\0')) ? sSymbolName : "?",
		sLibs,
		sSymbols,
		sPaths
	);
}

static int __xlPushProjectCwd(const XL_HostContext* pCtx, XL_RunTraceBuffer* pTrace, XL_CwdScope* pScope)
{
	if ( pScope == NULL ) {
		return XL_RC_BAD_ARG;
	}

	memset(pScope, 0, sizeof(*pScope));
	if ( (pCtx == NULL) || (pCtx->sProjectRoot[0] == '\0') ) {
		return XL_RC_OK;
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( GetCurrentDirectoryA((DWORD)sizeof(pScope->sPrev), pScope->sPrev) == 0u ) {
		return XL_RC_FAIL;
	}
	if ( !SetCurrentDirectoryA(pCtx->sProjectRoot) ) {
		return XL_RC_FAIL;
	}
#else
	if ( getcwd(pScope->sPrev, sizeof(pScope->sPrev)) == NULL ) {
		return XL_RC_FAIL;
	}
	if ( chdir(pCtx->sProjectRoot) != 0 ) {
		return XL_RC_FAIL;
	}
#endif

	pScope->bActive = 1;
	__xlRunTraceAppendFormat(pTrace, "cwd.prev=%s\n", pScope->sPrev);
	__xlRunTraceAppendFormat(pTrace, "cwd.exec=%s\n", pCtx->sProjectRoot);
	return XL_RC_OK;
}

static void __xlPopProjectCwd(XL_RunTraceBuffer* pTrace, XL_CwdScope* pScope)
{
	int bRestored;

	if ( (pScope == NULL) || (pScope->bActive == 0) ) {
		return;
	}

	bRestored = 0;
#if defined(_WIN32) || defined(_WIN64)
	bRestored = SetCurrentDirectoryA(pScope->sPrev) ? 1 : 0;
#else
	bRestored = (chdir(pScope->sPrev) == 0) ? 1 : 0;
#endif

	if ( bRestored ) {
		__xlRunTraceAppendFormat(pTrace, "cwd.restore=%s\n", pScope->sPrev);
	} else {
		__xlRunTraceAppendFormat(pTrace, "cwd.restore_failed=%s\n", pScope->sPrev);
	}
	pScope->bActive = 0;
}

static void __xlBuildPath2(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1)
{
	char sTmp[XL_PATH_MAX_LEN];

	snprintf(sTmp, sizeof(sTmp), "%s", (sBase != NULL) ? sBase : "");
	xlPathJoin(sOut, iOutSize, sTmp, sPart1);
}

static void __xlBuildPath3(char* sOut, size_t iOutSize, const char* sBase, const char* sPart1, const char* sPart2)
{
	char sTmp[XL_PATH_MAX_LEN];

	snprintf(sTmp, sizeof(sTmp), "%s", (sBase != NULL) ? sBase : "");
	xlPathJoin(sTmp, sizeof(sTmp), sTmp, sPart1);
	xlPathJoin(sOut, iOutSize, sTmp, sPart2);
}

static int __xlFileExists(const char* sFilePath)
{
	FILE* pFile;

	if ( (sFilePath == NULL) || (sFilePath[0] == '\0') ) {
		return 0;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return 0;
	}
	fclose(pFile);
	return 1;
}

static int __xlLoadLibraryNeedsFileProbe(const char* sFilePath)
{
	if ( (sFilePath == NULL) || (sFilePath[0] == '\0') ) {
		return 0;
	}
	if ( strchr(sFilePath, '\\') != NULL ) {
		return 1;
	}
	if ( strchr(sFilePath, '/') != NULL ) {
		return 1;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( (strlen(sFilePath) >= 2u) && (sFilePath[1] == ':') ) {
		return 1;
	}
#endif
	return 0;
}

static void __xlBuildLinkLibraryLoadName(char* sOut, size_t iOutSize, const char* sLibName)
{
	const char* sDot;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sOut[0] = '\0';
	if ( (sLibName == NULL) || (sLibName[0] == '\0') ) {
		return;
	}
	if ( (strchr(sLibName, '\\') != NULL) || (strchr(sLibName, '/') != NULL) ) {
		snprintf(sOut, iOutSize, "%s", sLibName);
		return;
	}

	sDot = strrchr(sLibName, '.');
#if defined(_WIN32) || defined(_WIN64)
	if ( sDot != NULL ) {
		snprintf(sOut, iOutSize, "%s", sLibName);
	} else {
		snprintf(sOut, iOutSize, "%s.dll", sLibName);
	}
#else
	if ( sDot != NULL ) {
		snprintf(sOut, iOutSize, "%s", sLibName);
	} else if ( strncmp(sLibName, "lib", 3u) == 0 ) {
		snprintf(sOut, iOutSize, "%s.so", sLibName);
	} else {
		snprintf(sOut, iOutSize, "lib%s.so", sLibName);
	}
#endif
}

static int __xlAsciiStrEqIgnoreCase(const char* a, const char* b)
{
	unsigned char ca;
	unsigned char cb;

	if ( (a == NULL) || (b == NULL) ) {
		return a == b;
	}
	while ( (*a != '\0') && (*b != '\0') ) {
		ca = (unsigned char)*a;
		cb = (unsigned char)*b;
		if ( (ca >= 'A') && (ca <= 'Z') ) ca = (unsigned char)(ca - 'A' + 'a');
		if ( (cb >= 'A') && (cb <= 'Z') ) cb = (unsigned char)(cb - 'A' + 'a');
		if ( ca != cb ) {
			return 0;
		}
		++a;
		++b;
	}
	return (*a == '\0') && (*b == '\0');
}

static int __xlAsciiExtEq(const char* sExt, const char* sWant)
{
	return (sExt != NULL) && __xlAsciiStrEqIgnoreCase(sExt, sWant);
}

static void __xlBuildTccLinkLibraryName(char* sOut, size_t iOutSize, const char* sLibName)
{
	const char* sNameStart;
	const char* sDot;
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sOut[0] = '\0';
	if ( (sLibName == NULL) || (sLibName[0] == '\0') ) {
		return;
	}
	if ( (strchr(sLibName, '\\') != NULL) || (strchr(sLibName, '/') != NULL) ) {
		snprintf(sOut, iOutSize, "%s", sLibName);
		return;
	}

	sNameStart = sLibName;
	sDot = strrchr(sLibName, '.');
	iLen = strlen(sLibName);
#if defined(_WIN32) || defined(_WIN64)
	if ( __xlAsciiExtEq(sDot, ".dll") || __xlAsciiExtEq(sDot, ".lib") || __xlAsciiExtEq(sDot, ".a") ) {
		iLen = (size_t)(sDot - sLibName);
	}
#else
	if ( strncmp(sNameStart, "lib", 3u) == 0 ) {
		sNameStart += 3;
		iLen -= 3u;
	}
	if ( __xlAsciiExtEq(sDot, ".so") || __xlAsciiExtEq(sDot, ".a") ) {
		iLen = (size_t)(sDot - sNameStart);
	}
#endif
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1u;
	}
	memcpy(sOut, sNameStart, iLen);
	sOut[iLen] = '\0';
}

static int __xlReadBinaryFile(const char* sFilePath, unsigned char** ppData, size_t* piSize)
{
	FILE* pFile;
	long iSize;
	size_t iRead;
	unsigned char* pData;

	if ( ppData != NULL ) {
		*ppData = NULL;
	}
	if ( piSize != NULL ) {
		*piSize = 0;
	}
	if ( (sFilePath == NULL) || (ppData == NULL) || (piSize == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	iSize = ftell(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	pData = (unsigned char*)malloc((size_t)iSize);
	if ( pData == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iRead = fread(pData, 1, (size_t)iSize, pFile);
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(pData);
		return XL_RC_IO_ERROR;
	}

	*ppData = pData;
	*piSize = (size_t)iSize;
	return XL_RC_OK;
}

static int __xlWriteBinaryFile(const char* sFilePath, const unsigned char* pData, size_t iSize)
{
	FILE* pFile;

	if ( (sFilePath == NULL) || (pData == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlPathEnsureParentDir(sFilePath) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pFile = fopen(sFilePath, "wb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}
	if ( (iSize != 0) && (fwrite(pData, 1, iSize, pFile) != iSize) ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}
	fclose(pFile);
	return XL_RC_OK;
}

static size_t __xlArParseDecimal(const unsigned char* pText, size_t iLen)
{
	char sBuf[32];
	size_t iCopyLen;
	size_t i;
	size_t iValue;

	if ( (pText == NULL) || (iLen == 0) ) {
		return 0;
	}

	iCopyLen = (iLen < (sizeof(sBuf) - 1)) ? iLen : (sizeof(sBuf) - 1);
	memcpy(sBuf, pText, iCopyLen);
	sBuf[iCopyLen] = '\0';
	for ( i = 0; i < iCopyLen; ++i ) {
		if ( (sBuf[i] == ' ') || (sBuf[i] == '\t') ) {
			sBuf[i] = '\0';
			break;
		}
	}

	iValue = 0;
	for ( i = 0; sBuf[i] != '\0'; ++i ) {
		if ( (sBuf[i] < '0') || (sBuf[i] > '9') ) {
			break;
		}
		iValue = (iValue * 10) + (size_t)(sBuf[i] - '0');
	}
	return iValue;
}

static void __xlArResolveMemberName(
	char* sOut,
	size_t iOutSize,
	const unsigned char* pRawName,
	const unsigned char* pLongNames,
	size_t iLongNamesSize
)
{
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	sOut[0] = '\0';
	if ( pRawName == NULL ) {
		return;
	}

	if ( (pRawName[0] == '/') && (pRawName[1] == ' ') ) {
		snprintf(sOut, iOutSize, "/");
		return;
	}
	if ( (pRawName[0] == '/') && (pRawName[1] == '/') ) {
		snprintf(sOut, iOutSize, "//");
		return;
	}
	if ( (pRawName[0] == '/') && (pRawName[1] >= '0') && (pRawName[1] <= '9') && (pLongNames != NULL) ) {
		size_t iOffset;
		size_t iOutLen;

		iOffset = __xlArParseDecimal(pRawName + 1, 15);
		if ( iOffset >= iLongNamesSize ) {
			return;
		}

		iOutLen = 0;
		for ( i = iOffset; i < iLongNamesSize; ++i ) {
			unsigned char ch;

			ch = pLongNames[i];
			if ( (ch == '\0') || (ch == '\n') || (ch == '\r') ) {
				break;
			}
			if ( ch == '/' ) {
				break;
			}
			if ( iOutLen + 1 >= iOutSize ) {
				break;
			}
			sOut[iOutLen++] = (char)ch;
		}
		sOut[iOutLen] = '\0';
		return;
	}

	for ( i = 0; i < 16; ++i ) {
		unsigned char ch;

		ch = pRawName[i];
		if ( (ch == '\0') || (ch == ' ') || (ch == '/') ) {
			break;
		}
		if ( i + 1 >= iOutSize ) {
			break;
		}
		sOut[i] = (char)ch;
	}
	sOut[i] = '\0';
}

static void __xlCleanupStaticExtractedFiles(XL_StaticExtractedFile* arrFiles, size_t iFileCount)
{
	size_t i;

	if ( arrFiles == NULL ) {
		return;
	}

	for ( i = 0; i < iFileCount; ++i ) {
		if ( arrFiles[i].sFile[0] != '\0' ) {
			remove(arrFiles[i].sFile);
		}
	}
}

static int __xlApplyStaticArchive(
	const XL_HostContext* pCtx,
	TCCState* pTcc,
	XL_RunTraceBuffer* pTrace,
	const char* sArchiveFile,
	size_t iArtifactIndex,
	XL_StaticExtractedFile* arrExtractedFiles,
	size_t iExtractedCap,
	size_t* piExtractedCount
)
{
	unsigned char* pArchive;
	size_t iArchiveSize;
	const unsigned char* pLongNames;
	size_t iLongNamesSize;
	size_t iPos;
	size_t iMemberIndex;
	int iRc;

	if ( (pCtx == NULL) || (pTcc == NULL) || (sArchiveFile == NULL) || (arrExtractedFiles == NULL) || (piExtractedCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	pArchive = NULL;
	iArchiveSize = 0;
	pLongNames = NULL;
	iLongNamesSize = 0;
	iRc = __xlReadBinaryFile(sArchiveFile, &pArchive, &iArchiveSize);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	if ( (iArchiveSize < 8) || (memcmp(pArchive, "!<arch>\n", 8) != 0) ) {
		free(pArchive);
		return XL_RC_FAIL;
	}

	iPos = 8;
	iMemberIndex = 0;
	while ( (iPos + 60) <= iArchiveSize ) {
		const unsigned char* pHeader;
		const unsigned char* pData;
		size_t iMemberSize;
		char sMemberName[128];

		pHeader = pArchive + iPos;
		iPos += 60;
		if ( (pHeader[58] != '`') || (pHeader[59] != '\n') ) {
			free(pArchive);
			return XL_RC_FAIL;
		}

		iMemberSize = __xlArParseDecimal(pHeader + 48, 10);
		if ( (iPos + iMemberSize) > iArchiveSize ) {
			free(pArchive);
			return XL_RC_FAIL;
		}

		pData = pArchive + iPos;
		__xlArResolveMemberName(sMemberName, sizeof(sMemberName), pHeader, pLongNames, iLongNamesSize);
		if ( strcmp(sMemberName, "/") == 0 ) {
			/* GNU/BSD symbol table member. */
		} else if ( strcmp(sMemberName, "//") == 0 ) {
			pLongNames = pData;
			iLongNamesSize = iMemberSize;
		} else if ( sMemberName[0] != '\0' ) {
			char sExtractPath[XL_PATH_MAX_LEN];
			char sExtractDir[XL_PATH_MAX_LEN];
			char sExtractName[64];

			if ( *piExtractedCount >= iExtractedCap ) {
				free(pArchive);
				return XL_RC_FAIL;
			}

			xlPathJoin(sExtractDir, sizeof(sExtractDir), pCtx->sReleaseRoot, "tmp");
			xlPathJoin(sExtractDir, sizeof(sExtractDir), sExtractDir, "host_static");
			snprintf(sExtractName, sizeof(sExtractName), "artifact_%zu_member_%zu.obj", iArtifactIndex, iMemberIndex);
			xlPathJoin(sExtractPath, sizeof(sExtractPath), sExtractDir, sExtractName);
			iRc = __xlWriteBinaryFile(sExtractPath, pData, iMemberSize);
			if ( iRc != XL_RC_OK ) {
				free(pArchive);
				return iRc;
			}
			snprintf(arrExtractedFiles[*piExtractedCount].sFile, sizeof(arrExtractedFiles[*piExtractedCount].sFile), "%s", sExtractPath);
			*piExtractedCount += 1;
			iMemberIndex += 1;

			__xlRunTraceAppendFormat(pTrace, "artifact.static.object=%s\n", sExtractPath);
			if ( tcc_add_file(pTcc, sExtractPath) < 0 ) {
				free(pArchive);
				return XL_RC_FAIL;
			}
		}

		iPos += iMemberSize;
		if ( (iPos & 1u) != 0u ) {
			iPos += 1;
		}
	}

	free(pArchive);
	return XL_RC_OK;
}

static int __xlApplyStaticArtifacts(
	const XL_HostContext* pCtx,
	TCCState* pTcc,
	XL_RunTraceBuffer* pTrace,
	XL_StaticExtractedFile* arrExtractedFiles,
	size_t iExtractedCap,
	size_t* piExtractedCount
)
{
	size_t i;

	if ( piExtractedCount != NULL ) {
		*piExtractedCount = 0;
	}
	if ( (pCtx == NULL) || (pTcc == NULL) || (arrExtractedFiles == NULL) || (piExtractedCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		const XL_HostArtifact* pArtifact;

		pArtifact = &pCtx->arrArtifacts[i];
		if ( strcmp(pArtifact->sKind, "static") != 0 ) {
			continue;
		}
		__xlRunTraceAppendFormat(pTrace, "artifact.static.link=%s\n", pArtifact->sFile);
		if ( !__xlFileExists(pArtifact->sFile) ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.static.missing=%s\n", pArtifact->sFile);
			return XL_RC_FAIL;
		}

		if ( __xlApplyStaticArchive(
			pCtx,
			pTcc,
			pTrace,
			pArtifact->sFile,
			i,
			arrExtractedFiles,
			iExtractedCap,
			piExtractedCount
		) == XL_RC_OK ) {
			continue;
		}

		if ( tcc_add_file(pTcc, pArtifact->sFile) < 0 ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.static.link_failed=%s\n", pArtifact->sFile);
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlApplySharedArtifacts(
	const XL_HostContext* pCtx,
	TCCState* pTcc,
	XL_RunTraceBuffer* pTrace
)
{
	size_t i;

	if ( (pCtx == NULL) || (pTcc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		const XL_HostArtifact* pArtifact;

		pArtifact = &pCtx->arrArtifacts[i];
		if ( strcmp(pArtifact->sKind, "shared") != 0 ) {
			continue;
		}
		__xlRunTraceAppendFormat(pTrace, "artifact.shared.link=%s\n", pArtifact->sFile);
		if ( !__xlFileExists(pArtifact->sFile) ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.missing=%s\n", pArtifact->sFile);
			return XL_RC_FAIL;
		}
		if ( tcc_add_file(pTcc, pArtifact->sFile) < 0 ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.link_failed=%s\n", pArtifact->sFile);
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlLoadSharedArtifacts(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	XL_RunTraceBuffer* pTrace,
	XL_SharedArtifactHandle* arrHandles,
	size_t iHandleCap,
	size_t* piHandleCount
)
{
	size_t i;

	if ( piHandleCount != NULL ) {
		*piHandleCount = 0;
	}
	if ( (pCtx == NULL) || (arrHandles == NULL) || (piHandleCount == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iArtifactCount; ++i ) {
		const XL_HostArtifact* pArtifact;
		XL_SharedArtifactHandle* pHandle;

		pArtifact = &pCtx->arrArtifacts[i];
		if ( strcmp(pArtifact->sKind, "shared") != 0 ) {
			continue;
		}
		if ( *piHandleCount >= iHandleCap ) {
			return XL_RC_FAIL;
		}
		__xlRunTraceAppendFormat(pTrace, "artifact.shared.load=%s\n", pArtifact->sFile);
		if ( !__xlFileExists(pArtifact->sFile) ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.missing=%s\n", pArtifact->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "shared artifact file was not found", pArtifact->sFile, NULL);
			return XL_RC_FAIL;
		}

		pHandle = &arrHandles[*piHandleCount];
		memset(pHandle, 0, sizeof(*pHandle));
		snprintf(pHandle->sFile, sizeof(pHandle->sFile), "%s", pArtifact->sFile);
#if defined(_WIN32) || defined(_WIN64)
		pHandle->hModule = LoadLibraryA(pArtifact->sFile);
		if ( pHandle->hModule == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.load_failed=%s\n", pArtifact->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load shared artifact", pArtifact->sFile, NULL);
			return XL_RC_FAIL;
		}
#else
		pHandle->pHandle = dlopen(pArtifact->sFile, RTLD_NOW | RTLD_LOCAL);
		if ( pHandle->pHandle == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.load_failed=%s\n", pArtifact->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load shared artifact", pArtifact->sFile, NULL);
			return XL_RC_FAIL;
		}
#endif
		__xlRunTraceAppendFormat(pTrace, "artifact.shared.ready=%s\n", pArtifact->sFile);
		*piHandleCount += 1;
	}

	for ( i = 0; i < pCtx->iLoadLibraryCount; ++i ) {
		const XL_HostLoadLibrary* pLib;
		XL_SharedArtifactHandle* pHandle;

		pLib = &pCtx->arrLoadLibraries[i];
		if ( pLib->sFile[0] == '\0' ) {
			continue;
		}
		if ( *piHandleCount >= iHandleCap ) {
			return XL_RC_FAIL;
		}
		__xlRunTraceAppendFormat(pTrace, "loadlib.load=%s\n", pLib->sFile);
		if ( __xlLoadLibraryNeedsFileProbe(pLib->sFile) && !__xlFileExists(pLib->sFile) ) {
			__xlRunTraceAppendFormat(pTrace, "loadlib.missing=%s\n", pLib->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "dynamic library file was not found", pLib->sFile, NULL);
			return XL_RC_FAIL;
		}

		pHandle = &arrHandles[*piHandleCount];
		memset(pHandle, 0, sizeof(*pHandle));
		snprintf(pHandle->sFile, sizeof(pHandle->sFile), "%s", pLib->sFile);
#if defined(_WIN32) || defined(_WIN64)
		pHandle->hModule = LoadLibraryA(pLib->sFile);
		if ( pHandle->hModule == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "loadlib.load_failed=%s\n", pLib->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load dynamic library", pLib->sFile, NULL);
			return XL_RC_FAIL;
		}
#else
		pHandle->pHandle = dlopen(pLib->sFile, RTLD_NOW | RTLD_LOCAL);
		if ( pHandle->pHandle == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "loadlib.load_failed=%s\n", pLib->sFile);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load dynamic library", pLib->sFile, NULL);
			return XL_RC_FAIL;
		}
#endif
		__xlRunTraceAppendFormat(pTrace, "loadlib.ready=%s\n", pLib->sFile);
		*piHandleCount += 1;
	}

	for ( i = 0; i < pCtx->iLinkLibraryCount; ++i ) {
		const XL_HostLinkLibrary* pLib;
		XL_SharedArtifactHandle* pHandle;
		char sLoadName[XL_PATH_MAX_LEN];

		pLib = &pCtx->arrLinkLibraries[i];
		if ( pLib->sName[0] == '\0' ) {
			continue;
		}
		if ( *piHandleCount >= iHandleCap ) {
			return XL_RC_FAIL;
		}

		__xlBuildLinkLibraryLoadName(sLoadName, sizeof(sLoadName), pLib->sName);
		if ( sLoadName[0] == '\0' ) {
			continue;
		}
		__xlRunTraceAppendFormat(pTrace, "linklib.load=%s\n", sLoadName);
		if ( __xlLoadLibraryNeedsFileProbe(sLoadName) && !__xlFileExists(sLoadName) ) {
			__xlRunTraceAppendFormat(pTrace, "linklib.missing=%s\n", sLoadName);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "linked library file was not found", sLoadName, NULL);
			return XL_RC_FAIL;
		}

		pHandle = &arrHandles[*piHandleCount];
		memset(pHandle, 0, sizeof(*pHandle));
		snprintf(pHandle->sFile, sizeof(pHandle->sFile), "%s", sLoadName);
#if defined(_WIN32) || defined(_WIN64)
		pHandle->hModule = LoadLibraryA(sLoadName);
		if ( pHandle->hModule == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "linklib.load_failed=%s\n", sLoadName);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load linked library", sLoadName, NULL);
			return XL_RC_FAIL;
		}
#else
		pHandle->pHandle = dlopen(sLoadName, RTLD_NOW | RTLD_LOCAL);
		if ( pHandle->pHandle == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "linklib.load_failed=%s\n", sLoadName);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to load linked library", sLoadName, NULL);
			return XL_RC_FAIL;
		}
#endif
		__xlRunTraceAppendFormat(pTrace, "linklib.ready=%s\n", sLoadName);
		*piHandleCount += 1;
	}

	return XL_RC_OK;
}

static void __xlUnloadSharedArtifacts(XL_RunTraceBuffer* pTrace, XL_SharedArtifactHandle* arrHandles, size_t iHandleCount)
{
	size_t i;

	if ( arrHandles == NULL ) {
		return;
	}

	for ( i = iHandleCount; i > 0; --i ) {
		XL_SharedArtifactHandle* pHandle;

		pHandle = &arrHandles[i - 1];
#if defined(_WIN32) || defined(_WIN64)
		if ( pHandle->hModule != NULL ) {
			FreeLibrary(pHandle->hModule);
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.unload=%s\n", pHandle->sFile);
		}
#else
		if ( pHandle->pHandle != NULL ) {
			dlclose(pHandle->pHandle);
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.unload=%s\n", pHandle->sFile);
		}
#endif
	}
}

static int __xlRegisterSharedArtifactSymbols(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	TCCState* pTcc,
	XL_RunTraceBuffer* pTrace,
	XL_SharedArtifactHandle* arrHandles,
	size_t iHandleCount
)
{
	size_t i;

	if ( (pCtx == NULL) || (pTcc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pCtx->iExternSymbolCount; ++i ) {
		size_t j;
		void* pSymbol;

		pSymbol = pCtx->arrExternSymbols[i].pAddress;
		for ( j = 0; (pSymbol == NULL) && (j < iHandleCount); ++j ) {
#if defined(_WIN32) || defined(_WIN64)
			pSymbol = (void*)GetProcAddress(arrHandles[j].hModule, pCtx->arrExternSymbols[i].sName);
#else
			pSymbol = dlsym(arrHandles[j].pHandle, pCtx->arrExternSymbols[i].sName);
#endif
		}
		if ( pSymbol == NULL ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.symbol_missing=%s\n", pCtx->arrExternSymbols[i].sName);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "external symbol was not found in loaded shared libraries", NULL, pCtx->arrExternSymbols[i].sName);
			return XL_RC_FAIL;
		}
		__xlRunTraceAppendFormat(pTrace, "artifact.shared.symbol=%s\n", pCtx->arrExternSymbols[i].sName);
		if ( tcc_add_symbol(pTcc, pCtx->arrExternSymbols[i].sName, pSymbol) < 0 ) {
			__xlRunTraceAppendFormat(pTrace, "artifact.shared.symbol_register_failed=%s\n", pCtx->arrExternSymbols[i].sName);
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to register external symbol with TCC", NULL, pCtx->arrExternSymbols[i].sName);
			return XL_RC_FAIL;
		}
#if defined(_WIN32) || defined(_WIN64)
		{
			char sAlias[160];

			snprintf(sAlias, sizeof(sAlias), "_%s", pCtx->arrExternSymbols[i].sName);
			tcc_add_symbol(pTcc, sAlias, pSymbol);
			snprintf(sAlias, sizeof(sAlias), "__imp_%s", pCtx->arrExternSymbols[i].sName);
			tcc_add_symbol(pTcc, sAlias, pSymbol);
			snprintf(sAlias, sizeof(sAlias), "__imp__%s", pCtx->arrExternSymbols[i].sName);
			tcc_add_symbol(pTcc, sAlias, pSymbol);
		}
#endif
	}

	return XL_RC_OK;
}

static char* __xlWrapSourceText(const char* sVirtualPath, const char* sCode)
{
	const char* sPath;
	size_t iHeadLen;
	size_t iCodeLen;
	size_t iOutLen;
	char* sOut;

	sPath = (sVirtualPath != NULL) ? sVirtualPath : "<memory>";
	if ( sCode == NULL ) {
		sCode = "";
	}

	iHeadLen = strlen(sPath) + 32;
	iCodeLen = strlen(sCode);
	iOutLen = iHeadLen + iCodeLen + 1;
	sOut = (char*)malloc(iOutLen);
	if ( sOut == NULL ) {
		return NULL;
	}

	snprintf(sOut, iOutLen, "#line 1 \"%s\"\n%s", sPath, sCode);
	return sOut;
}

static char* __xlWrapBuildText(
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	int bNeedMainEntry
)
{
	char* sBase;
	char* sOut;
	size_t iBaseLen;
	size_t iOutLen;

	sBase = __xlWrapSourceText(sVirtualPath, sCode);
	if ( sBase == NULL ) {
		return NULL;
	}

	if ( !bNeedMainEntry || (sEntrySymbol == NULL) || (sEntrySymbol[0] == '\0') ) {
		return sBase;
	}

	iBaseLen = strlen(sBase);
	iOutLen = iBaseLen + strlen(sEntrySymbol) + 96u;
	sOut = (char*)malloc(iOutLen);
	if ( sOut == NULL ) {
		free(sBase);
		return NULL;
	}

	snprintf(
		sOut,
		iOutLen,
		"%s\nint main(int argc, char** argv)\n{\n\t(void)argc;\n\t(void)argv;\n\treturn %s();\n}\n",
		sBase,
		sEntrySymbol
	);
	free(sBase);
	return sOut;
}

static int __xlConfigureTcc(
	const XL_HostContext* pCtx,
	TCCState* pTcc,
	XL_RunTraceBuffer* pTrace,
	int bRegisterSymbols
)
{
	char sIncludeWin[XL_PATH_MAX_LEN];
	char sIncludeWinApi[XL_PATH_MAX_LEN];
	char sProjectCommonInc[XL_PATH_MAX_LEN];
	char sProjectRuntimeInc[XL_PATH_MAX_LEN];
	char sProjectXrtInc[XL_PATH_MAX_LEN];
	char sLibDir[XL_PATH_MAX_LEN];

	if ( (pCtx == NULL) || (pTcc == NULL) ) {
		return XL_RC_BAD_ARG;
	}

#if XLANG_DEBUGMODE_ENABLED
	tcc_set_error_func(pTcc, pTrace, __xlTccErrorThunk);
#else
	tcc_set_error_func(pTcc, NULL, NULL);
#endif
	tcc_set_lib_path(pTcc, pCtx->sTccRoot);
	tcc_set_options(
		pTcc,
		pCtx->bDebugMode
			? "-std=c99 -D_CRT_SECURE_NO_WARNINGS -DXLANG_DEBUGMODE=1 -include xlrt_profile.h"
			: "-std=c99 -D_CRT_SECURE_NO_WARNINGS -include xlrt_profile.h"
	);

	__xlBuildPath2(sIncludeWin, sizeof(sIncludeWin), pCtx->sTccRoot, "include_win");
	__xlBuildPath3(sIncludeWinApi, sizeof(sIncludeWinApi), pCtx->sTccRoot, "include_win", "winapi");
	__xlBuildPath3(sProjectCommonInc, sizeof(sProjectCommonInc), pCtx->sReleaseRoot, "tcc", "support");
	xlPathJoin(sProjectCommonInc, sizeof(sProjectCommonInc), sProjectCommonInc, "common");
	__xlBuildPath3(sProjectRuntimeInc, sizeof(sProjectRuntimeInc), pCtx->sReleaseRoot, "tcc", "support");
	xlPathJoin(sProjectRuntimeInc, sizeof(sProjectRuntimeInc), sProjectRuntimeInc, "runtime");
	__xlBuildPath2(sProjectXrtInc, sizeof(sProjectXrtInc), pCtx->sTccRoot, "include");
	__xlBuildPath2(sLibDir, sizeof(sLibDir), pCtx->sTccRoot, "lib");

	__xlRunTraceAppendFormat(pTrace, "tcc_root=%s\n", pCtx->sTccRoot);
	__xlRunTraceAppendFormat(pTrace, "include=%s\n", sIncludeWin);
	__xlRunTraceAppendFormat(pTrace, "include=%s\n", sIncludeWinApi);
	__xlRunTraceAppendFormat(pTrace, "include=%s\n", sProjectCommonInc);
	__xlRunTraceAppendFormat(pTrace, "include=%s\n", sProjectRuntimeInc);
	__xlRunTraceAppendFormat(pTrace, "include=%s\n", sProjectXrtInc);
	__xlRunTraceAppendFormat(pTrace, "lib=%s\n", sLibDir);

	if ( tcc_add_include_path(pTcc, sIncludeWin) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_include_path(pTcc, sIncludeWinApi) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_include_path(pTcc, sProjectCommonInc) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_include_path(pTcc, sProjectRuntimeInc) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_include_path(pTcc, sProjectXrtInc) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_sysinclude_path(pTcc, sIncludeWin) < 0 ) return XL_RC_FAIL;
	if ( tcc_add_library_path(pTcc, sLibDir) < 0 ) return XL_RC_FAIL;
	{
		size_t i;

		for ( i = 0; i < pCtx->iIncludePathCount; ++i ) {
			const char* sPath;

			sPath = pCtx->arrIncludePaths[i].sPath;
			if ( sPath[0] == '\0' ) {
				continue;
			}
			__xlRunTraceAppendFormat(pTrace, "include=%s\n", sPath);
			if ( tcc_add_include_path(pTcc, sPath) < 0 ) return XL_RC_FAIL;
			if ( tcc_add_sysinclude_path(pTcc, sPath) < 0 ) return XL_RC_FAIL;
		}
		for ( i = 0; i < pCtx->iLibraryPathCount; ++i ) {
			const char* sPath;

			sPath = pCtx->arrLibraryPaths[i].sPath;
			if ( sPath[0] == '\0' ) {
				continue;
			}
			__xlRunTraceAppendFormat(pTrace, "lib=%s\n", sPath);
			if ( tcc_add_library_path(pTcc, sPath) < 0 ) return XL_RC_FAIL;
		}
	}
	if ( bRegisterSymbols && (xlHostRegisterSymbols(pTcc) != XL_RC_OK) ) return XL_RC_FAIL;
#if XLANG_DEBUGMODE_ENABLED
	if ( bRegisterSymbols && (xlDebugRegisterHostSymbols(pTcc) != XL_RC_OK) ) return XL_RC_FAIL;
#endif

	return XL_RC_OK;
}

static void __xlInitRunResult(XL_HostRunResult* pResult, const char* sEntrySymbol)
{
	if ( pResult == NULL ) {
		return;
	}

	memset(pResult, 0, sizeof(*pResult));
	snprintf(pResult->sEntrySymbol, sizeof(pResult->sEntrySymbol), "%s", (sEntrySymbol != NULL) ? sEntrySymbol : "");
}

static void __xlInitBuildResult(XL_HostBuildResult* pResult, const char* sOutputFile, const char* sOutputKind, const char* sEntrySymbol)
{
	if ( pResult == NULL ) {
		return;
	}

	memset(pResult, 0, sizeof(*pResult));
	snprintf(pResult->sOutputFile, sizeof(pResult->sOutputFile), "%s", (sOutputFile != NULL) ? sOutputFile : "");
	snprintf(pResult->sOutputKind, sizeof(pResult->sOutputKind), "%s", (sOutputKind != NULL) ? sOutputKind : "");
	snprintf(pResult->sEntrySymbol, sizeof(pResult->sEntrySymbol), "%s", (sEntrySymbol != NULL) ? sEntrySymbol : "");
}

static void __xlFillExceptionInfo(TCCState* pTcc, XL_RunTraceBuffer* pTrace, XL_HostRunResult* pResult)
{
	int* piHadException;
	int* piExceptionKind;
	const char** psExceptionText;
	const char** psExceptionFile;
	int* piExceptionLine;
	int* piExceptionColumn;
	int64_t* piExceptionInt;
	double* pfExceptionFloat;

	if ( (pTcc == NULL) || (pResult == NULL) ) {
		return;
	}

	piHadException = (int*)tcc_get_symbol(pTcc, "xl_generated_had_exception");
	piExceptionKind = (int*)tcc_get_symbol(pTcc, "xl_generated_exception_kind");
	psExceptionText = (const char**)tcc_get_symbol(pTcc, "xl_generated_exception_text");
	psExceptionFile = (const char**)tcc_get_symbol(pTcc, "xl_generated_exception_file");
	piExceptionLine = (int*)tcc_get_symbol(pTcc, "xl_generated_exception_line");
	piExceptionColumn = (int*)tcc_get_symbol(pTcc, "xl_generated_exception_column");
	piExceptionInt = (int64_t*)tcc_get_symbol(pTcc, "xl_generated_exception_int");
	pfExceptionFloat = (double*)tcc_get_symbol(pTcc, "xl_generated_exception_float");
	if ( (piHadException == NULL) || (*piHadException == 0) ) {
		return;
	}

	pResult->bHadException = 1;
	pResult->iExceptionKind = (piExceptionKind != NULL) ? *piExceptionKind : 0;
	pResult->iExceptionInt = (piExceptionInt != NULL) ? *piExceptionInt : 0;
	pResult->fExceptionFloat = (pfExceptionFloat != NULL) ? *pfExceptionFloat : 0.0;
	snprintf(
		pResult->sExceptionText,
		sizeof(pResult->sExceptionText),
		"%s",
		((psExceptionText != NULL) && (*psExceptionText != NULL)) ? *psExceptionText : "throw"
	);
	snprintf(
		pResult->sExceptionFile,
		sizeof(pResult->sExceptionFile),
		"%s",
		((psExceptionFile != NULL) && (*psExceptionFile != NULL)) ? *psExceptionFile : ""
	);
	pResult->iExceptionLine = (piExceptionLine != NULL) ? *piExceptionLine : 0;
	pResult->iExceptionColumn = (piExceptionColumn != NULL) ? *piExceptionColumn : 0;
	__xlRunTraceAppendFormat(pTrace, "exception.kind=%d\n", pResult->iExceptionKind);
	__xlRunTraceAppendFormat(pTrace, "exception.text=%s\n", pResult->sExceptionText);
	if ( pResult->iExceptionLine > 0 ) {
		__xlRunTraceAppendFormat(
			pTrace,
			"exception.source=%s:%d:%d\n",
			(pResult->sExceptionFile[0] != '\0') ? pResult->sExceptionFile : "<source>",
			pResult->iExceptionLine,
			pResult->iExceptionColumn
		);
	}
}

static XL_HostOutputType __xlDetectOutputType(const char* sOutputFile, const char** psKindText)
{
	const char* sExt;

	if ( psKindText != NULL ) {
		*psKindText = "exe";
	}
	if ( sOutputFile == NULL ) {
		return XL_HOST_OUTPUT_EXE;
	}

	sExt = strrchr(sOutputFile, '.');
	if ( sExt == NULL ) {
		return XL_HOST_OUTPUT_EXE;
	}
	if ( (_stricmp(sExt, ".obj") == 0) || (_stricmp(sExt, ".o") == 0) ) {
		if ( psKindText != NULL ) *psKindText = "obj";
		return XL_HOST_OUTPUT_OBJ;
	}
	if ( _stricmp(sExt, ".dll") == 0 ) {
		if ( psKindText != NULL ) *psKindText = "dll";
		return XL_HOST_OUTPUT_DLL;
	}
	return XL_HOST_OUTPUT_EXE;
}

static int __xlAddLinkLibrary(TCCState* pTcc, XL_RunTraceBuffer* pTrace, const char* sLibName)
{
	char sTccLibName[XL_PATH_MAX_LEN];

	if ( (pTcc == NULL) || (sLibName == NULL) || (sLibName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	__xlBuildTccLinkLibraryName(sTccLibName, sizeof(sTccLibName), sLibName);
	if ( sTccLibName[0] == '\0' ) {
		return XL_RC_BAD_ARG;
	}
	if ( strcmp(sTccLibName, sLibName) != 0 ) {
		__xlRunTraceAppendFormat(pTrace, "link_lib_source=%s\n", sLibName);
	}
	__xlRunTraceAppendFormat(pTrace, "link_lib=%s\n", sTccLibName);
	if ( tcc_add_library(pTcc, sTccLibName) < 0 ) {
		return XL_RC_FAIL;
	}
	return XL_RC_OK;
}

static int __xlHasLinkLibrary(const XL_HostContext* pCtx, const char* sLibName)
{
	size_t i;
	char sWant[XL_PATH_MAX_LEN];
	char sHave[XL_PATH_MAX_LEN];

	if ( (pCtx == NULL) || (sLibName == NULL) || (sLibName[0] == '\0') ) {
		return 0;
	}

	__xlBuildTccLinkLibraryName(sWant, sizeof(sWant), sLibName);
	for ( i = 0; i < pCtx->iLinkLibraryCount; ++i ) {
		__xlBuildTccLinkLibraryName(sHave, sizeof(sHave), pCtx->arrLinkLibraries[i].sName);
		if ( strcmp(sHave, sWant) == 0 ) {
			return 1;
		}
	}
	return 0;
}

static int __xlAddBuildLibraries(const XL_HostContext* pCtx, XL_DiagContext* pDiag, TCCState* pTcc, XL_RunTraceBuffer* pTrace)
{
	static const char* arrLibs[] = { "shell32", "ws2_32", "iphlpapi", "advapi32" };
	size_t i;

	for ( i = 0; i < XL_COUNTOF(arrLibs); ++i ) {
		if ( __xlHasLinkLibrary(pCtx, arrLibs[i]) ) {
			continue;
		}
		if ( __xlAddLinkLibrary(pTcc, pTrace, arrLibs[i]) != XL_RC_OK ) {
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to add default link library", arrLibs[i], NULL);
			return XL_RC_FAIL;
		}
	}
	for ( i = 0; (pCtx != NULL) && (i < pCtx->iLinkLibraryCount); ++i ) {
		if ( __xlAddLinkLibrary(pTcc, pTrace, pCtx->arrLinkLibraries[i].sName) != XL_RC_OK ) {
			__xlEmitHostExternLinkDiag(pDiag, pCtx, "failed to add requested link library", pCtx->arrLinkLibraries[i].sName, NULL);
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlAddBuildSupportSources(const XL_HostContext* pCtx, TCCState* pTcc, XL_RunTraceBuffer* pTrace)
{
	char sHostRuntimeImpl[XL_PATH_MAX_LEN];
	char sRuntimeDir[XL_PATH_MAX_LEN];
	char sRuntimeImpl[XL_PATH_MAX_LEN];
	static const char* arrRuntimeSources[] = {
		"xlrt_host.c",
		"xl_object_runtime.c",
		"xl_channel_runtime.c",
		"xl_sys_lib_runtime.c",
		"xlrt_generated.c",
		"xl_url_runtime.c",
		"xl_http_runtime.c",
		"xl_httpd_runtime.c",
		"xl_mime_runtime.c",
		"xl_multipart_runtime.c",
		"xl_net_dns_runtime.c",
		"xl_net_ip_runtime.c",
		"xl_net_tcp_runtime.c",
		"xl_net_udp_runtime.c",
		"xl_net_tls_runtime.c",
		"xl_net_proxy_runtime.c",
		"xl_net_ws_runtime.c"
	};
	size_t i;

	__xlBuildPath3(sRuntimeDir, sizeof(sRuntimeDir), pCtx->sReleaseRoot, "tcc", "support");
	xlPathJoin(sRuntimeDir, sizeof(sRuntimeDir), sRuntimeDir, "runtime");
	__xlBuildPath3(sHostRuntimeImpl, sizeof(sHostRuntimeImpl), pCtx->sReleaseRoot, "tcc", "support");
	xlPathJoin(sHostRuntimeImpl, sizeof(sHostRuntimeImpl), sHostRuntimeImpl, "host");
	xlPathJoin(sHostRuntimeImpl, sizeof(sHostRuntimeImpl), sHostRuntimeImpl, "xl_host_runtime.c");

	for ( i = 0; i < XL_COUNTOF(arrRuntimeSources); ++i ) {
		snprintf(sRuntimeImpl, sizeof(sRuntimeImpl), "%s", sRuntimeDir);
		xlPathJoin(sRuntimeImpl, sizeof(sRuntimeImpl), sRuntimeImpl, arrRuntimeSources[i]);
		__xlRunTraceAppendFormat(pTrace, "support_source=%s\n", sRuntimeImpl);
		if ( tcc_add_file(pTcc, sRuntimeImpl) < 0 ) return XL_RC_FAIL;
	}
	__xlRunTraceAppendFormat(pTrace, "support_source=%s\n", sHostRuntimeImpl);
	if ( tcc_add_file(pTcc, sHostRuntimeImpl) < 0 ) return XL_RC_FAIL;

	return XL_RC_OK;
}

int xlHostTccRunText(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sVirtualPath,
	const char* sCode,
	const char* sEntrySymbol,
	char** psTraceText,
	XL_HostRunResult* pResult
)
{
	XL_RunTraceBuffer* pTrace;
#if XLANG_DEBUGMODE_ENABLED
	XL_RunTraceBuffer trace;
#endif
	XL_CwdScope cwdScope;
	XL_SharedArtifactHandle arrSharedHandles[XL_HOST_RUNTIME_LIBRARY_HANDLE_MAX];
	XL_StaticExtractedFile arrStaticExtracted[XL_HOST_STATIC_EXTRACT_MAX];
	size_t iStaticExtractedCount;
	size_t iSharedHandleCount;
	TCCState* pTcc;
	char* sWrapped;
	int (*pfnEntry)(void);
	const char* sAsyncTrace;
	int* piAsyncTraceLen;
	int iRc;

	if ( (pCtx == NULL) || (sCode == NULL) || (pResult == NULL) ) return XL_RC_BAD_ARG;

#if XLANG_DEBUGMODE_ENABLED
	memset(&trace, 0, sizeof(trace));
	pTrace = &trace;
#else
	pTrace = NULL;
#endif
	memset(&cwdScope, 0, sizeof(cwdScope));
	memset(arrSharedHandles, 0, sizeof(arrSharedHandles));
	memset(arrStaticExtracted, 0, sizeof(arrStaticExtracted));
	iStaticExtractedCount = 0;
	iSharedHandleCount = 0;
	__xlInitRunResult(pResult, (sEntrySymbol != NULL) ? sEntrySymbol : "xl_generated_entry");
	__xlSetTraceText(psTraceText, pTrace);
	sWrapped = NULL;

	pTcc = tcc_new();
	if ( pTcc == NULL ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to create TCC state");
		return XL_RC_FAIL;
	}

	if ( tcc_set_output_type(pTcc, TCC_OUTPUT_MEMORY) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to set TCC output type");
		tcc_delete(pTcc);
		return XL_RC_FAIL;
	}
	if ( __xlConfigureTcc(pCtx, pTcc, pTrace, 1) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to configure TCC");
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlAddBuildLibraries(pCtx, pDiag, pTcc, pTrace) != XL_RC_OK ) {
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	sWrapped = __xlWrapSourceText(sVirtualPath, sCode);
	if ( sWrapped == NULL ) {
		tcc_delete(pTcc);
		return XL_RC_IO_ERROR;
	}

	__xlRunTraceAppendFormat(pTrace, "compile_text=%s\n", (sVirtualPath != NULL) ? sVirtualPath : "<memory>");
	if ( tcc_compile_string(pTcc, sWrapped) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "tcc compile failed");
		free(sWrapped);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	free(sWrapped);

	if ( __xlApplyStaticArtifacts(
		pCtx,
		pTcc,
		pTrace,
		arrStaticExtracted,
		XL_COUNTOF(arrStaticExtracted),
		&iStaticExtractedCount
	) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to link static xlp artifacts");
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlPushProjectCwd(pCtx, pTrace, &cwdScope) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to switch cwd: %s", pCtx->sProjectRoot);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlLoadSharedArtifacts(pCtx, pDiag, pTrace, arrSharedHandles, XL_COUNTOF(arrSharedHandles), &iSharedHandleCount) != XL_RC_OK ) {
		__xlPopProjectCwd(pTrace, &cwdScope);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlRegisterSharedArtifactSymbols(pCtx, pDiag, pTcc, pTrace, arrSharedHandles, iSharedHandleCount) != XL_RC_OK ) {
		__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
		__xlPopProjectCwd(pTrace, &cwdScope);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	__xlRunTraceAppend(pTrace, "relocate\n");
	if ( tcc_relocate(pTcc) < 0 ) {
		__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
		__xlPopProjectCwd(pTrace, &cwdScope);
		XL_DIAG_ERROR(pDiag, "host.tcc", "tcc relocate failed");
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	__xlRunTraceAppendFormat(pTrace, "resolve_symbol=%s\n", pResult->sEntrySymbol);
	pfnEntry = (int (*)(void))tcc_get_symbol(pTcc, pResult->sEntrySymbol);
	if ( pfnEntry == NULL ) {
		__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
		__xlPopProjectCwd(pTrace, &cwdScope);
		XL_DIAG_ERROR(pDiag, "host.tcc", "entry symbol not found: %s", pResult->sEntrySymbol);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	__xlRunTraceAppend(pTrace, "execute\n");
	pResult->iExitCode = pfnEntry();
	__xlRunTraceAppendFormat(pTrace, "exit_code=%d\n", pResult->iExitCode);
	__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
	__xlPopProjectCwd(pTrace, &cwdScope);
	__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);

	sAsyncTrace = (const char*)tcc_get_symbol(pTcc, "xl_generated_async_trace");
	piAsyncTraceLen = (int*)tcc_get_symbol(pTcc, "xl_generated_async_trace_len");
	if ( (sAsyncTrace != NULL) && (piAsyncTraceLen != NULL) && (*piAsyncTraceLen > 0) ) {
		__xlRunTraceAppend(pTrace, sAsyncTrace);
	}

	__xlFillExceptionInfo(pTcc, pTrace, pResult);
	iRc = (pResult->bHadException != 0) ? XL_RC_FAIL : XL_RC_OK;
	if ( pResult->bHadException ) {
		pResult->bHadRuntimeError = 1;
		if ( pResult->iExceptionLine > 0 ) {
			xlDiagEmitSource(
				pDiag,
				XL_DIAG_LEVEL_ERROR,
				"host.tcc",
				__FILE__,
				__LINE__,
				(pResult->sExceptionFile[0] != '\0') ? pResult->sExceptionFile : NULL,
				pResult->iExceptionLine,
				pResult->iExceptionColumn,
				"uncaught exception: %s",
				pResult->sExceptionText
			);
		} else {
			XL_DIAG_ERROR(pDiag, "host.tcc", "uncaught exception: %s", pResult->sExceptionText);
		}
	}

	xrtNetSyncShutdownHiddenEngine();
	tcc_delete(pTcc);
	__xlSetTraceText(psTraceText, pTrace);
	return iRc;
}

int xlHostTccRunFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	char** psTraceText,
	XL_HostRunResult* pResult
)
{
	XL_RunTraceBuffer* pTrace;
#if XLANG_DEBUGMODE_ENABLED
	XL_RunTraceBuffer trace;
#endif
	XL_CwdScope cwdScope;
	XL_SharedArtifactHandle arrSharedHandles[XL_HOST_RUNTIME_LIBRARY_HANDLE_MAX];
	XL_StaticExtractedFile arrStaticExtracted[XL_HOST_STATIC_EXTRACT_MAX];
	size_t iStaticExtractedCount;
	size_t iSharedHandleCount;
	TCCState* pTcc;
	char* arrArgv[2];
	int iRunRc;

	if ( (pCtx == NULL) || (sSourceFile == NULL) || (pResult == NULL) ) return XL_RC_BAD_ARG;

#if XLANG_DEBUGMODE_ENABLED
	memset(&trace, 0, sizeof(trace));
	pTrace = &trace;
#else
	pTrace = NULL;
#endif
	memset(&cwdScope, 0, sizeof(cwdScope));
	memset(arrSharedHandles, 0, sizeof(arrSharedHandles));
	memset(arrStaticExtracted, 0, sizeof(arrStaticExtracted));
	iStaticExtractedCount = 0;
	iSharedHandleCount = 0;
	__xlInitRunResult(pResult, "main");
	__xlSetTraceText(psTraceText, pTrace);

	pTcc = tcc_new();
	if ( pTcc == NULL ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to create TCC state");
		return XL_RC_FAIL;
	}

	if ( tcc_set_output_type(pTcc, TCC_OUTPUT_MEMORY) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to set TCC output type");
		tcc_delete(pTcc);
		return XL_RC_FAIL;
	}
	if ( __xlConfigureTcc(pCtx, pTcc, pTrace, 1) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to configure TCC");
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlAddBuildLibraries(pCtx, pDiag, pTcc, pTrace) != XL_RC_OK ) {
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	__xlRunTraceAppendFormat(pTrace, "compile_file=%s\n", sSourceFile);
	if ( tcc_add_file(pTcc, sSourceFile) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to add C file: %s", sSourceFile);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlApplyStaticArtifacts(
		pCtx,
		pTcc,
		pTrace,
		arrStaticExtracted,
		XL_COUNTOF(arrStaticExtracted),
		&iStaticExtractedCount
	) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to link static xlp artifacts");
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	arrArgv[0] = (char*)sSourceFile;
	arrArgv[1] = NULL;
	if ( __xlPushProjectCwd(pCtx, pTrace, &cwdScope) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to switch cwd: %s", pCtx->sProjectRoot);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlLoadSharedArtifacts(pCtx, pDiag, pTrace, arrSharedHandles, XL_COUNTOF(arrSharedHandles), &iSharedHandleCount) != XL_RC_OK ) {
		__xlPopProjectCwd(pTrace, &cwdScope);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	if ( __xlRegisterSharedArtifactSymbols(pCtx, pDiag, pTcc, pTrace, arrSharedHandles, iSharedHandleCount) != XL_RC_OK ) {
		__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
		__xlPopProjectCwd(pTrace, &cwdScope);
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		tcc_delete(pTcc);
		pResult->bHadRuntimeError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	__xlRunTraceAppend(pTrace, "tcc_run\n");
	iRunRc = tcc_run(pTcc, 1, arrArgv);
	__xlUnloadSharedArtifacts(pTrace, arrSharedHandles, iSharedHandleCount);
	__xlPopProjectCwd(pTrace, &cwdScope);
	__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
	pResult->iExitCode = iRunRc;
	__xlRunTraceAppendFormat(pTrace, "exit_code=%d\n", pResult->iExitCode);
	if ( iRunRc < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to run C file: %s", sSourceFile);
		pResult->bHadRuntimeError = 1;
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	tcc_delete(pTcc);
	__xlSetTraceText(psTraceText, pTrace);
	return XL_RC_OK;
}

static int __xlHostTccBuildCommon(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	TCCState* pTcc,
	const char* sOutputFile,
	const char* sOutputKind,
	XL_RunTraceBuffer* pTrace,
	XL_HostBuildResult* pResult
)
{
	XL_StaticExtractedFile arrStaticExtracted[XL_HOST_STATIC_EXTRACT_MAX];
	size_t iStaticExtractedCount;
	int bObjOutput;

	memset(arrStaticExtracted, 0, sizeof(arrStaticExtracted));
	iStaticExtractedCount = 0;
	bObjOutput = (sOutputKind != NULL) && (strcmp(sOutputKind, "obj") == 0);
	__xlRunTraceAppendFormat(pTrace, "output=%s\n", sOutputFile);
	__xlRunTraceAppendFormat(pTrace, "output_kind=%s\n", sOutputKind);
	if ( !bObjOutput ) {
		if ( __xlAddBuildSupportSources(pCtx, pTcc, pTrace) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "host.tcc", "failed to add runtime support sources");
			pResult->bHadBuildError = 1;
			return XL_RC_FAIL;
		}
		if ( __xlAddBuildLibraries(pCtx, pDiag, pTcc, pTrace) != XL_RC_OK ) {
			pResult->bHadBuildError = 1;
			return XL_RC_FAIL;
		}
		if ( __xlApplyStaticArtifacts(
			pCtx,
			pTcc,
			pTrace,
			arrStaticExtracted,
			XL_COUNTOF(arrStaticExtracted),
			&iStaticExtractedCount
		) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "host.tcc", "failed to link static xlp artifacts");
			__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
			pResult->bHadBuildError = 1;
			return XL_RC_FAIL;
		}
		if ( __xlApplySharedArtifacts(pCtx, pTcc, pTrace) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "host.tcc", "failed to link shared xlp artifacts");
			__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
			pResult->bHadBuildError = 1;
			return XL_RC_FAIL;
		}
	}
	if ( xlPathEnsureParentDir(sOutputFile) != XL_RC_OK ) {
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to prepare output directory: %s", sOutputFile);
		pResult->bHadBuildError = 1;
		return XL_RC_IO_ERROR;
	}
	if ( tcc_output_file(pTcc, sOutputFile) < 0 ) {
		__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to write output file: %s", sOutputFile);
		pResult->bHadBuildError = 1;
		return XL_RC_FAIL;
	}
	__xlCleanupStaticExtractedFiles(arrStaticExtracted, iStaticExtractedCount);
	return XL_RC_OK;
}

int xlHostTccBuildText(
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
	XL_RunTraceBuffer* pTrace;
#if XLANG_DEBUGMODE_ENABLED
	XL_RunTraceBuffer trace;
#endif
	TCCState* pTcc;
	char* sWrapped;
	const char* sKindText;
	XL_HostOutputType eOutputType;
	int iRc;

	if ( (pCtx == NULL) || (sCode == NULL) || (sOutputFile == NULL) || (pResult == NULL) ) return XL_RC_BAD_ARG;

#if XLANG_DEBUGMODE_ENABLED
	memset(&trace, 0, sizeof(trace));
	pTrace = &trace;
#else
	pTrace = NULL;
#endif
	sKindText = "exe";
	eOutputType = __xlDetectOutputType(sOutputFile, &sKindText);
	__xlInitBuildResult(pResult, sOutputFile, sKindText, sEntrySymbol);
	__xlSetTraceText(psTraceText, pTrace);

	pTcc = tcc_new();
	if ( pTcc == NULL ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to create TCC state");
		return XL_RC_FAIL;
	}

	if ( tcc_set_output_type(pTcc, (int)eOutputType) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to set build output type");
		tcc_delete(pTcc);
		return XL_RC_FAIL;
	}
	if ( __xlConfigureTcc(pCtx, pTcc, pTrace, 0) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to configure TCC");
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	sWrapped = __xlWrapBuildText(sVirtualPath, sCode, sEntrySymbol, eOutputType == XL_HOST_OUTPUT_EXE);
	if ( sWrapped == NULL ) {
		tcc_delete(pTcc);
		return XL_RC_IO_ERROR;
	}

	__xlRunTraceAppendFormat(pTrace, "compile_text=%s\n", (sVirtualPath != NULL) ? sVirtualPath : "<memory>");
	if ( tcc_compile_string(pTcc, sWrapped) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "tcc compile failed");
		free(sWrapped);
		tcc_delete(pTcc);
		pResult->bHadBuildError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}
	free(sWrapped);

	iRc = __xlHostTccBuildCommon(pCtx, pDiag, pTcc, sOutputFile, sKindText, pTrace, pResult);
	tcc_delete(pTcc);
	__xlSetTraceText(psTraceText, pTrace);
	return iRc;
}

int xlHostTccBuildFile(
	const XL_HostContext* pCtx,
	XL_DiagContext* pDiag,
	const char* sSourceFile,
	const char* sOutputFile,
	char** psTraceText,
	XL_HostBuildResult* pResult
)
{
	XL_RunTraceBuffer* pTrace;
#if XLANG_DEBUGMODE_ENABLED
	XL_RunTraceBuffer trace;
#endif
	TCCState* pTcc;
	const char* sKindText;
	XL_HostOutputType eOutputType;
	int iRc;

	if ( (pCtx == NULL) || (sSourceFile == NULL) || (sOutputFile == NULL) || (pResult == NULL) ) return XL_RC_BAD_ARG;

#if XLANG_DEBUGMODE_ENABLED
	memset(&trace, 0, sizeof(trace));
	pTrace = &trace;
#else
	pTrace = NULL;
#endif
	sKindText = "exe";
	eOutputType = __xlDetectOutputType(sOutputFile, &sKindText);
	__xlInitBuildResult(pResult, sOutputFile, sKindText, "main");
	__xlSetTraceText(psTraceText, pTrace);

	pTcc = tcc_new();
	if ( pTcc == NULL ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to create TCC state");
		return XL_RC_FAIL;
	}

	if ( tcc_set_output_type(pTcc, (int)eOutputType) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to set build output type");
		tcc_delete(pTcc);
		return XL_RC_FAIL;
	}
	if ( __xlConfigureTcc(pCtx, pTcc, pTrace, 0) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to configure TCC");
		tcc_delete(pTcc);
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	__xlRunTraceAppendFormat(pTrace, "compile_file=%s\n", sSourceFile);
	if ( tcc_add_file(pTcc, sSourceFile) < 0 ) {
		XL_DIAG_ERROR(pDiag, "host.tcc", "failed to add C file: %s", sSourceFile);
		tcc_delete(pTcc);
		pResult->bHadBuildError = 1;
		__xlSetTraceText(psTraceText, pTrace);
		return XL_RC_FAIL;
	}

	iRc = __xlHostTccBuildCommon(pCtx, pDiag, pTcc, sOutputFile, sKindText, pTrace, pResult);
	tcc_delete(pTcc);
	__xlSetTraceText(psTraceText, pTrace);
	return iRc;
}
