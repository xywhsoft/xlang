#include <stdio.h>

#include "../src/codegen/c/xl_cgen.h"
#include "../src/driver/xl_module_link.h"
#include "../src/lexer/xl_lexer_api.h"
#include "../src/lower/xl_lower.h"
#include "../src/parser/xl_parser_api.h"
#include "../src/runtime/xl_exec_ctx.h"
#include "../src/sem/xl_resolve.h"
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
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &diag, 0, &lowerResult);
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
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &diag, 0, &lowerResult);
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
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &diag, 0, &lowerResult);
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
		iRc = xlLowerAst(moduleResult.pLinkedRoot, &diag, 0, &lowerResult);
	}
	if ( iRc == XL_RC_OK ) {
		iRc = xlCgenMir(lowerResult.pMirRoot, &diag, &sCode);
	}
	if ( iRc == XL_RC_OK ) {
		xlExecCtxInit(&execCtx, sPath, pCtx->sReleaseRoot);
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

static int __xlTestLexMinimal(XL_TestContext* pCtx) { return __xlRunLexCase(pCtx, "examples\\lexer\\minimal.xl", 4, XL_KW_FUNC); }
static int __xlTestLexKeywords(XL_TestContext* pCtx) { return __xlRunLexCase(pCtx, "examples\\lexer\\keywords.xl", 16, XL_KW_FUNC); }
static int __xlTestParseForIn(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\full\\for_in_range.xl", 1, XL_AST_FORIN); }
static int __xlTestParseSyntaxError(XL_TestContext* pCtx) { return __xlRunParseCase(pCtx, "examples\\parser\\syntax_error.xl", 0, -1); }
static int __xlTestSemOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\overload.xl", 1, "int"); }
static int __xlTestSemUnresolved(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\unresolved.xl", 0, NULL); }
static int __xlTestSemNoOverload(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\no_overload.xl", 0, NULL); }
static int __xlTestSemForInGenerator(XL_TestContext* pCtx) { return __xlRunSemCase(pCtx, "examples\\sem\\for_in_generator.xl", 1, "generator"); }
static int __xlTestLowerBasic(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\lower\\basic.xl", 1, -1, 0, 0); }
static int __xlTestLowerAsync(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\async\\basic.xl", 1, -1, 1, 0); }
static int __xlTestLowerGenerator(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\generator\\basic.xl", 1, -1, 0, 1); }
static int __xlTestLowerForIn(XL_TestContext* pCtx) { return __xlRunLowerCase(pCtx, "examples\\full\\for_in_range.xl", 1, XL_AST_FORIN, 0, 0); }
static int __xlTestCgenModule(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\module\\use_math.xl", "xl_mod_math__"); }
static int __xlTestCgenNative(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\native\\use_std_core.xl", "xl_mod_std_core__"); }
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
static int __xlTestCgenGeneratorRun(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\generator_run.xl", "XL_GeneratorInt"); }
static int __xlTestCgenLoopJumpLevels(XL_TestContext* pCtx) { return __xlRunCgenCase(pCtx, "examples\\full\\loop_jump_levels.xl", "goto xl_for_break_"); }
static int __xlTestRunBasic(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\run_basic.xl", 1, 42, NULL, NULL); }
static int __xlTestRunThrow(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\throw_basic.xl", 0, 0, NULL, "exception.text=boom"); }
static int __xlTestRunModule(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\module\\use_math.xl", 1, 42, NULL, NULL); }
static int __xlTestRunNative(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\native\\use_std_core.xl", 1, 5, NULL, NULL); }
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
static int __xlTestRunGeneratorRun(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\generator_run.xl", 1, 20, "generator.next=make_values", NULL); }
static int __xlTestRunForIn(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\for_in_range.xl", 1, 6, NULL, NULL); }
static int __xlTestRunLoopJumpLevels(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\full\\loop_jump_levels.xl", 1, 22, NULL, NULL); }
static int __xlTestRunTask(XL_TestContext* pCtx) { return __xlRunProgramCase(pCtx, "examples\\task\\task_basic.xl", 1, 5, "task.cancel=", NULL); }

static const XL_TestCase __gXLTests[] = {
	{ "lexer.minimal", "lexer", __xlTestLexMinimal },
	{ "lexer.keywords", "lexer", __xlTestLexKeywords },
	{ "parser.for_in", "parser", __xlTestParseForIn },
	{ "parser.syntax_error", "parser", __xlTestParseSyntaxError },
	{ "sem.overload", "sem", __xlTestSemOverload },
	{ "sem.unresolved", "sem", __xlTestSemUnresolved },
	{ "sem.no_overload", "sem", __xlTestSemNoOverload },
	{ "sem.for_in_generator", "sem", __xlTestSemForInGenerator },
	{ "lower.basic", "lower", __xlTestLowerBasic },
	{ "lower.async_basic", "lower", __xlTestLowerAsync },
	{ "lower.generator_basic", "lower", __xlTestLowerGenerator },
	{ "lower.for_in_range", "lower", __xlTestLowerForIn },
	{ "cgen.module_use_math", "cgen", __xlTestCgenModule },
	{ "cgen.native_std_core", "cgen", __xlTestCgenNative },
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
	{ "cgen.generator_run", "cgen", __xlTestCgenGeneratorRun },
	{ "cgen.loop_jump_levels", "cgen", __xlTestCgenLoopJumpLevels },
	{ "runtime.run_basic", "runtime", __xlTestRunBasic },
	{ "runtime.throw_basic", "runtime", __xlTestRunThrow },
	{ "runtime.module_use_math", "runtime", __xlTestRunModule },
	{ "runtime.native_std_core", "runtime", __xlTestRunNative },
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
	{ "runtime.generator_run", "runtime", __xlTestRunGeneratorRun },
	{ "runtime.for_in_range", "runtime", __xlTestRunForIn },
	{ "runtime.loop_jump_levels", "runtime", __xlTestRunLoopJumpLevels },
	{ "runtime.task_basic", "runtime", __xlTestRunTask }
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
