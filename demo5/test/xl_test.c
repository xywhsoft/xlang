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
		iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
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
		iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
	}
	if ( (iRc == XL_RC_OK) && (ppNeedles != NULL) ) {
		for ( i = 0; i < iNeedleCount; ++i ) {
			if ( (ppNeedles[i] != NULL) && (strstr(sCode, ppNeedles[i]) == NULL) ) {
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

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;
	sTraceText = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
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
				iRc = XL_RC_FAIL;
			} else if ( (sNeedle != NULL) && !__xlTraceContains(sTraceText, sNeedle) ) {
				iRc = XL_RC_FAIL;
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

	__xlBuildProjectPath(pCtx, sPath, sizeof(sPath), sRelativePath);
	__xlInitSilentDiag(pCtx, &diag);
	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;
	sTraceText = NULL;

	iRc = xlModuleLinkFromFile(sPath, &diag, 0, &moduleResult);
	if ( iRc == XL_RC_OK ) {
		iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, &diag, 0, &semResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
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
				iRc = XL_RC_FAIL;
			} else if ( ppNeedles != NULL ) {
				for ( i = 0; i < iNeedleCount; ++i ) {
					if ( (ppNeedles[i] != NULL) && !__xlTraceContains(sTraceText, ppNeedles[i]) ) {
						iRc = XL_RC_FAIL;
						break;
					}
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
	if ( iRc == XL_RC_OK ) iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
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
static int __xlTestParseExportFunc(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\math_export_func.xl", 1, XL_AST_EXPORT, "FuncDecl"); }
static int __xlTestParseExportTypeMethod(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\text_ext_method.xl", 1, XL_AST_EXPORT, "text=\"wrap\""); }
static int __xlTestParseExportValues(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\module\\math_export_values.xl", 1, XL_AST_EXPORT, "VarDecl"); }
static int __xlTestParseTypedDecl(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\typed_decl_basic.xl", 1, XL_AST_VAR_DECL, "text=\"0\""); }
static int __xlTestParseExactNumericTypes(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\exact_numeric_types_basic.xl", 1, XL_AST_TYPE_REF, "text=\"int32\""); }
static int __xlTestParseAutoDecl(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\auto_decl_basic.xl", 1, XL_AST_VAR_DECL); }
static int __xlTestParseTypeDefBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\typedef_basic.xl", 1, XL_AST_TYPEDEF, "text=\"Text\""); }
static int __xlTestParsePointBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\point_basic.xl", 1, XL_AST_TYPE_REF, "text=\"point\""); }
static int __xlTestParseAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\async\\run_bool_main.xl", 1, XL_AST_TYPE_REF, "text=\"bool\""); }
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
static int __xlTestParseIndexAssignBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\index_assign_basic.xl", 1, XL_AST_ASSIGN, "Index"); }
static int __xlTestParseForInContainers(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\for_in_containers_basic.xl", 1, XL_AST_FORIN); }
static int __xlTestParseContainerMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_methods_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerAppendBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_append_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerClearBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_clear_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseContainerRemoveBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\container_remove_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseCoreLenBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\native\\core_len_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, XL_AST_FUNC_DECL, "text2=\"wrap\""); }
static int __xlTestParsePropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\property_length_basic.xl", 1, XL_AST_MEMBER); }
static int __xlTestParseStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseStringSplitBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\string_split_basic.xl", 1, XL_AST_CALL); }
static int __xlTestParseWhileBasic(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\while_basic.xl", 1, XL_AST_WHILE); }
static int __xlTestParseDoWhileBasic(XL_TestContext* pCtx) { return __xlRunParseDumpCase(pCtx, "examples\\full\\do_while_basic.xl", 1, XL_AST_WHILE, "flag=1"); }
static int __xlTestParseSyntaxError(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\parser\\syntax_error.xl", 0, -1); }
static int __xlTestSemOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\overload.xl", 1, "int"); }
static int __xlTestSemUnresolved(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\unresolved.xl", 0, NULL); }
static int __xlTestSemNoOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\no_overload.xl", 0, NULL); }
static int __xlTestSemForInGenerator(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\for_in_generator.xl", 1, "generator"); }
static int __xlTestSemTry(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, "error"); }
static int __xlTestSemErrorFields(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\error_fields_basic.xl", 1, "error"); }
static int __xlTestSemNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\named_args_default.xl", 1, "int"); }
static int __xlTestSemAwaitOutsideAsync(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\await_outside_async.xl", 0, NULL); }
static int __xlTestSemTryNestedRethrow(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\try_nested_rethrow.xl", 1, "error"); }
static int __xlTestSemAssertMessage(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\assert_message_basic.xl", 1, "error"); }
static int __xlTestSemIfInlineBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\if_inline_basic.xl", 1, "int"); }
static int __xlTestSemIfInlineElseBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\if_inline_else_basic.xl", 1, "int"); }
static int __xlTestSemNativeErrorBridge(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\native_error_bridge.xl", 1, "error"); }
static int __xlTestSemImportAlias(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_alias_basic.xl", 1, "int"); }
static int __xlTestSemImportMulti(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_multi_basic.xl", 1, "int"); }
static int __xlTestSemImportString(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\native\\import_string_basic.xl", 1, "int"); }
static int __xlTestSemExportFunc(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_math_export_func.xl", 1, "int"); }
static int __xlTestSemExportTypeMethod(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_text_ext_method.xl", 1, "int"); }
static int __xlTestSemExportValues(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\module\\use_math_export_values.xl", 1, "int"); }
static int __xlTestSemTypedDecl(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\typed_decl_basic.xl", 1, "int"); }
static int __xlTestSemExactNumericTypes(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\exact_numeric_types_basic.xl", 1, "int"); }
static int __xlTestSemAutoDecl(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\auto_decl_basic.xl", 1, "int"); }
static int __xlTestSemTypeDefBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\typedef_basic.xl", 1, "Text"); }
static int __xlTestSemTypeCtorBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\type_ctor_basic.xl", 1, "float64"); }
static int __xlTestSemPointBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\point_basic.xl", 1, "point"); }
static int __xlTestSemAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\async\\run_bool_main.xl", 1, "bool"); }
static int __xlTestSemTaskError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\task\\task_error_basic.xl", 1, NULL); }
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
static int __xlTestSemTypeMethodSelfError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\type_method_self_error.xl", 0, NULL); }
static int __xlTestSemPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\property_length_basic.xl", 1, "int"); }
static int __xlTestSemPropertyLengthError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\property_length_error.xl", 0, NULL); }
static int __xlTestSemStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, "int"); }
static int __xlTestSemStringSplitBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\string_split_basic.xl", 1, "int"); }
static int __xlTestSemStringSplitError(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\string_split_error.xl", 0, NULL); }
static int __xlTestSemWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\while_basic.xl", 1, "int"); }
static int __xlTestSemDoWhileBasic(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\full\\do_while_basic.xl", 1, "int"); }
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
		if ( (sJson == NULL) ||
			!__xlTraceContains(sJson, "\"dependencies\": []") ||
			!__xlTraceContains(sJson, "\"packageName\": \"use_math\"") ||
			!__xlTraceContains(sJson, "\"moduleAbiId\": \"use_math@0.0.0#abi1::math\"") ||
			!__xlTraceContains(sJson, "\"abiSymbol\": \"xl__xl_mod_math___xE5_x8A_xA0_xE4_xB8_x80__arity1__x__int__ret__int\"") ) {
			iRc = XL_RC_FAIL;
		}
	}

	free(sJson);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
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
static int __xlTestLowerPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\property_length_basic.xl", 1, XL_AST_MEMBER, 0, 0); }
static int __xlTestLowerStringMethodsBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\string_methods_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerStringSplitBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\string_split_basic.xl", 1, XL_AST_CALL, 0, 0); }
static int __xlTestLowerTry(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\try_catch_finally.xl", 1, XL_AST_TRY, 0, 0); }
static int __xlTestLowerNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\named_args_default.xl", 1, XL_AST_NAMED_ARG, 0, 0); }
static int __xlTestCgenModule(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\module\\use_math.xl", "xl_mod_math__"); }
static int __xlTestCgenExportTypeMethod(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"xl_mod_text_ext_method__wrap",
		"xl_mod_text_ext_method__exclaim"
	};

	return __xlRunCgenCaseAll(pCtx, "examples\\module\\use_text_ext_method.xl", aNeedles, XL_COUNTOF(aNeedles));
}
static int __xlTestCgenModuleInitThrow(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"static int xl_generated_module_init_done = 0;",
		"static int xl_mod_init_throw_dep__base = 0;",
		"xl_mod_init_throw_dep__base = xl__xl_mod_init_throw_dep__get_base__arity0__ret__int();",
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
static int __xlTestCgenTryFrame(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\try_nested_rethrow.xl", "xl_generated_exception_frame_push()"); }
static int __xlTestCgenErrorFields(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\error_fields_basic.xl", "Error(\"inner\", 42, \"\", \"\", NULL)"); }
static int __xlTestCgenNamedArgsDefault(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\named_args_default.xl", "xl__mix__arity3__a__int__b__int__c__int__ret__int(1, 2, 9)"); }
static int __xlTestCgenAsyncThrow(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\async\\run_throw_main.xl", "return xl_future_int_error(xl_generated_capture_exception_error());"); }
static int __xlTestCgenAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\async\\run_bool_main.xl", "xl_future_bool_await"); }
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
static int __xlTestCgenTask(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\task\\task_basic.xl", "xl_task_race_int"); }
static int __xlTestCgenTaskDelay(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\task\\task_delay_basic.xl", "xl_task_delay("); }
static int __xlTestCgenTaskError(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\task\\task_error_basic.xl", "xl_task_spawn_error"); }
static int __xlTestCgenGeneratorRun(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\generator_run.xl", "XL_GeneratorInt"); }
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

	__xlBuildProjectPath(pCtx, sFilePath, sizeof(sFilePath), "release\\tmp\\try_finally_basic\\note.txt");
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
static int __xlTestRunModuleInitThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_init_throw_dep.xl", 0, 0, NULL, "exception.text=module init boom"); }
static int __xlTestRunXlpImport(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp\\use_pkg_math.xl", 1, 42, NULL, NULL); }
static int __xlTestRunXlpNative(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\xlp_native\\use_pkg_clock.xl", 1, 86, "artifact.shared.ready=", NULL); }
static int __xlTestRunXlpNativeFuncs(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_probe"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe.xl", 1, 123, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_add_one"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_add.xl", 1, 42, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsString(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_text"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsStringParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_text_len"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsVoid(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_touch"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_touch.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsMixedParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_text_plus_len"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_text_plus_len.xl", 1, 10, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsBool(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_is_probe_text"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsBoolParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_bool_score"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_bool_score.xl", 1, 7, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsFloat(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_float_value"
	};

	return __xlRunProgramCaseAll(pCtx, "examples\\xlp_native\\use_pkg_probe_float.xl", 1, 0, aNeedles, XL_COUNTOF(aNeedles), NULL);
}
static int __xlTestRunXlpNativeFuncsFloatParams(XL_TestContext* pCtx)
{
	static const char* const aNeedles[] = {
		"artifact.shared.ready=",
		"artifact.shared.symbol=pkg_clock_artifact_add_half"
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
static int __xlTestRunPointBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\point_basic.xl", 1, 19, NULL, NULL); }
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
static int __xlTestRunBareContainerLiterals(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\bare_container_literals_basic.xl", 1, 255, NULL, NULL); }
static int __xlTestRunTypeMethodsBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\type_methods_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunPropertyLengthBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\property_length_basic.xl", 1, 13, "13", NULL); }
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
static int __xlTestRunAsyncMain(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_main.xl", 1, 9, "async.await=get_value", NULL); }
static int __xlTestRunAsyncMultiAwait(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_multi_await.xl", 1, 6, "async.await=get_value", NULL); }
static int __xlTestRunAsyncThrowMain(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_throw_main.xl", 0, 0, NULL, "exception.text=async boom"); }
static int __xlTestRunAsyncTryCatchThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_try_catch_throw.xl", 1, 17, "async.await=fail", NULL); }
static int __xlTestRunAsyncBoolMain(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\async\\run_bool_main.xl", 1, 7, "async.await=get_flag", NULL); }
static int __xlTestRunGeneratorRun(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_run.xl", 1, 20, "generator.next=make_values", NULL); }
static int __xlTestRunForIn(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\for_in_range.xl", 1, 6, NULL, NULL); }
static int __xlTestRunLoopJumpLevels(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\loop_jump_levels.xl", 1, 22, NULL, NULL); }
static int __xlTestRunTask(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_basic.xl", 1, 5, "task.cancel=", NULL); }
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
	{ "parser.export_func", "parser", __xlTestParseExportFunc },
	{ "parser.export_type_method", "parser", __xlTestParseExportTypeMethod },
	{ "parser.export_values", "parser", __xlTestParseExportValues },
	{ "parser.typed_decl", "parser", __xlTestParseTypedDecl },
	{ "parser.exact_numeric_types", "parser", __xlTestParseExactNumericTypes },
	{ "parser.auto_decl", "parser", __xlTestParseAutoDecl },
	{ "parser.typedef_basic", "parser", __xlTestParseTypeDefBasic },
	{ "parser.point_basic", "parser", __xlTestParsePointBasic },
	{ "parser.async_bool_main", "parser", __xlTestParseAsyncBoolMain },
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
	{ "parser.index_assign_basic", "parser", __xlTestParseIndexAssignBasic },
	{ "parser.for_in_containers", "parser", __xlTestParseForInContainers },
	{ "parser.container_methods_basic", "parser", __xlTestParseContainerMethodsBasic },
	{ "parser.container_append_basic", "parser", __xlTestParseContainerAppendBasic },
	{ "parser.container_clear_basic", "parser", __xlTestParseContainerClearBasic },
	{ "parser.container_remove_basic", "parser", __xlTestParseContainerRemoveBasic },
	{ "parser.core_len_basic", "parser", __xlTestParseCoreLenBasic },
	{ "parser.type_methods_basic", "parser", __xlTestParseTypeMethodsBasic },
	{ "parser.property_length_basic", "parser", __xlTestParsePropertyLengthBasic },
	{ "parser.string_methods_basic", "parser", __xlTestParseStringMethodsBasic },
	{ "parser.string_split_basic", "parser", __xlTestParseStringSplitBasic },
	{ "parser.while_basic", "parser", __xlTestParseWhileBasic },
	{ "parser.do_while_basic", "parser", __xlTestParseDoWhileBasic },
	{ "parser.syntax_error", "parser", __xlTestParseSyntaxError },
	{ "sem.overload", "sem", __xlTestSemOverload },
	{ "sem.unresolved", "sem", __xlTestSemUnresolved },
	{ "sem.no_overload", "sem", __xlTestSemNoOverload },
	{ "sem.for_in_generator", "sem", __xlTestSemForInGenerator },
	{ "sem.try_basic", "sem", __xlTestSemTry },
	{ "sem.try_nested_rethrow", "sem", __xlTestSemTryNestedRethrow },
	{ "sem.error_fields", "sem", __xlTestSemErrorFields },
	{ "sem.named_args_default", "sem", __xlTestSemNamedArgsDefault },
	{ "sem.await_outside_async", "sem", __xlTestSemAwaitOutsideAsync },
	{ "sem.assert_message", "sem", __xlTestSemAssertMessage },
	{ "sem.if_inline_basic", "sem", __xlTestSemIfInlineBasic },
	{ "sem.if_inline_else_basic", "sem", __xlTestSemIfInlineElseBasic },
	{ "sem.native_error_bridge", "sem", __xlTestSemNativeErrorBridge },
	{ "sem.import_alias", "sem", __xlTestSemImportAlias },
	{ "sem.import_multi", "sem", __xlTestSemImportMulti },
	{ "sem.import_string", "sem", __xlTestSemImportString },
	{ "sem.export_func", "sem", __xlTestSemExportFunc },
	{ "sem.export_type_method", "sem", __xlTestSemExportTypeMethod },
	{ "sem.export_values", "sem", __xlTestSemExportValues },
	{ "sem.typed_decl", "sem", __xlTestSemTypedDecl },
	{ "sem.exact_numeric_types", "sem", __xlTestSemExactNumericTypes },
	{ "sem.auto_decl", "sem", __xlTestSemAutoDecl },
	{ "sem.typedef_basic", "sem", __xlTestSemTypeDefBasic },
	{ "sem.type_ctor_basic", "sem", __xlTestSemTypeCtorBasic },
	{ "sem.point_basic", "sem", __xlTestSemPointBasic },
	{ "sem.async_bool_main", "sem", __xlTestSemAsyncBoolMain },
	{ "sem.task_error", "sem", __xlTestSemTaskError },
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
	{ "sem.type_method_self_error", "sem", __xlTestSemTypeMethodSelfError },
	{ "sem.property_length_basic", "sem", __xlTestSemPropertyLengthBasic },
	{ "sem.property_length_error", "sem", __xlTestSemPropertyLengthError },
	{ "sem.string_methods_basic", "sem", __xlTestSemStringMethodsBasic },
	{ "sem.string_split_basic", "sem", __xlTestSemStringSplitBasic },
	{ "sem.string_split_error", "sem", __xlTestSemStringSplitError },
	{ "sem.while_basic", "sem", __xlTestSemWhileBasic },
	{ "sem.do_while_basic", "sem", __xlTestSemDoWhileBasic },
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
	{ "sem.xlp_dependencies", "sem", __xlTestSemXlpDependencies },
	{ "lower.basic", "lower", __xlTestLowerBasic },
	{ "lower.async_basic", "lower", __xlTestLowerAsync },
	{ "lower.generator_basic", "lower", __xlTestLowerGenerator },
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
	{ "lower.property_length_basic", "lower", __xlTestLowerPropertyLengthBasic },
	{ "lower.string_methods_basic", "lower", __xlTestLowerStringMethodsBasic },
	{ "lower.string_split_basic", "lower", __xlTestLowerStringSplitBasic },
	{ "lower.try_basic", "lower", __xlTestLowerTry },
	{ "lower.named_args_default", "lower", __xlTestLowerNamedArgsDefault },
	{ "cgen.module_use_math", "cgen", __xlTestCgenModule },
	{ "cgen.export_type_method", "cgen", __xlTestCgenExportTypeMethod },
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
	{ "cgen.index_assign_basic", "cgen", __xlTestCgenIndexAssignBasic },
	{ "cgen.for_in_containers", "cgen", __xlTestCgenForInContainers },
	{ "cgen.container_methods_basic", "cgen", __xlTestCgenContainerMethodsBasic },
	{ "cgen.container_append_basic", "cgen", __xlTestCgenContainerAppendBasic },
	{ "cgen.container_clear_basic", "cgen", __xlTestCgenContainerClearBasic },
	{ "cgen.container_remove_basic", "cgen", __xlTestCgenContainerRemoveBasic },
	{ "cgen.bare_container_literals", "cgen", __xlTestCgenBareContainerLiterals },
	{ "cgen.core_len_basic", "cgen", __xlTestCgenCoreLenBasic },
	{ "cgen.type_methods_basic", "cgen", __xlTestCgenTypeMethodsBasic },
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
	{ "cgen.assert_message", "cgen", __xlTestCgenAssertMessage },
	{ "cgen.native_error_bridge", "cgen", __xlTestCgenNativeErrorBridge },
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
	{ "cgen.task_delay", "cgen", __xlTestCgenTaskDelay },
	{ "cgen.task_error", "cgen", __xlTestCgenTaskError },
	{ "cgen.generator_run", "cgen", __xlTestCgenGeneratorRun },
	{ "cgen.loop_jump_levels", "cgen", __xlTestCgenLoopJumpLevels },
	{ "host.run_c", "host", __xlTestHostRunC },
	{ "host.build_c", "host", __xlTestHostBuildC },
	{ "host.build_xlang", "host", __xlTestHostBuildXlang },
	{ "runtime.run_basic", "runtime", __xlTestRunBasic },
	{ "runtime.try_catch_basic", "runtime", __xlTestRunTryCatch },
	{ "runtime.try_nested_rethrow", "runtime", __xlTestRunTryNestedRethrow },
	{ "runtime.error_fields", "runtime", __xlTestRunErrorFields },
	{ "runtime.try_finally_basic", "runtime", __xlTestRunTryFinally },
	{ "runtime.try_catch_finally", "runtime", __xlTestRunTryCatchFinally },
	{ "runtime.named_args_default", "runtime", __xlTestRunNamedArgsDefault },
	{ "runtime.throw_basic", "runtime", __xlTestRunThrow },
	{ "runtime.assert_message", "runtime", __xlTestRunAssertMessage },
	{ "runtime.native_error_bridge", "runtime", __xlTestRunNativeErrorBridge },
	{ "runtime.module_use_math", "runtime", __xlTestRunModule },
	{ "runtime.export_type_method", "runtime", __xlTestRunExportTypeMethod },
	{ "runtime.module_init_throw", "runtime", __xlTestRunModuleInitThrow },
	{ "runtime.xlp_import", "runtime", __xlTestRunXlpImport },
	{ "runtime.xlp_native", "runtime", __xlTestRunXlpNative },
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
	{ "runtime.bare_container_literals", "runtime", __xlTestRunBareContainerLiterals },
	{ "runtime.core_len_basic", "runtime", __xlTestRunCoreLenBasic },
	{ "runtime.type_methods_basic", "runtime", __xlTestRunTypeMethodsBasic },
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
	{ "runtime.async_multi_await", "runtime", __xlTestRunAsyncMultiAwait },
	{ "runtime.async_throw_main", "runtime", __xlTestRunAsyncThrowMain },
	{ "runtime.async_try_catch_throw", "runtime", __xlTestRunAsyncTryCatchThrow },
	{ "runtime.async_bool_main", "runtime", __xlTestRunAsyncBoolMain },
	{ "runtime.generator_run", "runtime", __xlTestRunGeneratorRun },
	{ "runtime.for_in_range", "runtime", __xlTestRunForIn },
	{ "runtime.for_in_containers", "runtime", __xlTestRunForInContainers },
	{ "runtime.loop_jump_levels", "runtime", __xlTestRunLoopJumpLevels },
	{ "runtime.task_basic", "runtime", __xlTestRunTask },
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
		if ( ctx.pSink != NULL ) {
			fclose(ctx.pSink);
		}
		return 1;
	}
	__xlCopyText(ctx.sReleaseRoot, sizeof(ctx.sReleaseRoot), ctx.sProjectRoot);
	__xlAppendPathPart(ctx.sReleaseRoot, sizeof(ctx.sReleaseRoot), "release");

	fprintf(stdout, "demo5 unified test runner\n");
	fprintf(stdout, "project=demo5\n");
	if ( (sSelector != NULL) && (sSelector[0] != '\0') ) {
		fprintf(stdout, "selector=%s\n", sSelector);
	}

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
	}

	if ( ctx.pSink != NULL ) {
		fclose(ctx.pSink);
	}

	if ( iSelectedCount == 0 ) {
		fprintf(stdout, "[FAIL] no tests matched selector\n");
		return 1;
	}

	fprintf(
		stdout,
		"summary: total=%llu pass=%d fail=%d\n",
		(unsigned long long)iSelectedCount,
		ctx.iPassCount,
		ctx.iFailCount
	);
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
