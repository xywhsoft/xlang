#include <stdio.h>

#include "../src/ast/xl_ast_dump.h"
#include "../src/codegen/c/xl_cgen.h"
#include "../src/driver/xl_module_link.h"
#include "../src/driver/xl_xlp_dump.h"
#include "../src/driver/xl_xlp_load.h"
#include "../src/host/xl_host.h"
#include "../src/lexer/xl_lexer_api.h"
#include "../src/lower/xl_lower.h"
#include "../src/parser/xl_parser_api.h"
#include "../src/runtime/xl_exec_ctx.h"
#include "../src/sem/xl_resolve.h"
#include "../src/sem/xl_sem_dump.h"
#include "../src/sem/xl_type.h"
#include "../release/tmp/xl_test_build_stamp.h"
#include "xl_test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

typedef struct XL_TestContext {
	FILE* pOut;
	FILE* pSink;
	char sProjectRoot[XL_PATH_MAX_LEN];
	char sReleaseRoot[XL_PATH_MAX_LEN];
	int iPassCount;
	int iFailCount;
} XL_TestContext;

typedef int (*XL_TestFunc)(XL_TestContext* pCtx);

typedef struct XL_TestCase {
	const char* sName;
	const char* sGroup;
	XL_TestFunc fnRun;
} XL_TestCase;

#ifndef XL_TEST_BUILD_ID
#define XL_TEST_BUILD_ID __DATE__ " " __TIME__
#endif

static const char* __gXLTestBuildStamp = XL_TEST_BUILD_ID;

typedef struct XL_TraceCountExpectation {
	const char* sNeedle;
	size_t iExpectedCount;
} XL_TraceCountExpectation;

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

static void __xlBuildProjectPath(XL_TestContext* pCtx, char* sOut, size_t iOutSize, const char* sRelativePath)
{
	__xlCopyText(sOut, iOutSize, (pCtx != NULL) ? pCtx->sProjectRoot : "");
	__xlAppendPathPart(sOut, iOutSize, sRelativePath);
}

static void __xlInitSilentDiag(XL_TestContext* pCtx, XL_DiagContext* pDiag)
{
	xlDiagInit(pDiag, (pCtx != NULL) ? pCtx->pSink : NULL, (pCtx != NULL) ? pCtx->pSink : NULL);
}

static int __xlContainsAstKind(const XL_AstNode* pNode, XL_AstKind eKind)
{
	size_t i;

	if ( pNode == NULL ) {
		return 0;
	}
	if ( pNode->eKind == eKind ) {
		return 1;
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlContainsAstKind(pNode->ppChildren[i], eKind) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlTraceContains(const char* sTrace, const char* sNeedle)
{
	return (sTrace != NULL) && (sNeedle != NULL) && (strstr(sTrace, sNeedle) != NULL);
}

static size_t __xlTraceCount(const char* sTrace, const char* sNeedle)
{
	const char* sCur;
	size_t iCount;
	size_t iNeedleLen;

	if ( (sTrace == NULL) || (sNeedle == NULL) || (sNeedle[0] == '\0') ) {
		return 0u;
	}

	iCount = 0u;
	iNeedleLen = strlen(sNeedle);
	sCur = sTrace;
	while ( (sCur = strstr(sCur, sNeedle)) != NULL ) {
		iCount += 1u;
		sCur += iNeedleLen;
	}
	return iCount;
}

static int __xlTraceContainsOrdered(const char* sTrace, const char* const* ppNeedles, size_t iNeedleCount)
{
	const char* sCur;
	size_t i;

	if ( sTrace == NULL ) {
		return 0;
	}
	if ( (ppNeedles == NULL) || (iNeedleCount == 0u) ) {
		return 1;
	}

	sCur = sTrace;
	for ( i = 0u; i < iNeedleCount; ++i ) {
		if ( (ppNeedles[i] == NULL) || (ppNeedles[i][0] == '\0') ) {
			continue;
		}
		sCur = strstr(sCur, ppNeedles[i]);
		if ( sCur == NULL ) {
			return 0;
		}
		sCur += strlen(ppNeedles[i]);
	}
	return 1;
}

static int __xlTestEnvTruthy(const char* sName)
{
	const char* sValue;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}

	sValue = getenv(sName);
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return 0;
	}
	if ( strcmp(sValue, "0") == 0 ) {
		return 0;
	}
	if ( strcmp(sValue, "false") == 0 ) {
		return 0;
	}
	if ( strcmp(sValue, "FALSE") == 0 ) {
		return 0;
	}
	if ( strcmp(sValue, "off") == 0 ) {
		return 0;
	}
	if ( strcmp(sValue, "OFF") == 0 ) {
		return 0;
	}
	return 1;
}

static int __xlAddModuleArtifactsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		if ( pModule->bIsRoot ) {
			continue;
		}
		if ( (pModule->sConsumeMode == NULL) || (pModule->sArtifactFile == NULL) ) {
			continue;
		}
		if ( ((strcmp(pModule->sConsumeMode, "shared-bridge") != 0) &&
			(strcmp(pModule->sConsumeMode, "static-bridge") != 0)) ||
			(pModule->sArtifactFile[0] == '\0') ) {
			continue;
		}
		if ( xlExecCtxAddArtifact(pExecCtx, pModule->sArtifactKind, pModule->sArtifactFile) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlAddModuleArtifactsToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		if ( pModule->bIsRoot ) {
			continue;
		}
		if ( (pModule->sConsumeMode == NULL) || (pModule->sArtifactFile == NULL) ) {
			continue;
		}
		if ( ((strcmp(pModule->sConsumeMode, "shared-bridge") != 0) &&
			(strcmp(pModule->sConsumeMode, "static-bridge") != 0)) ||
			(pModule->sArtifactFile[0] == '\0') ) {
			continue;
		}
		if ( xlHostContextAddArtifact(pHostCtx, pModule->sArtifactKind, pModule->sArtifactFile) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlAddModuleExternSymbolsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;
		XL_XlpFunctionInfo* pFuncs;
		size_t iFuncCount;
		size_t j;

		pModule = &pModuleResult->pModules[i];
		if ( pModule->bIsRoot || !pModule->bIsNative || (pModule->sManifestPath == NULL) || (pModule->sManifestPath[0] == '\0') ||
			(pModule->sConsumeMode == NULL) || (strcmp(pModule->sConsumeMode, "shared-bridge") != 0) ) {
			continue;
		}
		pFuncs = NULL;
		iFuncCount = 0;
		if ( xlXlpReadFunctions(pModule->sManifestPath, pModule->sKey, &pFuncs, &iFuncCount) != XL_RC_OK ) {
			continue;
		}
		for ( j = 0; j < iFuncCount; ++j ) {
			if ( (pFuncs[j].sSymbol[0] != '\0') && (xlExecCtxAddExternSymbol(pExecCtx, pFuncs[j].sSymbol) != XL_RC_OK) ) {
				xlXlpFunctionInfoFree(pFuncs, iFuncCount);
				return XL_RC_FAIL;
			}
		}
		xlXlpFunctionInfoFree(pFuncs, iFuncCount);
	}

	return XL_RC_OK;
}

static int __xlAddModuleExternSymbolsToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;
		XL_XlpFunctionInfo* pFuncs;
		size_t iFuncCount;
		size_t j;

		pModule = &pModuleResult->pModules[i];
		if ( pModule->bIsRoot || !pModule->bIsNative || (pModule->sManifestPath == NULL) || (pModule->sManifestPath[0] == '\0') ||
			(pModule->sConsumeMode == NULL) || (strcmp(pModule->sConsumeMode, "shared-bridge") != 0) ) {
			continue;
		}
		pFuncs = NULL;
		iFuncCount = 0;
		if ( xlXlpReadFunctions(pModule->sManifestPath, pModule->sKey, &pFuncs, &iFuncCount) != XL_RC_OK ) {
			continue;
		}
		for ( j = 0; j < iFuncCount; ++j ) {
			if ( (pFuncs[j].sSymbol[0] != '\0') && (xlHostContextAddExternSymbol(pHostCtx, pFuncs[j].sSymbol) != XL_RC_OK) ) {
				xlXlpFunctionInfoFree(pFuncs, iFuncCount);
				return XL_RC_FAIL;
			}
		}
		xlXlpFunctionInfoFree(pFuncs, iFuncCount);
	}

	return XL_RC_OK;
}

static int __xlReadFileText(const char* sPath, char** psText)
{
	FILE* pFile;
	long long iSize;
	size_t iRead;
	char* sText;

	if ( (sPath == NULL) || (psText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*psText = NULL;
	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_FAIL;
	}
	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return XL_RC_FAIL;
	}
	iSize = _ftelli64(pFile);
	if ( iSize < 0 ) {
		fclose(pFile);
		return XL_RC_FAIL;
	}
	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return XL_RC_FAIL;
	}

	sText = (char*)malloc((size_t)iSize + 1);
	if ( sText == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iRead = fread(sText, 1, (size_t)iSize, pFile);
	fclose(pFile);
	if ( iRead != (size_t)iSize ) {
		free(sText);
		return XL_RC_FAIL;
	}

	sText[iRead] = '\0';
	*psText = sText;
	return XL_RC_OK;
}

static int __xlRunExe(const char* sExePath, int* piExitCode)
{
#if defined(_WIN32) || defined(_WIN64)
	STARTUPINFOA startupInfo;
	PROCESS_INFORMATION processInfo;
	char sCommand[XL_PATH_MAX_LEN * 2];
	DWORD iWaitRc;
	DWORD iExitCode;

	if ( (sExePath == NULL) || (piExitCode == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	memset(&startupInfo, 0, sizeof(startupInfo));
	memset(&processInfo, 0, sizeof(processInfo));
	startupInfo.cb = sizeof(startupInfo);
	snprintf(sCommand, sizeof(sCommand), "\"%s\"", sExePath);
	if ( !CreateProcessA(NULL, sCommand, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo) ) {
		return XL_RC_FAIL;
	}

	iWaitRc = WaitForSingleObject(processInfo.hProcess, 30000u);
	if ( iWaitRc != WAIT_OBJECT_0 ) {
		TerminateProcess(processInfo.hProcess, 1u);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return XL_RC_FAIL;
	}

	if ( !GetExitCodeProcess(processInfo.hProcess, &iExitCode) ) {
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return XL_RC_FAIL;
	}

	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	*piExitCode = (int)iExitCode;
	return XL_RC_OK;
#else
	XL_UNUSED(sExePath);
	XL_UNUSED(piExitCode);
	return XL_RC_NOT_IMPLEMENTED;
#endif
}

static int __xlRunLexCase(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	size_t iMinTokenCount,
	XL_TokenKind eFirstKind
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_Lexer* pLexer;
	XL_TokenList tokens;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	pLexer = xlLexerCreateFromFile(sPath);
	if ( pLexer == NULL ) {
		return XL_RC_FAIL;
	}

	xlTokenListInit(&tokens);
	iRc = xlLexerTokenizeAll(pLexer, &tokens);
	if ( (iRc == XL_RC_OK) && (tokens.iCount < iMinTokenCount) ) {
		iRc = XL_RC_FAIL;
	}
	if ( (iRc == XL_RC_OK) && (tokens.iCount != 0) && (tokens.pItems[0].eKind != eFirstKind) ) {
		iRc = XL_RC_FAIL;
	}

	xlTokenListFree(&tokens);
	xlLexerDestroy(pLexer);
	return iRc;
}

static int __xlRunParseCase(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iRequiredKind
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_Lexer* pLexer;
	XL_ParseResult result;
	XL_DiagContext diag;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	pLexer = xlLexerCreateFromFile(sPath);
	if ( pLexer == NULL ) {
		return XL_RC_FAIL;
	}

	__xlInitSilentDiag(pCtx, &diag);
	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, &diag, 0, &result);
	if ( bExpectOk ) {
		if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
			iRc = XL_RC_FAIL;
		} else if ( (iRequiredKind >= 0) && !__xlContainsAstKind(result.pRoot, (XL_AstKind)iRequiredKind) ) {
			iRc = XL_RC_FAIL;
		}
	} else {
		if ( (iRc == XL_RC_OK) || (result.iErrorCount == 0) ) {
			iRc = XL_RC_FAIL;
		} else {
			iRc = XL_RC_OK;
		}
	}

	xlParseResultFree(&result);
	xlLexerDestroy(pLexer);
	return iRc;
}

static int __xlRunParseDumpCase(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iRequiredKind,
	const char* sNeedle
)
{
	char sPath[XL_PATH_MAX_LEN];
	char* sDump;
	XL_Lexer* pLexer;
	XL_ParseResult result;
	XL_DiagContext diag;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	pLexer = xlLexerCreateFromFile(sPath);
	if ( pLexer == NULL ) {
		return XL_RC_FAIL;
	}

	sDump = NULL;
	__xlInitSilentDiag(pCtx, &diag);
	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, &diag, 0, &result);
	if ( bExpectOk ) {
		if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
			iRc = XL_RC_FAIL;
		} else if ( (iRequiredKind >= 0) && !__xlContainsAstKind(result.pRoot, (XL_AstKind)iRequiredKind) ) {
			iRc = XL_RC_FAIL;
		} else if ( sNeedle != NULL ) {
			sDump = xlAstDumpText(result.pRoot);
			if ( (sDump == NULL) || (strstr(sDump, sNeedle) == NULL) ) {
				iRc = XL_RC_FAIL;
			}
		}
	} else {
		if ( (iRc == XL_RC_OK) || (result.iErrorCount == 0) ) {
			iRc = XL_RC_FAIL;
		} else {
			iRc = XL_RC_OK;
		}
	}

	free(sDump);
	xlParseResultFree(&result);
	xlLexerDestroy(pLexer);
	return iRc;
}

static int __xlRunSemCase(XL_TestContext* pCtx, const char* sRelativePath, int bExpectOk, const char* sNeedType)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}

	if ( bExpectOk ) {
		if ( (iRc != XL_RC_OK) || (semResult.bHadError != 0) ) {
			iRc = XL_RC_FAIL;
		} else if ( (sNeedType != NULL) && (xlTypeTableFind(&semResult.types, sNeedType) == NULL) ) {
			iRc = XL_RC_FAIL;
		}
	} else {
		if ( (iRc == XL_RC_OK) || (semResult.bHadError == 0) ) {
			iRc = XL_RC_FAIL;
		} else {
			iRc = XL_RC_OK;
		}
	}

	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunLowerCase(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iNeedAstKind,
	int bNeedAsyncSm,
	int bNeedGeneratorSm
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}

	if ( bExpectOk ) {
		if ( (iRc != XL_RC_OK) || (lowerResult.pHirRoot == NULL) || (lowerResult.pMirRoot == NULL) ) {
			iRc = XL_RC_FAIL;
		} else if ( (iNeedAstKind >= 0) && !__xlContainsAstKind(moduleResult.pLinkedRoot, (XL_AstKind)iNeedAstKind) ) {
			iRc = XL_RC_FAIL;
		} else if ( bNeedAsyncSm && (lowerResult.asyncSm.iFuncCount == 0) ) {
			iRc = XL_RC_FAIL;
		} else if ( bNeedGeneratorSm && (lowerResult.generatorSm.iFuncCount == 0) ) {
			iRc = XL_RC_FAIL;
		}
	} else {
		iRc = (iRc == XL_RC_OK) ? XL_RC_FAIL : XL_RC_OK;
	}

	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunCgenCase(XL_TestContext* pCtx, const char* sRelativePath, const char* sNeedle)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sCode;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (sNeedle != NULL) && (strstr(sCode, sNeedle) == NULL) ) {
		iRc = XL_RC_FAIL;
	}

	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunCgenCaseAll(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	const char* const* ppNeedles,
	size_t iNeedleCount
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sCode;
	size_t i;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (ppNeedles != NULL) ) {
		for ( i = 0; i < iNeedleCount; ++i ) {
			if ( (ppNeedles[i] != NULL) && (strstr(sCode, ppNeedles[i]) == NULL) ) {
				fprintf(stderr, "[trace][cgen] missing substring: %s\n", ppNeedles[i]);
				iRc = XL_RC_FAIL;
				break;
			}
		}
	}

	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunProgramCase(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iExpectedExitCode,
	const char* sNeedle,
	const char* sExceptionNeedle
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_ExecCtx execCtx;
	XL_DiagContext diag;
	char* sCode;
	char* sTraceText;
	int iRc;
	int bPrintPassTrace;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;
	sTraceText = NULL;
	bPrintPassTrace = __xlTestEnvTruthy("XL_TEST_PRINT_PASS_TRACE");

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( iRc == XL_RC_OK ) {
		xlExecCtxInit(&execCtx, sPath, pCtx->sReleaseRoot);
		iRc = __xlAddModuleArtifactsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = __xlAddModuleExternSymbolsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlExecRunCode(&execCtx, &diag, sCode, &sTraceText);
		if ( bExpectOk ) {
			if ( (iRc != XL_RC_OK) || (execCtx.iExitCode != iExpectedExitCode) ) {
				fprintf(
					stderr,
					"[trace][run] expected success path=%s rc=%d exit=%d expected=%d\n",
					sRelativePath,
					iRc,
					execCtx.iExitCode,
					iExpectedExitCode
				);
				iRc = XL_RC_FAIL;
			} else if ( (sNeedle != NULL) && !__xlTraceContains(sTraceText, sNeedle) ) {
				fprintf(
					stderr,
					"[trace][run] missing success needle path=%s needle=%s\n",
					sRelativePath,
					sNeedle
				);
				iRc = XL_RC_FAIL;
			} else if ( bPrintPassTrace && (sTraceText != NULL) ) {
				fprintf(stderr, "[trace][run][pass] path=%s\n%s\n", sRelativePath, sTraceText);
			}
		} else {
			if ( iRc == XL_RC_OK ) {
				fprintf(
					stderr,
					"[trace][run] expected failure path=%s but rc=%d exit=%d\n",
					sRelativePath,
					iRc,
					execCtx.iExitCode
				);
				iRc = XL_RC_FAIL;
			} else if ( (sExceptionNeedle != NULL) && !__xlTraceContains(sTraceText, sExceptionNeedle) ) {
				fprintf(
					stderr,
					"[trace][run] missing exception needle path=%s needle=%s\n",
					sRelativePath,
					sExceptionNeedle
				);
				if ( sTraceText != NULL ) {
					fprintf(stderr, "%s\n", sTraceText);
				}
				iRc = XL_RC_FAIL;
			} else {
				iRc = XL_RC_OK;
			}
		}
	}

	free(sTraceText);
	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunProgramCaseAll(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iExpectedExitCode,
	const char* const* ppNeedles,
	size_t iNeedleCount,
	const char* sExceptionNeedle
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_ExecCtx execCtx;
	XL_DiagContext diag;
	char* sCode;
	char* sTraceText;
	size_t i;
	int iRc;
	int bPrintPassTrace;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;
	sTraceText = NULL;
	bPrintPassTrace = __xlTestEnvTruthy("XL_TEST_PRINT_PASS_TRACE");

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( iRc == XL_RC_OK ) {
		xlExecCtxInit(&execCtx, sPath, pCtx->sReleaseRoot);
		iRc = __xlAddModuleArtifactsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = __xlAddModuleExternSymbolsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlExecRunCode(&execCtx, &diag, sCode, &sTraceText);
		if ( bExpectOk ) {
			if ( (iRc != XL_RC_OK) || (execCtx.iExitCode != iExpectedExitCode) ) {
				fprintf(
					stderr,
					"[trace][run] expected success path=%s rc=%d exit=%d expected=%d\n",
					sRelativePath,
					iRc,
					execCtx.iExitCode,
					iExpectedExitCode
				);
				if ( sTraceText != NULL ) {
					fprintf(stderr, "%s\n", sTraceText);
				}
				iRc = XL_RC_FAIL;
			} else if ( ppNeedles != NULL ) {
				for ( i = 0; i < iNeedleCount; ++i ) {
					if ( (ppNeedles[i] != NULL) && !__xlTraceContains(sTraceText, ppNeedles[i]) ) {
						fprintf(
							stderr,
							"[trace][run] missing success needle path=%s needle=%s\n",
							sRelativePath,
							ppNeedles[i]
						);
						if ( sTraceText != NULL ) {
							fprintf(stderr, "%s\n", sTraceText);
						}
						iRc = XL_RC_FAIL;
						break;
					}
				}
				if ( (iRc == XL_RC_OK) && bPrintPassTrace && (sTraceText != NULL) ) {
					fprintf(stderr, "[trace][run][pass] path=%s\n%s\n", sRelativePath, sTraceText);
				}
			}
		} else {
			if ( iRc == XL_RC_OK ) {
				iRc = XL_RC_FAIL;
			} else if ( (sExceptionNeedle != NULL) && !__xlTraceContains(sTraceText, sExceptionNeedle) ) {
				iRc = XL_RC_FAIL;
			} else {
				iRc = XL_RC_OK;
			}
		}
	}

	free(sTraceText);
	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlTestHostRunC(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	XL_DiagContext diag;
	char* sTraceText;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\c\\run_host_bridge.c");
	__xlInitSilentDiag(pCtx, &diag);
	xlHostContextInit(&hostCtx, sPath, pCtx->sReleaseRoot);
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
	iRc = xlHostRunCFile(&hostCtx, &diag, sPath, &sTraceText, &result);
	if ( (iRc == XL_RC_OK) && (result.iExitCode != 7) ) {
		iRc = XL_RC_FAIL;
	}
	free(sTraceText);
	return iRc;
}

static int __xlTestHostBuildC(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	char sOutPath[XL_PATH_MAX_LEN];
	XL_HostContext hostCtx;
	XL_HostBuildResult result;
	XL_DiagContext diag;
	char* sTraceText;
	int iExitCode;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\c\\run_host_bridge.c");
	__xlBuildProjectPath(pCtx, sOutPath, sizeof(sOutPath), "release\\bin\\run_host_bridge_test.exe");
	remove(sOutPath);
	__xlInitSilentDiag(pCtx, &diag);
	xlHostContextInit(&hostCtx, sPath, pCtx->sReleaseRoot);
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
	iRc = xlHostBuildCFile(&hostCtx, &diag, sPath, sOutPath, &sTraceText, &result);
	if ( iRc != XL_RC_OK ) {
		free(sTraceText);
		return iRc;
	}
	iExitCode = 0;
	iRc = __xlRunExe(sOutPath, &iExitCode);
	if ( (iRc == XL_RC_OK) && (iExitCode != 7) ) {
		iRc = XL_RC_FAIL;
	}
	free(sTraceText);
	return iRc;
}

static int __xlTestHostBuildXlang(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	char sOutPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_HostContext hostCtx;
	XL_HostBuildResult result;
	XL_DiagContext diag;
	char* sCode;
	char* sTraceText;
	int iExitCode;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\full\\run_basic.xl");
	__xlBuildProjectPath(pCtx, sOutPath, sizeof(sOutPath), "release\\bin\\run_basic_test.exe");
	remove(sOutPath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlHostContextInit(&hostCtx, sPath, pCtx->sReleaseRoot);
	memset(&result, 0, sizeof(result));
	sCode = NULL;
	sTraceText = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	if ( iRc == XL_RC_OK ) iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	if ( iRc == XL_RC_OK ) iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	if ( iRc == XL_RC_OK ) iRc = __xlAddModuleArtifactsToHostCtx(&hostCtx, &moduleResult);
	if ( iRc == XL_RC_OK ) iRc = __xlAddModuleExternSymbolsToHostCtx(&hostCtx, &moduleResult);
	if ( iRc == XL_RC_OK ) iRc = xlHostBuildCText(&hostCtx, &diag, sPath, sCode, "xl_generated_entry", sOutPath, &sTraceText, &result);
	if ( iRc == XL_RC_OK ) {
		iExitCode = 0;
		iRc = __xlRunExe(sOutPath, &iExitCode);
		if ( (iRc == XL_RC_OK) && (iExitCode != 42) ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sCode);
	free(sTraceText);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlTestLexMinimal(XL_TestContext* pCtx) { return __xlRunLexCase(pCtx, "examples\\lexer\\minimal.xl", 4, XL_KW_FUNC); }
static int __xlTestLexKeywords(XL_TestContext* pCtx) { return __xlRunLexCase(pCtx, "examples\\lexer\\keywords.xl", 16, XL_KW_FUNC); }
static int __xlTestParseForIn(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\for_in_range.xl", 1, XL_AST_FORIN); }
static int __xlTestParseTry(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, XL_AST_TRY); }
static int __xlTestParseNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\named_args_default.xl", 1, XL_AST_NAMED_ARG); }
static int __xlTestParseAssertMessage(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\assert_message_basic.xl", 1, XL_AST_ASSERT); }
static int __xlTestParseIfInlineBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\if_inline_basic.xl", 1, XL_AST_IF, "Assign"); }
static int __xlTestParseIfInlineElseBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\if_inline_else_basic.xl", 1, XL_AST_IF, "text=\"42\" int=42"); }
static int __xlTestParseImportAlias(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\native\\import_alias_basic.xl", 1, XL_AST_IMPORT, "text2=\"s\""); }
static int __xlTestParseImportMulti(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\native\\import_multi_basic.xl", 1, XL_AST_IMPORT); }
static int __xlTestParseImportString(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\native\\import_string_basic.xl", 1, XL_AST_IMPORT, "text=\"\\\"std/str\\\"\""); }
static int __xlTestParseExternAtoiBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\native\\extern_atoi_basic.xl", 1, XL_AST_FUNC_DECL, "text2=\"atoi\""); }
static int __xlTestParseExportFunc(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\math_export_func.xl", 1, XL_AST_EXPORT, "FuncDecl"); }
static int __xlTestParseExportTypeMethod(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\text_ext_method.xl", 1, XL_AST_EXPORT, "text=\"wrap\""); }
static int __xlTestParseExportGenericTypeMethod(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\list_string_ext_method.xl", 1, XL_AST_EXPORT, "text2=\"wrap_join\""); }
static int __xlTestParseExportValues(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\math_export_values.xl", 1, XL_AST_EXPORT, "VarDecl"); }
static int __xlTestParseTypedDecl(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\typed_decl_basic.xl", 1, XL_AST_VAR_DECL, "text=\"0\""); }
static int __xlTestParseExactNumericTypes(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\exact_numeric_types_basic.xl", 1, XL_AST_TYPE_REF, "text=\"int32\""); }
static int __xlTestParseAutoDecl(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\auto_decl_basic.xl", 1, XL_AST_VAR_DECL); }
static int __xlTestParseTypeDefBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\typedef_basic.xl", 1, XL_AST_TYPEDEF, "text=\"Text\""); }
static int __xlTestParsePointBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\point_basic.xl", 1, XL_AST_TYPE_REF, "text=\"point\""); }
static int __xlTestParseAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\async\\run_bool_main.xl", 1, XL_AST_TYPE_REF, "text=\"bool\""); }
static int __xlTestParseFutureValueBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\async\\future_value_basic.xl", 1, XL_AST_TYPE_REF, "text=\"future\""); }
static int __xlTestParseChannelBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\channel_basic.xl", 1, XL_AST_UNPACK_DECL); }
static int __xlTestParseChannelRecvUnpackBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\channel_recv_unpack_basic.xl", 1, XL_AST_UNPACK_DECL); }
static int __xlTestParseAsyncGeneratorWhileBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_while_basic.xl", 1, XL_AST_WHILE); }
static int __xlTestParseAsyncGeneratorLoopControlBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, XL_AST_WHILE); }
static int __xlTestParseAsyncGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_resume_basic.xl", 1, XL_AST_YIELD); }
static int __xlTestParseAsyncGeneratorMultiAwaitGroupBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_multi_await_group_basic.xl", 1, XL_AST_YIELD); }
static int __xlTestParseAsyncGeneratorTryCatchBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_try_catch_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseAsyncGeneratorTryCatchConstElseBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_try_catch_const_else_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseAsyncGeneratorTryFinallyBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_try_finally_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseAsyncGeneratorTryFinallyConstElseBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_try_finally_const_else_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseAsyncGeneratorNestedTryFlowBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_nested_try_flow_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseAsyncGeneratorNestedTryFlowConstElseBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\async\\async_generator_nested_try_flow_const_else_basic.xl", 1, XL_AST_TRY); }
static int __xlTestParseTypeValueBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\type_value_basic.xl", 1, XL_AST_VAR_DECL, "text=\"a\""); }
static int __xlTestParsePublicTypesBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\public_types_basic.xl", 1, XL_AST_IDENT, "text=\"future\""); }
static int __xlTestParseTaskValueBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\task\\task_value_basic.xl", 1, XL_AST_TYPE_REF, "text=\"task\""); }
static int __xlTestParseAssignBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\assign_basic.xl", 1, XL_AST_ASSIGN); }
static int __xlTestParseAssignPlusBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_plus_basic.xl", 1, XL_AST_ASSIGN, "text=\"+=\""); }
static int __xlTestParseAssignMinusBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_minus_basic.xl", 1, XL_AST_ASSIGN, "text=\"-=\""); }
static int __xlTestParseAssignMulBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_mul_basic.xl", 1, XL_AST_ASSIGN, "text=\"*=\""); }
static int __xlTestParseAssignDivBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_div_basic.xl", 1, XL_AST_ASSIGN, "text=\"/=\""); }
static int __xlTestParseAssignModBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_mod_basic.xl", 1, XL_AST_ASSIGN, "text=\"%=\""); }
static int __xlTestParsePowerBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\power_basic.xl", 1, XL_AST_BINARY, "text=\"**\""); }
static int __xlTestParseAssignPowBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\assign_pow_basic.xl", 1, XL_AST_ASSIGN, "text=\"**=\""); }
static int __xlTestParseCoalesceBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\coalesce_basic.xl", 1, XL_AST_BINARY, "text=\"??\""); }
static int __xlTestParseNullAssignBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\null_assign_basic.xl", 1, XL_AST_ASSIGN, "text=\"?=\""); }
static int __xlTestParseTernaryBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\ternary_basic.xl", 1, XL_AST_TERNARY, "text=\"?:\""); }
static int __xlTestParseApproxBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\approx_basic.xl", 1, XL_AST_BINARY, "text=\"~=\""); }
static int __xlTestParseLogicBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\logic_basic.xl", 1, XL_AST_BINARY, "text=\"or\""); }
static int __xlTestParseBytesAliasString(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\bytes_alias_string.xl", 1, XL_AST_TYPE_REF, "text=\"string\""); }
static int __xlTestParseTemplateBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\template_basic.xl", 1, XL_AST_TEMPLATE, "text=\"\\\"he\\\\\\\"llo\\\"\""); }
static int __xlTestParseSafeMemberBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\safe_member_basic.xl", 1, XL_AST_MEMBER, "text=\"message\" flag=1"); }
static int __xlTestParseLeadingBlankLines(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\leading_blank_lines_basic.xl", 1, XL_AST_IMPORT); }
static int __xlTestParseVarBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\var_basic.xl", 1, XL_AST_VAR_DECL, "text=\"value\""); }
static int __xlTestParseArrayBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\array_basic.xl", 1, XL_AST_CALL, "text=\"__array_literal\""); }
static int __xlTestParseListBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\list_basic.xl", 1, XL_AST_CALL, "text=\"__list_literal\""); }
static int __xlTestParseSetBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\set_basic.xl", 1, XL_AST_CALL, "text=\"__set_literal\""); }
static int __xlTestParseDictBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\dict_basic.xl", 1, XL_AST_CALL, "text=\"__dict_literal\""); }
static int __xlTestParseBareContainerLiterals(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\bare_container_literals_basic.xl", 1, XL_AST_CALL, "text=\"__dict_literal\""); }
static int __xlTestParseVarArgArrayBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\vararg_array_basic.xl", 1, XL_AST_PARAM, "text=\"arg\" flag=1"); }
static int __xlTestParseVarArgDictBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\vararg_dict_basic.xl", 1, XL_AST_PARAM, "text=\"arg\" flag=2"); }
static int __xlTestParseMultiReturnBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\multi_return_basic.xl", 1, XL_AST_UNPACK_DECL, "text=\"auto\""); }
static int __xlTestParseMultiReturnCallExpandBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\multi_return_call_expand_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseFunctionValueBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\function_value_basic.xl", 1, XL_AST_TYPE_REF, "text=\"function<int(int,int)>\""); }
static int __xlTestParseFunctionValueFlowBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\function_value_flow_basic.xl", 1, XL_AST_FUNC_DECL); }
static int __xlTestParseFunctionValueContainerBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\function_value_container_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseFunctionValueParenCallBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\function_value_paren_call_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseIndexAssignBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\index_assign_basic.xl", 1, XL_AST_ASSIGN, "Index"); }
static int __xlTestParseForInContainers(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\for_in_containers_basic.xl", 1, XL_AST_FORIN); }
static int __xlTestParseContainerMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_methods_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerAppendBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_append_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerClearBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_clear_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerRemoveBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_remove_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseCoreLenBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\native\\core_len_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, XL_AST_FUNC_DECL, "text2=\"wrap\""); }
static int __xlTestParseVarMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\var_methods_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseVarTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\var_type_methods_basic.xl", 1, XL_AST_FUNC_DECL, "text2=\"wrap\""); }
static int __xlTestParseVarContainerTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\var_container_type_methods_basic.xl", 1, XL_AST_FUNC_DECL, "text2=\"wrap_join\""); }
static int __xlTestParsePropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\property_length_basic.xl", 1, XL_AST_MEMBER); }
static int __xlTestParseStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseStringSplitBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\string_split_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseWhileBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\while_basic.xl", 1, XL_AST_WHILE); }
static int __xlTestParseDoWhileBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\do_while_basic.xl", 1, XL_AST_WHILE, "flag=1"); }
static int __xlTestParseSyntaxError(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\parser\\syntax_error.xl", 0, -1); }
static int __xlTestParseGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\generator_resume_basic.xl", 1, XL_AST_YIELD); }
static int __xlTestSemOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\overload.xl", 1, "int"); }
static int __xlTestSemUnresolved(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\unresolved.xl", 0, NULL); }
static int __xlTestSemNoOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\no_overload.xl", 0, NULL); }
static int __xlTestSemForInGenerator(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\for_in_generator.xl", 1, "generator"); }
static int __xlTestSemGeneratorVarForInBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_var_for_in_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorParamResumeBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_param_resume_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorReturnWrapperBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_return_wrapper_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorReturnAbstractBranchResumeBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_return_abstract_branch_resume_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorReturnAbstractBranchForInBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_return_abstract_branch_for_in_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorStateCapture(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_state_capture_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_while_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorLoopControlBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_loop_control_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorTryCatchBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_try_catch_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorTryFinallyBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_try_finally_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorNestedTryFlowBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_nested_try_flow_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorParamUnknownError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\generator_param_unknown_error.xl", 0, NULL); }
static int __xlTestSemTry(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, "error"); }
static int __xlTestSemTryFinallyEscapeBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\try_finally_escape_basic.xl", 1, "int"); }
static int __xlTestSemErrorFields(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\error_fields_basic.xl", 1, "error"); }
static int __xlTestSemNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\named_args_default.xl", 1, "int"); }
static int __xlTestSemAwaitOutsideAsync(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\await_outside_async.xl", 0, NULL); }
static int __xlTestSemExternNestedError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\extern_nested_error.xl", 0, NULL); }
static int __xlTestSemTryNestedRethrow(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\try_nested_rethrow.xl", 1, "error"); }
static int __xlTestSemAssertMessage(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assert_message_basic.xl", 1, "error"); }
static int __xlTestSemIfInlineBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\if_inline_basic.xl", 1, "int"); }
static int __xlTestSemIfInlineElseBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\if_inline_else_basic.xl", 1, "int"); }
static int __xlTestSemNativeErrorBridge(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\native_error_bridge.xl", 1, "error"); }
static int __xlTestSemImportAlias(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_alias_basic.xl", 1, "int"); }
static int __xlTestSemImportMulti(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_multi_basic.xl", 1, "int"); }
static int __xlTestSemImportString(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_string_basic.xl", 1, "int"); }
static int __xlTestSemExternAtoiBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\extern_atoi_basic.xl", 1, "int"); }
static int __xlTestSemExportFunc(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_math_export_func.xl", 1, "int"); }
static int __xlTestSemExportTypeMethod(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_text_ext_method.xl", 1, "int"); }
static int __xlTestSemExportGenericTypeMethod(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_list_string_ext_method.xl", 1, "int"); }
static int __xlTestSemExportValues(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_math_export_values.xl", 1, "int"); }
static int __xlTestSemTypedDecl(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\typed_decl_basic.xl", 1, "int"); }
static int __xlTestSemExactNumericTypes(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\exact_numeric_types_basic.xl", 1, "int"); }
static int __xlTestSemAutoDecl(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\auto_decl_basic.xl", 1, "int"); }
static int __xlTestSemTypeDefBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\typedef_basic.xl", 1, "Text"); }
static int __xlTestSemTypeCtorBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\type_ctor_basic.xl", 1, "float64"); }
static int __xlTestSemPointBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\point_basic.xl", 1, "point"); }
static int __xlTestSemAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\run_bool_main.xl", 1, "bool"); }
static int __xlTestSemFutureValueBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\future_value_basic.xl", 1, "future"); }
static int __xlTestSemChannelBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\channel_basic.xl", 1, "channel"); }
static int __xlTestSemChannelRecvUnpackBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\channel_recv_unpack_basic.xl", 1, "channel"); }
static int __xlTestSemAsyncGeneratorWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_while_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorLoopControlBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_resume_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorMultiAwaitGroupBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_multi_await_group_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorTryCatchBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_try_catch_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorTryCatchConstElseBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_try_catch_const_else_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorTryFinallyBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_try_finally_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorTryFinallyConstElseBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_try_finally_const_else_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorNestedTryFlowBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_nested_try_flow_basic.xl", 1, "generator"); }
static int __xlTestSemAsyncGeneratorNestedTryFlowConstElseBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\async_generator_nested_try_flow_const_else_basic.xl", 1, "generator"); }
static int __xlTestSemTypeValueBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\type_value_basic.xl", 1, "type"); }
static int __xlTestSemPublicTypesBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\public_types_basic.xl", 1, "type"); }
static int __xlTestSemTaskValueBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\task\\task_value_basic.xl", 1, "task"); }
static int __xlTestSemTaskError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\task\\task_error_basic.xl", 1, NULL); }
static int __xlTestSemAwaitableProtocols(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\task\\task_value_basic.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( (iRc == XL_RC_OK) &&
		(!xlProtocolTableHas(&semResult.protocols, "future", XL_PROTOCOL_AWAITABLE) ||
		 !xlProtocolTableHas(&semResult.protocols, "task", XL_PROTOCOL_AWAITABLE)) ) {
		iRc = XL_RC_FAIL;
	}

	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunCgenCaseAllWithout(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	const char* const* ppNeedles,
	size_t iNeedleCount,
	const char* const* ppForbiddenNeedles,
	size_t iForbiddenNeedleCount
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sCode;
	size_t i;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (ppNeedles != NULL) ) {
		for ( i = 0; i < iNeedleCount; ++i ) {
			if ( (ppNeedles[i] != NULL) && (strstr(sCode, ppNeedles[i]) == NULL) ) {
				fprintf(stderr, "[trace][cgen] missing substring path=%s needle=%s\n", sRelativePath, ppNeedles[i]);
				iRc = XL_RC_FAIL;
				break;
			}
		}
	}
	if ( (iRc == XL_RC_OK) && (ppForbiddenNeedles != NULL) ) {
		for ( i = 0; i < iForbiddenNeedleCount; ++i ) {
			if ( (ppForbiddenNeedles[i] != NULL) && (strstr(sCode, ppForbiddenNeedles[i]) != NULL) ) {
				fprintf(stderr, "[trace][cgen] forbidden substring path=%s needle=%s\n", sRelativePath, ppForbiddenNeedles[i]);
				iRc = XL_RC_FAIL;
				break;
			}
		}
	}

	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunProgramCaseAllCounts(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	int bExpectOk,
	int iExpectedExitCode,
	const char* const* ppNeedles,
	size_t iNeedleCount,
	const XL_TraceCountExpectation* pCountExpectations,
	size_t iCountExpectationCount,
	const char* const* ppOrderedNeedles,
	size_t iOrderedNeedleCount,
	const char* sExceptionNeedle
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_ExecCtx execCtx;
	XL_DiagContext diag;
	char* sCode;
	char* sTraceText;
	size_t i;
	int iRc;
	int bPrintPassTrace;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;
	sTraceText = NULL;
	bPrintPassTrace = __xlTestEnvTruthy("XL_TEST_PRINT_PASS_TRACE");

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( iRc == XL_RC_OK ) {
		xlExecCtxInit(&execCtx, sPath, pCtx->sReleaseRoot);
		iRc = __xlAddModuleArtifactsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = __xlAddModuleExternSymbolsToExecCtx(&execCtx, &moduleResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlExecRunCode(&execCtx, &diag, sCode, &sTraceText);
		if ( bExpectOk ) {
			if ( (iRc != XL_RC_OK) || (execCtx.iExitCode != iExpectedExitCode) ) {
				fprintf(
					stderr,
					"[trace][run] expected success path=%s rc=%d exit=%d expected=%d\n",
					sRelativePath,
					iRc,
					execCtx.iExitCode,
					iExpectedExitCode
				);
				iRc = XL_RC_FAIL;
			} else if ( ppNeedles != NULL ) {
				for ( i = 0; i < iNeedleCount; ++i ) {
					if ( (ppNeedles[i] != NULL) && !__xlTraceContains(sTraceText, ppNeedles[i]) ) {
						fprintf(
							stderr,
							"[trace][run] missing success needle path=%s needle=%s\n",
							sRelativePath,
							ppNeedles[i]
						);
						if ( sTraceText != NULL ) {
							fprintf(stderr, "%s\n", sTraceText);
						}
						iRc = XL_RC_FAIL;
						break;
					}
				}
			}
			if ( (iRc == XL_RC_OK) && (pCountExpectations != NULL) ) {
				for ( i = 0; i < iCountExpectationCount; ++i ) {
					size_t iActualCount;

					if ( (pCountExpectations[i].sNeedle == NULL) || (pCountExpectations[i].sNeedle[0] == '\0') ) {
						continue;
					}
					iActualCount = __xlTraceCount(sTraceText, pCountExpectations[i].sNeedle);
					if ( iActualCount != pCountExpectations[i].iExpectedCount ) {
						fprintf(
							stderr,
							"[trace][run] unexpected needle count path=%s needle=%s actual=%llu expected=%llu\n",
							sRelativePath,
							pCountExpectations[i].sNeedle,
							(unsigned long long)iActualCount,
							(unsigned long long)pCountExpectations[i].iExpectedCount
						);
						if ( sTraceText != NULL ) {
							fprintf(stderr, "%s\n", sTraceText);
						}
						iRc = XL_RC_FAIL;
						break;
					}
				}
			}
			if ( (iRc == XL_RC_OK) && (ppOrderedNeedles != NULL) &&
				!__xlTraceContainsOrdered(sTraceText, ppOrderedNeedles, iOrderedNeedleCount) ) {
				fprintf(stderr, "[trace][run] unexpected needle order path=%s\n", sRelativePath);
				if ( sTraceText != NULL ) {
					fprintf(stderr, "%s\n", sTraceText);
				}
				iRc = XL_RC_FAIL;
			}
			if ( (iRc == XL_RC_OK) && bPrintPassTrace && (sTraceText != NULL) ) {
				fprintf(stderr, "[trace][run][pass] path=%s\n%s\n", sRelativePath, sTraceText);
			}
		} else {
			if ( iRc == XL_RC_OK ) {
				iRc = XL_RC_FAIL;
			} else if ( (sExceptionNeedle != NULL) && !__xlTraceContains(sTraceText, sExceptionNeedle) ) {
				iRc = XL_RC_FAIL;
			} else {
				iRc = XL_RC_OK;
			}
		}
	}

	free(sTraceText);
	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunCgenCaseCount(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	const char* sNeedle,
	size_t iExpectedCount
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sCode;
	size_t iActualCount;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (sNeedle != NULL) ) {
		iActualCount = __xlTraceCount(sCode, sNeedle);
		if ( iActualCount != iExpectedCount ) {
			fprintf(
				stderr,
				"[trace][cgen] unexpected substring count needle=%s actual=%llu expected=%llu\n",
				sNeedle,
				(unsigned long long)iActualCount,
				(unsigned long long)iExpectedCount
			);
			iRc = XL_RC_FAIL;
		}
	}

	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunCgenCaseAllCounts(
	XL_TestContext* pCtx,
	const char* sRelativePath,
	const char* const* ppNeedles,
	size_t iNeedleCount,
	const XL_TraceCountExpectation* pCountExpectations,
	size_t iCountExpectationCount,
	const char* const* ppOrderedNeedles,
	size_t iOrderedNeedleCount
)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sCode;
	size_t i;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(&lowerResult, &moduleResult, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (ppNeedles != NULL) ) {
		for ( i = 0; i < iNeedleCount; ++i ) {
			if ( (ppNeedles[i] != NULL) && !__xlTraceContains(sCode, ppNeedles[i]) ) {
				fprintf(stderr, "[trace][cgen] missing substring path=%s needle=%s\n", sRelativePath, ppNeedles[i]);
				iRc = XL_RC_FAIL;
				break;
			}
		}
	}
	if ( (iRc == XL_RC_OK) && (pCountExpectations != NULL) ) {
		for ( i = 0; i < iCountExpectationCount; ++i ) {
			size_t iActualCount;

			if ( (pCountExpectations[i].sNeedle == NULL) || (pCountExpectations[i].sNeedle[0] == '\0') ) {
				continue;
			}
			iActualCount = __xlTraceCount(sCode, pCountExpectations[i].sNeedle);
			if ( iActualCount != pCountExpectations[i].iExpectedCount ) {
				fprintf(
					stderr,
					"[trace][cgen] unexpected substring count path=%s needle=%s actual=%llu expected=%llu\n",
					sRelativePath,
					pCountExpectations[i].sNeedle,
					(unsigned long long)iActualCount,
					(unsigned long long)pCountExpectations[i].iExpectedCount
				);
				iRc = XL_RC_FAIL;
				break;
			}
		}
	}
	if ( (iRc == XL_RC_OK) && (ppOrderedNeedles != NULL) &&
		!__xlTraceContainsOrdered(sCode, ppOrderedNeedles, iOrderedNeedleCount) ) {
		fprintf(stderr, "[trace][cgen] unexpected substring order path=%s\n", sRelativePath);
		iRc = XL_RC_FAIL;
	}

	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunLowerDumpCase(XL_TestContext* pCtx, const char* sRelativePath, int bExpectOk, const char* sNeedle)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sDump;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sDump = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		sDump = xlLowerDumpText(&lowerResult);
		if ( (sNeedle != NULL) && ((sDump == NULL) || (strstr(sDump, sNeedle) == NULL)) ) {
			iRc = XL_RC_FAIL;
		}
	}

	if ( !bExpectOk ) {
		iRc = (iRc == XL_RC_OK) ? XL_RC_FAIL : XL_RC_OK;
	}

	free(sDump);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlRunLowerJsonCase(XL_TestContext* pCtx, const char* sRelativePath, int bExpectOk, const char* sNeedle)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_DiagContext diag;
	char* sDump;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sDump = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		sDump = xlLowerDumpJson(&lowerResult);
		if ( (sNeedle != NULL) && ((sDump == NULL) || (strstr(sDump, sNeedle) == NULL)) ) {
			iRc = XL_RC_FAIL;
		}
	}

	if ( !bExpectOk ) {
		iRc = (iRc == XL_RC_OK) ? XL_RC_FAIL : XL_RC_OK;
	}

	free(sDump);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlTestSemMultiReturnBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\multi_return_basic.xl", 1, "int"); }
static int __xlTestSemMultiReturnCallExpandBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\multi_return_call_expand_basic.xl", 1, "int"); }
static int __xlTestSemMultiReturnSingleBindError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\multi_return_single_bind_error.xl", 0, NULL); }
static int __xlTestSemFunctionValueBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\function_value_basic.xl", 1, "int"); }
static int __xlTestSemFunctionValueFlowBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\function_value_flow_basic.xl", 1, "int"); }
static int __xlTestSemFunctionValueContainerBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\function_value_container_basic.xl", 1, "int"); }
static int __xlTestSemFunctionValueParenCallBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\function_value_paren_call_basic.xl", 1, "int"); }
static int __xlTestSemFunctionValueOverloadBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\function_value_overload_basic.xl", 1, "int"); }
static int __xlTestSemFunctionValueAutoAmbiguousError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\function_value_auto_ambiguous_error.xl", 0, NULL); }
static int __xlTestSemAutoParamError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\auto_param_error.xl", 0, NULL); }
static int __xlTestSemAutoReturnError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\auto_return_error.xl", 0, NULL); }
static int __xlTestSemTypeDefUnknownError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\typedef_unknown_error.xl", 0, NULL); }
static int __xlTestSemTypeCtorArityError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\type_ctor_arity_error.xl", 0, NULL); }
static int __xlTestSemPointCtorError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\point_ctor_error.xl", 0, NULL); }
static int __xlTestSemAssignBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_basic.xl", 1, "int"); }
static int __xlTestSemAssignConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_const_error.xl", 0, NULL); }
static int __xlTestSemAssignPlusBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_plus_basic.xl", 1, "int"); }
static int __xlTestSemAssignPlusConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_plus_const_error.xl", 0, NULL); }
static int __xlTestSemAssignMinusBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_minus_basic.xl", 1, "int"); }
static int __xlTestSemAssignMinusConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_minus_const_error.xl", 0, NULL); }
static int __xlTestSemAssignMulBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_mul_basic.xl", 1, "int"); }
static int __xlTestSemAssignMulConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_mul_const_error.xl", 0, NULL); }
static int __xlTestSemAssignDivBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_div_basic.xl", 1, "int"); }
static int __xlTestSemAssignDivConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_div_const_error.xl", 0, NULL); }
static int __xlTestSemAssignModBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_mod_basic.xl", 1, "int"); }
static int __xlTestSemAssignModConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_mod_const_error.xl", 0, NULL); }
static int __xlTestSemPowerBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\power_basic.xl", 1, "int"); }
static int __xlTestSemAssignPowBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assign_pow_basic.xl", 1, "int"); }
static int __xlTestSemAssignPowConstError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\assign_pow_const_error.xl", 0, NULL); }
static int __xlTestSemCoalesceBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\coalesce_basic.xl", 1, "int"); }
static int __xlTestSemCoalesceTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\coalesce_type_error.xl", 0, NULL); }
static int __xlTestSemNullAssignBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\null_assign_basic.xl", 1, "int"); }
static int __xlTestSemNullAssignTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\null_assign_type_error.xl", 0, NULL); }
static int __xlTestSemTernaryBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\ternary_basic.xl", 1, "int"); }
static int __xlTestSemTernaryTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\ternary_type_error.xl", 0, NULL); }
static int __xlTestSemApproxBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\approx_basic.xl", 1, "int"); }
static int __xlTestSemApproxTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\approx_type_error.xl", 0, NULL); }
static int __xlTestSemLogicBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\logic_basic.xl", 1, "int"); }
static int __xlTestSemLogicTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\logic_type_error.xl", 0, NULL); }
static int __xlTestSemNotTypeError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\not_type_error.xl", 0, NULL); }
static int __xlTestSemBytesAliasString(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\bytes_alias_string.xl", 1, "int"); }
static int __xlTestSemTemplateBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\template_basic.xl", 1, "int"); }
static int __xlTestSemSafeMemberBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\safe_member_basic.xl", 1, "int"); }
static int __xlTestSemVarBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\var_basic.xl", 1, "int"); }
static int __xlTestSemArrayBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\array_basic.xl", 1, "int"); }
static int __xlTestSemListBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\list_basic.xl", 1, "int"); }
static int __xlTestSemSetBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\set_basic.xl", 1, "int"); }
static int __xlTestSemDictBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\dict_basic.xl", 1, "int"); }
static int __xlTestSemBareContainerLiterals(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\bare_container_literals_basic.xl", 1, "int"); }
static int __xlTestSemVarArgArrayBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\vararg_array_basic.xl", 1, "int"); }
static int __xlTestSemVarArgDictBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\vararg_dict_basic.xl", 1, "int"); }
static int __xlTestSemVarArgMixedPackError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\vararg_mixed_pack_error.xl", 0, NULL); }
static int __xlTestSemVarArgReservedArgError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\vararg_reserved_arg_error.xl", 0, NULL); }
static int __xlTestSemBareIndexedListError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\bare_indexed_list_error.xl", 0, NULL); }
static int __xlTestSemIndexAssignBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\index_assign_basic.xl", 1, "int"); }
static int __xlTestSemIndexAssignPlusError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\index_assign_plus_error.xl", 0, NULL); }
static int __xlTestSemForInContainers(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\for_in_containers_basic.xl", 1, "int"); }
static int __xlTestSemContainerMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\container_methods_basic.xl", 1, "int"); }
static int __xlTestSemContainerMethodError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\container_method_error.xl", 0, NULL); }
static int __xlTestSemContainerAppendBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\container_append_basic.xl", 1, "int"); }
static int __xlTestSemContainerAppendError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\container_append_error.xl", 0, NULL); }
static int __xlTestSemContainerClearBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\container_clear_basic.xl", 1, "int"); }
static int __xlTestSemContainerClearError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\container_clear_error.xl", 0, NULL); }
static int __xlTestSemContainerRemoveBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\container_remove_basic.xl", 1, "int"); }
static int __xlTestSemContainerRemoveError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\container_remove_error.xl", 0, NULL); }
static int __xlTestSemCoreLenBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\core_len_basic.xl", 1, "int"); }
static int __xlTestSemTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, "int"); }
static int __xlTestSemVarMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\var_methods_basic.xl", 1, "int"); }
static int __xlTestSemVarTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\var_type_methods_basic.xl", 1, "int"); }
static int __xlTestSemVarContainerTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\var_container_type_methods_basic.xl", 1, "int"); }
static int __xlTestSemTypeMethodSelfError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\type_method_self_error.xl", 0, NULL); }
static int __xlTestSemPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\property_length_basic.xl", 1, "int"); }
static int __xlTestSemPropertyLengthError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\property_length_error.xl", 0, NULL); }
static int __xlTestSemStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, "int"); }
static int __xlTestSemStringSplitBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\string_split_basic.xl", 1, "int"); }
static int __xlTestSemStringSplitError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\string_split_error.xl", 0, NULL); }
static int __xlTestSemWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\while_basic.xl", 1, "int"); }
static int __xlTestSemDoWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\do_while_basic.xl", 1, "int"); }
static int __xlTestSemGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\generator_resume_basic.xl", 1, "int"); }
static int __xlTestSemYieldTopLevelError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\yield_top_level_error.xl", 0, NULL); }
static int __xlTestSemXlpImport(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp\\use_pkg_math.xl", 1, "int"); }
static int __xlTestSemXlpNative(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_clock.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncs(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_add.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsString(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_text.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsStringParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_text_len.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsVoid(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_touch.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsMixedParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_text_plus_len.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsBool(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_bool.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsBoolParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_bool_score.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsFloat(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_float.xl", 1, "int"); }
static int __xlTestSemXlpNativeFuncsFloatParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_native\\use_pkg_probe_add_half.xl", 1, "int"); }
static int __xlTestSemXlpStatic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_clock.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncs(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_add.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsString(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_text.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsStringParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_text_len.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsVoid(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_touch.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsMixedParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_text_plus_len.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsBool(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_bool.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsBoolParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_bool_score.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsFloat(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_float.xl", 1, "int"); }
static int __xlTestSemXlpStaticFuncsFloatParams(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_static\\use_pkg_probe_add_half.xl", 1, "int"); }
static int __xlTestSemXlpSource(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_source\\use_pkg_math.xl", 1, "int"); }
static int __xlTestSemXlpTransitive(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_transitive\\use_pkg_app.xl", 1, "int"); }
static int __xlTestSemXlpVersions(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\xlp_versions\\use_dual_versions.xl", 1, "int"); }
static int __xlTestSemXlpExternalPackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp\\use_pkg_math.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"name\": \"pkg.math\"") ||
			!__xlTraceContains(sJson, "\"version\": \"1.0.0\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.math@1.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"kind\": \"source\"") ||
			!__xlTraceContains(sJson, "\"packageName\": \"use_pkg_math\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"use_pkg_math@0.0.0#abi1::<root>\"") ||
			!__xlTraceContains(sJson, "\"manifest\": \"") ||
			!__xlTraceContains(sJson, "examples\\\\xlp\\\\packages\\\\pkg.math\\\\xlp.json\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.math@1.0.0#abi1::pkg.math\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpVersionsPackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp_versions\\use_dual_versions.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.math@1.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.math@2.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"usedBy\": [\"pkg.a\"]") ||
			!__xlTraceContains(sJson, "\"usedBy\": [\"pkg.b\"]") ||
			!__xlTraceContains(sJson, "\"dependsOn\": [\"pkg.math@1.0.0\"]") ||
			!__xlTraceContains(sJson, "\"dependsOn\": [\"pkg.math@2.0.0\"]") ||
			!__xlTraceContains(sJson, "\"initSymbol\": \"xl_mod_pkg_math_1_0_0_abi1__pkg_math__init\"") ||
			!__xlTraceContains(sJson, "\"initSymbol\": \"xl_mod_pkg_math_2_0_0_abi1__pkg_math__init\"") ||
			!__xlTraceContains(sJson, "\"exportsSymbol\": \"xl_mod_pkg_math_1_0_0_abi1__pkg_math__exports\"") ||
			!__xlTraceContains(sJson, "\"exportsSymbol\": \"xl_mod_pkg_math_2_0_0_abi1__pkg_math__exports\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.math@1.0.0#abi1::pkg.math\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.math@2.0.0#abi1::pkg.math\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpNativePackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp_native\\use_pkg_clock.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"name\": \"pkg.clock\"") ||
			!__xlTraceContains(sJson, "\"kind\": \"native\"") ||
			!__xlTraceContains(sJson, "\"artifactKind\": \"shared\"") ||
			!__xlTraceContains(sJson, "\"artifactPath\": \"bin\\\\pkg_clock.dll\"") ||
			!__xlTraceContains(sJson, "\"artifactFile\": \"") ||
			!__xlTraceContains(sJson, "pkg_clock.dll") ||
			!__xlTraceContains(sJson, "\"consumeMode\": \"shared-bridge\"") ||
			!__xlTraceContains(sJson, "\"binding\": \"std.time\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.clock@1.0.0#abi1::pkg.clock\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpStaticPackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp_static\\use_pkg_clock.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"artifactKind\": \"static\"") ||
			!__xlTraceContains(sJson, "\"artifactPath\": \"lib\\\\pkg_clock.a\"") ||
			!__xlTraceContains(sJson, "\"artifactFile\": \"") ||
			!__xlTraceContains(sJson, "pkg_clock.a") ||
			!__xlTraceContains(sJson, "\"consumeMode\": \"static-bridge\"") ||
			!__xlTraceContains(sJson, "\"binding\": \"std.time\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpSourcePackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp_source\\use_pkg_math.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"artifactKind\": \"source\"") ||
			!__xlTraceContains(sJson, "\"artifactPath\": \"src\\\\math_pkg_impl.xl\"") ||
			!__xlTraceContains(sJson, "\"artifactFile\": \"") ||
			!__xlTraceContains(sJson, "math_pkg_impl.xl") ||
			!__xlTraceContains(sJson, "\"consumeMode\": \"source-parse\"") ||
			!__xlTraceContains(sJson, "\"kind\": \"source\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpTransitivePackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\xlp_transitive\\use_pkg_app.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.app@1.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.mid@1.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"pkg.math@1.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"dependsOn\": [\"pkg.mid@1.0.0\"]") ||
			!__xlTraceContains(sJson, "\"dependsOnAbi\": [\"pkg.mid@1.0.0#abi1\"]") ||
			!__xlTraceContains(sJson, "\"dependsOn\": [\"pkg.math@1.0.0\"]") ||
			!__xlTraceContains(sJson, "\"dependsOnAbi\": [\"pkg.math@1.0.0#abi1\"]") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.app@1.0.0#abi1::pkg.app\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.mid@1.0.0#abi1::pkg.mid\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"pkg.math@1.0.0#abi1::pkg.math\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpFuncs(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\full\\overload_runtime.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlSemDumpXlpFunctionsJson(&semResult);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"overloadGroup\": \"global::convert\"") ||
			!__xlTraceContains(sJson, "\"signatureId\": \"convert(int)\"") ||
			!__xlTraceContains(sJson, "\"dispatchKey\": \"convert$arity1|value:int\"") ||
			!__xlTraceContains(sJson, "\"languageAbi\": \"xlang@v1\"") ||
			!__xlTraceContains(sJson, "\"nativeAbi\": \"c@v1\"") ||
			!__xlTraceContains(sJson, "\"abiSymbol\": \"xl__convert__arity1__value__int__ret__int\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpPackage(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int i;
	const char* pNeedles[] = {
		"\"format\": \"xlp-0\"",
		"\"abiId\": \"use_math@0.0.0#abi1\"",
		"\"packageName\": \"use_math\"",
		"\"moduleAbiId\": \"use_math@0.0.0#abi1::math\"",
		"\"functions\": [",
		"\"returnType\": \"int\""
	};
	const int iNeedleCount = (int)(sizeof(pNeedles) / sizeof(pNeedles[0]));
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\module\\use_math.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( sJson == NULL ) {
			iRc = XL_RC_FAIL;
		} else {
			for ( i = 0; i < iNeedleCount; ++i ) {
				if ( !__xlTraceContains(sJson, pNeedles[i]) ) {
					fprintf(stderr, "[trace][sem.xlp_package] missing substring: %s\n", pNeedles[i]);
					iRc = XL_RC_FAIL;
					break;
				}
			}
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemModuleAbiSummary(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_DiagContext diag;
	const XL_ModuleSummary* pMath;
	const XL_ModuleSummary* pRoot;
	size_t i;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\module\\use_math.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	pMath = NULL;
	pRoot = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		for ( i = 0; i < moduleResult.iModuleCount; ++i ) {
			if ( (moduleResult.pModules[i].sKey != NULL) && (strcmp(moduleResult.pModules[i].sKey, "math") == 0) ) {
				pMath = &moduleResult.pModules[i];
			} else if ( (moduleResult.pModules[i].sKey != NULL) && (strcmp(moduleResult.pModules[i].sKey, "<root>") == 0) ) {
				pRoot = &moduleResult.pModules[i];
			}
		}
		if ( (pMath == NULL) || (pRoot == NULL) ) {
			fprintf(stderr, "[trace][sem.module_abi_summary] missing math/root summary\n");
			iRc = XL_RC_FAIL;
		} else if (
			(strcmp((pMath->sInitSymbol != NULL) ? pMath->sInitSymbol : "", "xl_mod_math__init") != 0) ||
			(strcmp((pMath->sExportsSymbol != NULL) ? pMath->sExportsSymbol : "", "xl_mod_math__exports") != 0) ||
			(strcmp((pMath->sDescriptorSymbol != NULL) ? pMath->sDescriptorSymbol : "", "xl_mod_math__descriptor") != 0) ||
			(strcmp((pMath->sPackageName != NULL) ? pMath->sPackageName : "", "use_math") != 0) ||
			(strcmp((pMath->sPackageAbiId != NULL) ? pMath->sPackageAbiId : "", "use_math@0.0.0#abi1") != 0) ||
			(strcmp((pRoot->sInitSymbol != NULL) ? pRoot->sInitSymbol : "", "xl_mod__root___init") != 0) ||
			(strcmp((pRoot->sExportsSymbol != NULL) ? pRoot->sExportsSymbol : "", "xl_mod__root___exports") != 0) ||
			(strcmp((pRoot->sDescriptorSymbol != NULL) ? pRoot->sDescriptorSymbol : "", "xl_mod__root___descriptor") != 0) ||
			(strcmp((pRoot->sPackageName != NULL) ? pRoot->sPackageName : "", "use_math") != 0) ||
			(strcmp((pRoot->sPackageAbiId != NULL) ? pRoot->sPackageAbiId : "", "use_math@0.0.0#abi1") != 0)
		) {
			fprintf(stderr, "[trace][sem.module_abi_summary] summary symbols/package mismatch\n");
			iRc = XL_RC_FAIL;
		} else if (
			!__xlTraceContains(moduleResult.sTextDump, "abi init=xl_mod_math__init exports=xl_mod_math__exports descriptor=xl_mod_math__descriptor") ||
			!__xlTraceContains(moduleResult.sJsonDump, "\"initSymbol\": \"xl_mod_math__init\"") ||
			!__xlTraceContains(moduleResult.sJsonDump, "\"exportsSymbol\": \"xl_mod_math__exports\"") ||
			!__xlTraceContains(moduleResult.sJsonDump, "\"descriptorSymbol\": \"xl_mod_math__descriptor\"")
		) {
			fprintf(stderr, "[trace][sem.module_abi_summary] dump text/json missing module abi symbols\n");
			iRc = XL_RC_FAIL;
		}
	}

	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestSemXlpReadFunctionsAbi(XL_TestContext* pCtx)
{
	char sManifestPath[XL_PATH_MAX_LEN];
	FILE* pFile;
	static const char* sJson =
		"{\n"
		"\t\"format\": \"xlp-0\",\n"
		"\t\"package\": {\n"
		"\t\t\"name\": \"demo.pkg\",\n"
		"\t\t\"version\": \"1.0.0\",\n"
		"\t\t\"abiVersion\": \"1\",\n"
		"\t\t\"xlangAbi\": \"1\",\n"
		"\t\t\"abiId\": \"demo.pkg@1.0.0#abi1\"\n"
		"\t},\n"
		"\t\"artifact\": {\n"
		"\t\t\"kind\": \"source\",\n"
		"\t\t\"path\": \"src\\\\demo.xl\"\n"
		"\t},\n"
		"\t\"dependencies\": [],\n"
		"\t\"modules\": [\n"
		"\t\t{\n"
		"\t\t\t\"key\": \"math\",\n"
		"\t\t\t\"abiId\": \"demo.pkg@1.0.0#abi1::math\",\n"
		"\t\t\t\"file\": \"src\\\\demo.xl\",\n"
		"\t\t\t\"root\": 1,\n"
		"\t\t\t\"native\": 0,\n"
		"\t\t\t\"binding\": \"\",\n"
		"\t\t\t\"initSymbol\": \"xl_mod_math__init\",\n"
		"\t\t\t\"exportsSymbol\": \"xl_mod_math__exports\",\n"
		"\t\t\t\"descriptorSymbol\": \"xl_mod_math__descriptor\",\n"
		"\t\t\t\"imports\": [],\n"
		"\t\t\t\"exports\": [\n"
		"\t\t\t\t{\n"
		"\t\t\t\t\t\"name\": \"convert\",\n"
		"\t\t\t\t\t\"symbol\": \"convert\",\n"
		"\t\t\t\t\t\"kind\": \"function\",\n"
		"\t\t\t\t\t\"overloadCount\": 1\n"
		"\t\t\t\t}\n"
		"\t\t\t]\n"
		"\t\t}\n"
		"\t],\n"
		"\t\"functions\": [\n"
		"\t\t{\n"
		"\t\t\t\"module\": \"math\",\n"
		"\t\t\t\"moduleAbiId\": \"demo.pkg@1.0.0#abi1::math\",\n"
		"\t\t\t\"exportName\": \"convert\",\n"
		"\t\t\t\"symbol\": \"convert\",\n"
		"\t\t\t\"overloadGroup\": \"global::convert\",\n"
		"\t\t\t\"signatureId\": \"convert(int)\",\n"
		"\t\t\t\"dispatchKey\": \"convert$arity1|value:int\",\n"
		"\t\t\t\"languageAbi\": \"xlang@v1\",\n"
		"\t\t\t\"nativeAbi\": \"c@v1\",\n"
		"\t\t\t\"languageAbiId\": \"demo.pkg@1.0.0#abi1::math::convert(int)#xlang@v1\",\n"
		"\t\t\t\"nativeAbiId\": \"demo.pkg@1.0.0#abi1::math::xl__convert__arity1__value__int__ret__int#c@v1\",\n"
		"\t\t\t\"abiSymbol\": \"xl__convert__arity1__value__int__ret__int\",\n"
		"\t\t\t\"returnType\": \"int\",\n"
		"\t\t\t\"async\": 0,\n"
		"\t\t\t\"generator\": 0,\n"
		"\t\t\t\"params\": [\n"
		"\t\t\t\t{\n"
		"\t\t\t\t\t\"name\": \"value\",\n"
		"\t\t\t\t\t\"label\": \"value\",\n"
		"\t\t\t\t\t\"type\": \"int\",\n"
		"\t\t\t\t\t\"hasDefault\": 0\n"
		"\t\t\t\t}\n"
		"\t\t\t]\n"
		"\t\t}\n"
		"\t]\n"
		"}\n";
	XL_XlpFunctionInfo* pFuncs;
	size_t iFuncCount;
	size_t i;
	int iRc;
	int bFound;

	__xlBuildProjectPath(pCtx, sManifestPath, sizeof(sManifestPath), "release\\tmp_xlp_read_functions_abi.json");
	pFuncs = NULL;
	iFuncCount = 0;
	iRc = XL_RC_OK;
	pFile = fopen(sManifestPath, "wb");
	if ( pFile == NULL ) {
		iRc = XL_RC_FAIL;
	} else {
		if ( fwrite(sJson, 1, strlen(sJson), pFile) != strlen(sJson) ) {
			iRc = XL_RC_FAIL;
		}
		fclose(pFile);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlXlpReadFunctions(sManifestPath, "math", &pFuncs, &iFuncCount);
	}
	if ( iRc == XL_RC_OK ) {
		bFound = 0;
		for ( i = 0; i < iFuncCount; ++i ) {
			if ( pFuncs[i].sExportName[0] == '\0' ) {
				continue;
			}
			if ( (strcmp(pFuncs[i].sLanguageAbi, "xlang@v1") != 0) ||
				(strcmp(pFuncs[i].sNativeAbi, "c@v1") != 0) ||
				(strcmp(pFuncs[i].sLanguageAbiId, "demo.pkg@1.0.0#abi1::math::convert(int)#xlang@v1") != 0) ||
				(strcmp(pFuncs[i].sNativeAbiId, "demo.pkg@1.0.0#abi1::math::xl__convert__arity1__value__int__ret__int#c@v1") != 0) ||
				(pFuncs[i].sAbiSymbol[0] == '\0') ) {
				iRc = XL_RC_FAIL;
				break;
			}
			bFound = 1;
		}
		if ( (iRc == XL_RC_OK) && !bFound ) {
			iRc = XL_RC_FAIL;
		}
	}

	xlXlpFunctionInfoFree(pFuncs, iFuncCount);
	remove(sManifestPath);
	return iRc;
}
static int __xlTestSemXlpDependencies(XL_TestContext* pCtx)
{
	char sPath[XL_PATH_MAX_LEN];
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_DiagContext diag;
	char* sJson;
	int iRc;

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), "examples\\native\\stdlib_pipeline.xl");
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sJson = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		sJson = xlXlpDumpPackageJson(&moduleResult, &semResult, sPath);
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"name\": \"std.fs\"") ||
			!__xlTraceContains(sJson, "\"name\": \"std.str\"") ||
			!__xlTraceContains(sJson, "\"name\": \"std.process\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"std.fs@0.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"std.str@0.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"abiId\": \"std.process@0.0.0#abi1\"") ||
			!__xlTraceContains(sJson, "\"usedBy\": [\"<root>\"]") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}
static int __xlTestLowerBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\lower\\basic.xl", 1, -1, 0, 0); }
static int __xlTestLowerAsync(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\basic.xl", 1, -1, 1, 0); }
static int __xlTestLowerGenerator(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\generator\\basic.xl", 1, -1, 0, 1); }
static int __xlTestLowerGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\generator_resume_basic.xl", 1, XL_AST_YIELD, 0, 1); }
static int __xlTestLowerGeneratorResumeAbiJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\full\\generator_resume_basic.xl", 1, "\"abiSymbol\": \"xl__make_values__arity0__ret__int\""); }
static int __xlTestLowerAsyncGeneratorBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_basic.xl", 1, XL_AST_YIELD, 1, 1); }
static int __xlTestLowerAsyncGeneratorForInBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_for_in_basic.xl", 1, XL_AST_FORIN, 1, 1); }
static int __xlTestLowerAsyncGeneratorWhileBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_while_basic.xl", 1, XL_AST_WHILE, 1, 1); }
static int __xlTestLowerAsyncGeneratorLoopControlBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, XL_AST_WHILE, 1, 1); }
static int __xlTestLowerAsyncGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_resume_basic.xl", 1, XL_AST_YIELD, 1, 1); }
static int __xlTestLowerAsyncGeneratorMultiAwaitGroupBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_multi_await_group_basic.xl", 1, XL_AST_YIELD, 1, 1); }
static int __xlTestLowerAsyncGeneratorTryCatchBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_try_catch_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorTryCatchConstElseBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_try_catch_const_else_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorTryFinallyBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_try_finally_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorTryFinallyConstElseBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_try_finally_const_else_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorNestedTryFlowBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_nested_try_flow_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorNestedTryFlowConstElseBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\async_generator_nested_try_flow_const_else_basic.xl", 1, XL_AST_TRY, 1, 1); }
static int __xlTestLowerAsyncGeneratorHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_for_in_basic.xl", 1, "kind=await+yield"); }
static int __xlTestLowerAsyncGeneratorHybridProjectionJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_for_in_basic.xl", 1, "\"kind\": \"await+yield\""); }
static int __xlTestLowerAsyncGeneratorLoopControlHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, "kind=await @ 8:8 asyncState=3 generatorState=-1 group=0 projectedGeneratorState=6 targetPoint=1"); }
static int __xlTestLowerAsyncGeneratorLoopControlHybridProjectionJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, "\"projectedGeneratorState\": 6"); }
static int __xlTestLowerAsyncGeneratorResumeHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_resume_basic.xl", 1, "kind=await+yield @ 4:7 asyncState=1 generatorState=1 group=0 projectedGeneratorState=1 targetPoint=0"); }
static int __xlTestLowerAsyncGeneratorResumeHybridProjectionJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_resume_basic.xl", 1, "\"groupCount\": 2"); }
static int __xlTestLowerAsyncGeneratorMultiAwaitGroupProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_multi_await_group_basic.xl", 1, "G0 awaitCount=2 pointCount=3"); }
static int __xlTestLowerAsyncGeneratorMultiAwaitGroupProjectionJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_multi_await_group_basic.xl", 1, "\"awaitCount\": 2"); }
static int __xlTestLowerAsyncGeneratorNestedTryHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_nested_try_flow_basic.xl", 1, "groupCount=3"); }
static int __xlTestLowerAsyncGeneratorTryCatchConstElseHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_try_catch_const_else_basic.xl", 1, "groupCount=3"); }
static int __xlTestLowerAsyncGeneratorTryFinallyConstElseHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_try_finally_const_else_basic.xl", 1, "\"groupCount\": 2"); }
static int __xlTestLowerAsyncGeneratorNestedTryConstElseHybridProjection(XL_TestContext* pCtx) { return __xlRunLowerDumpCase(pCtx, "examples\\async\\async_generator_nested_try_flow_const_else_basic.xl", 1, "groupCount=3"); }
static int __xlTestLowerAsyncGeneratorLoopControlHybridGroupsJson(XL_TestContext* pCtx) { return __xlRunLowerJsonCase(pCtx, "examples\\async\\async_generator_loop_control_basic.xl", 1, "\"groups\": ["); }
static int __xlTestLowerExportGenericTypeMethod(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\module\\use_list_string_ext_method.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerForIn(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\for_in_range.xl", 1, XL_AST_FORIN, 0, 0); }
static int __xlTestLowerIndexAssignBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\index_assign_basic.xl", 1, XL_AST_ASSIGN, 0, 0); }
static int __xlTestLowerForInContainers(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\for_in_containers_basic.xl", 1, XL_AST_FORIN, 0, 0); }
static int __xlTestLowerContainerMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\container_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerContainerAppendBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\container_append_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerContainerClearBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\container_clear_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerContainerRemoveBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\container_remove_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerBareContainerLiterals(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\bare_container_literals_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerCoreLenBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\native\\core_len_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerTypeDefBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\typedef_basic.xl", 1, XL_AST_TYPEDEF, 0, 0); }
static int __xlTestLowerTypeCtorBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\type_ctor_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerPointBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\point_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerVarMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\var_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerVarTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\var_type_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerVarContainerTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\var_container_type_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\property_length_basic.xl", 1, XL_AST_MEMBER, 0, 0); }
static int __xlTestLowerStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerStringSplitBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\string_split_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerTry(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, XL_AST_TRY, 0, 0); }
static int __xlTestLowerNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\named_args_default.xl", 1, XL_AST_NAMED_ARG, 0, 0); }
static int __xlTestCgenModule(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\module\\use_math.xl", "xl_mod_math__"); }
static int __xlTestCgenExportTypeMethod(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_mod_use_text_ext_method_0_0_0_abi1__text_ext_method__wrap",
		"xl_mod_use_text_ext_method_0_0_0_abi1__text_ext_method__exclaim"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\module\\use_text_ext_method.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenExportGenericTypeMethod(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"list_string_ext_method",
		"wrap_join"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\module\\use_list_string_ext_method.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenModuleInitThrow(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int xl_generated_module_init_done = 0;",
		"static int xl_mod_use_init_throw_dep_0_0_0_abi1__init_throw_dep__base = 0;",
		"xl_mod_use_init_throw_dep_0_0_0_abi1__init_throw_dep__base = xl__xl_mod_use_init_throw_dep_0_0_0_abi1__init_throw_dep__get_base__arity0__ret__int();",
		"xl_generated_module_init();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\module\\use_init_throw_dep.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenOverloadAbi(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int xl__convert__arity1__value__int__ret__int(",
		"static const char* xl__convert__arity1__value__string__ret__string(",
		"xl__convert__arity1__value__int__ret__int(41)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\overload_runtime.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenNative(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\use_std_core.xl", "xl_mod_std_core__"); }
static int __xlTestCgenImportAlias(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\import_alias_basic.xl", "xl_str_contains(\"alphabet\", \"pha\")"); }
static int __xlTestCgenImportMulti(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\import_multi_basic.xl", "xl_fmt_int_text(42"); }
static int __xlTestCgenImportString(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\import_string_basic.xl", "xl_str_contains(\"alphabet\", \"pha\")"); }
static int __xlTestCgenExportFunc(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\module\\use_math_export_func.xl", "xl_mod_math_export_func__"); }
static int __xlTestCgenExportValues(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\module\\use_math_export_values.xl", "xl_mod_math_export_values__"); }
static int __xlTestCgenTypedDecl(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int base = 0;",
		"base = 40;",
		"int delta = 0;"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\typed_decl_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenExactNumericTypes(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int32_t base = 0;",
		"static uint64_t wide = 0;",
		"static float ratio32 = 0.0;",
		"static double ratio64 = 0.0;",
		"int32_t total = (xl__bump_int32__arity1__value__int32__ret__int32(base) + 1);",
		"uint64_t carry = (wide + 1);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\exact_numeric_types_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAutoDecl(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int base = 0;",
		"base = 40;",
		"int delta = 2;"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\auto_decl_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTypeDefBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\typedef_basic.xl", "char* xl__join__arity2__left__string__right__string__ret__string"); }
static int __xlTestCgenTypeCtorBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoGetInt((xvalue)(xl_type_ctor_value))",
		"xvoGetFloat((xvalue)(xl_type_ctor_value))",
		"xl_generated_copy_text(xl_value_to_string((xvalue)(xl_type_ctor_value)))",
		"xvoGetTime((xvalue)(xl_type_ctor_value))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\type_ctor_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenPointBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoCreatePoint((ptr)(",
		"xvoGetPoint((xvalue)(xl_type_ctor_value))",
		"xl_generated_copy_text(xl_value_to_string((xvalue)(xl_type_ctor_value)))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\point_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 0",
		"value = 42"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignPlusBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 40",
		"value += 2"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_plus_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignMinusBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 50",
		"value -= 8"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_minus_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignMulBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 6",
		"value *= 7"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_mul_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignDivBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 84",
		"value /= 2"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_div_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignModBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 85",
		"value %= 43"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_mod_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenPowerBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_generated_power_int(2, 5)",
		"return xl_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\power_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssignPowBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"value = 2",
		"xl_generated_power_assign_int(&(value), 5)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\assign_pow_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenCoalesceBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\coalesce_basic.xl", "current != NULL ? current : Error(\"fallback\", 9, \"\", \"\", NULL)"); }
static int __xlTestCgenNullAssignBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_generated_null_assign_error(&(current), Error(\"fallback\", 7, \"\", \"\", NULL))",
		"xl_generated_null_assign_error(&(current), Error(\"ignored\", 3, \"\", \"\", NULL))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\null_assign_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTernaryBasic(XL_TestContext* pCtx)
{
	return __xlRunCgenCase(pCtx, "examples\\full\\ternary_basic.xl", "(ok ? 42 : 7)");
}
static int __xlTestCgenApproxBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_generated_number_approx(near_left, near_right)",
		"xl_generated_text_approx(\"hello\", \"h*o\")",
		"xl_generated_time_approx(first, second)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\approx_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenIfInlineBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"if ( xl_if_cond_",
		"(value = 42);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\if_inline_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenIfInlineElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"if ( xl_if_cond_",
		"(value = 1);",
		"else",
		"(value = 42);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\if_inline_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenLogicBasic(XL_TestContext* pCtx)
{
	return __xlRunCgenCase(pCtx, "examples\\full\\logic_basic.xl", "((true && (!false)) || false)");
}
static int __xlTestCgenBytesAliasString(XL_TestContext* pCtx)
{
	return __xlRunCgenCase(pCtx, "examples\\full\\bytes_alias_string.xl", "const char* raw = \"abc\";");
}
static int __xlTestCgenTemplateBasic(XL_TestContext* pCtx)
{
	return __xlRunCgenCase(pCtx, "examples\\full\\template_basic.xl", "const char* text = \"he\\\"llo\";");
}
static int __xlTestCgenSafeMemberBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"({ XL_GeneratedError* xl_safe_error = current; (xl_safe_error != NULL) ? xl_generated_error_message(xl_safe_error) : NULL; })",
		"({ XL_GeneratedError* xl_safe_error = current; (xl_safe_error != NULL) ? xl_generated_error_code(xl_safe_error) : 0; })"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\safe_member_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenVarBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvalue value = xvoCreateInt((int64)(40));",
		"(value = xvoCreateInt((int64)(41)))",
		"int local = ((int)xvoGetInt((xvalue)(v)));",
		"xvalue xl_ret_2_2 = xvoCopy((xvalue)(v));",
		"xvoCanCompareBasic(xl_cmp_left, xl_cmp_right)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\var_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenArrayBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvalue values = ({ xvalue xl_arr_literal = xvoCreateArray();",
		"xvoArrayAppendValue(xl_arr_literal, xvoCreateInt((int64)(40)), TRUE);",
		"xvoArrayGetInt((xvalue)(values), (uint32)(0))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\array_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenListBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvalue values = ({ xvalue xl_list_ctor = xvoCreateList();",
		"xvoListSetValue(xl_list_ctor, (int64)(0), xvoCreateInt((int64)(40)), TRUE);",
		"xvoListGetInt((xvalue)(values), (int64)(0))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\list_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenSetBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvalue values = ({ xvalue xl_set_ctor = xvoCreateColl();",
		"xvoCollSetValue(xl_set_ctor, xvoCreateInt((int64)(40)), TRUE);",
		"xvoCollExists((xvalue)(values), xl_set_item)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\set_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenDictBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvalue values = ({ xvalue xl_dict_ctor = xvoCreateTable();",
		"xvoTableSetValue(xl_dict_ctor, (const void*)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\"), xvoCreateInt((int64)(42)), TRUE);",
		"xvoTableGetInt((xvalue)(values), (const void*)xl_dict_key, (uint32)strlen((xl_dict_key != NULL) ? xl_dict_key : \"\"))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\dict_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenVarArgArrayBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\vararg_array_basic.xl", "xvoCreateArray()"); }
static int __xlTestCgenVarArgDictBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\vararg_dict_basic.xl", "xvoCreateTable()"); }
static int __xlTestCgenMultiReturnBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"__ret__array_x3Cvar_x3E",
		"xvoArrayGetInt((xvalue)(xl_unpack_",
		"xvoArrayGetText((xvalue)(xl_unpack_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\multi_return_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenMultiReturnCallExpandBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_mret_",
		"xl_call_result",
		"xvoArrayGetText((xvalue)(xl_mret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\multi_return_call_expand_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenFunctionValueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__add__arity2__a__int__b__int__ret__int__boxed",
		"xfunction op = ((xfunction)xl__add__arity2__a__int__b__int__ret__int__boxed);",
		"xfunction also = ((xfunction)xl__add__arity2__a__int__b__int__ret__int__boxed);",
		"xvalue dyn = xvoCreateFunc((xfunction)(((xfunction)xl__add__arity2__a__int__b__int__ret__int__boxed)))",
		"((xfunction)(((xfunction)xvoGetFunc((xvalue)(dyn)))))"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\function_value_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenFunctionValueContainerBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoArrayGetValue((xvalue)(arr), (uint32)(0))",
		"((xfunction)xvoGetFunc((xvalue)(xvoArrayGetValue((xvalue)(arr), (uint32)(0)))))",
		"xvoArrayGetValue((xvalue)(dyn_arr), (uint32)(0))",
		"xvoTableGetValue((xvalue)(named)",
		"xvoTableGetValue((xvalue)(dyn_dict)",
		"((xfunction)xvoGetFunc((xvalue)(({ const char* xl_dict_key = \"sum\"; xvoTableGetValue((xvalue)(named)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\function_value_container_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenModuleAbiSymbols(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"const XL_GeneratedModuleExport xl_mod_math__exports[] = {",
		"void xl_mod_math__init(void)",
		"const XL_GeneratedModuleDescriptor xl_mod_math__descriptor = {",
		"\"use_math@0.0.0#abi1::math\"",
		"\"use_math@0.0.0#abi1::<root>\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\module\\use_math.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenIndexAssignBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoArraySetValue((xvalue)(xl_assign_base_",
		"xvoListSetValue((xvalue)(xl_assign_base_",
		"xvoTableSetValue((xvalue)(xl_assign_base_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\index_assign_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenForInContainers(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoArrayItemCount((xvalue)(xl_iter_",
		"xvoArrayGetValue((xvalue)(xl_iter_",
		"xvoListItemCount((xvalue)(xl_iter_",
		"xvoListGetValue((xvalue)(xl_iter_",
		"xrtAVLTreeIterBegin(xl_iter_",
		"xrtAVLTB_IterBegin((xavltbase)xl_iter_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\for_in_containers_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenContainerMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_core_len_array(",
		"xl_core_len_list(",
		"xl_core_len_set(",
		"xl_core_len_dict(",
		"xvoCollExists((xvalue)(",
		"xvoTableExists((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\container_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenContainerAppendBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_array_append((xvalue)(",
		"xl_list_append((xvalue)(",
		"xl_core_len_array(",
		"xl_core_len_list("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\container_append_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenContainerClearBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_array_clear((xvalue)(",
		"xl_list_clear((xvalue)(",
		"xl_set_clear((xvalue)(",
		"xl_dict_clear((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\container_clear_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenBareContainerLiterals(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xvoCreateTable()",
		"xvoCreateColl()",
		"xvoCreateList()",
		"xvoCreateArray()",
		"xvoListSetValue(xl_list_ctor, (int64)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\bare_container_literals_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenContainerRemoveBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_array_remove((xvalue)(",
		"xl_list_remove((xvalue)(",
		"xl_set_remove((xvalue)(",
		"xl_dict_remove((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\container_remove_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenCoreLenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_core_len_string(",
		"xl_core_len_array(",
		"xl_core_len_list(",
		"xl_core_len_set(",
		"xl_core_len_dict("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\core_len_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTypeMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_str_replace(",
		"xl_array_append((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\type_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenVarMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_str_replace(",
		"xl_array_append((xvalue)(",
		"xl_list_join(",
		"xl_core_len_array(",
		"xl_core_len_list(",
		"xvoCollExists((xvalue)(",
		"xvoTableExists((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\var_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenVarTypeMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__wrap__arity3__self__string__left__string__right__string__ret__string(xl_generated_copy_text(xl_value_to_string((xvalue)(text))), \"[\", \"]\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\var_type_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenVarContainerTypeMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__wrap_join__arity4__self__list_x3Cstring_x3E__left__string__sep__string__right__string__ret__string"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\var_container_type_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenPropertyLengthBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_core_len_string(",
		"xl_core_len_array(",
		"xl_core_len_list(",
		"xl_core_len_set(",
		"xl_core_len_dict("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\property_length_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenStringMethodsBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_str_trim(",
		"xl_str_trim_left(",
		"xl_str_trim_right(",
		"xl_str_index_of(",
		"xl_str_last_index_of(",
		"xl_str_slice(",
		"xl_str_repeat(",
		"xl_str_lower(",
		"xl_str_upper(",
		"xl_str_replace(",
		"xl_str_contains(",
		"xl_str_split_lines(",
		"xl_str_starts_with(",
		"xl_str_ends_with(",
		"xl_str_split(",
		"xl_array_join(",
		"xl_list_join(",
		"xl_core_len_list("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\string_methods_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenStringSplitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_str_split(\"red,green,blue\", \",\")",
		"xl_core_len_list(",
		"xl_str_concat("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\string_split_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenWhileBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"while ( 1 )",
		"int xl_while_cond_",
		"goto xl_while_break_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\while_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenDoWhileBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"while ( 1 )",
		"xl_while_continue_",
		"int xl_while_cond_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\do_while_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTryFinally(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\try_finally_basic.xl", "xl_generated_restore_exception("); }
static int __xlTestCgenTryFinallyEscapeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"int xl_escape_kind_",
		"goto xl_try_finally_",
		"xl_try_dispatch_",
		"xl_escape_value_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\try_finally_escape_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTryFrame(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\try_nested_rethrow.xl", "xl_generated_exception_frame_push()"); }
static int __xlTestCgenErrorFields(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\error_fields_basic.xl", "Error(\"inner\", 42, \"\", \"\", NULL)"); }
static int __xlTestCgenNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\named_args_default.xl", "xl__mix__arity3__a__int__b__int__c__int__ret__int(1, 2, 9)"); }
static int __xlTestCgenAsyncThrow(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\async\\run_throw_main.xl", "return xl_future_int_error(xl_generated_capture_exception_error());"); }
static int __xlTestCgenAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\async\\run_bool_main.xl", "xl_future_bool_await"); }
static int __xlTestCgenAsyncRunMain(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_generated_future_forward_state(",
		"xl_future_int_forward_state(\"get_value\"",
		"xFutureFinallyEngine(pFuture, NULL, 0u, xl_generated_async_forward_state_finally, pCtx)",
		"xFutureRelease(pHook);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\run_main.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncBindAwaitReturnBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"xFutureFinallyEngine(xl_async_future_1.handle, NULL, 0u, xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume, xl_async_state_1)",
		"int current = (int)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);",
		"int total = (base + current);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\bind_await_return_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncBindExprAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"int xl_async_return_await_",
		"int a = (1 + xl_async_return_await_",
		"int xl_async_ret_7_2 = (a + 2);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\bind_expr_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"int xl_async_return_await_",
		"(value = xl_async_return_await_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\assign_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncAssignStringArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* text;",
		"const char* xl_async_return_await_",
		"(text = xl_async_return_await_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\assign_string_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncAssignMultiAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"(value = xl_async_return_await_",
		"(value = (value + xl_async_return_await_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\assign_multi_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncStringExprAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_future_string_await_state(\"next_text\"",
		"xl_str_concat(\"a\", xl_future_string_await_state(",
		"xl_generated_text_equal(xl_str_concat(",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\string_expr_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfBodyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_body_prelude_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfDynamicThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"xl_future_int_ready(0)",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_dynamic_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfDynamicElseSyncBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 2));",
		"(value = (value + 3));",
		"xl_future_int_ready(0)",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_dynamic_else_sync_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfDynamicElseAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 2));",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_dynamic_else_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 30));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_dynamic_both_await_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 30));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_dynamic_both_await_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 20));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_nested_dynamic_both_await_prelude_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 30));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_nested_dynamic_both_await_prelude_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 20));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_nested_dynamic_both_await_const_else_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 30));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_nested_dynamic_both_await_const_else_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfBodyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_body_prelude_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicElseSyncBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 2));",
		"(value = (value + 3));",
		"(mark = (mark + 3));",
		"xl_future_int_ready(0)",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_else_sync_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicElseAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"(mark = (mark + 2));",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_else_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 30));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_both_await_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 30));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_both_await_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_both_await_prelude_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_dynamic_both_await_prelude_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_nested_dynamic_both_await_prelude_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_nested_dynamic_both_await_prelude_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_nested_dynamic_both_await_const_else_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_if_nested_dynamic_both_await_const_else_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"int xl_if_cond_",
		"if ( xl_if_cond_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_cond_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"int xl_if_cond_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_cond_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncIfCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue values;",
		"xl_array_append((xvalue)(values), xvoCreateInt((int64)(3)))",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\if_cond_container_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"int xl_while_cond_",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"int xl_while_cond_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int value;",
		"xvalue items;",
		"xl_array_append((xvalue)(items), xvoCreateInt((int64)(value)))",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_container_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondBreakBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"goto xl_async_while_break_",
		"xl_async_while_break_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_break_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondContinueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"goto xl_async_while_continue_",
		"xl_async_while_continue_",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_continue_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_while_break_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_break_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncWhileCondContinueThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_while_continue_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\while_cond_continue_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"int value = 0;",
		"(value = (value + 1));",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"int value = 0;",
		"(value = (value + 1));",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"int value = 0;",
		"xvalue items = ({ xvalue xl_arr_literal = xvoCreateArray();",
		"xl_array_append((xvalue)(items), xvoCreateInt((int64)(value)))",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_container_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondBreakBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_do_while_break_",
		"xl_async_do_while_break_",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_break_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondContinueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_do_while_continue_",
		"xl_async_do_while_continue_",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_continue_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_do_while_break_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_break_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncDoWhileCondContinueThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_async_do_while_continue_",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\do_while_cond_continue_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"xl_async_hook_2 = xFutureFinallyEngine(",
		"async linear try/finally failed",
		"(mark = 10);",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"xl_async_hook_2 = xFutureFinallyEngine(",
		"async linear try/finally failed",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = 10);",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume(",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"xl_generated_error_from_text(",
		"xl_async_hook_3 = xFutureFinallyEngine(",
		"(mark = 10);",
		"int xl_async_ret_23_2 = ((value + mark) + xl_async_return_await_23_24);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"xl_generated_error_from_text(",
		"xl_async_hook_3 = xFutureFinallyEngine(",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"xl_async_hook_3 = xFutureFinallyEngine(",
		"int xl_async_ret_17_2 = ((value + mark) + xl_async_return_await_17_24);",
		"(mark = 10);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"xl_async_hook_3 = xFutureFinallyEngine(",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyTailThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = (mark + 10));",
		"(mark = (mark + 2));",
		"xl_async_hook_3 = xFutureFinallyEngine(",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_tail_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyTailThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = (mark + 10));",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"xl_async_hook_3 = xFutureFinallyEngine("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_tail_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyTailThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = 99);",
		"xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_tail_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyTailThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = 99);",
		"xl_generated_error_from_text(",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_tail_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyIfThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_if_cond_",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = 10);",
		"(mark = (mark + 3));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_if_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyIfThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_if_cond_",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = (mark + 40));",
		"(mark = 10);",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_if_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyWhileThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = 99);",
		"xl_generated_error_from_text(",
		"(mark = (mark + 10));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_while_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyWhileThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"if ( !xl_generated_async_has_pending_error() )",
		"(mark = 99);",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_while_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"xl_generated_async_trace_event(\"await\", \"next_value\")",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_prelude_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyPreludeThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 40));",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 50));",
		"xl_generated_async_trace_event(\"await\", \"next_value\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_prelude_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 10));",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));",
		"xl_generated_async_trace_event(\"await\", \"bonus\")"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_prelude_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyPreludeThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(mark = (mark + 40));",
		"(mark = (mark + 10));",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 50));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_prelude_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 10));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 2));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_success_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 2));",
		"(mark = (mark + 30));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_success_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_prelude_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_dynamic_both_await_prelude_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_prelude_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_prelude_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_success_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_success_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_success_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_async_trace_event(\"await\", \"next_then\")",
		"xl_generated_async_trace_event(\"await\", \"next_else\")",
		"if ( xl_generated_async_has_pending_error() ) {",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_success_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyDoWhileThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(value = (value + 1));",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_do_while_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyDoWhileThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"(mark = (mark + 20));",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_do_while_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyDoWhileBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"xl_generated_async_trace_event(\"await\", \"bonus\")",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_do_while_break_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryFinallyDoWhileBreakThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"(mark = (mark + 20));",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_finally_do_while_break_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedInnerCatchThenOuterFinallyBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 20));",
		"(mark = (mark + 2));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_inner_catch_then_outer_finally_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedInnerCatchThenOuterFinallyConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_inner_catch_then_outer_finally_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedInnerFinallyOuterCatchBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = 99);",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_inner_finally_outer_catch_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedInnerFinallyOuterCatchConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_inner_finally_outer_catch_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_if_cond_",
		"(mark = (mark + 10));",
		"(mark = (mark + 20));",
		"(mark = (mark + 3));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_finally_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_if_cond_",
		"xl_generated_async_trace_event(\"await\", \"next_flag\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_finally_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 4));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_dynamic_both_await_inner_finally_outer_catch_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 4));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_dynamic_both_await_inner_finally_outer_catch_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 4));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_then_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_branch_sel_0;",
		"xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"xl_generated_async_trace_event(\"await\", \"fail_then\")",
		"xl_generated_async_trace_event(\"await\", \"fail_else\")",
		"(mark = (mark + 40));",
		"(mark = (mark + 4));",
		"(mark = (mark + 3));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedWhileFinallyOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = 99);",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_while_finally_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedWhileFinallyOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_while_finally_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(value = (value + 1));",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_do_while_finally_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"(value = (value + 1));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_do_while_finally_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedDoWhileContinueOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_do_while_continue_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedDoWhileContinueOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_async_trace_event(\"await\", \"keep_going\")",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_do_while_continue_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedFullChainInnerCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 20));",
		"(mark = (mark + 2));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_full_chain_inner_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedFullChainInnerCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));",
		"((value + mark) + xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_full_chain_inner_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedFullChainOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = 99);",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_full_chain_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryNestedFullChainOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_nested_full_chain_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedInnerCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 30));",
		"(mark = (mark + 40));",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_inner_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedInnerCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 30));",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_inner_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = (mark + 30));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 2));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = (mark + 30));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedWhileOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"xl_generated_raise_error(xl_generated_error_from_text(",
		"(mark = (mark + 10));",
		"(mark = (mark + 30));",
		"XL_GeneratedError* e = xl_generated_capture_exception_error();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_while_outer_catch_then_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncTryTripleNestedWhileOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_while_cond_",
		"(mark = (mark + 10));",
		"(mark = (mark + 30));",
		"(mark = (mark + 40));",
		"(mark = (mark + 50));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\try_triple_nested_while_outer_catch_then_await_const_else_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncCallArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"int a = xl__add__arity2__a__int__b__int__ret__int(xl_async_return_await_",
		"int xl_async_ret_11_2 = xl__add__arity2__a__int__b__int__ret__int(a, xl_async_return_await_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\call_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncFunctionValueArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue xl_call_args = xvoCreateArray();",
		"xvoCreateInt((int64)(xl_async_boxed_value))",
		"int a = ({ xvalue xl_call_args = xvoCreateArray();",
		"int xl_async_ret_12_2 = ({ xvalue xl_call_args = xvoCreateArray();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\function_value_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncStringMethodArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_33 = ",
		"const char* a = xl_str_replace(\"a-b\", xl_async_return_await_6_33, \".\");",
		"int xl_async_ret_7_2 = (xl_str_index_of(a, \".\") + 14);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\string_method_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerMethodArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_33 = ",
		"int ok = ({ xvalue xl_set_item = ",
		"xvoCollExists((xvalue)(({ xvalue xl_set_ctor = xvoCreateColl();",
		"int xl_async_ret_7_2 = (ok ? 16 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_method_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerJoinArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_38 = ",
		"const char* text = xl_array_join((xvalue)(({ xvalue xl_arr_literal = xvoCreateArray();",
		"int xl_async_ret_7_2 = (xl_generated_text_equal(text, \"A|B\") ? 17 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_join_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerDictHasArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_47 = ",
		"int ok = ({ const char* xl_dict_key = xl_async_return_await_6_47;",
		"xvoTableExists((xvalue)(({ xvalue xl_dict_ctor = xvoCreateTable();",
		"int xl_async_ret_7_2 = (ok ? 18 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_dict_has_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerDictRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_50 = ",
		"int ok = ({ const char* xl_dict_key = xl_async_return_await_6_50;",
		"xl_dict_remove((xvalue)(({ xvalue xl_dict_ctor = xvoCreateTable();",
		"int xl_async_ret_7_2 = (ok ? 19 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_dict_remove_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerListRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_return_await_6_36 = ",
		"int ok = xl_list_remove((xvalue)(({ xvalue xl_list_ctor = xvoCreateList();",
		"int xl_async_ret_7_2 = (ok ? 20 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_list_remove_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerListJoinArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_42 = ",
		"const char* text = xl_list_join((xvalue)(({ xvalue xl_list_ctor = xvoCreateList();",
		"int xl_async_ret_7_2 = (xl_generated_text_equal(text, \"A|B\") ? 21 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_list_join_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerListAppendArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue values;",
		"int xl_async_return_await_7_16 = ",
		"xl_list_append((xvalue)(values), ({ int xl_async_boxed_value = xl_async_return_await_7_16; xvoCreateInt((int64)(xl_async_boxed_value)); }));",
		"int xl_async_ret_8_2 = (xl_core_len_list(values) + xvoListGetInt((xvalue)(values), (int64)(2)));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_list_append_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerDictAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue pairs;",
		"const char* xl_async_return_await_7_15 = ",
		"xvoTableSetValue((xvalue)(",
		"xvoTableGetText((xvalue)(pairs)",
		"int xl_async_ret_8_2 = "
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_dict_assign_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerArrayIndexAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue values;",
		"int xl_async_return_await_",
		"xvoArraySetValue((xvalue)(",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_array_index_assign_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerListIndexAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue values;",
		"int xl_async_return_await_",
		"xvoListSetValue((xvalue)(",
		"int xl_async_ret_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_list_index_assign_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerArrayAppendArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"xvalue values;",
		"int xl_async_return_await_7_16 = ",
		"xl_array_append((xvalue)(values), ({ int xl_async_boxed_value = xl_async_return_await_7_16; xvoCreateInt((int64)(xl_async_boxed_value)); }));",
		"int xl_async_ret_8_2 = (xl_core_len_array(values) + xvoArrayGetInt((xvalue)(values), (uint32)(2)));"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_array_append_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerArrayRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"int xl_async_return_await_6_35 = ",
		"int ok = xl_array_remove((xvalue)(({ xvalue xl_arr_literal = xvoCreateArray();",
		"int xl_async_ret_7_2 = (ok ? 22 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_array_remove_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncContainerSetRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"const char* xl_async_return_await_6_36 = ",
		"int ok = ({ xvalue xl_set_item = ({ const char* xl_async_boxed_value = xl_async_return_await_6_36;",
		"int xl_set_removed = xl_set_remove((xvalue)(({ xvalue xl_set_ctor = xvoCreateColl();",
		"int xl_async_ret_7_2 = (ok ? 23 : 0);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\container_set_remove_arg_await_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncMultiAwaitState(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"typedef struct xl__main__arity0__ret__future_x3Cint_x3E__async_linear_state {",
		"static void xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1(",
		"xFutureFinallyEngine(xl_async_future_1.handle, NULL, 0u, xl__main__arity0__ret__future_x3Cint_x3E__async_linear_resume_1, pState)",
		"int xl_async_return_await_7_13 = (int)(intptr_t)((pIn != NULL) ? pIn->pValue : NULL);",
		"int xl_async_ret_7_2 = (a + xl_async_return_await_7_13);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\run_multi_await.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenExternAtoiBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int atoi(const char* text);",
		"static int xl__parse_int__arity1__text__string__ret__int(",
		"return atoi(text);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\extern_atoi_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenFutureValueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_FutureInt pending",
		"XL_FutureInt named",
		"xl_future_int_await_state(\"pending\"",
		"xl_future_int_await_state(\"named\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\future_value_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenChannelBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_channel_create(2)",
		"xl_channel_send((xvalue)(",
		"xl_future_ptr_ready(xl_channel_recv((xvalue)(",
		"xl_channel_close((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\channel_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenChannelRecvUnpackBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_channel_create(2)",
		"xl_future_ptr_ready(xl_channel_recv((xvalue)(",
		"xl_unpack_",
		"xl_channel_close((xvalue)("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\async\\channel_recv_unpack_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTypeValueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_generated_type_of_value((xvalue)(c))",
		"xl_generated_text_equal(",
		"\"int\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\type_value_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenPublicTypesBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"\"future\"",
		"\"task\"",
		"\"channel\"",
		"\"error\"",
		"\"module\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\public_types_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAssertMessage(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\assert_message_basic.xl", "xl_generated_raise_error(Error(xl_assert_msg_"); }
static int __xlTestCgenNativeErrorBridge(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\native_error_bridge.xl", "xl_generated_capture_native_error()"); }
static int __xlTestCgenXlpNativeFuncs(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_probe(",
		"return pkg_clock_artifact_probe();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_add_one(int value);",
		"return pkg_clock_artifact_add_one(value);",
		"add_one__arity1__value__int__ret__int(41);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_add.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsString(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern const char* pkg_clock_artifact_text(",
		"return pkg_clock_artifact_text();",
		"text__arity0__ret__string()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsStringParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_text_len(const char* value);",
		"return pkg_clock_artifact_text_len(value);",
		"text_len__arity1__value__string__ret__int(\"probe-text\");"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_len.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsVoid(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern void pkg_clock_artifact_touch(",
		"pkg_clock_artifact_touch();",
		"return;",
		"touch__arity0__ret__void();"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_touch.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsMixedParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_text_plus_len(const char* value, int extra);",
		"return pkg_clock_artifact_text_plus_len(value, extra);",
		"text_plus_len__arity2__value__string__extra__int__ret__int(\"probe\", 5);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_plus_len.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsBool(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_is_probe_text(",
		"return pkg_clock_artifact_is_probe_text();",
		"is_probe_text__arity0__ret__bool()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsBoolParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern int pkg_clock_artifact_bool_score(int flag);",
		"return pkg_clock_artifact_bool_score(flag);",
		"bool_score__arity1__flag__bool__ret__int(true);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool_score.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsFloat(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern double pkg_clock_artifact_float_value(",
		"return pkg_clock_artifact_float_value();",
		"float_value__arity0__ret__float()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_float.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenXlpNativeFuncsFloatParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"extern double pkg_clock_artifact_add_half(double value);",
		"return pkg_clock_artifact_add_half(value);",
		"add_half__arity1__value__float__ret__float(3.0)"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_add_half.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenHelloWorld(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"#include \"xrt.h\"",
		"#include \"xlrt_generated.h\"",
		"xlrt_attach_generated_runtime()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\hello_world.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenHelloWorldIo(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\hello_world_io.xl", "xl_core_println_string(\"hello io\")"); }
static int __xlTestCgenTimeBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\time_basic.xl", "xl_time_to_string(now)"); }
static int __xlTestCgenPathBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\path_basic.xl", "xl_path_join(\"C:\\\\demo\", \"file.txt\")"); }
static int __xlTestCgenFsBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\fs_basic.xl", "xl_fs_write_all("); }
static int __xlTestCgenStrBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\str_basic.xl", "xl_str_contains(\"alphabet\", \"pha\")"); }
static int __xlTestCgenProcessBasic(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\process_basic.xl", "xl_process_capture(\"echo hello-process\")"); }
static int __xlTestCgenFsPlus(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_fs_is_file(",
		"xl_fs_is_dir(",
		"xl_fs_size(",
		"xl_fs_copy(",
		"xl_fs_move(",
		"xl_fs_delete("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\fs_plus.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenPathPlus(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_path_name_ext(",
		"xl_path_random("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\path_plus.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenStrPlus(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_str_lower(",
		"xl_str_upper(",
		"xl_str_replace("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\str_plus.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenRandBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_rand_range(",
		"xl_rand_int32()",
		"xl_rand_text("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\rand_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTimeText(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_time_now_text()",
		"xl_time_date_text()",
		"xl_time_time_text()",
		"xl_time_parse(",
		"xl_time_relative("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\time_text.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenHostBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_host_name()",
		"xl_host_ip()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\host_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenFmtBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_fmt_int_text(",
		"xl_fmt_float_text("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\fmt_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenEnvBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_env_get(",
		"xl_env_exists(",
		"xl_env_cwd()"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\env_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenDirBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_dir_list(",
		"xl_dir_files(",
		"xl_dir_dirs(",
		"xl_dir_tree("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\dir_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenProcessInBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_process_capture_in(",
		"xl_process_exec_in("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\process_in_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenStdlibPipeline(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"#include \"xrt.h\"",
		"xl_fs_mkdir_all(",
		"xl_fs_write_all(",
		"xl_process_capture(",
		"xl_str_contains("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\native\\stdlib_pipeline.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTask(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_task_race_int(",
		"xl_task_spawn_int(\"xl_mod_std_task__spawn\", xl__slow_add__arity0__ret__future_x3Cint_x3E())",
		"xl_future_int_await(\"xl_mod_std_task__race\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\task\\task_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTaskValueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_FutureInt pending = ",
		"XL_FutureInt waited = ",
		"xl_future_int_await(\"pending\"",
		"xl_future_int_await(\"waited\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\task\\task_value_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTaskDelay(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_task_delay(",
		"xTaskRunDelayed(NULL, 0u, iDelay, xl_generated_task_delay_engine",
		"xl_future_int_await(\"xl_mod_std_task__delay\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\task\\task_delay_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTaskError(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_task_spawn_error(",
		"xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL)",
		"xl_task_race_error(",
		"xl_future_error_await(\"xl_mod_std_task__race\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\task\\task_error_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenTaskAll(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_task_all_int(",
		"xFutureWhenAll(arrFuture, 2)",
		"xFutureThenEngine(pAll, NULL, 0u, xl_generated_future_all_pick_last, NULL)",
		"xFutureRelease(pAll);",
		"xl_future_int_await(\"xl_mod_std_task__all\""
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\task\\task_all_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenAsyncGeneratorBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__stream__arity0__ret__future_x3Cint_x3E__generator_state",
		"/* hybrid.await->yield state 1 */",
		"/* hybrid.group 0 awaitCount=1 */",
		"xl_future_int_await(\"",
		"__generator_next(",
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"case 3:",
		"xl_gen_resume_3:"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"stream\"", 1u },
		{ "\"resume_group_0\", \"stream\"", 1u },
		{ "\"group_1\", \"stream\"", 1u },
		{ "\"resume_group_1\", \"stream\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_for_in_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorWhileBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__countdown__arity1__start__int__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"while ( 1 )",
		"\"group_0\", \"countdown\"",
		"\"resume_group_0\", \"countdown\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"countdown\"", 1u },
		{ "\"resume_group_0\", \"countdown\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"countdown\"",
		"\"resume_group_0\", \"countdown\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_while_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorLoopControlBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__odd_before_break__arity1__stop__int__ret__future_x3Cint_x3E__generator_state",
		"/* hybrid.await->yield state 6 */",
		"/* hybrid.group 0 awaitCount=1 */",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"\"group_0\", \"odd_before_break\"",
		"\"resume_group_0\", \"odd_before_break\"",
		"goto xl_while_continue_",
		"goto xl_while_break_",
		"case 6:",
		"xl_gen_resume_6:"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"odd_before_break\"", 1u },
		{ "\"resume_group_0\", \"odd_before_break\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"odd_before_break\"",
		"\"resume_group_0\", \"odd_before_break\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_loop_control_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorResumeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__stream__arity0__ret__future_x3Cint_x3E__generator_state",
		"xl_gen_resume_value",
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"/* hybrid.group 0 awaitCount=1 */",
		"/* hybrid.await->yield state "
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"stream\"", 1u },
		{ "\"resume_group_0\", \"stream\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"resume_group_0\", \"stream\"",
		"\"group_0\", \"stream\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_resume_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorMultiAwaitGroupBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__stream__arity0__ret__future_x3Cint_x3E__generator_state",
		"/* hybrid.group 0 awaitCount=2 */",
		"/* hybrid.await->yield state ",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"case ",
		"xl_gen_resume_"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"stream\"", 1u },
		{ "\"resume_group_0\", \"stream\"", 1u },
		{ "\"group_1\", \"stream\"", 1u },
		{ "\"resume_group_1\", \"stream\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_multi_await_group_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorTryCatchBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__guarded__arity0__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_raise_error((XL_GeneratedError*)(Error(\"boom\"",
		"\"group_0\", \"guarded\"",
		"\"resume_group_0\", \"guarded\"",
		"\"group_1\", \"guarded\"",
		"\"resume_group_1\", \"guarded\"",
		"\"group_2\", \"guarded\"",
		"\"resume_group_2\", \"guarded\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"guarded\"", 1u },
		{ "\"resume_group_0\", \"guarded\"", 1u },
		{ "\"group_1\", \"guarded\"", 1u },
		{ "\"resume_group_1\", \"guarded\"", 1u },
		{ "\"group_2\", \"guarded\"", 1u },
		{ "\"resume_group_2\", \"guarded\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"guarded\"",
		"\"resume_group_0\", \"guarded\"",
		"\"group_1\", \"guarded\"",
		"\"resume_group_1\", \"guarded\"",
		"\"group_2\", \"guarded\"",
		"\"resume_group_2\", \"guarded\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_catch_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorTryCatchConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__guarded__arity0__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_raise_error((XL_GeneratedError*)(Error(\"boom\"",
		"\"group_0\", \"guarded\"",
		"\"resume_group_0\", \"guarded\"",
		"\"group_1\", \"guarded\"",
		"\"resume_group_1\", \"guarded\"",
		"\"group_2\", \"guarded\"",
		"\"resume_group_2\", \"guarded\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"guarded\"", 1u },
		{ "\"resume_group_0\", \"guarded\"", 1u },
		{ "\"group_1\", \"guarded\"", 1u },
		{ "\"resume_group_1\", \"guarded\"", 1u },
		{ "\"group_2\", \"guarded\"", 1u },
		{ "\"resume_group_2\", \"guarded\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"guarded\"",
		"\"resume_group_0\", \"guarded\"",
		"\"group_1\", \"guarded\"",
		"\"resume_group_1\", \"guarded\"",
		"\"group_2\", \"guarded\"",
		"\"resume_group_2\", \"guarded\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_catch_const_else_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorTryFinallyBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__staged__arity0__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_exception_frame_restore(",
		"\"group_0\", \"staged\"",
		"\"resume_group_0\", \"staged\"",
		"\"group_1\", \"staged\"",
		"\"resume_group_1\", \"staged\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"staged\"", 1u },
		{ "\"resume_group_0\", \"staged\"", 1u },
		{ "\"group_1\", \"staged\"", 1u },
		{ "\"resume_group_1\", \"staged\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"staged\"",
		"\"resume_group_0\", \"staged\"",
		"\"group_1\", \"staged\"",
		"\"resume_group_1\", \"staged\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_finally_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorTryFinallyConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__staged__arity0__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_exception_frame_restore(",
		"\"group_0\", \"staged\"",
		"\"resume_group_0\", \"staged\"",
		"\"group_1\", \"staged\"",
		"\"resume_group_1\", \"staged\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"staged\"", 1u },
		{ "\"resume_group_0\", \"staged\"", 1u },
		{ "\"group_1\", \"staged\"", 1u },
		{ "\"resume_group_1\", \"staged\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"staged\"",
		"\"resume_group_0\", \"staged\"",
		"\"group_1\", \"staged\"",
		"\"resume_group_1\", \"staged\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_finally_const_else_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorNestedTryFlowBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__stream__arity1__stop__int__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_raise_error((XL_GeneratedError*)(Error(\"skip\"",
		"xl_generated_exception_frame_restore(",
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"\"group_2\", \"stream\"",
		"\"resume_group_2\", \"stream\"",
		"case 8:",
		"xl_gen_resume_8:"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"stream\"", 1u },
		{ "\"resume_group_0\", \"stream\"", 1u },
		{ "\"group_1\", \"stream\"", 1u },
		{ "\"resume_group_1\", \"stream\"", 1u },
		{ "\"group_2\", \"stream\"", 1u },
		{ "\"resume_group_2\", \"stream\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"\"group_2\", \"stream\"",
		"\"resume_group_2\", \"stream\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_nested_try_flow_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder)
	);
}
static int __xlTestCgenAsyncGeneratorNestedTryFlowConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__stream__arity1__stop__int__ret__future_x3Cint_x3E__generator_state",
		"xl_future_int_await(\"xl_mod_std_task__delay\"",
		"xl_generated_raise_error((XL_GeneratedError*)(Error(\"skip\"",
		"xl_generated_exception_frame_restore(",
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"\"group_2\", \"stream\"",
		"\"resume_group_2\", \"stream\""
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "\"group_0\", \"stream\"", 1u },
		{ "\"resume_group_0\", \"stream\"", 1u },
		{ "\"group_1\", \"stream\"", 1u },
		{ "\"resume_group_1\", \"stream\"", 1u },
		{ "\"group_2\", \"stream\"", 1u },
		{ "\"resume_group_2\", \"stream\"", 1u }
	};
	static const char* const aOrder[] = {
		"\"group_0\", \"stream\"",
		"\"resume_group_0\", \"stream\"",
		"\"group_1\", \"stream\"",
		"\"resume_group_1\", \"stream\"",
		"\"group_2\", \"stream\"",
		"\"resume_group_2\", \"stream\""
	};

	return __xlRunCgenCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_nested_try_flow_const_else_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u
	);
}
static int __xlTestCgenGeneratorResumeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"__generator_next(",
		"xl_gen_resume_value",
		"xvoArrayAppendValue("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_resume_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorRun(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\generator_run.xl", "__generator_next"); }
static int __xlTestCgenGeneratorLazyThrow(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\generator_lazy_throw_basic.xl", "__generator_next"); }
static int __xlTestCgenGeneratorVarForInBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"__generator_state g = ",
		"__generator_next(",
		"(sum += v);"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_var_for_in_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorParamResumeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedGeneratorValue g",
		"xl_generated_generator_step(",
		"xl_generated_generator_make("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_param_resume_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorReturnWrapperBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl__wrap__arity0__ret__generator(",
		"__generator_state xl_ret_",
		"__generator_next("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_return_wrapper_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorReturnAbstractBranchResumeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedGeneratorValue xl__choose",
		"xl_generated_generator_clone(xl__choose",
		"xl_generated_generator_step("
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_return_abstract_branch_resume_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorReturnAbstractBranchForInBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedGeneratorValue xl_gen_",
		"xl_generated_generator_step(&xl_gen_",
		"xvoUnref(xl_gen_value_"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_return_abstract_branch_for_in_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorStateCapture(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"__generator_state",
		"int seed;",
		"__generator_next"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_state_capture_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorWhileBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"while ( 1 )",
		"int current;",
		"case 2:",
		"goto xl_gen_resume_2;"
	};
	static const char* const aForbidden[] = {
		"case 1:",
		"goto xl_gen_resume_1;"
	};

	return __xlRunCgenCaseAllWithout(
		pCtx,
		"examples\\full\\generator_while_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aForbidden,
		XL_COUNTOF(aForbidden)
	);
}
static int __xlTestCgenGeneratorLoopControlBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"goto xl_while_continue_",
		"goto xl_while_break_",
		"case 5:",
		"goto xl_gen_resume_5;"
	};
	static const char* const aForbidden[] = {
		"case 1:",
		"goto xl_gen_resume_1;"
	};

	return __xlRunCgenCaseAllWithout(
		pCtx,
		"examples\\full\\generator_loop_control_basic.xl",
		aNeedles,
		XL_COUNTOF(aNeedles),
		aForbidden,
		XL_COUNTOF(aForbidden)
	);
}
static int __xlTestCgenGeneratorTryCatchBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedError* e;",
		"XL_GeneratedExceptionFrame xl_try_frame_",
		"xl_generated_text_equal(xl_generated_error_message(e), \"boom\")",
		"case 1:",
		"goto xl_gen_resume_1;",
		"case 4:",
		"goto xl_gen_resume_4;",
		"case 6:",
		"goto xl_gen_resume_6;"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_try_catch_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorTryFinallyBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedExceptionFrame xl_try_frame_",
		"&pGen->xl_try_frame_",
		"xl_try_finally_",
		"case 1:",
		"goto xl_gen_resume_1;",
		"case 3:",
		"goto xl_gen_resume_3;"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_try_finally_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenGeneratorNestedTryFlowBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"XL_GeneratedError* e;",
		"XL_GeneratedExceptionFrame xl_try_frame_",
		"xl_try_finally_",
		"case 4:",
		"goto xl_gen_resume_4;",
		"case 6:",
		"goto xl_gen_resume_6;",
		"case 8:",
		"goto xl_gen_resume_8;"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\full\\generator_nested_try_flow_basic.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenLoopJumpLevels(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\loop_jump_levels.xl", "goto xl_for_break_"); }
static int __xlTestRunBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\run_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunTryCatch(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\try_catch_basic.xl", 1, 7, NULL, NULL); }
static int __xlTestRunTryNestedRethrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\try_nested_rethrow.xl", 1, 23, NULL, NULL); }
static int __xlTestRunErrorFields(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\error_fields_basic.xl", 1, 13, NULL, NULL); }
static int __xlTestRunNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\named_args_default.xl", 1, 129, NULL, NULL); }
static int __xlTestRunThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\throw_basic.xl", 0, 0, NULL, "exception.text=boom"); }
static int __xlTestRunAssertMessage(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assert_message_basic.xl", 0, 0, NULL, "exception.text=assert boom"); }
static int __xlTestRunNativeErrorBridge(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\native_error_bridge.xl", 1, 31, NULL, NULL); }
static int __xlTestRunTryFinally(XL_TestContext* pCtx)
{
	char sFilePath[XL_PATH_MAX_LEN];
	char* sText;
	int iRc;

	__xlBuildProjectPath(pCtx, sFilePath, sizeof(sFilePath), "release\\tmp\\try_finally_basic_out\\note.txt");
	remove(sFilePath);

	iRc = __xlRunProgramCase(pCtx, "examples\\full\\try_finally_basic.xl", 1, 0, NULL, NULL);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	sText = NULL;
	iRc = __xlReadFileText(sFilePath, &sText);
	if ( iRc == XL_RC_OK ) {
		if ( strcmp((sText != NULL) ? sText : "", "body|finally") != 0 ) {
			iRc = XL_RC_FAIL;
		}
	}
free(sText);
return iRc;
}
static int __xlTestRunTryFinallyEscapeBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\try_finally_escape_basic.xl", 1, 49, NULL, NULL); }
static int __xlTestRunTryCatchFinally(XL_TestContext* pCtx)
{
	char sFilePath[XL_PATH_MAX_LEN];
	char* sText;
	int iRc;

	__xlBuildProjectPath(pCtx, sFilePath, sizeof(sFilePath), "release\\tmp\\try_catch_finally\\note.txt");
	remove(sFilePath);

	iRc = __xlRunProgramCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, 0, NULL, NULL);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	sText = NULL;
	iRc = __xlReadFileText(sFilePath, &sText);
	if ( iRc == XL_RC_OK ) {
		if ( strcmp((sText != NULL) ? sText : "", "IOError: boom|finally") != 0 ) {
			iRc = XL_RC_FAIL;
		}
	}
	free(sText);
	return iRc;
}
static int __xlTestRunModule(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_math.xl", 1, 42, NULL, NULL); }
static int __xlTestRunExportTypeMethod(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_text_ext_method.xl", 1, 77, NULL, NULL); }
static int __xlTestRunExportGenericTypeMethod(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_list_string_ext_method.xl", 1, 79, NULL, NULL); }
static int __xlTestRunModuleInitThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_init_throw_dep.xl", 0, 0, NULL, "exception.text=module init boom"); }
static int __xlTestRunXlpImport(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp\\use_pkg_math.xl", 1, 42, NULL, NULL); }
static int __xlTestRunXlpNative(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_native\\use_pkg_clock.xl", 1, 86, "artifact.shared.", NULL); }
static int __xlTestRunXlpNativeFallbackShared(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_point.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncs(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe.xl", 1, 123, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_add.xl", 1, 42, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsString(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsStringParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsVoid(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_touch.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsMixedParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_plus_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsBool(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsBoolParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool_score.xl", 1, 7, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsFloat(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_float.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsFloatParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared."
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_add_half.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStatic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_static\\use_pkg_clock.xl", 1, 86, "artifact.static.link=", NULL); }
static int __xlTestRunXlpStaticFuncs(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe.xl", 1, 123, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_add.xl", 1, 42, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsString(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_text.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsStringParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_text_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsVoid(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_touch.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsMixedParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_text_plus_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsBool(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_bool.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsBoolParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_bool_score.xl", 1, 7, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsFloat(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_float.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpStaticFuncsFloatParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.static.link=",
		"artifact.static.object="
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_static\\use_pkg_probe_add_half.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpSource(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_source\\use_pkg_math.xl", 1, 42, NULL, NULL); }
static int __xlTestRunXlpTransitive(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_transitive\\use_pkg_app.xl", 1, 42, NULL, NULL); }
static int __xlTestRunXlpVersions(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_versions\\use_dual_versions.xl", 1, 30, NULL, NULL); }
static int __xlTestRunOverloadAbi(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\overload_runtime.xl", 1, 42, NULL, NULL); }
static int __xlTestRunNative(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\use_std_core.xl", 1, 5, NULL, NULL); }
static int __xlTestRunCoreLenBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\core_len_basic.xl", 1, 14, NULL, NULL); }
static int __xlTestRunImportAlias(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\import_alias_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunImportMulti(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\import_multi_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunImportString(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\import_string_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunExportFunc(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_math_export_func.xl", 1, 42, NULL, NULL); }
static int __xlTestRunExportValues(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_math_export_values.xl", 1, 42, NULL, NULL); }
static int __xlTestRunTypedDecl(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\typed_decl_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunExactNumericTypes(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\exact_numeric_types_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAutoDecl(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\auto_decl_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunTypeDefBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\typedef_basic.xl", 1, 11, NULL, NULL); }
static int __xlTestRunTypeCtorBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\type_ctor_basic.xl", 1, 64, NULL, NULL); }
static int __xlTestRunPointBasic(XL_TestContext* pCtx)
{
	return __xlRunProgramCase(pCtx, "examples\\full\\point_basic.xl", 1, 19, "execute", NULL);
}
static int __xlTestRunAssignBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAssignPlusBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_plus_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAssignMinusBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_minus_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAssignMulBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_mul_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAssignDivBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_div_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunAssignModBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_mod_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunPowerBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\power_basic.xl", 1, 32, NULL, NULL); }
static int __xlTestRunAssignPowBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\assign_pow_basic.xl", 1, 32, NULL, NULL); }
static int __xlTestRunCoalesceBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\coalesce_basic.xl", 1, 9, NULL, NULL); }
static int __xlTestRunNullAssignBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\null_assign_basic.xl", 1, 7, NULL, NULL); }
static int __xlTestRunTernaryBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\ternary_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunApproxBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\approx_basic.xl", 1, 1, NULL, NULL); }
static int __xlTestRunIfInlineBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\if_inline_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunIfInlineElseBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\if_inline_else_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunLogicBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\logic_basic.xl", 1, 1, NULL, NULL); }
static int __xlTestRunBytesAliasString(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\bytes_alias_string.xl", 1, 7, NULL, NULL); }
static int __xlTestRunTemplateBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\template_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunSafeMemberBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\safe_member_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunLeadingBlankLines(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\leading_blank_lines_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunVarBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\var_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunArrayBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\array_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunListBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\list_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunSetBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\set_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunDictBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\dict_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunIndexAssignBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\index_assign_basic.xl", 1, 97, NULL, NULL); }
static int __xlTestRunForInContainers(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\for_in_containers_basic.xl", 1, 65, NULL, NULL); }
static int __xlTestRunContainerMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\container_methods_basic.xl", 1, 12, NULL, NULL); }
static int __xlTestRunContainerAppendBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\container_append_basic.xl", 1, 14, NULL, NULL); }
static int __xlTestRunContainerClearBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\container_clear_basic.xl", 1, 47, NULL, NULL); }
static int __xlTestRunContainerRemoveBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\container_remove_basic.xl", 1, 113, NULL, NULL); }
static int __xlTestRunVarArgArrayBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\vararg_array_basic.xl", 1, 35, NULL, NULL); }
static int __xlTestRunVarArgDictBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\vararg_dict_basic.xl", 1, 35, NULL, NULL); }
static int __xlTestRunMultiReturnBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\multi_return_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunMultiReturnCallExpandBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\multi_return_call_expand_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunFunctionValueBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\function_value_basic.xl", 1, 42, "execute", NULL); }
static int __xlTestRunFunctionValueFlowBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\function_value_flow_basic.xl", 1, 42, "execute", NULL); }
static int __xlTestRunFunctionValueContainerBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\function_value_container_basic.xl", 1, 42, "execute", NULL); }
static int __xlTestRunFunctionValueParenCallBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\function_value_paren_call_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunFunctionValueOverloadBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\function_value_overload_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunBareContainerLiterals(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\bare_container_literals_basic.xl", 1, 255, NULL, NULL); }
static int __xlTestRunTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunVarMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\var_methods_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunVarTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\var_type_methods_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunVarContainerTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\var_container_type_methods_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\property_length_basic.xl", 1, 13, NULL, NULL); }
static int __xlTestRunStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, 88, NULL, NULL); }
static int __xlTestRunStringSplitBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\string_split_basic.xl", 1, 9, NULL, NULL); }
static int __xlTestRunWhileBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\while_basic.xl", 1, 7, NULL, NULL); }
static int __xlTestRunDoWhileBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\do_while_basic.xl", 1, 7, NULL, NULL); }
static int __xlTestRunHelloWorld(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\hello_world.xl", 1, 0, NULL, NULL); }
static int __xlTestRunHelloWorldIo(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\hello_world_io.xl", 1, 0, NULL, NULL); }
static int __xlTestRunTimeBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\time_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunPathBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\path_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunFsBasic(XL_TestContext* pCtx)
{
	char sFilePath[XL_PATH_MAX_LEN];
	char* sText;
	int iRc;

	__xlBuildProjectPath(pCtx, sFilePath, sizeof(sFilePath), "release\\tmp\\fs_basic\\note.txt");
	remove(sFilePath);

	iRc = __xlRunProgramCase(pCtx, "examples\\native\\fs_basic.xl", 1, 0, NULL, NULL);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	sText = NULL;
	iRc = __xlReadFileText(sFilePath, &sText);
	if ( iRc == XL_RC_OK ) {
		if ( strcmp((sText != NULL) ? sText : "", "alpha\nbeta") != 0 ) {
			iRc = XL_RC_FAIL;
		}
	}
	free(sText);
	return iRc;
}
static int __xlTestRunStrBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\str_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunProcessBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\process_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunFsPlus(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\fs_plus.xl", 1, 0, NULL, NULL); }
static int __xlTestRunPathPlus(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\path_plus.xl", 1, 0, NULL, NULL); }
static int __xlTestRunStrPlus(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\str_plus.xl", 1, 0, NULL, NULL); }
static int __xlTestRunRandBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\rand_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunTimeText(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\time_text.xl", 1, 0, NULL, NULL); }
static int __xlTestRunHostBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\host_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunFmtBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\fmt_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunEnvBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\env_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunDirBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\dir_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunProcessInBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\process_in_basic.xl", 1, 0, NULL, NULL); }
static int __xlTestRunStdlibPipeline(XL_TestContext* pCtx)
{
	char sFilePath[XL_PATH_MAX_LEN];
	char* sText;
	int iRc;

	__xlBuildProjectPath(pCtx, sFilePath, sizeof(sFilePath), "release\\tmp\\stdlib_pipeline\\report.txt");
	remove(sFilePath);

	iRc = __xlRunProgramCase(pCtx, "examples\\native\\stdlib_pipeline.xl", 1, 0, NULL, NULL);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	sText = NULL;
	iRc = __xlReadFileText(sFilePath, &sText);
	if ( iRc == XL_RC_OK ) {
		if ( !__xlTraceContains(sText, "stamp=") ) {
			iRc = XL_RC_FAIL;
		}
	}
	free(sText);
	return iRc;
}
static int __xlTestRunAsyncMain(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\run_main.xl",
		1,
		9,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncBindAwaitReturnBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\bind_await_return_basic.xl",
		1,
		23,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncBindExprAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\bind_expr_await_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\assign_arg_await_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncAssignStringArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_text",
		"async.resume=next_text",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\assign_string_arg_await_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncAssignMultiAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=first_value",
		"async.resume=first_value",
		"async.await=second_value",
		"async.resume=second_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=first_value", 1u },
		{ "async.resume=first_value", 1u },
		{ "async.await=second_value", 1u },
		{ "async.resume=second_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\assign_multi_await_basic.xl",
		1,
		16,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncStringExprAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_text",
		"async.resume=next_text",
		"async.return=main"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\string_expr_await_basic.xl",
		1,
		14,
		aNeedles,
		XL_COUNTOF(aNeedles),
		NULL,
		0u,
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfBodyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_body_prelude_then_await_basic.xl",
		1,
		42,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfDynamicThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_dynamic_then_await_basic.xl",
		1,
		42,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfDynamicElseSyncBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 0u },
		{ "async.resume=next_value", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_dynamic_else_sync_basic.xl",
		1,
		11,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfDynamicElseAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_dynamic_else_await_basic.xl",
		1,
		42,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_dynamic_both_await_then_basic.xl",
		1,
		42,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_dynamic_both_await_else_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_nested_dynamic_both_await_prelude_then_basic.xl",
		1,
		132,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_nested_dynamic_both_await_prelude_else_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_nested_dynamic_both_await_const_else_then_basic.xl",
		1,
		132,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_nested_dynamic_both_await_const_else_else_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfBodyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_body_prelude_then_await_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_then_await_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicElseSyncBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 0u },
		{ "async.resume=next_value", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_else_sync_basic.xl",
		1,
		14,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicElseAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_else_await_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_both_await_then_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_both_await_else_basic.xl",
		1,
		48,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_both_await_prelude_then_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_dynamic_both_await_prelude_else_basic.xl",
		1,
		138,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_nested_dynamic_both_await_prelude_then_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_nested_dynamic_both_await_prelude_else_basic.xl",
		1,
		138,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_nested_dynamic_both_await_const_else_then_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_if_nested_dynamic_both_await_const_else_else_basic.xl",
		1,
		138,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_cond_await_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_cond_then_await_basic.xl",
		1,
		14,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncIfCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\if_cond_container_then_await_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_await_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_container_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondBreakBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_break_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondContinueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 4u },
		{ "async.resume=keep_going", 4u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_continue_basic.xl",
		1,
		9,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_break_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncWhileCondContinueThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 4u },
		{ "async.resume=keep_going", 4u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\while_cond_continue_then_await_basic.xl",
		1,
		8,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_await_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondContainerThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_container_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondBreakBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.return=main"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_break_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		NULL,
		0u,
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondContinueBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_continue_basic.xl",
		1,
		8,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_break_then_await_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncDoWhileCondContinueThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\do_while_cond_continue_then_await_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_await_basic.xl",
		1,
		17,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_await_const_else_basic.xl",
		1,
		67,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_await_basic.xl",
		1,
		17,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_await_const_else_basic.xl",
		1,
		67,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_then_await_basic.xl",
		1,
		22,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_then_await_const_else_basic.xl",
		1,
		72,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_then_await_basic.xl",
		1,
		22,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_then_await_const_else_basic.xl",
		1,
		72,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyTailThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_tail_then_await_basic.xl",
		1,
		25,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyTailThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_tail_then_await_const_else_basic.xl",
		1,
		115,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyTailThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_tail_then_await_basic.xl",
		1,
		23,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyTailThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_tail_then_await_const_else_basic.xl",
		1,
		113,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyIfThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_if_then_await_basic.xl",
		1,
		21,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyIfThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_if_then_await_const_else_basic.xl",
		1,
		71,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyWhileThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_while_then_await_basic.xl",
		1,
		24,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyWhileThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_while_then_await_const_else_basic.xl",
		1,
		114,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_prelude_then_await_basic.xl",
		1,
		46,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyPreludeThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_value", 1u },
		{ "async.resume=next_value", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_prelude_then_await_const_else_basic.xl",
		1,
		136,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyPreludeThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_prelude_then_await_basic.xl",
		1,
		25,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyPreludeThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_prelude_then_await_const_else_basic.xl",
		1,
		115,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_then_basic.xl",
		1,
		88,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_else_basic.xl",
		1,
		90,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_success_then_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_success_else_basic.xl",
		1,
		48,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_prelude_then_basic.xl",
		1,
		128,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_dynamic_both_await_prelude_else_basic.xl",
		1,
		130,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_prelude_then_basic.xl",
		1,
		128,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_prelude_else_basic.xl",
		1,
		130,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_success_then_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_success_else_basic.xl",
		1,
		138,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_then",
		"async.resume=next_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 1u },
		{ "async.resume=next_then", 1u },
		{ "async.await=next_else", 0u },
		{ "async.resume=next_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_success_then_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_else",
		"async.resume=next_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_then", 0u },
		{ "async.resume=next_then", 0u },
		{ "async.await=next_else", 1u },
		{ "async.resume=next_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_success_else_basic.xl",
		1,
		138,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_then_basic.xl",
		1,
		128,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_catch_finally_if_nested_dynamic_both_await_const_else_else_basic.xl",
		1,
		130,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyDoWhileThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_do_while_then_await_basic.xl",
		1,
		31,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyDoWhileThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_do_while_then_await_const_else_basic.xl",
		1,
		121,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyDoWhileBreakThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_do_while_break_then_await_basic.xl",
		1,
		31,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryFinallyDoWhileBreakThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_finally_do_while_break_then_await_const_else_basic.xl",
		1,
		121,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedInnerCatchThenOuterFinallyBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_inner_catch_then_outer_finally_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedInnerCatchThenOuterFinallyConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_inner_catch_then_outer_finally_const_else_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedInnerFinallyOuterCatchBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_inner_finally_outer_catch_basic.xl",
		1,
		25,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedInnerFinallyOuterCatchConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_inner_finally_outer_catch_const_else_basic.xl",
		1,
		115,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfFinallyThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_finally_then_await_basic.xl",
		1,
		41,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfFinallyThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_flag",
		"async.resume=next_flag",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=next_flag", 1u },
		{ "async.resume=next_flag", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_finally_then_await_const_else_basic.xl",
		1,
		131,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_dynamic_both_await_inner_finally_outer_catch_then_basic.xl",
		1,
		92,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_dynamic_both_await_inner_finally_outer_catch_else_basic.xl",
		1,
		94,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchThenBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_then",
		"async.resume=fail_then",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 1u },
		{ "async.resume=fail_then", 1u },
		{ "async.await=fail_else", 0u },
		{ "async.resume=fail_else", 0u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_then_basic.xl",
		1,
		132,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail_else",
		"async.resume=fail_else",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail_then", 0u },
		{ "async.resume=fail_then", 0u },
		{ "async.await=fail_else", 1u },
		{ "async.resume=fail_else", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_else_basic.xl",
		1,
		134,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedWhileFinallyOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_while_finally_outer_catch_then_await_basic.xl",
		1,
		26,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedWhileFinallyOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_while_finally_outer_catch_then_await_const_else_basic.xl",
		1,
		116,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_do_while_finally_outer_catch_then_await_basic.xl",
		1,
		26,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_do_while_finally_outer_catch_then_await_const_else_basic.xl",
		1,
		116,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedDoWhileContinueOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_do_while_continue_outer_catch_then_await_basic.xl",
		1,
		28,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedDoWhileContinueOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 3u },
		{ "async.resume=keep_going", 3u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_do_while_continue_outer_catch_then_await_const_else_basic.xl",
		1,
		118,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedFullChainInnerCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_full_chain_inner_catch_then_await_basic.xl",
		1,
		45,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedFullChainInnerCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_full_chain_inner_catch_then_await_const_else_basic.xl",
		1,
		135,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedFullChainOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_full_chain_outer_catch_then_await_basic.xl",
		1,
		25,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryNestedFullChainOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_nested_full_chain_outer_catch_then_await_const_else_basic.xl",
		1,
		115,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedInnerCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_inner_catch_then_await_basic.xl",
		1,
		115,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedInnerCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_inner_catch_then_await_const_else_basic.xl",
		1,
		205,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_outer_catch_then_await_basic.xl",
		1,
		55,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=fail",
		"async.resume=fail",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=fail", 1u },
		{ "async.resume=fail", 1u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_outer_catch_then_await_const_else_basic.xl",
		1,
		145,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedWhileOuterCatchThenAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_while_outer_catch_then_await_basic.xl",
		1,
		56,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncTryTripleNestedWhileOuterCatchThenAwaitConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=keep_going",
		"async.resume=keep_going",
		"async.await=bonus",
		"async.resume=bonus",
		"async.return=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.await=keep_going", 2u },
		{ "async.resume=keep_going", 2u },
		{ "async.await=bonus", 1u },
		{ "async.resume=bonus", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\try_triple_nested_while_outer_catch_then_await_const_else_basic.xl",
		1,
		146,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncCallArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main",
		"async.state_3=main",
		"async.state_4=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u },
		{ "async.state_4=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\call_arg_await_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncFunctionValueArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main",
		"async.state_4=main",
		"async.state_5=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u },
		{ "async.state_4=main", 1u },
		{ "async.state_5=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\function_value_arg_await_basic.xl",
		1,
		14,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncStringMethodArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=old_text",
		"async.resume=old_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\string_method_arg_await_basic.xl",
		1,
		15,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerMethodArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=key_text",
		"async.resume=key_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u },
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_method_arg_await_basic.xl",
		1,
		16,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerJoinArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=sep_text",
		"async.resume=sep_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_join_arg_await_basic.xl",
		1,
		17,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerDictHasArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=key_text",
		"async.resume=key_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_dict_has_arg_await_basic.xl",
		1,
		18,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerDictRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=key_text",
		"async.resume=key_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_dict_remove_arg_await_basic.xl",
		1,
		19,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerListRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=index_value",
		"async.resume=index_value",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_list_remove_arg_await_basic.xl",
		1,
		20,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerListJoinArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=sep_text",
		"async.resume=sep_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_list_join_arg_await_basic.xl",
		1,
		21,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerArrayAppendArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_array_append_arg_await_basic.xl",
		1,
		9,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerListAppendArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_list_append_arg_await_basic.xl",
		1,
		9,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerDictAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_text",
		"async.resume=next_text",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_dict_assign_arg_await_basic.xl",
		1,
		42,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerArrayIndexAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_array_index_assign_arg_await_basic.xl",
		1,
		13,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerListIndexAssignArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=next_value",
		"async.resume=next_value",
		"async.return=main",
		"async.state_2=main",
		"async.state_3=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_list_index_assign_arg_await_basic.xl",
		1,
		13,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerArrayRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=index_value",
		"async.resume=index_value",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_array_remove_arg_await_basic.xl",
		1,
		22,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncContainerSetRemoveArgAwaitBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=key_text",
		"async.resume=key_text",
		"async.return=main",
		"async.state_1=main",
		"async.state_2=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u }
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\container_set_remove_arg_await_basic.xl",
		1,
		23,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		NULL,
		0u,
		NULL
	);
}
static int __xlTestRunAsyncMultiAwait(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"async.await=get_value",
		"async.resume=get_value",
		"async.state_1=main",
		"async.state_2=main",
		"async.state_3=main",
		"async.state_4=main"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "async.state_1=main", 1u },
		{ "async.state_2=main", 1u },
		{ "async.state_3=main", 1u },
		{ "async.state_4=main", 1u }
	};
	static const char* const aOrder[] = {
		"async.state_1=main",
		"async.state_2=main",
		"async.state_3=main",
		"async.state_4=main"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\run_multi_await.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncThrowMain(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_throw_main.xl", 0, 0, NULL, "exception.text=async boom"); }
static int __xlTestRunAsyncTryCatchThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_try_catch_throw.xl", 1, 17, "async.await=fail", NULL); }
static int __xlTestRunAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_bool_main.xl", 1, 7, "async.await=get_flag", NULL); }
static int __xlTestRunFutureValueBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\future_value_basic.xl", 1, 14, "async.await=pending", NULL); }
static int __xlTestRunChannelBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\channel_basic.xl", 1, 60, NULL, NULL); }
static int __xlTestRunChannelRecvUnpackBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\channel_recv_unpack_basic.xl", 1, 30, NULL, NULL); }
static int __xlTestRunAsyncGeneratorBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=stream", 1u },
		{ "generator.resume_group_0=stream", 1u },
		{ "generator.group_1=stream", 1u },
		{ "generator.resume_group_1=stream", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_for_in_basic.xl",
		1,
		5,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorWhileBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=countdown",
		"generator.resume_group_0=countdown"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=countdown", 3u },
		{ "generator.resume_group_0=countdown", 3u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=countdown",
		"generator.resume_group_0=countdown",
		"generator.group_0=countdown",
		"generator.resume_group_0=countdown",
		"generator.group_0=countdown",
		"generator.resume_group_0=countdown"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_while_basic.xl",
		1,
		6,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorLoopControlBasic(XL_TestContext* pCtx)
{
	static const char* aNeedles[] = {
		"generator.group_0=odd_before_break",
		"generator.resume_group_0=odd_before_break"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=odd_before_break", 3u },
		{ "generator.resume_group_0=odd_before_break", 3u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=odd_before_break",
		"generator.resume_group_0=odd_before_break",
		"generator.group_0=odd_before_break",
		"generator.resume_group_0=odd_before_break",
		"generator.group_0=odd_before_break",
		"generator.resume_group_0=odd_before_break"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_loop_control_basic.xl",
		1,
		8,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorResumeBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=stream", 1u },
		{ "generator.resume_group_0=stream", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_resume_basic.xl",
		1,
		9,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorMultiAwaitGroupBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=stream", 1u },
		{ "generator.resume_group_0=stream", 1u },
		{ "generator.group_1=stream", 1u },
		{ "generator.resume_group_1=stream", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_multi_await_group_basic.xl",
		1,
		12,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorTryCatchBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=guarded",
		"generator.resume_group_0=guarded",
		"async.throw=guarded",
		"generator.group_1=guarded",
		"generator.resume_group_1=guarded",
		"generator.group_2=guarded",
		"generator.resume_group_2=guarded"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=guarded", 1u },
		{ "generator.resume_group_0=guarded", 1u },
		{ "async.throw=guarded", 1u },
		{ "generator.group_1=guarded", 1u },
		{ "generator.resume_group_1=guarded", 1u },
		{ "generator.group_2=guarded", 1u },
		{ "generator.resume_group_2=guarded", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=guarded",
		"generator.resume_group_0=guarded",
		"async.throw=guarded",
		"generator.group_1=guarded",
		"generator.resume_group_1=guarded",
		"generator.group_2=guarded",
		"generator.resume_group_2=guarded"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_catch_basic.xl",
		1,
		7,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorTryCatchConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=guarded",
		"generator.resume_group_0=guarded",
		"async.throw=guarded",
		"generator.group_1=guarded",
		"generator.resume_group_1=guarded",
		"generator.group_2=guarded",
		"generator.resume_group_2=guarded"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=guarded", 1u },
		{ "generator.resume_group_0=guarded", 1u },
		{ "async.throw=guarded", 1u },
		{ "generator.group_1=guarded", 1u },
		{ "generator.resume_group_1=guarded", 1u },
		{ "generator.group_2=guarded", 1u },
		{ "generator.resume_group_2=guarded", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=guarded",
		"generator.resume_group_0=guarded",
		"async.throw=guarded",
		"generator.group_1=guarded",
		"generator.resume_group_1=guarded",
		"generator.group_2=guarded",
		"generator.resume_group_2=guarded"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_catch_const_else_basic.xl",
		1,
		37,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorTryFinallyBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=staged",
		"generator.resume_group_0=staged",
		"generator.group_1=staged",
		"generator.resume_group_1=staged"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=staged", 1u },
		{ "generator.resume_group_0=staged", 1u },
		{ "generator.group_1=staged", 1u },
		{ "generator.resume_group_1=staged", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=staged",
		"generator.resume_group_0=staged",
		"generator.group_1=staged",
		"generator.resume_group_1=staged"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_finally_basic.xl",
		1,
		3,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorTryFinallyConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=staged",
		"generator.resume_group_0=staged",
		"generator.group_1=staged",
		"generator.resume_group_1=staged"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=staged", 1u },
		{ "generator.resume_group_0=staged", 1u },
		{ "generator.group_1=staged", 1u },
		{ "generator.resume_group_1=staged", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=staged",
		"generator.resume_group_0=staged",
		"generator.group_1=staged",
		"generator.resume_group_1=staged"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_try_finally_const_else_basic.xl",
		1,
		23,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorNestedTryFlowBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream",
		"generator.group_2=stream",
		"generator.resume_group_2=stream"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=stream", 2u },
		{ "generator.resume_group_0=stream", 2u },
		{ "generator.group_1=stream", 1u },
		{ "generator.resume_group_1=stream", 1u },
		{ "generator.group_2=stream", 1u },
		{ "generator.resume_group_2=stream", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream",
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_2=stream",
		"generator.resume_group_2=stream"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_nested_try_flow_basic.xl",
		1,
		34,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunAsyncGeneratorNestedTryFlowConstElseBasic(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream",
		"generator.group_2=stream",
		"generator.resume_group_2=stream"
	};
	static const XL_TraceCountExpectation aCounts[] = {
		{ "generator.group_0=stream", 2u },
		{ "generator.resume_group_0=stream", 2u },
		{ "generator.group_1=stream", 1u },
		{ "generator.resume_group_1=stream", 1u },
		{ "generator.group_2=stream", 1u },
		{ "generator.resume_group_2=stream", 1u }
	};
	static const char* const aOrder[] = {
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_1=stream",
		"generator.resume_group_1=stream",
		"generator.group_0=stream",
		"generator.resume_group_0=stream",
		"generator.group_2=stream",
		"generator.resume_group_2=stream"
	};

	return __xlRunProgramCaseAllCounts(
		pCtx,
		"examples\\async\\async_generator_nested_try_flow_const_else_basic.xl",
		1,
		74,
		aNeedles,
		XL_COUNTOF(aNeedles),
		aCounts,
		XL_COUNTOF(aCounts),
		aOrder,
		XL_COUNTOF(aOrder),
		NULL
	);
}
static int __xlTestRunGeneratorResumeBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_resume_basic.xl", 1, 9, NULL, NULL); }
static int __xlTestRunTypeValueBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\type_value_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunPublicTypesBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\public_types_basic.xl", 1, 77, NULL, NULL); }
static int __xlTestRunExternAtoiBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\extern_atoi_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunGeneratorRun(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_run.xl", 1, 20, "generator.next=make_values", NULL); }
static int __xlTestRunGeneratorLazyThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_lazy_throw_basic.xl", 1, 1, "generator.next=make_values", NULL); }
static int __xlTestRunGeneratorVarForInBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_var_for_in_basic.xl", 1, 3, "generator.next=make_values", NULL); }
static int __xlTestRunGeneratorParamResumeBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_param_resume_basic.xl", 1, 9, NULL, NULL); }
static int __xlTestRunGeneratorReturnWrapperBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_return_wrapper_basic.xl", 1, 3, "generator.next=make_values", NULL); }
static int __xlTestRunGeneratorReturnAbstractBranchResumeBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_return_abstract_branch_resume_basic.xl", 1, 9, NULL, NULL); }
static int __xlTestRunGeneratorReturnAbstractBranchForInBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_return_abstract_branch_for_in_basic.xl", 1, 3, NULL, NULL); }
static int __xlTestRunGeneratorStateCapture(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_state_capture_basic.xl", 1, 21, "generator.next=series", NULL); }
static int __xlTestRunGeneratorWhileBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_while_basic.xl", 1, 6, "generator.next=countdown", NULL); }
static int __xlTestRunGeneratorLoopControlBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_loop_control_basic.xl", 1, 8, "generator.next=odd_before_break", NULL); }
static int __xlTestRunGeneratorTryCatchBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_try_catch_basic.xl", 1, 7, "generator.next=guarded", NULL); }
static int __xlTestRunGeneratorTryFinallyBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_try_finally_basic.xl", 1, 3, "generator.next=staged", NULL); }
static int __xlTestRunGeneratorNestedTryFlowBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_nested_try_flow_basic.xl", 1, 34, "generator.next=stream", NULL); }
static int __xlTestRunForIn(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\for_in_range.xl", 1, 6, NULL, NULL); }
static int __xlTestRunLoopJumpLevels(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\loop_jump_levels.xl", 1, 22, NULL, NULL); }
static int __xlTestRunTask(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_basic.xl", 1, 5, NULL, NULL); }
static int __xlTestRunTaskAll(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_all_basic.xl", 1, 3, NULL, NULL); }
static int __xlTestRunTaskValueBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_value_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunTaskDelay(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_delay_basic.xl", 1, 2, NULL, NULL); }
static int __xlTestRunTaskError(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_error_basic.xl", 1, 1, NULL, NULL); }

static const XL_TestCase __gXLTests[] = {
	{ "lexer.minimal", "lexer", __xlTestLexMinimal },
	{ "lexer.keywords", "lexer", __xlTestLexKeywords },
	{ "parser.for_in", "parser", __xlTestParseForIn },
	{ "parser.try_basic", "parser", __xlTestParseTry },
	{ "parser.named_args_default", "parser", __xlTestParseNamedArgsDefault },
	{ "parser.assert_message", "parser", __xlTestParseAssertMessage },
	{ "parser.if_inline_basic", "parser", __xlTestParseIfInlineBasic },
	{ "parser.if_inline_else_basic", "parser", __xlTestParseIfInlineElseBasic },
	{ "parser.import_alias", "parser", __xlTestParseImportAlias },
	{ "parser.import_multi", "parser", __xlTestParseImportMulti },
	{ "parser.import_string", "parser", __xlTestParseImportString },
	{ "parser.extern_atoi_basic", "parser", __xlTestParseExternAtoiBasic },
	{ "parser.export_func", "parser", __xlTestParseExportFunc },
	{ "parser.export_type_method", "parser", __xlTestParseExportTypeMethod },
	{ "parser.export_generic_type_method", "parser", __xlTestParseExportGenericTypeMethod },
	{ "parser.export_values", "parser", __xlTestParseExportValues },
	{ "parser.typed_decl", "parser", __xlTestParseTypedDecl },
	{ "parser.exact_numeric_types", "parser", __xlTestParseExactNumericTypes },
	{ "parser.auto_decl", "parser", __xlTestParseAutoDecl },
	{ "parser.typedef_basic", "parser", __xlTestParseTypeDefBasic },
	{ "parser.point_basic", "parser", __xlTestParsePointBasic },
	{ "parser.async_bool_main", "parser", __xlTestParseAsyncBoolMain },
	{ "parser.future_value_basic", "parser", __xlTestParseFutureValueBasic },
	{ "parser.channel_basic", "parser", __xlTestParseChannelBasic },
	{ "parser.channel_recv_unpack_basic", "parser", __xlTestParseChannelRecvUnpackBasic },
	{ "parser.async_generator_while_basic", "parser", __xlTestParseAsyncGeneratorWhileBasic },
	{ "parser.async_generator_loop_control_basic", "parser", __xlTestParseAsyncGeneratorLoopControlBasic },
	{ "parser.async_generator_resume_basic", "parser", __xlTestParseAsyncGeneratorResumeBasic },
	{ "parser.async_generator_multi_await_group_basic", "parser", __xlTestParseAsyncGeneratorMultiAwaitGroupBasic },
	{ "parser.async_generator_try_catch_basic", "parser", __xlTestParseAsyncGeneratorTryCatchBasic },
	{ "parser.async_generator_try_catch_const_else_basic", "parser", __xlTestParseAsyncGeneratorTryCatchConstElseBasic },
	{ "parser.async_generator_try_finally_basic", "parser", __xlTestParseAsyncGeneratorTryFinallyBasic },
	{ "parser.async_generator_try_finally_const_else_basic", "parser", __xlTestParseAsyncGeneratorTryFinallyConstElseBasic },
	{ "parser.async_generator_nested_try_flow_basic", "parser", __xlTestParseAsyncGeneratorNestedTryFlowBasic },
	{ "parser.async_generator_nested_try_flow_const_else_basic", "parser", __xlTestParseAsyncGeneratorNestedTryFlowConstElseBasic },
	{ "parser.type_value_basic", "parser", __xlTestParseTypeValueBasic },
	{ "parser.public_types_basic", "parser", __xlTestParsePublicTypesBasic },
	{ "parser.task_value_basic", "parser", __xlTestParseTaskValueBasic },
	{ "parser.assign_basic", "parser", __xlTestParseAssignBasic },
	{ "parser.assign_plus_basic", "parser", __xlTestParseAssignPlusBasic },
	{ "parser.assign_minus_basic", "parser", __xlTestParseAssignMinusBasic },
	{ "parser.assign_mul_basic", "parser", __xlTestParseAssignMulBasic },
	{ "parser.assign_div_basic", "parser", __xlTestParseAssignDivBasic },
	{ "parser.assign_mod_basic", "parser", __xlTestParseAssignModBasic },
	{ "parser.power_basic", "parser", __xlTestParsePowerBasic },
	{ "parser.assign_pow_basic", "parser", __xlTestParseAssignPowBasic },
	{ "parser.coalesce_basic", "parser", __xlTestParseCoalesceBasic },
	{ "parser.null_assign_basic", "parser", __xlTestParseNullAssignBasic },
	{ "parser.ternary_basic", "parser", __xlTestParseTernaryBasic },
	{ "parser.approx_basic", "parser", __xlTestParseApproxBasic },
	{ "parser.logic_basic", "parser", __xlTestParseLogicBasic },
	{ "parser.bytes_alias_string", "parser", __xlTestParseBytesAliasString },
	{ "parser.template_basic", "parser", __xlTestParseTemplateBasic },
	{ "parser.safe_member_basic", "parser", __xlTestParseSafeMemberBasic },
	{ "parser.leading_blank_lines", "parser", __xlTestParseLeadingBlankLines },
	{ "parser.var_basic", "parser", __xlTestParseVarBasic },
	{ "parser.array_basic", "parser", __xlTestParseArrayBasic },
	{ "parser.list_basic", "parser", __xlTestParseListBasic },
	{ "parser.set_basic", "parser", __xlTestParseSetBasic },
	{ "parser.dict_basic", "parser", __xlTestParseDictBasic },
	{ "parser.bare_container_literals", "parser", __xlTestParseBareContainerLiterals },
	{ "parser.vararg_array_basic", "parser", __xlTestParseVarArgArrayBasic },
	{ "parser.vararg_dict_basic", "parser", __xlTestParseVarArgDictBasic },
	{ "parser.multi_return_basic", "parser", __xlTestParseMultiReturnBasic },
	{ "parser.multi_return_call_expand_basic", "parser", __xlTestParseMultiReturnCallExpandBasic },
	{ "parser.function_value_basic", "parser", __xlTestParseFunctionValueBasic },
	{ "parser.function_value_flow_basic", "parser", __xlTestParseFunctionValueFlowBasic },
	{ "parser.function_value_container_basic", "parser", __xlTestParseFunctionValueContainerBasic },
	{ "parser.function_value_paren_call_basic", "parser", __xlTestParseFunctionValueParenCallBasic },
	{ "parser.index_assign_basic", "parser", __xlTestParseIndexAssignBasic },
	{ "parser.for_in_containers", "parser", __xlTestParseForInContainers },
	{ "parser.container_methods_basic", "parser", __xlTestParseContainerMethodsBasic },
	{ "parser.container_append_basic", "parser", __xlTestParseContainerAppendBasic },
	{ "parser.container_clear_basic", "parser", __xlTestParseContainerClearBasic },
	{ "parser.container_remove_basic", "parser", __xlTestParseContainerRemoveBasic },
	{ "parser.core_len_basic", "parser", __xlTestParseCoreLenBasic },
	{ "parser.type_methods_basic", "parser", __xlTestParseTypeMethodsBasic },
	{ "parser.var_methods_basic", "parser", __xlTestParseVarMethodsBasic },
	{ "parser.var_type_methods_basic", "parser", __xlTestParseVarTypeMethodsBasic },
	{ "parser.var_container_type_methods_basic", "parser", __xlTestParseVarContainerTypeMethodsBasic },
	{ "parser.property_length_basic", "parser", __xlTestParsePropertyLengthBasic },
	{ "parser.string_methods_basic", "parser", __xlTestParseStringMethodsBasic },
	{ "parser.string_split_basic", "parser", __xlTestParseStringSplitBasic },
	{ "parser.while_basic", "parser", __xlTestParseWhileBasic },
	{ "parser.do_while_basic", "parser", __xlTestParseDoWhileBasic },
	{ "parser.syntax_error", "parser", __xlTestParseSyntaxError },
	{ "parser.generator_resume_basic", "parser", __xlTestParseGeneratorResumeBasic },
	{ "sem.overload", "sem", __xlTestSemOverload },
	{ "sem.unresolved", "sem", __xlTestSemUnresolved },
	{ "sem.no_overload", "sem", __xlTestSemNoOverload },
	{ "sem.for_in_generator", "sem", __xlTestSemForInGenerator },
	{ "sem.generator_var_for_in_basic", "sem", __xlTestSemGeneratorVarForInBasic },
	{ "sem.generator_param_resume_basic", "sem", __xlTestSemGeneratorParamResumeBasic },
	{ "sem.generator_return_wrapper_basic", "sem", __xlTestSemGeneratorReturnWrapperBasic },
	{ "sem.generator_return_abstract_branch_resume_basic", "sem", __xlTestSemGeneratorReturnAbstractBranchResumeBasic },
	{ "sem.generator_return_abstract_branch_for_in_basic", "sem", __xlTestSemGeneratorReturnAbstractBranchForInBasic },
	{ "sem.generator_state_capture", "sem", __xlTestSemGeneratorStateCapture },
	{ "sem.generator_while_basic", "sem", __xlTestSemGeneratorWhileBasic },
	{ "sem.generator_loop_control_basic", "sem", __xlTestSemGeneratorLoopControlBasic },
	{ "sem.generator_try_catch_basic", "sem", __xlTestSemGeneratorTryCatchBasic },
	{ "sem.generator_try_finally_basic", "sem", __xlTestSemGeneratorTryFinallyBasic },
	{ "sem.generator_nested_try_flow_basic", "sem", __xlTestSemGeneratorNestedTryFlowBasic },
	{ "sem.generator_param_unknown_error", "sem", __xlTestSemGeneratorParamUnknownError },
	{ "sem.try_basic", "sem", __xlTestSemTry },
	{ "sem.try_finally_escape_basic", "sem", __xlTestSemTryFinallyEscapeBasic },
	{ "sem.try_nested_rethrow", "sem", __xlTestSemTryNestedRethrow },
	{ "sem.error_fields", "sem", __xlTestSemErrorFields },
	{ "sem.named_args_default", "sem", __xlTestSemNamedArgsDefault },
	{ "sem.await_outside_async", "sem", __xlTestSemAwaitOutsideAsync },
	{ "sem.extern_nested_error", "sem", __xlTestSemExternNestedError },
	{ "sem.assert_message", "sem", __xlTestSemAssertMessage },
	{ "sem.if_inline_basic", "sem", __xlTestSemIfInlineBasic },
	{ "sem.if_inline_else_basic", "sem", __xlTestSemIfInlineElseBasic },
	{ "sem.native_error_bridge", "sem", __xlTestSemNativeErrorBridge },
	{ "sem.import_alias", "sem", __xlTestSemImportAlias },
	{ "sem.import_multi", "sem", __xlTestSemImportMulti },
	{ "sem.import_string", "sem", __xlTestSemImportString },
	{ "sem.extern_atoi_basic", "sem", __xlTestSemExternAtoiBasic },
	{ "sem.export_func", "sem", __xlTestSemExportFunc },
	{ "sem.export_type_method", "sem", __xlTestSemExportTypeMethod },
	{ "sem.export_generic_type_method", "sem", __xlTestSemExportGenericTypeMethod },
	{ "sem.export_values", "sem", __xlTestSemExportValues },
	{ "sem.typed_decl", "sem", __xlTestSemTypedDecl },
	{ "sem.exact_numeric_types", "sem", __xlTestSemExactNumericTypes },
	{ "sem.auto_decl", "sem", __xlTestSemAutoDecl },
	{ "sem.typedef_basic", "sem", __xlTestSemTypeDefBasic },
	{ "sem.type_ctor_basic", "sem", __xlTestSemTypeCtorBasic },
	{ "sem.point_basic", "sem", __xlTestSemPointBasic },
	{ "sem.async_bool_main", "sem", __xlTestSemAsyncBoolMain },
	{ "sem.future_value_basic", "sem", __xlTestSemFutureValueBasic },
	{ "sem.channel_basic", "sem", __xlTestSemChannelBasic },
	{ "sem.channel_recv_unpack_basic", "sem", __xlTestSemChannelRecvUnpackBasic },
	{ "sem.async_generator_while_basic", "sem", __xlTestSemAsyncGeneratorWhileBasic },
	{ "sem.async_generator_loop_control_basic", "sem", __xlTestSemAsyncGeneratorLoopControlBasic },
	{ "sem.async_generator_resume_basic", "sem", __xlTestSemAsyncGeneratorResumeBasic },
	{ "sem.async_generator_multi_await_group_basic", "sem", __xlTestSemAsyncGeneratorMultiAwaitGroupBasic },
	{ "sem.async_generator_try_catch_basic", "sem", __xlTestSemAsyncGeneratorTryCatchBasic },
	{ "sem.async_generator_try_catch_const_else_basic", "sem", __xlTestSemAsyncGeneratorTryCatchConstElseBasic },
	{ "sem.async_generator_try_finally_basic", "sem", __xlTestSemAsyncGeneratorTryFinallyBasic },
	{ "sem.async_generator_try_finally_const_else_basic", "sem", __xlTestSemAsyncGeneratorTryFinallyConstElseBasic },
	{ "sem.async_generator_nested_try_flow_basic", "sem", __xlTestSemAsyncGeneratorNestedTryFlowBasic },
	{ "sem.async_generator_nested_try_flow_const_else_basic", "sem", __xlTestSemAsyncGeneratorNestedTryFlowConstElseBasic },
	{ "sem.type_value_basic", "sem", __xlTestSemTypeValueBasic },
	{ "sem.public_types_basic", "sem", __xlTestSemPublicTypesBasic },
	{ "sem.task_value_basic", "sem", __xlTestSemTaskValueBasic },
	{ "sem.task_error", "sem", __xlTestSemTaskError },
	{ "sem.awaitable_protocols", "sem", __xlTestSemAwaitableProtocols },
	{ "sem.multi_return_basic", "sem", __xlTestSemMultiReturnBasic },
	{ "sem.multi_return_call_expand_basic", "sem", __xlTestSemMultiReturnCallExpandBasic },
	{ "sem.multi_return_single_bind_error", "sem", __xlTestSemMultiReturnSingleBindError },
	{ "sem.function_value_basic", "sem", __xlTestSemFunctionValueBasic },
	{ "sem.function_value_flow_basic", "sem", __xlTestSemFunctionValueFlowBasic },
	{ "sem.function_value_container_basic", "sem", __xlTestSemFunctionValueContainerBasic },
	{ "sem.function_value_paren_call_basic", "sem", __xlTestSemFunctionValueParenCallBasic },
	{ "sem.function_value_overload_basic", "sem", __xlTestSemFunctionValueOverloadBasic },
	{ "sem.function_value_auto_ambiguous_error", "sem", __xlTestSemFunctionValueAutoAmbiguousError },
	{ "sem.auto_param_error", "sem", __xlTestSemAutoParamError },
	{ "sem.auto_return_error", "sem", __xlTestSemAutoReturnError },
	{ "sem.typedef_unknown_error", "sem", __xlTestSemTypeDefUnknownError },
	{ "sem.type_ctor_arity_error", "sem", __xlTestSemTypeCtorArityError },
	{ "sem.point_ctor_error", "sem", __xlTestSemPointCtorError },
	{ "sem.assign_basic", "sem", __xlTestSemAssignBasic },
	{ "sem.assign_const_error", "sem", __xlTestSemAssignConstError },
	{ "sem.assign_plus_basic", "sem", __xlTestSemAssignPlusBasic },
	{ "sem.assign_plus_const_error", "sem", __xlTestSemAssignPlusConstError },
	{ "sem.assign_minus_basic", "sem", __xlTestSemAssignMinusBasic },
	{ "sem.assign_minus_const_error", "sem", __xlTestSemAssignMinusConstError },
	{ "sem.assign_mul_basic", "sem", __xlTestSemAssignMulBasic },
	{ "sem.assign_mul_const_error", "sem", __xlTestSemAssignMulConstError },
	{ "sem.assign_div_basic", "sem", __xlTestSemAssignDivBasic },
	{ "sem.assign_div_const_error", "sem", __xlTestSemAssignDivConstError },
	{ "sem.assign_mod_basic", "sem", __xlTestSemAssignModBasic },
	{ "sem.assign_mod_const_error", "sem", __xlTestSemAssignModConstError },
	{ "sem.power_basic", "sem", __xlTestSemPowerBasic },
	{ "sem.assign_pow_basic", "sem", __xlTestSemAssignPowBasic },
	{ "sem.assign_pow_const_error", "sem", __xlTestSemAssignPowConstError },
	{ "sem.coalesce_basic", "sem", __xlTestSemCoalesceBasic },
	{ "sem.coalesce_type_error", "sem", __xlTestSemCoalesceTypeError },
	{ "sem.null_assign_basic", "sem", __xlTestSemNullAssignBasic },
	{ "sem.null_assign_type_error", "sem", __xlTestSemNullAssignTypeError },
	{ "sem.ternary_basic", "sem", __xlTestSemTernaryBasic },
	{ "sem.ternary_type_error", "sem", __xlTestSemTernaryTypeError },
	{ "sem.approx_basic", "sem", __xlTestSemApproxBasic },
	{ "sem.approx_type_error", "sem", __xlTestSemApproxTypeError },
	{ "sem.logic_basic", "sem", __xlTestSemLogicBasic },
	{ "sem.logic_type_error", "sem", __xlTestSemLogicTypeError },
	{ "sem.not_type_error", "sem", __xlTestSemNotTypeError },
	{ "sem.bytes_alias_string", "sem", __xlTestSemBytesAliasString },
	{ "sem.template_basic", "sem", __xlTestSemTemplateBasic },
	{ "sem.safe_member_basic", "sem", __xlTestSemSafeMemberBasic },
	{ "sem.var_basic", "sem", __xlTestSemVarBasic },
	{ "sem.array_basic", "sem", __xlTestSemArrayBasic },
	{ "sem.list_basic", "sem", __xlTestSemListBasic },
	{ "sem.set_basic", "sem", __xlTestSemSetBasic },
	{ "sem.dict_basic", "sem", __xlTestSemDictBasic },
	{ "sem.bare_container_literals", "sem", __xlTestSemBareContainerLiterals },
	{ "sem.vararg_array_basic", "sem", __xlTestSemVarArgArrayBasic },
	{ "sem.vararg_dict_basic", "sem", __xlTestSemVarArgDictBasic },
	{ "sem.vararg_mixed_pack_error", "sem", __xlTestSemVarArgMixedPackError },
	{ "sem.vararg_reserved_arg_error", "sem", __xlTestSemVarArgReservedArgError },
	{ "sem.bare_indexed_list_error", "sem", __xlTestSemBareIndexedListError },
	{ "sem.index_assign_basic", "sem", __xlTestSemIndexAssignBasic },
	{ "sem.index_assign_plus_error", "sem", __xlTestSemIndexAssignPlusError },
	{ "sem.for_in_containers", "sem", __xlTestSemForInContainers },
	{ "sem.container_methods_basic", "sem", __xlTestSemContainerMethodsBasic },
	{ "sem.container_method_error", "sem", __xlTestSemContainerMethodError },
	{ "sem.container_append_basic", "sem", __xlTestSemContainerAppendBasic },
	{ "sem.container_append_error", "sem", __xlTestSemContainerAppendError },
	{ "sem.container_clear_basic", "sem", __xlTestSemContainerClearBasic },
	{ "sem.container_clear_error", "sem", __xlTestSemContainerClearError },
	{ "sem.container_remove_basic", "sem", __xlTestSemContainerRemoveBasic },
	{ "sem.container_remove_error", "sem", __xlTestSemContainerRemoveError },
	{ "sem.core_len_basic", "sem", __xlTestSemCoreLenBasic },
	{ "sem.type_methods_basic", "sem", __xlTestSemTypeMethodsBasic },
	{ "sem.var_methods_basic", "sem", __xlTestSemVarMethodsBasic },
	{ "sem.var_type_methods_basic", "sem", __xlTestSemVarTypeMethodsBasic },
	{ "sem.var_container_type_methods_basic", "sem", __xlTestSemVarContainerTypeMethodsBasic },
	{ "sem.type_method_self_error", "sem", __xlTestSemTypeMethodSelfError },
	{ "sem.property_length_basic", "sem", __xlTestSemPropertyLengthBasic },
	{ "sem.property_length_error", "sem", __xlTestSemPropertyLengthError },
	{ "sem.string_methods_basic", "sem", __xlTestSemStringMethodsBasic },
	{ "sem.string_split_basic", "sem", __xlTestSemStringSplitBasic },
	{ "sem.string_split_error", "sem", __xlTestSemStringSplitError },
	{ "sem.while_basic", "sem", __xlTestSemWhileBasic },
	{ "sem.do_while_basic", "sem", __xlTestSemDoWhileBasic },
	{ "sem.generator_resume_basic", "sem", __xlTestSemGeneratorResumeBasic },
	{ "sem.yield_top_level_error", "sem", __xlTestSemYieldTopLevelError },
	{ "sem.xlp_import", "sem", __xlTestSemXlpImport },
	{ "sem.xlp_native", "sem", __xlTestSemXlpNative },
	{ "sem.xlp_native_funcs", "sem", __xlTestSemXlpNativeFuncs },
	{ "sem.xlp_native_funcs_params", "sem", __xlTestSemXlpNativeFuncsParams },
	{ "sem.xlp_native_funcs_string", "sem", __xlTestSemXlpNativeFuncsString },
	{ "sem.xlp_native_funcs_string_params", "sem", __xlTestSemXlpNativeFuncsStringParams },
	{ "sem.xlp_native_funcs_void", "sem", __xlTestSemXlpNativeFuncsVoid },
	{ "sem.xlp_native_funcs_mixed_params", "sem", __xlTestSemXlpNativeFuncsMixedParams },
	{ "sem.xlp_native_funcs_bool", "sem", __xlTestSemXlpNativeFuncsBool },
	{ "sem.xlp_native_funcs_bool_params", "sem", __xlTestSemXlpNativeFuncsBoolParams },
	{ "sem.xlp_native_funcs_float", "sem", __xlTestSemXlpNativeFuncsFloat },
	{ "sem.xlp_native_funcs_float_params", "sem", __xlTestSemXlpNativeFuncsFloatParams },
	{ "sem.xlp_static", "sem", __xlTestSemXlpStatic },
	{ "sem.xlp_static_funcs", "sem", __xlTestSemXlpStaticFuncs },
	{ "sem.xlp_static_funcs_params", "sem", __xlTestSemXlpStaticFuncsParams },
	{ "sem.xlp_static_funcs_string", "sem", __xlTestSemXlpStaticFuncsString },
	{ "sem.xlp_static_funcs_string_params", "sem", __xlTestSemXlpStaticFuncsStringParams },
	{ "sem.xlp_static_funcs_void", "sem", __xlTestSemXlpStaticFuncsVoid },
	{ "sem.xlp_static_funcs_mixed_params", "sem", __xlTestSemXlpStaticFuncsMixedParams },
	{ "sem.xlp_static_funcs_bool", "sem", __xlTestSemXlpStaticFuncsBool },
	{ "sem.xlp_static_funcs_bool_params", "sem", __xlTestSemXlpStaticFuncsBoolParams },
	{ "sem.xlp_static_funcs_float", "sem", __xlTestSemXlpStaticFuncsFloat },
	{ "sem.xlp_static_funcs_float_params", "sem", __xlTestSemXlpStaticFuncsFloatParams },
	{ "sem.xlp_source", "sem", __xlTestSemXlpSource },
	{ "sem.xlp_transitive", "sem", __xlTestSemXlpTransitive },
	{ "sem.xlp_versions", "sem", __xlTestSemXlpVersions },
	{ "sem.xlp_external_package", "sem", __xlTestSemXlpExternalPackage },
	{ "sem.xlp_native_package", "sem", __xlTestSemXlpNativePackage },
	{ "sem.xlp_static_package", "sem", __xlTestSemXlpStaticPackage },
	{ "sem.xlp_source_package", "sem", __xlTestSemXlpSourcePackage },
	{ "sem.xlp_versions_package", "sem", __xlTestSemXlpVersionsPackage },
	{ "sem.xlp_transitive_package", "sem", __xlTestSemXlpTransitivePackage },
	{ "sem.xlp_funcs", "sem", __xlTestSemXlpFuncs },
	{ "sem.xlp_package", "sem", __xlTestSemXlpPackage },
	{ "sem.module_abi_summary", "sem", __xlTestSemModuleAbiSummary },
	{ "sem.xlp_read_functions_abi", "sem", __xlTestSemXlpReadFunctionsAbi },
	{ "sem.xlp_dependencies", "sem", __xlTestSemXlpDependencies },
	{ "lower.basic", "lower", __xlTestLowerBasic },
	{ "lower.async_basic", "lower", __xlTestLowerAsync },
	{ "lower.generator_basic", "lower", __xlTestLowerGenerator },
	{ "lower.generator_resume_basic", "lower", __xlTestLowerGeneratorResumeBasic },
	{ "lower.generator_resume_abi_json", "lower", __xlTestLowerGeneratorResumeAbiJson },
	{ "lower.async_generator_basic", "lower", __xlTestLowerAsyncGeneratorBasic },
	{ "lower.async_generator_for_in_basic", "lower", __xlTestLowerAsyncGeneratorForInBasic },
	{ "lower.async_generator_while_basic", "lower", __xlTestLowerAsyncGeneratorWhileBasic },
	{ "lower.async_generator_loop_control_basic", "lower", __xlTestLowerAsyncGeneratorLoopControlBasic },
	{ "lower.async_generator_resume_basic", "lower", __xlTestLowerAsyncGeneratorResumeBasic },
	{ "lower.async_generator_multi_await_group_basic", "lower", __xlTestLowerAsyncGeneratorMultiAwaitGroupBasic },
	{ "lower.async_generator_try_catch_basic", "lower", __xlTestLowerAsyncGeneratorTryCatchBasic },
	{ "lower.async_generator_try_catch_const_else_basic", "lower", __xlTestLowerAsyncGeneratorTryCatchConstElseBasic },
	{ "lower.async_generator_try_finally_basic", "lower", __xlTestLowerAsyncGeneratorTryFinallyBasic },
	{ "lower.async_generator_try_finally_const_else_basic", "lower", __xlTestLowerAsyncGeneratorTryFinallyConstElseBasic },
	{ "lower.async_generator_nested_try_flow_basic", "lower", __xlTestLowerAsyncGeneratorNestedTryFlowBasic },
	{ "lower.async_generator_nested_try_flow_const_else_basic", "lower", __xlTestLowerAsyncGeneratorNestedTryFlowConstElseBasic },
	{ "lower.async_generator_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorHybridProjection },
	{ "lower.async_generator_hybrid_projection_json", "lower", __xlTestLowerAsyncGeneratorHybridProjectionJson },
	{ "lower.async_generator_loop_control_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorLoopControlHybridProjection },
	{ "lower.async_generator_loop_control_hybrid_projection_json", "lower", __xlTestLowerAsyncGeneratorLoopControlHybridProjectionJson },
	{ "lower.async_generator_resume_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorResumeHybridProjection },
	{ "lower.async_generator_resume_hybrid_projection_json", "lower", __xlTestLowerAsyncGeneratorResumeHybridProjectionJson },
	{ "lower.async_generator_multi_await_group_projection", "lower", __xlTestLowerAsyncGeneratorMultiAwaitGroupProjection },
	{ "lower.async_generator_multi_await_group_projection_json", "lower", __xlTestLowerAsyncGeneratorMultiAwaitGroupProjectionJson },
	{ "lower.async_generator_nested_try_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorNestedTryHybridProjection },
	{ "lower.async_generator_try_catch_const_else_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorTryCatchConstElseHybridProjection },
	{ "lower.async_generator_try_finally_const_else_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorTryFinallyConstElseHybridProjection },
	{ "lower.async_generator_nested_try_const_else_hybrid_projection", "lower", __xlTestLowerAsyncGeneratorNestedTryConstElseHybridProjection },
	{ "lower.async_generator_loop_control_hybrid_groups_json", "lower", __xlTestLowerAsyncGeneratorLoopControlHybridGroupsJson },
	{ "lower.export_generic_type_method", "lower", __xlTestLowerExportGenericTypeMethod },
	{ "lower.for_in_range", "lower", __xlTestLowerForIn },
	{ "lower.index_assign_basic", "lower", __xlTestLowerIndexAssignBasic },
	{ "lower.for_in_containers", "lower", __xlTestLowerForInContainers },
	{ "lower.container_methods_basic", "lower", __xlTestLowerContainerMethodsBasic },
	{ "lower.container_append_basic", "lower", __xlTestLowerContainerAppendBasic },
	{ "lower.container_clear_basic", "lower", __xlTestLowerContainerClearBasic },
	{ "lower.container_remove_basic", "lower", __xlTestLowerContainerRemoveBasic },
	{ "lower.bare_container_literals", "lower", __xlTestLowerBareContainerLiterals },
	{ "lower.core_len_basic", "lower", __xlTestLowerCoreLenBasic },
	{ "lower.typedef_basic", "lower", __xlTestLowerTypeDefBasic },
	{ "lower.type_ctor_basic", "lower", __xlTestLowerTypeCtorBasic },
	{ "lower.point_basic", "lower", __xlTestLowerPointBasic },
	{ "lower.type_methods_basic", "lower", __xlTestLowerTypeMethodsBasic },
	{ "lower.var_methods_basic", "lower", __xlTestLowerVarMethodsBasic },
	{ "lower.var_type_methods_basic", "lower", __xlTestLowerVarTypeMethodsBasic },
	{ "lower.var_container_type_methods_basic", "lower", __xlTestLowerVarContainerTypeMethodsBasic },
	{ "lower.property_length_basic", "lower", __xlTestLowerPropertyLengthBasic },
	{ "lower.string_methods_basic", "lower", __xlTestLowerStringMethodsBasic },
	{ "lower.string_split_basic", "lower", __xlTestLowerStringSplitBasic },
	{ "lower.try_basic", "lower", __xlTestLowerTry },
	{ "lower.named_args_default", "lower", __xlTestLowerNamedArgsDefault },
	{ "cgen.module_use_math", "cgen", __xlTestCgenModule },
	{ "cgen.export_type_method", "cgen", __xlTestCgenExportTypeMethod },
	{ "cgen.export_generic_type_method", "cgen", __xlTestCgenExportGenericTypeMethod },
	{ "cgen.module_init_throw", "cgen", __xlTestCgenModuleInitThrow },
	{ "cgen.overload_abi", "cgen", __xlTestCgenOverloadAbi },
	{ "cgen.native_std_core", "cgen", __xlTestCgenNative },
	{ "cgen.import_alias", "cgen", __xlTestCgenImportAlias },
	{ "cgen.import_multi", "cgen", __xlTestCgenImportMulti },
	{ "cgen.import_string", "cgen", __xlTestCgenImportString },
	{ "cgen.export_func", "cgen", __xlTestCgenExportFunc },
	{ "cgen.export_values", "cgen", __xlTestCgenExportValues },
	{ "cgen.typed_decl", "cgen", __xlTestCgenTypedDecl },
	{ "cgen.exact_numeric_types", "cgen", __xlTestCgenExactNumericTypes },
	{ "cgen.auto_decl", "cgen", __xlTestCgenAutoDecl },
	{ "cgen.typedef_basic", "cgen", __xlTestCgenTypeDefBasic },
	{ "cgen.type_ctor_basic", "cgen", __xlTestCgenTypeCtorBasic },
	{ "cgen.point_basic", "cgen", __xlTestCgenPointBasic },
	{ "cgen.assign_basic", "cgen", __xlTestCgenAssignBasic },
	{ "cgen.assign_plus_basic", "cgen", __xlTestCgenAssignPlusBasic },
	{ "cgen.assign_minus_basic", "cgen", __xlTestCgenAssignMinusBasic },
	{ "cgen.assign_mul_basic", "cgen", __xlTestCgenAssignMulBasic },
	{ "cgen.assign_div_basic", "cgen", __xlTestCgenAssignDivBasic },
	{ "cgen.assign_mod_basic", "cgen", __xlTestCgenAssignModBasic },
	{ "cgen.power_basic", "cgen", __xlTestCgenPowerBasic },
	{ "cgen.assign_pow_basic", "cgen", __xlTestCgenAssignPowBasic },
	{ "cgen.coalesce_basic", "cgen", __xlTestCgenCoalesceBasic },
	{ "cgen.null_assign_basic", "cgen", __xlTestCgenNullAssignBasic },
	{ "cgen.ternary_basic", "cgen", __xlTestCgenTernaryBasic },
	{ "cgen.approx_basic", "cgen", __xlTestCgenApproxBasic },
	{ "cgen.if_inline_basic", "cgen", __xlTestCgenIfInlineBasic },
	{ "cgen.if_inline_else_basic", "cgen", __xlTestCgenIfInlineElseBasic },
	{ "cgen.logic_basic", "cgen", __xlTestCgenLogicBasic },
	{ "cgen.bytes_alias_string", "cgen", __xlTestCgenBytesAliasString },
	{ "cgen.template_basic", "cgen", __xlTestCgenTemplateBasic },
	{ "cgen.safe_member_basic", "cgen", __xlTestCgenSafeMemberBasic },
	{ "cgen.var_basic", "cgen", __xlTestCgenVarBasic },
	{ "cgen.array_basic", "cgen", __xlTestCgenArrayBasic },
	{ "cgen.list_basic", "cgen", __xlTestCgenListBasic },
	{ "cgen.set_basic", "cgen", __xlTestCgenSetBasic },
	{ "cgen.dict_basic", "cgen", __xlTestCgenDictBasic },
	{ "cgen.vararg_array_basic", "cgen", __xlTestCgenVarArgArrayBasic },
	{ "cgen.vararg_dict_basic", "cgen", __xlTestCgenVarArgDictBasic },
	{ "cgen.multi_return_basic", "cgen", __xlTestCgenMultiReturnBasic },
	{ "cgen.multi_return_call_expand_basic", "cgen", __xlTestCgenMultiReturnCallExpandBasic },
	{ "cgen.function_value_basic", "cgen", __xlTestCgenFunctionValueBasic },
	{ "cgen.function_value_container_basic", "cgen", __xlTestCgenFunctionValueContainerBasic },
	{ "cgen.module_abi_symbols", "cgen", __xlTestCgenModuleAbiSymbols },
	{ "cgen.index_assign_basic", "cgen", __xlTestCgenIndexAssignBasic },
	{ "cgen.for_in_containers", "cgen", __xlTestCgenForInContainers },
	{ "cgen.container_methods_basic", "cgen", __xlTestCgenContainerMethodsBasic },
	{ "cgen.container_append_basic", "cgen", __xlTestCgenContainerAppendBasic },
	{ "cgen.container_clear_basic", "cgen", __xlTestCgenContainerClearBasic },
	{ "cgen.container_remove_basic", "cgen", __xlTestCgenContainerRemoveBasic },
	{ "cgen.bare_container_literals", "cgen", __xlTestCgenBareContainerLiterals },
	{ "cgen.core_len_basic", "cgen", __xlTestCgenCoreLenBasic },
	{ "cgen.type_methods_basic", "cgen", __xlTestCgenTypeMethodsBasic },
	{ "cgen.var_methods_basic", "cgen", __xlTestCgenVarMethodsBasic },
	{ "cgen.var_type_methods_basic", "cgen", __xlTestCgenVarTypeMethodsBasic },
	{ "cgen.var_container_type_methods_basic", "cgen", __xlTestCgenVarContainerTypeMethodsBasic },
	{ "cgen.property_length_basic", "cgen", __xlTestCgenPropertyLengthBasic },
	{ "cgen.string_methods_basic", "cgen", __xlTestCgenStringMethodsBasic },
	{ "cgen.string_split_basic", "cgen", __xlTestCgenStringSplitBasic },
	{ "cgen.while_basic", "cgen", __xlTestCgenWhileBasic },
	{ "cgen.do_while_basic", "cgen", __xlTestCgenDoWhileBasic },
	{ "cgen.try_finally", "cgen", __xlTestCgenTryFinally },
	{ "cgen.try_frame", "cgen", __xlTestCgenTryFrame },
	{ "cgen.error_fields", "cgen", __xlTestCgenErrorFields },
	{ "cgen.named_args_default", "cgen", __xlTestCgenNamedArgsDefault },
	{ "cgen.async_throw", "cgen", __xlTestCgenAsyncThrow },
	{ "cgen.async_bool_main", "cgen", __xlTestCgenAsyncBoolMain },
	{ "cgen.async_run_main", "cgen", __xlTestCgenAsyncRunMain },
	{ "cgen.async_bind_await_return_basic", "cgen", __xlTestCgenAsyncBindAwaitReturnBasic },
	{ "cgen.async_bind_expr_await_basic", "cgen", __xlTestCgenAsyncBindExprAwaitBasic },
	{ "cgen.async_call_arg_await_basic", "cgen", __xlTestCgenAsyncCallArgAwaitBasic },
	{ "cgen.async_function_value_arg_await_basic", "cgen", __xlTestCgenAsyncFunctionValueArgAwaitBasic },
	{ "cgen.async_string_method_arg_await_basic", "cgen", __xlTestCgenAsyncStringMethodArgAwaitBasic },
	{ "cgen.async_container_method_arg_await_basic", "cgen", __xlTestCgenAsyncContainerMethodArgAwaitBasic },
	{ "cgen.async_container_join_arg_await_basic", "cgen", __xlTestCgenAsyncContainerJoinArgAwaitBasic },
	{ "cgen.async_container_dict_has_arg_await_basic", "cgen", __xlTestCgenAsyncContainerDictHasArgAwaitBasic },
	{ "cgen.async_container_dict_remove_arg_await_basic", "cgen", __xlTestCgenAsyncContainerDictRemoveArgAwaitBasic },
	{ "cgen.async_container_list_remove_arg_await_basic", "cgen", __xlTestCgenAsyncContainerListRemoveArgAwaitBasic },
	{ "cgen.async_container_list_join_arg_await_basic", "cgen", __xlTestCgenAsyncContainerListJoinArgAwaitBasic },
	{ "cgen.async_container_list_append_arg_await_basic", "cgen", __xlTestCgenAsyncContainerListAppendArgAwaitBasic },
	{ "cgen.async_container_dict_assign_arg_await_basic", "cgen", __xlTestCgenAsyncContainerDictAssignArgAwaitBasic },
	{ "cgen.async_container_array_index_assign_arg_await_basic", "cgen", __xlTestCgenAsyncContainerArrayIndexAssignArgAwaitBasic },
	{ "cgen.async_container_list_index_assign_arg_await_basic", "cgen", __xlTestCgenAsyncContainerListIndexAssignArgAwaitBasic },
	{ "cgen.async_container_array_append_arg_await_basic", "cgen", __xlTestCgenAsyncContainerArrayAppendArgAwaitBasic },
	{ "cgen.async_container_array_remove_arg_await_basic", "cgen", __xlTestCgenAsyncContainerArrayRemoveArgAwaitBasic },
	{ "cgen.async_container_set_remove_arg_await_basic", "cgen", __xlTestCgenAsyncContainerSetRemoveArgAwaitBasic },
	{ "cgen.async_multi_await_state", "cgen", __xlTestCgenAsyncMultiAwaitState },
	{ "cgen.extern_atoi_basic", "cgen", __xlTestCgenExternAtoiBasic },
	{ "cgen.future_value_basic", "cgen", __xlTestCgenFutureValueBasic },
	{ "cgen.channel_basic", "cgen", __xlTestCgenChannelBasic },
	{ "cgen.channel_recv_unpack_basic", "cgen", __xlTestCgenChannelRecvUnpackBasic },
	{ "cgen.type_value_basic", "cgen", __xlTestCgenTypeValueBasic },
	{ "cgen.public_types_basic", "cgen", __xlTestCgenPublicTypesBasic },
	{ "cgen.assert_message", "cgen", __xlTestCgenAssertMessage },
	{ "cgen.native_error_bridge", "cgen", __xlTestCgenNativeErrorBridge },
	{ "cgen.async_assign_arg_await_basic", "cgen", __xlTestCgenAsyncAssignArgAwaitBasic },
	{ "cgen.async_assign_string_arg_await_basic", "cgen", __xlTestCgenAsyncAssignStringArgAwaitBasic },
	{ "cgen.async_assign_multi_await_basic", "cgen", __xlTestCgenAsyncAssignMultiAwaitBasic },
	{ "cgen.async_string_expr_await_basic", "cgen", __xlTestCgenAsyncStringExprAwaitBasic },
	{ "cgen.async_if_body_prelude_then_await_basic", "cgen", __xlTestCgenAsyncIfBodyPreludeThenAwaitBasic },
	{ "cgen.async_if_dynamic_then_await_basic", "cgen", __xlTestCgenAsyncIfDynamicThenAwaitBasic },
	{ "cgen.async_if_dynamic_else_sync_basic", "cgen", __xlTestCgenAsyncIfDynamicElseSyncBasic },
	{ "cgen.async_if_dynamic_else_await_basic", "cgen", __xlTestCgenAsyncIfDynamicElseAwaitBasic },
	{ "cgen.async_if_dynamic_both_await_then_basic", "cgen", __xlTestCgenAsyncIfDynamicBothAwaitThenBasic },
	{ "cgen.async_if_dynamic_both_await_else_basic", "cgen", __xlTestCgenAsyncIfDynamicBothAwaitElseBasic },
	{ "cgen.async_if_nested_dynamic_both_await_prelude_then_basic", "cgen", __xlTestCgenAsyncIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "cgen.async_if_nested_dynamic_both_await_prelude_else_basic", "cgen", __xlTestCgenAsyncIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "cgen.async_if_nested_dynamic_both_await_const_else_then_basic", "cgen", __xlTestCgenAsyncIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "cgen.async_if_nested_dynamic_both_await_const_else_else_basic", "cgen", __xlTestCgenAsyncIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "cgen.async_try_if_body_prelude_then_await_basic", "cgen", __xlTestCgenAsyncTryIfBodyPreludeThenAwaitBasic },
	{ "cgen.async_try_if_dynamic_then_await_basic", "cgen", __xlTestCgenAsyncTryIfDynamicThenAwaitBasic },
	{ "cgen.async_try_if_dynamic_else_sync_basic", "cgen", __xlTestCgenAsyncTryIfDynamicElseSyncBasic },
	{ "cgen.async_try_if_dynamic_else_await_basic", "cgen", __xlTestCgenAsyncTryIfDynamicElseAwaitBasic },
	{ "cgen.async_try_if_dynamic_both_await_then_basic", "cgen", __xlTestCgenAsyncTryIfDynamicBothAwaitThenBasic },
	{ "cgen.async_try_if_dynamic_both_await_else_basic", "cgen", __xlTestCgenAsyncTryIfDynamicBothAwaitElseBasic },
	{ "cgen.async_try_if_dynamic_both_await_prelude_then_basic", "cgen", __xlTestCgenAsyncTryIfDynamicBothAwaitPreludeThenBasic },
	{ "cgen.async_try_if_dynamic_both_await_prelude_else_basic", "cgen", __xlTestCgenAsyncTryIfDynamicBothAwaitPreludeElseBasic },
	{ "cgen.async_try_if_nested_dynamic_both_await_prelude_then_basic", "cgen", __xlTestCgenAsyncTryIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "cgen.async_try_if_nested_dynamic_both_await_prelude_else_basic", "cgen", __xlTestCgenAsyncTryIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "cgen.async_try_if_nested_dynamic_both_await_const_else_then_basic", "cgen", __xlTestCgenAsyncTryIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "cgen.async_try_if_nested_dynamic_both_await_const_else_else_basic", "cgen", __xlTestCgenAsyncTryIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "cgen.async_if_cond_await_basic", "cgen", __xlTestCgenAsyncIfCondAwaitBasic },
	{ "cgen.async_if_cond_then_await_basic", "cgen", __xlTestCgenAsyncIfCondThenAwaitBasic },
	{ "cgen.async_if_cond_container_then_await_basic", "cgen", __xlTestCgenAsyncIfCondContainerThenAwaitBasic },
	{ "cgen.async_while_cond_await_basic", "cgen", __xlTestCgenAsyncWhileCondAwaitBasic },
	{ "cgen.async_while_cond_then_await_basic", "cgen", __xlTestCgenAsyncWhileCondThenAwaitBasic },
	{ "cgen.async_while_cond_container_then_await_basic", "cgen", __xlTestCgenAsyncWhileCondContainerThenAwaitBasic },
	{ "cgen.async_while_cond_break_basic", "cgen", __xlTestCgenAsyncWhileCondBreakBasic },
	{ "cgen.async_while_cond_continue_basic", "cgen", __xlTestCgenAsyncWhileCondContinueBasic },
	{ "cgen.async_while_cond_break_then_await_basic", "cgen", __xlTestCgenAsyncWhileCondBreakThenAwaitBasic },
	{ "cgen.async_while_cond_continue_then_await_basic", "cgen", __xlTestCgenAsyncWhileCondContinueThenAwaitBasic },
	{ "cgen.async_do_while_cond_await_basic", "cgen", __xlTestCgenAsyncDoWhileCondAwaitBasic },
	{ "cgen.async_do_while_cond_then_await_basic", "cgen", __xlTestCgenAsyncDoWhileCondThenAwaitBasic },
	{ "cgen.async_do_while_cond_container_then_await_basic", "cgen", __xlTestCgenAsyncDoWhileCondContainerThenAwaitBasic },
	{ "cgen.async_do_while_cond_break_basic", "cgen", __xlTestCgenAsyncDoWhileCondBreakBasic },
	{ "cgen.async_do_while_cond_continue_basic", "cgen", __xlTestCgenAsyncDoWhileCondContinueBasic },
	{ "cgen.async_do_while_cond_break_then_await_basic", "cgen", __xlTestCgenAsyncDoWhileCondBreakThenAwaitBasic },
	{ "cgen.async_do_while_cond_continue_then_await_basic", "cgen", __xlTestCgenAsyncDoWhileCondContinueThenAwaitBasic },
	{ "cgen.async_try_finally_await_basic", "cgen", __xlTestCgenAsyncTryFinallyAwaitBasic },
	{ "cgen.async_try_finally_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_await_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyAwaitBasic },
	{ "cgen.async_try_catch_finally_await_const_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_then_await_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyThenAwaitBasic },
	{ "cgen.async_try_catch_finally_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyThenAwaitConstElseBasic },
	{ "cgen.async_try_finally_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyThenAwaitBasic },
	{ "cgen.async_try_finally_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyThenAwaitConstElseBasic },
	{ "cgen.async_try_finally_tail_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyTailThenAwaitBasic },
	{ "cgen.async_try_finally_tail_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyTailThenAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_tail_then_await_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyTailThenAwaitBasic },
	{ "cgen.async_try_catch_finally_tail_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyTailThenAwaitConstElseBasic },
	{ "cgen.async_try_finally_if_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyIfThenAwaitBasic },
	{ "cgen.async_try_finally_if_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyIfThenAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_while_then_await_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyWhileThenAwaitBasic },
	{ "cgen.async_try_catch_finally_while_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyWhileThenAwaitConstElseBasic },
	{ "cgen.async_try_finally_prelude_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyPreludeThenAwaitBasic },
	{ "cgen.async_try_finally_prelude_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyPreludeThenAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_prelude_then_await_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyPreludeThenAwaitBasic },
	{ "cgen.async_try_catch_finally_prelude_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyPreludeThenAwaitConstElseBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitThenBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitElseBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_success_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitSuccessThenBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_success_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitSuccessElseBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_prelude_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitPreludeThenBasic },
	{ "cgen.async_try_catch_finally_if_dynamic_both_await_prelude_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfDynamicBothAwaitPreludeElseBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_const_else_success_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessThenBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_const_else_success_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessElseBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_const_else_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_const_else_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_success_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessThenBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_success_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessElseBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_prelude_then_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "cgen.async_try_catch_finally_if_nested_dynamic_both_await_prelude_else_basic", "cgen", __xlTestCgenAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "cgen.async_try_finally_do_while_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyDoWhileThenAwaitBasic },
	{ "cgen.async_try_finally_do_while_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyDoWhileThenAwaitConstElseBasic },
	{ "cgen.async_try_finally_do_while_break_then_await_basic", "cgen", __xlTestCgenAsyncTryFinallyDoWhileBreakThenAwaitBasic },
	{ "cgen.async_try_finally_do_while_break_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryFinallyDoWhileBreakThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_inner_catch_then_outer_finally_basic", "cgen", __xlTestCgenAsyncTryNestedInnerCatchThenOuterFinallyBasic },
	{ "cgen.async_try_nested_inner_catch_then_outer_finally_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedInnerCatchThenOuterFinallyConstElseBasic },
	{ "cgen.async_try_nested_inner_finally_outer_catch_basic", "cgen", __xlTestCgenAsyncTryNestedInnerFinallyOuterCatchBasic },
	{ "cgen.async_try_nested_inner_finally_outer_catch_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedInnerFinallyOuterCatchConstElseBasic },
	{ "cgen.async_try_nested_if_finally_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedIfFinallyThenAwaitBasic },
	{ "cgen.async_try_nested_if_finally_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedIfFinallyThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_if_dynamic_both_await_inner_finally_outer_catch_then_basic", "cgen", __xlTestCgenAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchThenBasic },
	{ "cgen.async_try_nested_if_dynamic_both_await_inner_finally_outer_catch_else_basic", "cgen", __xlTestCgenAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchElseBasic },
	{ "cgen.async_try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_then_basic", "cgen", __xlTestCgenAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchThenBasic },
	{ "cgen.async_try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_else_basic", "cgen", __xlTestCgenAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchElseBasic },
	{ "cgen.async_try_nested_while_finally_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedWhileFinallyOuterCatchThenAwaitBasic },
	{ "cgen.async_try_nested_while_finally_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedWhileFinallyOuterCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_do_while_finally_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitBasic },
	{ "cgen.async_try_nested_do_while_finally_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_do_while_continue_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedDoWhileContinueOuterCatchThenAwaitBasic },
	{ "cgen.async_try_nested_do_while_continue_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedDoWhileContinueOuterCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_full_chain_inner_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedFullChainInnerCatchThenAwaitBasic },
	{ "cgen.async_try_nested_full_chain_inner_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedFullChainInnerCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_nested_full_chain_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryNestedFullChainOuterCatchThenAwaitBasic },
	{ "cgen.async_try_nested_full_chain_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryNestedFullChainOuterCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_triple_nested_inner_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryTripleNestedInnerCatchThenAwaitBasic },
	{ "cgen.async_try_triple_nested_inner_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryTripleNestedInnerCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_triple_nested_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryTripleNestedOuterCatchThenAwaitBasic },
	{ "cgen.async_try_triple_nested_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryTripleNestedOuterCatchThenAwaitConstElseBasic },
	{ "cgen.async_try_triple_nested_while_outer_catch_then_await_basic", "cgen", __xlTestCgenAsyncTryTripleNestedWhileOuterCatchThenAwaitBasic },
	{ "cgen.async_try_triple_nested_while_outer_catch_then_await_const_else_basic", "cgen", __xlTestCgenAsyncTryTripleNestedWhileOuterCatchThenAwaitConstElseBasic },
	{ "cgen.xlp_native_funcs", "cgen", __xlTestCgenXlpNativeFuncs },
	{ "cgen.xlp_native_funcs_params", "cgen", __xlTestCgenXlpNativeFuncsParams },
	{ "cgen.xlp_native_funcs_string", "cgen", __xlTestCgenXlpNativeFuncsString },
	{ "cgen.xlp_native_funcs_string_params", "cgen", __xlTestCgenXlpNativeFuncsStringParams },
	{ "cgen.xlp_native_funcs_void", "cgen", __xlTestCgenXlpNativeFuncsVoid },
	{ "cgen.xlp_native_funcs_mixed_params", "cgen", __xlTestCgenXlpNativeFuncsMixedParams },
	{ "cgen.xlp_native_funcs_bool", "cgen", __xlTestCgenXlpNativeFuncsBool },
	{ "cgen.xlp_native_funcs_bool_params", "cgen", __xlTestCgenXlpNativeFuncsBoolParams },
	{ "cgen.xlp_native_funcs_float", "cgen", __xlTestCgenXlpNativeFuncsFloat },
	{ "cgen.xlp_native_funcs_float_params", "cgen", __xlTestCgenXlpNativeFuncsFloatParams },
	{ "cgen.hello_world", "cgen", __xlTestCgenHelloWorld },
	{ "cgen.hello_world_io", "cgen", __xlTestCgenHelloWorldIo },
	{ "cgen.time_basic", "cgen", __xlTestCgenTimeBasic },
	{ "cgen.path_basic", "cgen", __xlTestCgenPathBasic },
	{ "cgen.fs_basic", "cgen", __xlTestCgenFsBasic },
	{ "cgen.str_basic", "cgen", __xlTestCgenStrBasic },
	{ "cgen.process_basic", "cgen", __xlTestCgenProcessBasic },
	{ "cgen.fs_plus", "cgen", __xlTestCgenFsPlus },
	{ "cgen.path_plus", "cgen", __xlTestCgenPathPlus },
	{ "cgen.str_plus", "cgen", __xlTestCgenStrPlus },
	{ "cgen.rand_basic", "cgen", __xlTestCgenRandBasic },
	{ "cgen.time_text", "cgen", __xlTestCgenTimeText },
	{ "cgen.host_basic", "cgen", __xlTestCgenHostBasic },
	{ "cgen.fmt_basic", "cgen", __xlTestCgenFmtBasic },
	{ "cgen.env_basic", "cgen", __xlTestCgenEnvBasic },
	{ "cgen.dir_basic", "cgen", __xlTestCgenDirBasic },
	{ "cgen.process_in_basic", "cgen", __xlTestCgenProcessInBasic },
	{ "cgen.stdlib_pipeline", "cgen", __xlTestCgenStdlibPipeline },
	{ "cgen.task_basic", "cgen", __xlTestCgenTask },
	{ "cgen.task_value_basic", "cgen", __xlTestCgenTaskValueBasic },
	{ "cgen.task_all", "cgen", __xlTestCgenTaskAll },
	{ "cgen.task_delay", "cgen", __xlTestCgenTaskDelay },
	{ "cgen.task_error", "cgen", __xlTestCgenTaskError },
	{ "cgen.async_generator_basic", "cgen", __xlTestCgenAsyncGeneratorBasic },
	{ "cgen.async_generator_while_basic", "cgen", __xlTestCgenAsyncGeneratorWhileBasic },
	{ "cgen.async_generator_loop_control_basic", "cgen", __xlTestCgenAsyncGeneratorLoopControlBasic },
	{ "cgen.async_generator_resume_basic", "cgen", __xlTestCgenAsyncGeneratorResumeBasic },
	{ "cgen.async_generator_multi_await_group_basic", "cgen", __xlTestCgenAsyncGeneratorMultiAwaitGroupBasic },
	{ "cgen.async_generator_try_catch_basic", "cgen", __xlTestCgenAsyncGeneratorTryCatchBasic },
	{ "cgen.async_generator_try_catch_const_else_basic", "cgen", __xlTestCgenAsyncGeneratorTryCatchConstElseBasic },
	{ "cgen.async_generator_try_finally_basic", "cgen", __xlTestCgenAsyncGeneratorTryFinallyBasic },
	{ "cgen.async_generator_try_finally_const_else_basic", "cgen", __xlTestCgenAsyncGeneratorTryFinallyConstElseBasic },
	{ "cgen.async_generator_nested_try_flow_basic", "cgen", __xlTestCgenAsyncGeneratorNestedTryFlowBasic },
	{ "cgen.async_generator_nested_try_flow_const_else_basic", "cgen", __xlTestCgenAsyncGeneratorNestedTryFlowConstElseBasic },
	{ "cgen.generator_resume_basic", "cgen", __xlTestCgenGeneratorResumeBasic },
	{ "cgen.generator_run", "cgen", __xlTestCgenGeneratorRun },
	{ "cgen.generator_lazy_throw", "cgen", __xlTestCgenGeneratorLazyThrow },
	{ "cgen.generator_var_for_in_basic", "cgen", __xlTestCgenGeneratorVarForInBasic },
	{ "cgen.generator_param_resume_basic", "cgen", __xlTestCgenGeneratorParamResumeBasic },
	{ "cgen.generator_return_wrapper_basic", "cgen", __xlTestCgenGeneratorReturnWrapperBasic },
	{ "cgen.generator_return_abstract_branch_resume_basic", "cgen", __xlTestCgenGeneratorReturnAbstractBranchResumeBasic },
	{ "cgen.generator_return_abstract_branch_for_in_basic", "cgen", __xlTestCgenGeneratorReturnAbstractBranchForInBasic },
	{ "cgen.generator_state_capture", "cgen", __xlTestCgenGeneratorStateCapture },
	{ "cgen.generator_while_basic", "cgen", __xlTestCgenGeneratorWhileBasic },
	{ "cgen.generator_loop_control_basic", "cgen", __xlTestCgenGeneratorLoopControlBasic },
	{ "cgen.generator_try_catch_basic", "cgen", __xlTestCgenGeneratorTryCatchBasic },
	{ "cgen.generator_try_finally_basic", "cgen", __xlTestCgenGeneratorTryFinallyBasic },
	{ "cgen.generator_nested_try_flow_basic", "cgen", __xlTestCgenGeneratorNestedTryFlowBasic },
	{ "cgen.try_finally_escape_basic", "cgen", __xlTestCgenTryFinallyEscapeBasic },
	{ "cgen.loop_jump_levels", "cgen", __xlTestCgenLoopJumpLevels },
	{ "host.run_c", "host", __xlTestHostRunC },
	{ "host.build_c", "host", __xlTestHostBuildC },
	{ "host.build_xlang", "host", __xlTestHostBuildXlang },
	{ "runtime.run_basic", "runtime", __xlTestRunBasic },
	{ "runtime.try_catch_basic", "runtime", __xlTestRunTryCatch },
	{ "runtime.try_nested_rethrow", "runtime", __xlTestRunTryNestedRethrow },
	{ "runtime.error_fields", "runtime", __xlTestRunErrorFields },
	{ "runtime.try_finally_basic", "runtime", __xlTestRunTryFinally },
	{ "runtime.try_finally_escape_basic", "runtime", __xlTestRunTryFinallyEscapeBasic },
	{ "runtime.try_catch_finally", "runtime", __xlTestRunTryCatchFinally },
	{ "runtime.named_args_default", "runtime", __xlTestRunNamedArgsDefault },
	{ "runtime.throw_basic", "runtime", __xlTestRunThrow },
	{ "runtime.assert_message", "runtime", __xlTestRunAssertMessage },
	{ "runtime.native_error_bridge", "runtime", __xlTestRunNativeErrorBridge },
	{ "runtime.module_use_math", "runtime", __xlTestRunModule },
	{ "runtime.export_type_method", "runtime", __xlTestRunExportTypeMethod },
	{ "runtime.export_generic_type_method", "runtime", __xlTestRunExportGenericTypeMethod },
	{ "runtime.module_init_throw", "runtime", __xlTestRunModuleInitThrow },
	{ "runtime.xlp_import", "runtime", __xlTestRunXlpImport },
	{ "runtime.xlp_native", "runtime", __xlTestRunXlpNative },
	{ "runtime.xlp_native_fallback_shared", "runtime", __xlTestRunXlpNativeFallbackShared },
	{ "runtime.xlp_native_funcs", "runtime", __xlTestRunXlpNativeFuncs },
	{ "runtime.xlp_native_funcs_params", "runtime", __xlTestRunXlpNativeFuncsParams },
	{ "runtime.xlp_native_funcs_string", "runtime", __xlTestRunXlpNativeFuncsString },
	{ "runtime.xlp_native_funcs_string_params", "runtime", __xlTestRunXlpNativeFuncsStringParams },
	{ "runtime.xlp_native_funcs_void", "runtime", __xlTestRunXlpNativeFuncsVoid },
	{ "runtime.xlp_native_funcs_mixed_params", "runtime", __xlTestRunXlpNativeFuncsMixedParams },
	{ "runtime.xlp_native_funcs_bool", "runtime", __xlTestRunXlpNativeFuncsBool },
	{ "runtime.xlp_native_funcs_bool_params", "runtime", __xlTestRunXlpNativeFuncsBoolParams },
	{ "runtime.xlp_native_funcs_float", "runtime", __xlTestRunXlpNativeFuncsFloat },
	{ "runtime.xlp_native_funcs_float_params", "runtime", __xlTestRunXlpNativeFuncsFloatParams },
	{ "runtime.xlp_static", "runtime", __xlTestRunXlpStatic },
	{ "runtime.xlp_static_funcs", "runtime", __xlTestRunXlpStaticFuncs },
	{ "runtime.xlp_static_funcs_params", "runtime", __xlTestRunXlpStaticFuncsParams },
	{ "runtime.xlp_static_funcs_string", "runtime", __xlTestRunXlpStaticFuncsString },
	{ "runtime.xlp_static_funcs_string_params", "runtime", __xlTestRunXlpStaticFuncsStringParams },
	{ "runtime.xlp_static_funcs_void", "runtime", __xlTestRunXlpStaticFuncsVoid },
	{ "runtime.xlp_static_funcs_mixed_params", "runtime", __xlTestRunXlpStaticFuncsMixedParams },
	{ "runtime.xlp_static_funcs_bool", "runtime", __xlTestRunXlpStaticFuncsBool },
	{ "runtime.xlp_static_funcs_bool_params", "runtime", __xlTestRunXlpStaticFuncsBoolParams },
	{ "runtime.xlp_static_funcs_float", "runtime", __xlTestRunXlpStaticFuncsFloat },
	{ "runtime.xlp_static_funcs_float_params", "runtime", __xlTestRunXlpStaticFuncsFloatParams },
	{ "runtime.xlp_source", "runtime", __xlTestRunXlpSource },
	{ "runtime.xlp_transitive", "runtime", __xlTestRunXlpTransitive },
	{ "runtime.xlp_versions", "runtime", __xlTestRunXlpVersions },
	{ "runtime.overload_abi", "runtime", __xlTestRunOverloadAbi },
	{ "runtime.native_std_core", "runtime", __xlTestRunNative },
	{ "runtime.container_methods_basic", "runtime", __xlTestRunContainerMethodsBasic },
	{ "runtime.container_append_basic", "runtime", __xlTestRunContainerAppendBasic },
	{ "runtime.container_clear_basic", "runtime", __xlTestRunContainerClearBasic },
	{ "runtime.container_remove_basic", "runtime", __xlTestRunContainerRemoveBasic },
	{ "runtime.vararg_array_basic", "runtime", __xlTestRunVarArgArrayBasic },
	{ "runtime.vararg_dict_basic", "runtime", __xlTestRunVarArgDictBasic },
	{ "runtime.multi_return_basic", "runtime", __xlTestRunMultiReturnBasic },
	{ "runtime.multi_return_call_expand_basic", "runtime", __xlTestRunMultiReturnCallExpandBasic },
	{ "runtime.function_value_basic", "runtime", __xlTestRunFunctionValueBasic },
	{ "runtime.function_value_flow_basic", "runtime", __xlTestRunFunctionValueFlowBasic },
	{ "runtime.function_value_container_basic", "runtime", __xlTestRunFunctionValueContainerBasic },
	{ "runtime.function_value_paren_call_basic", "runtime", __xlTestRunFunctionValueParenCallBasic },
	{ "runtime.function_value_overload_basic", "runtime", __xlTestRunFunctionValueOverloadBasic },
	{ "runtime.bare_container_literals", "runtime", __xlTestRunBareContainerLiterals },
	{ "runtime.core_len_basic", "runtime", __xlTestRunCoreLenBasic },
	{ "runtime.type_methods_basic", "runtime", __xlTestRunTypeMethodsBasic },
	{ "runtime.var_methods_basic", "runtime", __xlTestRunVarMethodsBasic },
	{ "runtime.var_type_methods_basic", "runtime", __xlTestRunVarTypeMethodsBasic },
	{ "runtime.var_container_type_methods_basic", "runtime", __xlTestRunVarContainerTypeMethodsBasic },
	{ "runtime.property_length_basic", "runtime", __xlTestRunPropertyLengthBasic },
	{ "runtime.string_methods_basic", "runtime", __xlTestRunStringMethodsBasic },
	{ "runtime.string_split_basic", "runtime", __xlTestRunStringSplitBasic },
	{ "runtime.import_alias", "runtime", __xlTestRunImportAlias },
	{ "runtime.import_multi", "runtime", __xlTestRunImportMulti },
	{ "runtime.import_string", "runtime", __xlTestRunImportString },
	{ "runtime.export_func", "runtime", __xlTestRunExportFunc },
	{ "runtime.export_values", "runtime", __xlTestRunExportValues },
	{ "runtime.typed_decl", "runtime", __xlTestRunTypedDecl },
	{ "runtime.exact_numeric_types", "runtime", __xlTestRunExactNumericTypes },
	{ "runtime.auto_decl", "runtime", __xlTestRunAutoDecl },
	{ "runtime.typedef_basic", "runtime", __xlTestRunTypeDefBasic },
	{ "runtime.type_ctor_basic", "runtime", __xlTestRunTypeCtorBasic },
	{ "runtime.point_basic", "runtime", __xlTestRunPointBasic },
	{ "runtime.assign_basic", "runtime", __xlTestRunAssignBasic },
	{ "runtime.assign_plus_basic", "runtime", __xlTestRunAssignPlusBasic },
	{ "runtime.assign_minus_basic", "runtime", __xlTestRunAssignMinusBasic },
	{ "runtime.assign_mul_basic", "runtime", __xlTestRunAssignMulBasic },
	{ "runtime.assign_div_basic", "runtime", __xlTestRunAssignDivBasic },
	{ "runtime.assign_mod_basic", "runtime", __xlTestRunAssignModBasic },
	{ "runtime.power_basic", "runtime", __xlTestRunPowerBasic },
	{ "runtime.assign_pow_basic", "runtime", __xlTestRunAssignPowBasic },
	{ "runtime.coalesce_basic", "runtime", __xlTestRunCoalesceBasic },
	{ "runtime.null_assign_basic", "runtime", __xlTestRunNullAssignBasic },
	{ "runtime.ternary_basic", "runtime", __xlTestRunTernaryBasic },
	{ "runtime.approx_basic", "runtime", __xlTestRunApproxBasic },
	{ "runtime.if_inline_basic", "runtime", __xlTestRunIfInlineBasic },
	{ "runtime.if_inline_else_basic", "runtime", __xlTestRunIfInlineElseBasic },
	{ "runtime.logic_basic", "runtime", __xlTestRunLogicBasic },
	{ "runtime.bytes_alias_string", "runtime", __xlTestRunBytesAliasString },
	{ "runtime.template_basic", "runtime", __xlTestRunTemplateBasic },
	{ "runtime.safe_member_basic", "runtime", __xlTestRunSafeMemberBasic },
	{ "runtime.leading_blank_lines", "runtime", __xlTestRunLeadingBlankLines },
	{ "runtime.var_basic", "runtime", __xlTestRunVarBasic },
	{ "runtime.array_basic", "runtime", __xlTestRunArrayBasic },
	{ "runtime.list_basic", "runtime", __xlTestRunListBasic },
	{ "runtime.set_basic", "runtime", __xlTestRunSetBasic },
	{ "runtime.dict_basic", "runtime", __xlTestRunDictBasic },
	{ "runtime.index_assign_basic", "runtime", __xlTestRunIndexAssignBasic },
	{ "runtime.while_basic", "runtime", __xlTestRunWhileBasic },
	{ "runtime.do_while_basic", "runtime", __xlTestRunDoWhileBasic },
	{ "runtime.hello_world", "runtime", __xlTestRunHelloWorld },
	{ "runtime.hello_world_io", "runtime", __xlTestRunHelloWorldIo },
	{ "runtime.time_basic", "runtime", __xlTestRunTimeBasic },
	{ "runtime.path_basic", "runtime", __xlTestRunPathBasic },
	{ "runtime.fs_basic", "runtime", __xlTestRunFsBasic },
	{ "runtime.str_basic", "runtime", __xlTestRunStrBasic },
	{ "runtime.process_basic", "runtime", __xlTestRunProcessBasic },
	{ "runtime.fs_plus", "runtime", __xlTestRunFsPlus },
	{ "runtime.path_plus", "runtime", __xlTestRunPathPlus },
	{ "runtime.str_plus", "runtime", __xlTestRunStrPlus },
	{ "runtime.rand_basic", "runtime", __xlTestRunRandBasic },
	{ "runtime.time_text", "runtime", __xlTestRunTimeText },
	{ "runtime.host_basic", "runtime", __xlTestRunHostBasic },
	{ "runtime.fmt_basic", "runtime", __xlTestRunFmtBasic },
	{ "runtime.env_basic", "runtime", __xlTestRunEnvBasic },
	{ "runtime.dir_basic", "runtime", __xlTestRunDirBasic },
	{ "runtime.process_in_basic", "runtime", __xlTestRunProcessInBasic },
	{ "runtime.stdlib_pipeline", "runtime", __xlTestRunStdlibPipeline },
	{ "runtime.async_run_main", "runtime", __xlTestRunAsyncMain },
	{ "runtime.async_bind_await_return_basic", "runtime", __xlTestRunAsyncBindAwaitReturnBasic },
	{ "runtime.async_bind_expr_await_basic", "runtime", __xlTestRunAsyncBindExprAwaitBasic },
	{ "runtime.async_assign_arg_await_basic", "runtime", __xlTestRunAsyncAssignArgAwaitBasic },
	{ "runtime.async_assign_string_arg_await_basic", "runtime", __xlTestRunAsyncAssignStringArgAwaitBasic },
	{ "runtime.async_assign_multi_await_basic", "runtime", __xlTestRunAsyncAssignMultiAwaitBasic },
	{ "runtime.async_string_expr_await_basic", "runtime", __xlTestRunAsyncStringExprAwaitBasic },
	{ "runtime.async_if_body_prelude_then_await_basic", "runtime", __xlTestRunAsyncIfBodyPreludeThenAwaitBasic },
	{ "runtime.async_if_dynamic_then_await_basic", "runtime", __xlTestRunAsyncIfDynamicThenAwaitBasic },
	{ "runtime.async_if_dynamic_else_sync_basic", "runtime", __xlTestRunAsyncIfDynamicElseSyncBasic },
	{ "runtime.async_if_dynamic_else_await_basic", "runtime", __xlTestRunAsyncIfDynamicElseAwaitBasic },
	{ "runtime.async_if_dynamic_both_await_then_basic", "runtime", __xlTestRunAsyncIfDynamicBothAwaitThenBasic },
	{ "runtime.async_if_dynamic_both_await_else_basic", "runtime", __xlTestRunAsyncIfDynamicBothAwaitElseBasic },
	{ "runtime.async_if_nested_dynamic_both_await_prelude_then_basic", "runtime", __xlTestRunAsyncIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "runtime.async_if_nested_dynamic_both_await_prelude_else_basic", "runtime", __xlTestRunAsyncIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "runtime.async_if_nested_dynamic_both_await_const_else_then_basic", "runtime", __xlTestRunAsyncIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "runtime.async_if_nested_dynamic_both_await_const_else_else_basic", "runtime", __xlTestRunAsyncIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "runtime.async_try_if_body_prelude_then_await_basic", "runtime", __xlTestRunAsyncTryIfBodyPreludeThenAwaitBasic },
	{ "runtime.async_try_if_dynamic_then_await_basic", "runtime", __xlTestRunAsyncTryIfDynamicThenAwaitBasic },
	{ "runtime.async_try_if_dynamic_else_sync_basic", "runtime", __xlTestRunAsyncTryIfDynamicElseSyncBasic },
	{ "runtime.async_try_if_dynamic_else_await_basic", "runtime", __xlTestRunAsyncTryIfDynamicElseAwaitBasic },
	{ "runtime.async_try_if_dynamic_both_await_then_basic", "runtime", __xlTestRunAsyncTryIfDynamicBothAwaitThenBasic },
	{ "runtime.async_try_if_dynamic_both_await_else_basic", "runtime", __xlTestRunAsyncTryIfDynamicBothAwaitElseBasic },
	{ "runtime.async_try_if_dynamic_both_await_prelude_then_basic", "runtime", __xlTestRunAsyncTryIfDynamicBothAwaitPreludeThenBasic },
	{ "runtime.async_try_if_dynamic_both_await_prelude_else_basic", "runtime", __xlTestRunAsyncTryIfDynamicBothAwaitPreludeElseBasic },
	{ "runtime.async_try_if_nested_dynamic_both_await_prelude_then_basic", "runtime", __xlTestRunAsyncTryIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "runtime.async_try_if_nested_dynamic_both_await_prelude_else_basic", "runtime", __xlTestRunAsyncTryIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "runtime.async_try_if_nested_dynamic_both_await_const_else_then_basic", "runtime", __xlTestRunAsyncTryIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "runtime.async_try_if_nested_dynamic_both_await_const_else_else_basic", "runtime", __xlTestRunAsyncTryIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "runtime.async_if_cond_await_basic", "runtime", __xlTestRunAsyncIfCondAwaitBasic },
	{ "runtime.async_if_cond_then_await_basic", "runtime", __xlTestRunAsyncIfCondThenAwaitBasic },
	{ "runtime.async_if_cond_container_then_await_basic", "runtime", __xlTestRunAsyncIfCondContainerThenAwaitBasic },
	{ "runtime.async_while_cond_await_basic", "runtime", __xlTestRunAsyncWhileCondAwaitBasic },
	{ "runtime.async_while_cond_then_await_basic", "runtime", __xlTestRunAsyncWhileCondThenAwaitBasic },
	{ "runtime.async_while_cond_container_then_await_basic", "runtime", __xlTestRunAsyncWhileCondContainerThenAwaitBasic },
	{ "runtime.async_while_cond_break_basic", "runtime", __xlTestRunAsyncWhileCondBreakBasic },
	{ "runtime.async_while_cond_continue_basic", "runtime", __xlTestRunAsyncWhileCondContinueBasic },
	{ "runtime.async_while_cond_break_then_await_basic", "runtime", __xlTestRunAsyncWhileCondBreakThenAwaitBasic },
	{ "runtime.async_while_cond_continue_then_await_basic", "runtime", __xlTestRunAsyncWhileCondContinueThenAwaitBasic },
	{ "runtime.async_do_while_cond_await_basic", "runtime", __xlTestRunAsyncDoWhileCondAwaitBasic },
	{ "runtime.async_do_while_cond_then_await_basic", "runtime", __xlTestRunAsyncDoWhileCondThenAwaitBasic },
	{ "runtime.async_do_while_cond_container_then_await_basic", "runtime", __xlTestRunAsyncDoWhileCondContainerThenAwaitBasic },
	{ "runtime.async_do_while_cond_break_basic", "runtime", __xlTestRunAsyncDoWhileCondBreakBasic },
	{ "runtime.async_do_while_cond_continue_basic", "runtime", __xlTestRunAsyncDoWhileCondContinueBasic },
	{ "runtime.async_do_while_cond_break_then_await_basic", "runtime", __xlTestRunAsyncDoWhileCondBreakThenAwaitBasic },
	{ "runtime.async_do_while_cond_continue_then_await_basic", "runtime", __xlTestRunAsyncDoWhileCondContinueThenAwaitBasic },
	{ "runtime.async_try_finally_await_basic", "runtime", __xlTestRunAsyncTryFinallyAwaitBasic },
	{ "runtime.async_try_finally_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_await_basic", "runtime", __xlTestRunAsyncTryCatchFinallyAwaitBasic },
	{ "runtime.async_try_catch_finally_await_const_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_then_await_basic", "runtime", __xlTestRunAsyncTryCatchFinallyThenAwaitBasic },
	{ "runtime.async_try_catch_finally_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyThenAwaitConstElseBasic },
	{ "runtime.async_try_finally_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyThenAwaitBasic },
	{ "runtime.async_try_finally_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyThenAwaitConstElseBasic },
	{ "runtime.async_try_finally_tail_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyTailThenAwaitBasic },
	{ "runtime.async_try_finally_tail_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyTailThenAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_tail_then_await_basic", "runtime", __xlTestRunAsyncTryCatchFinallyTailThenAwaitBasic },
	{ "runtime.async_try_catch_finally_tail_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyTailThenAwaitConstElseBasic },
	{ "runtime.async_try_finally_if_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyIfThenAwaitBasic },
	{ "runtime.async_try_finally_if_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyIfThenAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_while_then_await_basic", "runtime", __xlTestRunAsyncTryCatchFinallyWhileThenAwaitBasic },
	{ "runtime.async_try_catch_finally_while_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyWhileThenAwaitConstElseBasic },
	{ "runtime.async_try_finally_prelude_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyPreludeThenAwaitBasic },
	{ "runtime.async_try_finally_prelude_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyPreludeThenAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_prelude_then_await_basic", "runtime", __xlTestRunAsyncTryCatchFinallyPreludeThenAwaitBasic },
	{ "runtime.async_try_catch_finally_prelude_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyPreludeThenAwaitConstElseBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitThenBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitElseBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_success_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitSuccessThenBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_success_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitSuccessElseBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_prelude_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitPreludeThenBasic },
	{ "runtime.async_try_catch_finally_if_dynamic_both_await_prelude_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfDynamicBothAwaitPreludeElseBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_const_else_success_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessThenBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_const_else_success_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseSuccessElseBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_const_else_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseThenBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_const_else_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitConstElseElseBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_success_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessThenBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_success_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitSuccessElseBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_prelude_then_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeThenBasic },
	{ "runtime.async_try_catch_finally_if_nested_dynamic_both_await_prelude_else_basic", "runtime", __xlTestRunAsyncTryCatchFinallyIfNestedDynamicBothAwaitPreludeElseBasic },
	{ "runtime.async_try_finally_do_while_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyDoWhileThenAwaitBasic },
	{ "runtime.async_try_finally_do_while_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyDoWhileThenAwaitConstElseBasic },
	{ "runtime.async_try_finally_do_while_break_then_await_basic", "runtime", __xlTestRunAsyncTryFinallyDoWhileBreakThenAwaitBasic },
	{ "runtime.async_try_finally_do_while_break_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryFinallyDoWhileBreakThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_inner_catch_then_outer_finally_basic", "runtime", __xlTestRunAsyncTryNestedInnerCatchThenOuterFinallyBasic },
	{ "runtime.async_try_nested_inner_catch_then_outer_finally_const_else_basic", "runtime", __xlTestRunAsyncTryNestedInnerCatchThenOuterFinallyConstElseBasic },
	{ "runtime.async_try_nested_inner_finally_outer_catch_basic", "runtime", __xlTestRunAsyncTryNestedInnerFinallyOuterCatchBasic },
	{ "runtime.async_try_nested_inner_finally_outer_catch_const_else_basic", "runtime", __xlTestRunAsyncTryNestedInnerFinallyOuterCatchConstElseBasic },
	{ "runtime.async_try_nested_if_finally_then_await_basic", "runtime", __xlTestRunAsyncTryNestedIfFinallyThenAwaitBasic },
	{ "runtime.async_try_nested_if_finally_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedIfFinallyThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_if_dynamic_both_await_inner_finally_outer_catch_then_basic", "runtime", __xlTestRunAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchThenBasic },
	{ "runtime.async_try_nested_if_dynamic_both_await_inner_finally_outer_catch_else_basic", "runtime", __xlTestRunAsyncTryNestedIfDynamicBothAwaitInnerFinallyOuterCatchElseBasic },
	{ "runtime.async_try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_then_basic", "runtime", __xlTestRunAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchThenBasic },
	{ "runtime.async_try_nested_if_dynamic_both_await_const_else_inner_finally_outer_catch_else_basic", "runtime", __xlTestRunAsyncTryNestedIfDynamicBothAwaitConstElseInnerFinallyOuterCatchElseBasic },
	{ "runtime.async_try_nested_while_finally_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryNestedWhileFinallyOuterCatchThenAwaitBasic },
	{ "runtime.async_try_nested_while_finally_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedWhileFinallyOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_do_while_finally_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitBasic },
	{ "runtime.async_try_nested_do_while_finally_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedDoWhileFinallyOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_do_while_continue_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryNestedDoWhileContinueOuterCatchThenAwaitBasic },
	{ "runtime.async_try_nested_do_while_continue_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedDoWhileContinueOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_full_chain_inner_catch_then_await_basic", "runtime", __xlTestRunAsyncTryNestedFullChainInnerCatchThenAwaitBasic },
	{ "runtime.async_try_nested_full_chain_inner_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedFullChainInnerCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_nested_full_chain_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryNestedFullChainOuterCatchThenAwaitBasic },
	{ "runtime.async_try_nested_full_chain_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryNestedFullChainOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_triple_nested_inner_catch_then_await_basic", "runtime", __xlTestRunAsyncTryTripleNestedInnerCatchThenAwaitBasic },
	{ "runtime.async_try_triple_nested_inner_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryTripleNestedInnerCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_triple_nested_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryTripleNestedOuterCatchThenAwaitBasic },
	{ "runtime.async_try_triple_nested_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryTripleNestedOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_try_triple_nested_while_outer_catch_then_await_basic", "runtime", __xlTestRunAsyncTryTripleNestedWhileOuterCatchThenAwaitBasic },
	{ "runtime.async_try_triple_nested_while_outer_catch_then_await_const_else_basic", "runtime", __xlTestRunAsyncTryTripleNestedWhileOuterCatchThenAwaitConstElseBasic },
	{ "runtime.async_call_arg_await_basic", "runtime", __xlTestRunAsyncCallArgAwaitBasic },
	{ "runtime.async_function_value_arg_await_basic", "runtime", __xlTestRunAsyncFunctionValueArgAwaitBasic },
	{ "runtime.async_string_method_arg_await_basic", "runtime", __xlTestRunAsyncStringMethodArgAwaitBasic },
	{ "runtime.async_container_method_arg_await_basic", "runtime", __xlTestRunAsyncContainerMethodArgAwaitBasic },
	{ "runtime.async_container_join_arg_await_basic", "runtime", __xlTestRunAsyncContainerJoinArgAwaitBasic },
	{ "runtime.async_container_dict_has_arg_await_basic", "runtime", __xlTestRunAsyncContainerDictHasArgAwaitBasic },
	{ "runtime.async_container_dict_remove_arg_await_basic", "runtime", __xlTestRunAsyncContainerDictRemoveArgAwaitBasic },
	{ "runtime.async_container_list_remove_arg_await_basic", "runtime", __xlTestRunAsyncContainerListRemoveArgAwaitBasic },
	{ "runtime.async_container_list_join_arg_await_basic", "runtime", __xlTestRunAsyncContainerListJoinArgAwaitBasic },
	{ "runtime.async_container_list_append_arg_await_basic", "runtime", __xlTestRunAsyncContainerListAppendArgAwaitBasic },
	{ "runtime.async_container_dict_assign_arg_await_basic", "runtime", __xlTestRunAsyncContainerDictAssignArgAwaitBasic },
	{ "runtime.async_container_array_index_assign_arg_await_basic", "runtime", __xlTestRunAsyncContainerArrayIndexAssignArgAwaitBasic },
	{ "runtime.async_container_list_index_assign_arg_await_basic", "runtime", __xlTestRunAsyncContainerListIndexAssignArgAwaitBasic },
	{ "runtime.async_container_array_append_arg_await_basic", "runtime", __xlTestRunAsyncContainerArrayAppendArgAwaitBasic },
	{ "runtime.async_container_array_remove_arg_await_basic", "runtime", __xlTestRunAsyncContainerArrayRemoveArgAwaitBasic },
	{ "runtime.async_container_set_remove_arg_await_basic", "runtime", __xlTestRunAsyncContainerSetRemoveArgAwaitBasic },
	{ "runtime.async_multi_await", "runtime", __xlTestRunAsyncMultiAwait },
	{ "runtime.async_throw_main", "runtime", __xlTestRunAsyncThrowMain },
	{ "runtime.async_try_catch_throw", "runtime", __xlTestRunAsyncTryCatchThrow },
	{ "runtime.async_bool_main", "runtime", __xlTestRunAsyncBoolMain },
	{ "runtime.async_generator_basic", "runtime", __xlTestRunAsyncGeneratorBasic },
	{ "runtime.async_generator_while_basic", "runtime", __xlTestRunAsyncGeneratorWhileBasic },
	{ "runtime.async_generator_loop_control_basic", "runtime", __xlTestRunAsyncGeneratorLoopControlBasic },
	{ "runtime.async_generator_resume_basic", "runtime", __xlTestRunAsyncGeneratorResumeBasic },
	{ "runtime.async_generator_multi_await_group_basic", "runtime", __xlTestRunAsyncGeneratorMultiAwaitGroupBasic },
	{ "runtime.async_generator_try_catch_basic", "runtime", __xlTestRunAsyncGeneratorTryCatchBasic },
	{ "runtime.async_generator_try_catch_const_else_basic", "runtime", __xlTestRunAsyncGeneratorTryCatchConstElseBasic },
	{ "runtime.async_generator_try_finally_basic", "runtime", __xlTestRunAsyncGeneratorTryFinallyBasic },
	{ "runtime.async_generator_try_finally_const_else_basic", "runtime", __xlTestRunAsyncGeneratorTryFinallyConstElseBasic },
	{ "runtime.async_generator_nested_try_flow_basic", "runtime", __xlTestRunAsyncGeneratorNestedTryFlowBasic },
	{ "runtime.async_generator_nested_try_flow_const_else_basic", "runtime", __xlTestRunAsyncGeneratorNestedTryFlowConstElseBasic },
	{ "runtime.generator_resume_basic", "runtime", __xlTestRunGeneratorResumeBasic },
	{ "runtime.future_value_basic", "runtime", __xlTestRunFutureValueBasic },
	{ "runtime.channel_basic", "runtime", __xlTestRunChannelBasic },
	{ "runtime.channel_recv_unpack_basic", "runtime", __xlTestRunChannelRecvUnpackBasic },
	{ "runtime.type_value_basic", "runtime", __xlTestRunTypeValueBasic },
	{ "runtime.public_types_basic", "runtime", __xlTestRunPublicTypesBasic },
	{ "runtime.extern_atoi_basic", "runtime", __xlTestRunExternAtoiBasic },
	{ "runtime.generator_run", "runtime", __xlTestRunGeneratorRun },
	{ "runtime.generator_lazy_throw", "runtime", __xlTestRunGeneratorLazyThrow },
	{ "runtime.generator_var_for_in_basic", "runtime", __xlTestRunGeneratorVarForInBasic },
	{ "runtime.generator_param_resume_basic", "runtime", __xlTestRunGeneratorParamResumeBasic },
	{ "runtime.generator_return_wrapper_basic", "runtime", __xlTestRunGeneratorReturnWrapperBasic },
	{ "runtime.generator_return_abstract_branch_resume_basic", "runtime", __xlTestRunGeneratorReturnAbstractBranchResumeBasic },
	{ "runtime.generator_return_abstract_branch_for_in_basic", "runtime", __xlTestRunGeneratorReturnAbstractBranchForInBasic },
	{ "runtime.generator_state_capture", "runtime", __xlTestRunGeneratorStateCapture },
	{ "runtime.generator_while_basic", "runtime", __xlTestRunGeneratorWhileBasic },
	{ "runtime.generator_loop_control_basic", "runtime", __xlTestRunGeneratorLoopControlBasic },
	{ "runtime.generator_try_catch_basic", "runtime", __xlTestRunGeneratorTryCatchBasic },
	{ "runtime.generator_try_finally_basic", "runtime", __xlTestRunGeneratorTryFinallyBasic },
	{ "runtime.generator_nested_try_flow_basic", "runtime", __xlTestRunGeneratorNestedTryFlowBasic },
	{ "runtime.for_in_range", "runtime", __xlTestRunForIn },
	{ "runtime.for_in_containers", "runtime", __xlTestRunForInContainers },
	{ "runtime.loop_jump_levels", "runtime", __xlTestRunLoopJumpLevels },
	{ "runtime.task_basic", "runtime", __xlTestRunTask },
	{ "runtime.task_all", "runtime", __xlTestRunTaskAll },
	{ "runtime.task_value_basic", "runtime", __xlTestRunTaskValueBasic },
	{ "runtime.task_delay", "runtime", __xlTestRunTaskDelay },
	{ "runtime.task_error", "runtime", __xlTestRunTaskError }
};

static int __xlTestMatchesSelector(const XL_TestCase* pCase, const char* sSelector)
{
	if ( (pCase == NULL) || (sSelector == NULL) || (sSelector[0] == '\0') ) {
		return 1;
	}
	return (strstr(pCase->sName, sSelector) != NULL) || (strstr(pCase->sGroup, sSelector) != NULL);
}

static int __xlTestRunInternal(const char* sSelector)
{
	XL_TestContext ctx;
	size_t i;
	size_t iSelectedCount;
	int iRc;

	memset(&ctx, 0, sizeof(ctx));
	ctx.pOut = stdout;
	ctx.pSink = fopen("NUL", "w");
	iRc = __xlDetectProjectRoot(ctx.sProjectRoot, sizeof(ctx.sProjectRoot));
 	if ( iRc != XL_RC_OK ) {
 		fprintf(stdout, "[FAIL] test.init: failed to detect project root\n");
 		fflush(stdout);
 		if ( ctx.pSink != NULL ) {
 			fclose(ctx.pSink);
 		}
 		return 1;
 	}
	__xlCopyText(ctx.sReleaseRoot, sizeof(ctx.sReleaseRoot), ctx.sProjectRoot);
	__xlAppendPathPart(ctx.sReleaseRoot, sizeof(ctx.sReleaseRoot), "release");

 	fprintf(stdout, "demo5 unified test runner\n");
 	fprintf(stdout, "project=demo5\n");
 	fprintf(stdout, "build_stamp=%s\n", __gXLTestBuildStamp);
 	fprintf(stdout, "test_count=%llu\n", (unsigned long long)XL_COUNTOF(__gXLTests));
 	if ( (sSelector != NULL) && (sSelector[0] != '\0') ) {
 		fprintf(stdout, "selector=%s\n", sSelector);
 	}
 	fflush(stdout);

	iSelectedCount = 0;
	for ( i = 0; i < XL_COUNTOF(__gXLTests); ++i ) {
		if ( !__xlTestMatchesSelector(&__gXLTests[i], sSelector) ) {
			continue;
		}
		iSelectedCount += 1;
		iRc = __gXLTests[i].fnRun(&ctx);
 		if ( iRc == XL_RC_OK ) {
 			ctx.iPassCount += 1;
 			fprintf(stdout, "[PASS] %s\n", __gXLTests[i].sName);
 		} else {
 			ctx.iFailCount += 1;
 			fprintf(stdout, "[FAIL] %s rc=%d\n", __gXLTests[i].sName, iRc);
 		}
 		fflush(stdout);
 	}

	if ( ctx.pSink != NULL ) {
		fclose(ctx.pSink);
	}

 	if ( iSelectedCount == 0 ) {
 		fprintf(stdout, "[FAIL] no tests matched selector\n");
 		fflush(stdout);
 		return 1;
 	}

 	fprintf(
 		stdout,
 		"summary: total=%llu pass=%d fail=%d\n",
 		(unsigned long long)iSelectedCount,
 		ctx.iPassCount,
 		ctx.iFailCount
 	);
 	fflush(stdout);
 	return (ctx.iFailCount == 0) ? 0 : 1;
 }

int xlTestRunAll(void)
{
	return __xlTestRunInternal(NULL);
}

int xlTestRunSelector(const char* sSelector)
{
	return __xlTestRunInternal(sSelector);
}
