#include "xl_driver_debug.h"
#include "../ast/xl_ast_dump.h"
#include "../codegen/c/xl_cgen.h"
#include "../diag/xl_dump.h"
#include "../host/xl_host.h"
#include "../lexer/xl_lexer_api.h"
#include "../lower/xl_lower.h"
#include "../parser/xl_parser_api.h"
#include "../sem/xl_sem_dump.h"
#include "../debug/xl_debugger_debug.h"
#include "xl_module_link.h"
#include "xl_xle_dump.h"

#if XLANG_DEBUGMODE_ENABLED
#include "../runtime/xlrt_generated.h"
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

#define XL_DAP_ARGUMENTS_REF_BASE 100000
#define XL_DAP_LOCALS_REF_BASE 200000
#define XL_DAP_VARIABLE_REF_STRIDE 1000
#define XL_DAP_CHILDREN_REF_BASE 300000
#define XL_DAP_CHILDREN_REF_MAX 256

static int __xlWriteTextFileDebug(const char* sFilePath, const char* sText)
{
	FILE* pFile;

	if ( (sFilePath == NULL) || (sText == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( xlPathEnsureParentDir(sFilePath) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pFile = fopen(sFilePath, "wb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}

	fputs(sText, pFile);
	fclose(pFile);
	return XL_RC_OK;
}

static char* __xlWrapJsonTextDebug(const char* sKey, const char* sText)
{
	size_t i;
	size_t iLen;
	size_t iCap;
	size_t iOut;
	char* sOut;

	if ( sKey == NULL ) {
		sKey = "value";
	}
	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	iCap = (strlen(sKey) * 2) + (iLen * 2) + 32;
	sOut = (char*)malloc(iCap);
	if ( sOut == NULL ) {
		return NULL;
	}

	iOut = (size_t)snprintf(sOut, iCap, "{\n\t\"%s\": \"", sKey);
	for ( i = 0; i < iLen; ++i ) {
		const unsigned char ch = (unsigned char)sText[i];

		if ( (iOut + 8) >= iCap ) {
			free(sOut);
			return NULL;
		}

		switch ( ch ) {
			case '\\':
				sOut[iOut++] = '\\';
				sOut[iOut++] = '\\';
				break;
			case '"':
				sOut[iOut++] = '\\';
				sOut[iOut++] = '"';
				break;
			case '\r':
				sOut[iOut++] = '\\';
				sOut[iOut++] = 'r';
				break;
			case '\n':
				sOut[iOut++] = '\\';
				sOut[iOut++] = 'n';
				break;
			case '\t':
				sOut[iOut++] = '\\';
				sOut[iOut++] = 't';
				break;
			default:
				sOut[iOut++] = (char)ch;
				break;
		}
	}

	iOut += (size_t)snprintf(sOut + iOut, iCap - iOut, "\"\n}\n");
	return sOut;
}

static char* __xlConcatTextDebug(const char* sText1, const char* sText2)
{
	size_t iLen1;
	size_t iLen2;
	char* sOut;

	if ( sText1 == NULL ) {
		sText1 = "";
	}
	if ( sText2 == NULL ) {
		sText2 = "";
	}

	iLen1 = strlen(sText1);
	iLen2 = strlen(sText2);
	sOut = (char*)malloc(iLen1 + iLen2 + 1);
	if ( sOut == NULL ) {
		return NULL;
	}

	memcpy(sOut, sText1, iLen1);
	memcpy(sOut + iLen1, sText2, iLen2);
	sOut[iLen1 + iLen2] = '\0';
	return sOut;
}

static char* __xlCommentTextDebug(const char* sText)
{
	size_t i;
	size_t iLen;
	size_t iOut;
	char* sOut;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sOut = (char*)malloc(iLen + 64);
	if ( sOut == NULL ) {
		return NULL;
	}

	iOut = (size_t)snprintf(sOut, iLen + 64, "/*\n");
	for ( i = 0; i < iLen; ++i ) {
		if ( (sText[i] == '*') && ((i + 1) < iLen) && (sText[i + 1] == '/') ) {
			sOut[iOut++] = '*';
			sOut[iOut++] = ' ';
			continue;
		}
		sOut[iOut++] = sText[i];
	}
	iOut += (size_t)snprintf(sOut + iOut, (iLen + 64) - iOut, "*/\n");
	return sOut;
}

static void __xlBuildDumpPathDebug(
	char* sOut,
	size_t iOutSize,
	const char* sGroup,
	const char* sInputFile,
	const char* sSuffix
)
{
	char sStem[XL_PATH_MAX_LEN];
	const char* sInput;
	const char* sDot;
	size_t iInLen;
	size_t iOutLen;
	size_t i;

	sInput = (sInputFile != NULL) ? sInputFile : "stdin";
	sDot = strrchr(sInput, '.');
	iInLen = (sDot != NULL) ? (size_t)(sDot - sInput) : strlen(sInput);
	if ( iInLen >= sizeof(sStem) ) {
		iInLen = sizeof(sStem) - 1;
	}

	iOutLen = 0;
	for ( i = 0; i < iInLen; ++i ) {
		unsigned char ch = (unsigned char)sInput[i];

		if ( xlPathIsSep(ch) || (ch == ':') ) {
			if ( (iOutLen == 0) || (sStem[iOutLen - 1] != '_') ) {
				sStem[iOutLen++] = '_';
			}
			continue;
		}
		if ( ((ch >= 'a') && (ch <= 'z')) ||
			((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= '0') && (ch <= '9')) ||
			(ch == '_') || (ch == '-') ) {
			sStem[iOutLen++] = (char)ch;
		} else {
			sStem[iOutLen++] = '_';
		}
		if ( iOutLen >= (sizeof(sStem) - 1) ) {
			break;
		}
	}
	if ( iOutLen == 0 ) {
		snprintf(sStem, sizeof(sStem), "stdin");
	} else {
		sStem[iOutLen] = '\0';
	}

	snprintf(sOut, iOutSize, "dump\\%s\\%s%s", (sGroup != NULL) ? sGroup : "misc", sStem, sSuffix);
}

int xlDriverRunLexDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_Lexer* pLexer;
	XL_TokenList list;
	char* sTextDump;
	char* sJsonDump;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	pLexer = xlLexerCreateFromFile(pOpts->sInputFile);
	if ( pLexer == NULL ) {
		XL_DIAG_ERROR(pDiag, "lexer", "failed to open input file: %s", pOpts->sInputFile);
		return XL_RC_IO_ERROR;
	}

	xlTokenListInit(&list);
	sTextDump = NULL;
	sJsonDump = NULL;

	iRc = xlLexerTokenizeAll(pLexer, &list);
	sTextDump = xlLexerTokensToText(&list);
	sJsonDump = xlLexerTokensToJson(&list);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) ) {
		XL_DIAG_ERROR(pDiag, "lexer", "failed to build token dump output");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFileDebug(pOpts->sEmitPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "lexer", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( XL_CLI_DUMP_TOKEN(pOpts) || XL_CLI_DUMP_JSON(pOpts) ) {
		__xlBuildDumpPathDebug(
			sDefaultPath,
			sizeof(sDefaultPath),
			"lexer",
			pOpts->sInputFile,
			XL_CLI_DUMP_JSON(pOpts) ? ".tokens.json" : ".tokens.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lexer", "failed to write default dump: %s", sDefaultPath);
		}
	}

	if ( xlLexerHadError(pLexer) ) {
		XL_DIAG_WARN(pDiag, "lexer", "tokenized with lexer errors: %s", xlLexerGetError(pLexer));
		if ( iRc == XL_RC_OK ) {
			iRc = XL_RC_FAIL;
		}
	}

cleanup:
	free(sTextDump);
	free(sJsonDump);
	xlTokenListFree(&list);
	xlLexerDestroy(pLexer);
	return iRc;
}

int xlDriverRunParseDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_Lexer* pLexer;
	XL_ParseResult result;
	char* sTextDump;
	char* sJsonDump;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	pLexer = xlLexerCreateFromFile(pOpts->sInputFile);
	if ( pLexer == NULL ) {
		XL_DIAG_ERROR(pDiag, "parser", "failed to open input file: %s", pOpts->sInputFile);
		return XL_RC_IO_ERROR;
	}

	xlParseResultInit(&result);
	sTextDump = NULL;
	sJsonDump = NULL;

	iRc = xlParserParseLexer(pLexer, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &result);
	if ( result.pRoot == NULL ) {
		xlLexerDestroy(pLexer);
		xlParseResultFree(&result);
		return (iRc == XL_RC_OK) ? XL_RC_FAIL : iRc;
	}

	sTextDump = xlAstDumpText(result.pRoot);
	sJsonDump = xlAstDumpJson(result.pRoot);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) ) {
		XL_DIAG_ERROR(pDiag, "parser", "failed to build AST dump output");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFileDebug(pOpts->sEmitPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "parser", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( XL_CLI_DUMP_AST(pOpts) || XL_CLI_DUMP_JSON(pOpts) ) {
		__xlBuildDumpPathDebug(
			sDefaultPath,
			sizeof(sDefaultPath),
			"parser",
			pOpts->sInputFile,
			XL_CLI_DUMP_JSON(pOpts) ? ".ast.json" : ".ast.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "parser", "failed to write default AST dump: %s", sDefaultPath);
		}
	}

cleanup:
	free(sTextDump);
	free(sJsonDump);
	xlParseResultFree(&result);
	xlLexerDestroy(pLexer);
	return iRc;
}

int xlDriverRunSemDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	char* sTextDump;
	char* sJsonDump;
	char* sSymbolsJson;
	char* sTypesJson;
	char* sFuncsJson;
	char* sXleFuncsJson;
	char* sXleJson;
	char* sDiagText;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sTextDump = NULL;
	sJsonDump = NULL;
	sSymbolsJson = NULL;
	sTypesJson = NULL;
	sFuncsJson = NULL;
	sXleFuncsJson = NULL;
	sXleJson = NULL;
	sDiagText = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, XL_CLI_TRACE_SEM(pOpts) ? 1 : 0, &semResult);
	sTextDump = xlSemDumpText(&semResult);
	sJsonDump = xlSemDumpJson(&semResult);
	sSymbolsJson = xlSemDumpSymbolsJson(&semResult);
	sTypesJson = xlSemDumpTypesJson(&semResult);
	sFuncsJson = xlSemDumpFuncsJson(&semResult);
	sXleFuncsJson = xlSemDumpXleFunctionsJson(&semResult);
	sXleJson = xlXleDumpPackageJson(&moduleResult, &semResult, pOpts->sInputFile);
	sDiagText = xlSemDumpDiagText(&semResult);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) || (sSymbolsJson == NULL) || (sTypesJson == NULL) ||
		(sFuncsJson == NULL) || (sXleFuncsJson == NULL) || (sXleJson == NULL) || (sDiagText == NULL) ) {
		XL_DIAG_ERROR(pDiag, "sem", "failed to build semantic dump output");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFileDebug(pOpts->sEmitPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "sem", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( XL_CLI_DUMP_SEM(pOpts) || XL_CLI_DUMP_JSON(pOpts) ) {
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, XL_CLI_DUMP_JSON(pOpts) ? ".sem.json" : ".sem.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write semantic dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".symbols.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSymbolsJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write symbol dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".types.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sTypesJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write type dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".funcs.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sFuncsJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write function dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".xle.functions.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sXleFuncsJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write xle function dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".package.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sXleJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write xle package dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".diag.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, sDiagText) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write diagnostic dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write module dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write module json dump: %s", sDefaultPath);
	}

cleanup:
	free(sTextDump); free(sJsonDump); free(sSymbolsJson); free(sTypesJson); free(sFuncsJson); free(sXleFuncsJson); free(sXleJson); free(sDiagText);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

int xlDriverRunLowerDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	char* sTextDump;
	char* sJsonDump;
	char* sHirJson;
	char* sMirJson;
	char* sSmText;
	char* sSmJson;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sTextDump = NULL; sJsonDump = NULL; sHirJson = NULL; sMirJson = NULL; sSmText = NULL; sSmJson = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, XL_CLI_TRACE_SEM(pOpts) ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, XL_CLI_TRACE_LOWER(pOpts) ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;

	sTextDump = xlLowerDumpText(&lowerResult);
	sJsonDump = xlLowerDumpJson(&lowerResult);
	sHirJson = xlHirDumpJson(lowerResult.pHirRoot);
	sMirJson = xlMirDumpJson(lowerResult.pMirRoot);
	sSmText = xlLowerDumpStateMachinesText(&lowerResult);
	sSmJson = xlLowerDumpStateMachinesJson(&lowerResult);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) || (sHirJson == NULL) || (sMirJson == NULL) || (sSmText == NULL) || (sSmJson == NULL) ) {
		XL_DIAG_ERROR(pDiag, "lower", "failed to build lowering dump output");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( XL_CLI_DUMP_JSON(pOpts) ) fputs(sJsonDump, stdout); else fputs(sTextDump, stdout);
	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFileDebug(pOpts->sEmitPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "lower", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}
	if ( XL_CLI_DUMP_HIR(pOpts) || XL_CLI_DUMP_MIR(pOpts) || XL_CLI_DUMP_JSON(pOpts) ) {
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, XL_CLI_DUMP_JSON(pOpts) ? ".lower.json" : ".lower.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sTextDump) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write lower dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".hir.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sHirJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write HIR dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".mir.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sMirJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write MIR dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".sm.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSmText) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write state-machine dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".sm.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSmJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write state-machine json dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write module dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "lower", "failed to write module json dump: %s", sDefaultPath);
	}

cleanup:
	free(sTextDump); free(sJsonDump); free(sHirJson); free(sMirJson); free(sSmText); free(sSmJson);
	xlLowerResultFree(&lowerResult); xlSemResultFree(&semResult); xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

int xlDriverRunCgenDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	char* sCode;
	char* sJsonDump;
	char* sModuleComment;
	char* sCodeWithModules;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL; sJsonDump = NULL; sModuleComment = NULL; sCodeWithModules = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, XL_CLI_TRACE_SEM(pOpts) ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, XL_CLI_TRACE_LOWER(pOpts) ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlCgenMirDebug(&lowerResult, &moduleResult, pDiag, XL_CLI_DUMP_C(pOpts) ? 1 : 0, &sCode);
	if ( iRc != XL_RC_OK ) goto cleanup;

	sModuleComment = __xlCommentTextDebug((moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "");
	sCodeWithModules = __xlConcatTextDebug((sModuleComment != NULL) ? sModuleComment : "", sCode);
	sJsonDump = __xlWrapJsonTextDebug("code", (sCodeWithModules != NULL) ? sCodeWithModules : sCode);
	if ( (sCode == NULL) || (sJsonDump == NULL) || (sCodeWithModules == NULL) ) {
		XL_DIAG_ERROR(pDiag, "cgen", "failed to build C code dump");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( XL_CLI_DUMP_JSON(pOpts) ) fputs(sJsonDump, stdout); else fputs(sCodeWithModules, stdout);
	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFileDebug(pOpts->sEmitPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sCodeWithModules) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "cgen", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}
	if ( XL_CLI_DUMP_C(pOpts) || XL_CLI_DUMP_JSON(pOpts) ) {
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "cgen", pOpts->sInputFile, XL_CLI_DUMP_JSON(pOpts) ? ".c.json" : ".c");
		if ( xlDumpWriteText(pDump, sDefaultPath, XL_CLI_DUMP_JSON(pOpts) ? sJsonDump : sCodeWithModules) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "cgen", "failed to write C dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "cgen", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "cgen", "failed to write module dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "cgen", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "cgen", "failed to write module json dump: %s", sDefaultPath);
	}

cleanup:
	free(sCode); free(sJsonDump); free(sModuleComment); free(sCodeWithModules);
	xlLowerResultFree(&lowerResult); xlSemResultFree(&semResult); xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

typedef struct XL_DapStdoutGuard {
#if defined(_WIN32) || defined(_WIN64)
	int iSavedFd;
	FILE* pNull;
#endif
	int bActive;
} XL_DapStdoutGuard;

typedef struct XL_DapSession XL_DapSession;

typedef struct XL_DapDebuggee {
	XL_DapSession* pSession;
	char sProgram[XL_PATH_MAX_LEN];
	XL_HostRunResult result;
	char* sTraceText;
	int iRc;
	int bStarted;
	int bDone;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hThread;
	HANDLE hDoneEvent;
#endif
} XL_DapDebuggee;

typedef struct XL_DapVariableChildrenRef {
	int iReference;
	size_t iStopSequence;
	void* pValue;
} XL_DapVariableChildrenRef;

struct XL_DapSession {
	int iNextSeq;
	int iLastExitCode;
	int bDebuggeeCompleted;
	XL_DiagContext* pDiag;
	XL_DebugController controller;
	XL_DapStdoutGuard stdoutGuard;
	XL_DapDebuggee debuggee;
	XL_DapVariableChildrenRef arrVariableChildrenRefs[XL_DAP_CHILDREN_REF_MAX];
	size_t iVariableChildrenRefNext;
};

static int __xlDapSendEvent(XL_DapSession* pSession, const char* sEvent, const char* sBody);
static int __xlDapSendStoppedEvent(XL_DapSession* pSession, const char* sReason);
static int __xlDapHandleRequest(XL_DapSession* pSession, const char* sRequest, int* pbQuit);
static void __xlDapJsonEscapeString(char* sOut, size_t iOutSize, const char* sText);
static int __xlDapSendDebuggeeFailureOutput(XL_DapSession* pSession);

static void __xlDapSetBinaryMode(void)
{
#if defined(_WIN32) || defined(_WIN64)
	(void)_setmode(_fileno(stdin), _O_BINARY);
	(void)_setmode(_fileno(stdout), _O_BINARY);
#endif
}

static int __xlDapAsciiEqIgnoreCaseN(const char* sLeft, const char* sRight, size_t iLen)
{
	size_t i;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return 0;
	}

	for ( i = 0; i < iLen; ++i ) {
		unsigned char a;
		unsigned char b;

		a = (unsigned char)sLeft[i];
		b = (unsigned char)sRight[i];
		if ( (a == '\0') || (b == '\0') ) {
			return 0;
		}
		if ( (a >= 'A') && (a <= 'Z') ) a = (unsigned char)(a - 'A' + 'a');
		if ( (b >= 'A') && (b <= 'Z') ) b = (unsigned char)(b - 'A' + 'a');
		if ( a != b ) {
			return 0;
		}
	}
	return 1;
}

static int __xlDapReadHeaderLine(FILE* pIn, char* sLine, size_t iLineSize)
{
	int ch;
	size_t iLen;

	if ( (pIn == NULL) || (sLine == NULL) || (iLineSize == 0u) ) {
		return -1;
	}

	iLen = 0u;
	while ( (ch = fgetc(pIn)) != EOF ) {
		if ( ch == '\n' ) {
			break;
		}
		if ( (iLen + 1u) < iLineSize ) {
			sLine[iLen++] = (char)ch;
		}
	}

	if ( (ch == EOF) && (iLen == 0u) ) {
		return 0;
	}

	while ( (iLen > 0u) && ((sLine[iLen - 1u] == '\r') || (sLine[iLen - 1u] == '\n')) ) {
		--iLen;
	}
	sLine[iLen] = '\0';
	return 1;
}

static int __xlDapParseContentLength(const char* sLine, size_t* piContentLength)
{
	const char* sValue;
	char* sEnd;
	unsigned long iLen;

	if ( (sLine == NULL) || (piContentLength == NULL) ) {
		return 0;
	}
	if ( !__xlDapAsciiEqIgnoreCaseN(sLine, "Content-Length:", 15u) ) {
		return 0;
	}

	sValue = sLine + 15u;
	while ( (*sValue == ' ') || (*sValue == '\t') ) {
		++sValue;
	}
	errno = 0;
	iLen = strtoul(sValue, &sEnd, 10);
	if ( (errno != 0) || (sEnd == sValue) ) {
		return -1;
	}
	*piContentLength = (size_t)iLen;
	return 1;
}

static int __xlDapReadMessage(char** psBody)
{
	char sLine[512];
	size_t iContentLength;
	size_t iRead;
	char* sBody;
	int bSawHeader;

	if ( psBody == NULL ) {
		return -1;
	}
	*psBody = NULL;
	iContentLength = 0u;
	bSawHeader = 0;

	for (;;) {
		int iLineRc;

		iLineRc = __xlDapReadHeaderLine(stdin, sLine, sizeof(sLine));
		if ( iLineRc == 0 ) {
			return bSawHeader ? -1 : 0;
		}
		if ( iLineRc < 0 ) {
			return -1;
		}

		if ( sLine[0] == '\0' ) {
			break;
		}

		bSawHeader = 1;
		{
			size_t iParsedLen;
			int iParsed;

			iParsedLen = 0u;
			iParsed = __xlDapParseContentLength(sLine, &iParsedLen);
			if ( iParsed < 0 ) {
				return -1;
			}
			if ( iParsed > 0 ) {
				iContentLength = iParsedLen;
			}
		}
	}

	if ( iContentLength == 0u ) {
		return -1;
	}

	sBody = (char*)malloc(iContentLength + 1u);
	if ( sBody == NULL ) {
		return -1;
	}

	iRead = fread(sBody, 1u, iContentLength, stdin);
	if ( iRead != iContentLength ) {
		free(sBody);
		return -1;
	}
	sBody[iContentLength] = '\0';
	*psBody = sBody;
	return 1;
}

static const char* __xlDapJsonFindKeyValue(const char* sJson, const char* sKey)
{
	char sNeedle[80];
	const char* sFound;
	const char* sValue;

	if ( (sJson == NULL) || (sKey == NULL) || (sKey[0] == '\0') ) {
		return NULL;
	}
	snprintf(sNeedle, sizeof(sNeedle), "\"%s\"", sKey);
	sFound = strstr(sJson, sNeedle);
	if ( sFound == NULL ) {
		return NULL;
	}
	sValue = sFound + strlen(sNeedle);
	while ( (*sValue == ' ') || (*sValue == '\t') || (*sValue == '\r') || (*sValue == '\n') ) {
		++sValue;
	}
	if ( *sValue != ':' ) {
		return NULL;
	}
	++sValue;
	while ( (*sValue == ' ') || (*sValue == '\t') || (*sValue == '\r') || (*sValue == '\n') ) {
		++sValue;
	}
	return sValue;
}

static int __xlDapJsonGetInt(const char* sJson, const char* sKey, int* piValue)
{
	const char* sValue;
	char* sEnd;
	long iValue;

	if ( piValue == NULL ) {
		return 0;
	}
	*piValue = 0;

	sValue = __xlDapJsonFindKeyValue(sJson, sKey);
	if ( sValue == NULL ) {
		return 0;
	}

	errno = 0;
	iValue = strtol(sValue, &sEnd, 10);
	if ( (errno != 0) || (sEnd == sValue) ) {
		return 0;
	}
	*piValue = (int)iValue;
	return 1;
}

static int __xlDapJsonGetBool(const char* sJson, const char* sKey, int* pbValue)
{
	const char* sValue;

	if ( pbValue == NULL ) {
		return 0;
	}
	*pbValue = 0;

	sValue = __xlDapJsonFindKeyValue(sJson, sKey);
	if ( sValue == NULL ) {
		return 0;
	}
	if ( strncmp(sValue, "true", 4u) == 0 ) {
		*pbValue = 1;
		return 1;
	}
	if ( strncmp(sValue, "false", 5u) == 0 ) {
		*pbValue = 0;
		return 1;
	}
	if ( *sValue == '1' ) {
		*pbValue = 1;
		return 1;
	}
	if ( *sValue == '0' ) {
		*pbValue = 0;
		return 1;
	}
	return 0;
}

static int __xlDapJsonGetString(const char* sJson, const char* sKey, char* sOut, size_t iOutSize)
{
	const char* sValue;
	size_t iOut;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return 0;
	}
	sOut[0] = '\0';

	sValue = __xlDapJsonFindKeyValue(sJson, sKey);
	if ( (sValue == NULL) || (*sValue != '"') ) {
		return 0;
	}
	++sValue;

	iOut = 0u;
	while ( *sValue != '\0' ) {
		char ch;

		ch = *sValue++;
		if ( ch == '"' ) {
			sOut[iOut] = '\0';
			return 1;
		}
		if ( ch == '\\' ) {
			if ( *sValue == '\0' ) {
				break;
			}
			ch = *sValue++;
		}
		if ( (iOut + 1u) < iOutSize ) {
			sOut[iOut++] = ch;
		}
	}

	sOut[iOut] = '\0';
	return 0;
}

static int __xlDapJsonIsWhitespace(char ch)
{
	return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
}

static int __xlDapJsonValidate(const char* sJson)
{
	char arrStack[256];
	size_t iDepth;
	const char* sScan;
	int bInString;
	int bEscape;

	if ( sJson == NULL ) {
		return 0;
	}
	sScan = sJson;
	while ( __xlDapJsonIsWhitespace(*sScan) ) {
		++sScan;
	}
	if ( *sScan != '{' ) {
		return 0;
	}

	iDepth = 0u;
	bInString = 0;
	bEscape = 0;
	for ( ; *sScan != '\0'; ++sScan ) {
		char ch;

		ch = *sScan;
		if ( bInString ) {
			if ( bEscape ) {
				bEscape = 0;
				continue;
			}
			if ( ch == '\\' ) {
				bEscape = 1;
				continue;
			}
			if ( ch == '"' ) {
				bInString = 0;
				continue;
			}
			if ( ((unsigned char)ch) < 0x20u ) {
				return 0;
			}
			continue;
		}

		if ( ch == '"' ) {
			bInString = 1;
			continue;
		}
		if ( (ch == '{') || (ch == '[') ) {
			if ( iDepth >= XL_COUNTOF(arrStack) ) {
				return 0;
			}
			arrStack[iDepth++] = ch;
			continue;
		}
		if ( (ch == '}') || (ch == ']') ) {
			char chOpen;

			if ( iDepth == 0u ) {
				return 0;
			}
			chOpen = arrStack[--iDepth];
			if ( ((ch == '}') && (chOpen != '{')) || ((ch == ']') && (chOpen != '[')) ) {
				return 0;
			}
			if ( iDepth == 0u ) {
				++sScan;
				break;
			}
		}
	}

	if ( bInString || bEscape || (iDepth != 0u) ) {
		return 0;
	}
	while ( __xlDapJsonIsWhitespace(*sScan) ) {
		++sScan;
	}
	return *sScan == '\0';
}

static size_t __xlDapCollectBreakpointLines(const char* sJson, int* arrLines, int* arrColumns, int* arrUnsupported, size_t iCap)
{
	const char* sScan;
	size_t iCount;

	if ( (sJson == NULL) || (arrLines == NULL) || (iCap == 0u) ) {
		return 0u;
	}

	sScan = strstr(sJson, "\"breakpoints\"");
	if ( sScan == NULL ) {
		return 0u;
	}

	iCount = 0u;
	while ( (iCount < iCap) && ((sScan = strstr(sScan, "\"line\"")) != NULL) ) {
		const char* sValue;
		const char* sCondition;
		const char* sNextLine;
		char* sEnd;
		long iLine;

		sValue = __xlDapJsonFindKeyValue(sScan, "line");
		if ( sValue == NULL ) {
			break;
		}
		errno = 0;
		iLine = strtol(sValue, &sEnd, 10);
		if ( (errno == 0) && (sEnd != sValue) && (iLine > 0) ) {
			arrLines[iCount] = (int)iLine;
			if ( arrColumns != NULL ) {
				arrColumns[iCount] = 0;
			}
			if ( arrUnsupported != NULL ) {
				sCondition = strstr(sEnd, "\"condition\"");
				sNextLine = strstr(sEnd, "\"line\"");
				arrUnsupported[iCount] = ((sCondition != NULL) && ((sNextLine == NULL) || (sCondition < sNextLine))) ? 1 : 0;
			}
			++iCount;
		}
		sScan = (sEnd != sValue) ? sEnd : (sScan + 6);
	}

	return iCount;
}

static int __xlDapBuildBreakpointResponseBody(const XL_DebugController* pController, const char* sSource, char* sOut, size_t iOutSize)
{
	size_t i;
	size_t iLen;
	size_t iCount;
	size_t iOutCount;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return XL_RC_BAD_ARG;
	}

	iLen = (size_t)snprintf(sOut, iOutSize, "{\"breakpoints\":[");
	iCount = xlDebugControllerBreakpointCount(pController);
	iOutCount = 0u;
	for ( i = 0u; i < iCount; ++i ) {
		const XL_DebugBreakpoint* pBreakpoint;
		char sMessage[512];
		int iNeed;

		pBreakpoint = xlDebugControllerBreakpointAt(pController, i);
		if ( pBreakpoint == NULL ) {
			continue;
		}
		if ( (sSource != NULL) && !xlDebugSourceMatches(pBreakpoint->sSource, sSource) ) {
			continue;
		}
		if ( iLen >= iOutSize ) {
			return XL_RC_IO_ERROR;
		}
		__xlDapJsonEscapeString(sMessage, sizeof(sMessage), pBreakpoint->sMessage);
		iNeed = snprintf(
			sOut + iLen,
			iOutSize - iLen,
			"%s{\"verified\":%s,\"line\":%d,\"message\":\"%s\"}",
			(iOutCount == 0u) ? "" : ",",
			pBreakpoint->bVerified ? "true" : "false",
			(pBreakpoint->iActualLine > 0) ? pBreakpoint->iActualLine : pBreakpoint->iLine,
			sMessage
		);
		if ( iNeed < 0 ) {
			return XL_RC_IO_ERROR;
		}
		iLen += (size_t)iNeed;
		++iOutCount;
	}
	if ( iLen >= iOutSize ) {
		return XL_RC_IO_ERROR;
	}
	snprintf(sOut + iLen, iOutSize - iLen, "]}");
	return XL_RC_OK;
}

static int __xlDapSendJson(XL_DapSession* pSession, const char* sJson)
{
	char sHeader[64];
	size_t iLen;
	size_t iHeaderLen;

	if ( sJson == NULL ) {
		return XL_RC_BAD_ARG;
	}

	iLen = strlen(sJson);
	iHeaderLen = (size_t)snprintf(sHeader, sizeof(sHeader), "Content-Length: %lu\r\n\r\n", (unsigned long)iLen);
	if ( iHeaderLen >= sizeof(sHeader) ) {
		return XL_RC_IO_ERROR;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( (pSession != NULL) && pSession->stdoutGuard.bActive && (pSession->stdoutGuard.iSavedFd >= 0) ) {
		if ( _write(pSession->stdoutGuard.iSavedFd, sHeader, (unsigned int)iHeaderLen) != (int)iHeaderLen ) {
			return XL_RC_IO_ERROR;
		}
		if ( (iLen != 0u) &&
			(_write(pSession->stdoutGuard.iSavedFd, sJson, (unsigned int)iLen) != (int)iLen) ) {
			return XL_RC_IO_ERROR;
		}
		if ( _commit(pSession->stdoutGuard.iSavedFd) != 0 ) {
			return XL_RC_IO_ERROR;
		}
		return XL_RC_OK;
	}
#else
	XL_UNUSED(pSession);
#endif
	if ( fwrite(sHeader, 1u, iHeaderLen, stdout) != iHeaderLen ) {
		return XL_RC_IO_ERROR;
	}
	if ( (iLen != 0u) && (fwrite(sJson, 1u, iLen, stdout) != iLen) ) {
		return XL_RC_IO_ERROR;
	}
	if ( fflush(stdout) != 0 ) {
		return XL_RC_IO_ERROR;
	}
	return XL_RC_OK;
}

static void __xlDapStdoutGuardInit(XL_DapStdoutGuard* pGuard)
{
	if ( pGuard == NULL ) {
		return;
	}
	memset(pGuard, 0, sizeof(*pGuard));
#if defined(_WIN32) || defined(_WIN64)
	pGuard->iSavedFd = -1;
#endif
}

static int __xlDapStdoutSuppress(XL_DapStdoutGuard* pGuard)
{
	if ( pGuard == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pGuard->bActive ) {
		return XL_RC_OK;
	}
	__xlDapStdoutGuardInit(pGuard);
#if defined(_WIN32) || defined(_WIN64)
	fflush(stdout);
	pGuard->iSavedFd = _dup(_fileno(stdout));
	if ( pGuard->iSavedFd < 0 ) {
		return XL_RC_IO_ERROR;
	}
	pGuard->pNull = fopen("NUL", "wb");
	if ( pGuard->pNull == NULL ) {
		_close(pGuard->iSavedFd);
		pGuard->iSavedFd = -1;
		return XL_RC_IO_ERROR;
	}
	if ( _dup2(_fileno(pGuard->pNull), _fileno(stdout)) != 0 ) {
		fclose(pGuard->pNull);
		pGuard->pNull = NULL;
		_close(pGuard->iSavedFd);
		pGuard->iSavedFd = -1;
		return XL_RC_IO_ERROR;
	}
	pGuard->bActive = 1;
#else
	pGuard->bActive = 0;
#endif
	return XL_RC_OK;
}

static void __xlDapStdoutRestore(XL_DapStdoutGuard* pGuard)
{
	if ( pGuard == NULL ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( pGuard->bActive ) {
		fflush(stdout);
		(void)_dup2(pGuard->iSavedFd, _fileno(stdout));
	}
	if ( pGuard->iSavedFd >= 0 ) {
		_close(pGuard->iSavedFd);
		pGuard->iSavedFd = -1;
	}
	if ( pGuard->pNull != NULL ) {
		fclose(pGuard->pNull);
		pGuard->pNull = NULL;
	}
#endif
	pGuard->bActive = 0;
}

static int __xlDapStdinHasData(void)
{
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hInput;
	DWORD iAvailable;

	hInput = GetStdHandle(STD_INPUT_HANDLE);
	iAvailable = 0u;
	if ( (hInput == NULL) || (hInput == INVALID_HANDLE_VALUE) ) {
		return 0;
	}
	if ( !PeekNamedPipe(hInput, NULL, 0u, NULL, &iAvailable, NULL) ) {
		return 0;
	}
	return iAvailable != 0u;
#else
	return 0;
#endif
}

static int __xlDapHostCtxAddMirExternSymbols(XL_HostContext* pHostCtx, const XL_MirNode* pNode)
{
	size_t i;

	if ( pHostCtx == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pNode == NULL ) {
		return XL_RC_OK;
	}
	if ( (pNode->sExternName != NULL) && (pNode->sExternName[0] != '\0') ) {
		if ( xlHostContextAddExternSymbol(pHostCtx, pNode->sExternName) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlDapHostCtxAddMirExternSymbols(pHostCtx, pNode->ppChildren[i]) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int __xlDapHostCtxAddModuleInputs(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		if ( !pModule->bIsRoot &&
			(pModule->sConsumeMode != NULL) &&
			(pModule->sArtifactFile != NULL) &&
			(pModule->sArtifactFile[0] != '\0') &&
			((strcmp(pModule->sConsumeMode, "shared-bridge") == 0) ||
				(strcmp(pModule->sConsumeMode, "static-bridge") == 0) ||
				(strcmp(pModule->sConsumeMode, "c-source-bridge") == 0)) ) {
			if ( xlHostContextAddArtifact(pHostCtx, pModule->sArtifactKind, pModule->sArtifactFile) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		for ( j = 0; j < pModule->iLinkLibCount; ++j ) {
			if ( xlHostContextAddLinkLibrary(pHostCtx, pModule->arrLinkLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		for ( j = 0; j < pModule->iLibPathCount; ++j ) {
			if ( xlHostContextAddLibraryPath(pHostCtx, pModule->arrLibPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		for ( j = 0; j < pModule->iIncPathCount; ++j ) {
			if ( xlHostContextAddIncludePath(pHostCtx, pModule->arrIncPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
		for ( j = 0; j < pModule->iLoadLibCount; ++j ) {
			if ( xlHostContextAddLoadLibrary(pHostCtx, pModule->arrLoadLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}

	return XL_RC_OK;
}

static const char* __xlDapRootSourcePath(const XL_ModuleLinkResult* pModules, const char* sFallback)
{
	size_t i;

	if ( (pModules != NULL) && (pModules->pModules != NULL) ) {
		for ( i = 0u; i < pModules->iModuleCount; ++i ) {
			const XL_ModuleSummary* pModule;

			pModule = &pModules->pModules[i];
			if ( pModule->bIsRoot && (pModule->sFilePath != NULL) && (pModule->sFilePath[0] != '\0') ) {
				return pModule->sFilePath;
			}
		}
	}
	return sFallback;
}

static int __xlDapMirIsDebugExecutableStmt(const XL_MirNode* pNode, int bStmtContext)
{
	if ( pNode == NULL ) {
		return 0;
	}
	if ( (pNode->iLine <= 0) || (pNode->iColumn <= 0) ) {
		return 0;
	}
	switch ( pNode->eKind ) {
		case XL_MIR_FUNC:
		case XL_MIR_BIND:
		case XL_MIR_BRANCH:
		case XL_MIR_TRY:
		case XL_MIR_WHILE:
		case XL_MIR_FORIN:
		case XL_MIR_RETURN:
		case XL_MIR_BREAK:
		case XL_MIR_CONTINUE:
		case XL_MIR_THROW:
		case XL_MIR_ASSERT:
		case XL_MIR_YIELD:
			return 1;
		case XL_MIR_VALUE:
			return bStmtContext &&
				(pNode->iChildCount == 2) &&
				(pNode->sText != NULL) &&
				(strcmp(pNode->sText, "=") == 0);
		default:
			return 0;
	}
}

static const XL_MirNode* __xlDapMirDebugExecutableNode(const XL_MirNode* pNode, int bStmtContext)
{
	if ( pNode == NULL ) {
		return NULL;
	}
	if ( __xlDapMirIsDebugExecutableStmt(pNode, bStmtContext) ) {
		return pNode;
	}
	if ( bStmtContext &&
		(pNode->eKind == XL_MIR_VALUE) &&
		(pNode->iChildCount == 1) &&
		((pNode->sText == NULL) || (pNode->sText[0] == '\0')) &&
		__xlDapMirIsDebugExecutableStmt(pNode->ppChildren[0], 1) ) {
		return pNode->ppChildren[0];
	}
	return NULL;
}

static int __xlDapCollectExecutableLines(
	XL_DebugController* pController,
	const char* sSource,
	const XL_MirNode* pNode,
	int bStmtContext,
	const char* sFunction
)
{
	size_t i;
	int iRc;
	int bChildStmtContext;
	const XL_MirNode* pExecutableNode;
	const char* sChildFunction;
	const char* sExecutableFunction;

	if ( pNode == NULL ) {
		return XL_RC_OK;
	}
	pExecutableNode = __xlDapMirDebugExecutableNode(pNode, bStmtContext);
	if ( (pExecutableNode != NULL) && (pExecutableNode->iLine > 0) && (pExecutableNode->iColumn > 0) ) {
		sExecutableFunction = (pExecutableNode->eKind == XL_MIR_FUNC) ? pExecutableNode->sText : sFunction;
		iRc = xlDebugControllerAddExecutableLine(
			pController,
			sSource,
			pExecutableNode->iLine,
			pExecutableNode->iColumn,
			xlMirKindName(pExecutableNode->eKind),
			sExecutableFunction
		);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	bChildStmtContext = (pNode->eKind == XL_MIR_BLOCK) ? 1 : 0;
	sChildFunction = (pNode->eKind == XL_MIR_FUNC) ? pNode->sText : sFunction;
	for ( i = 0u; i < pNode->iChildCount; ++i ) {
		iRc = __xlDapCollectExecutableLines(pController, sSource, pNode->ppChildren[i], bChildStmtContext, sChildFunction);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}
	return XL_RC_OK;
}

static int __xlDapSendBreakpointChangedEvents(XL_DapSession* pSession)
{
	size_t i;
	size_t iCount;

	if ( pSession == NULL ) {
		return XL_RC_BAD_ARG;
	}
	iCount = xlDebugControllerBreakpointCount(&pSession->controller);
	for ( i = 0u; i < iCount; ++i ) {
		const XL_DebugBreakpoint* pBreakpoint;
		char sMessage[512];
		char sBody[1024];
		int iNeed;

		pBreakpoint = xlDebugControllerBreakpointAt(&pSession->controller, i);
		if ( pBreakpoint == NULL ) {
			continue;
		}
		__xlDapJsonEscapeString(sMessage, sizeof(sMessage), pBreakpoint->sMessage);
		iNeed = snprintf(
			sBody,
			sizeof(sBody),
			"{\"reason\":\"changed\",\"breakpoint\":{\"verified\":%s,\"line\":%d,\"message\":\"%s\"}}",
			pBreakpoint->bVerified ? "true" : "false",
			(pBreakpoint->iActualLine > 0) ? pBreakpoint->iActualLine : pBreakpoint->iLine,
			sMessage
		);
		if ( (iNeed < 0) || ((size_t)iNeed >= sizeof(sBody)) ) {
			return XL_RC_IO_ERROR;
		}
		if ( __xlDapSendEvent(pSession, "breakpoint", sBody) != XL_RC_OK ) {
			return XL_RC_IO_ERROR;
		}
	}
	return XL_RC_OK;
}

static int __xlDapCheckProgramCompiles(XL_DapSession* pSession, XL_DiagContext* pDiag, const char* sProgram)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	char* sCode;
	int iRc;

	if ( (pSession == NULL) || (pDiag == NULL) || (sProgram == NULL) || (sProgram[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sProgram, pDiag, 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	xlDebugControllerClearExecutableLines(&pSession->controller);
	iRc = __xlDapCollectExecutableLines(
		&pSession->controller,
		__xlDapRootSourcePath(&moduleResult, sProgram),
		lowerResult.pMirRoot,
		1,
		NULL
	);
	if ( iRc != XL_RC_OK ) goto cleanup;
	xlDebugControllerBindBreakpoints(&pSession->controller);
	iRc = xlCgenMirDebug(&lowerResult, &moduleResult, pDiag, 1, &sCode);

cleanup:
	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDapRunProgram(
	XL_DapSession* pSession,
	XL_DiagContext* pDiag,
	const char* sProgram,
	XL_HostRunResult* pResult,
	char** psTraceText
)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_HostContext hostCtx;
	char* sCode;
	int iRc;

	if ( (pSession == NULL) || (pDiag == NULL) || (sProgram == NULL) || (sProgram[0] == '\0') || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlHostContextInit(&hostCtx, sProgram, NULL);
	memset(pResult, 0, sizeof(*pResult));
	if ( psTraceText != NULL ) {
		*psTraceText = NULL;
	}
	sCode = NULL;

	iRc = xlModuleLinkFromFile(sProgram, pDiag, 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlCgenMirDebug(&lowerResult, &moduleResult, pDiag, 1, &sCode);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = __xlDapHostCtxAddModuleInputs(&hostCtx, &moduleResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = __xlDapHostCtxAddMirExternSymbols(&hostCtx, lowerResult.pMirRoot);
	if ( iRc != XL_RC_OK ) goto cleanup;

	xlDebugControllerMakeCurrent(&pSession->controller);
	iRc = xlHostRunCText(&hostCtx, pDiag, sProgram, sCode, "xl_generated_entry", psTraceText, pResult);
	xlDebugControllerClearCurrent(&pSession->controller);
	pSession->iLastExitCode = pResult->iExitCode;

cleanup:
	free(sCode);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __xlDapDebuggeeThreadProc(LPVOID pArg)
{
	XL_DapDebuggee* pDebuggee;

	pDebuggee = (XL_DapDebuggee*)pArg;
	if ( (pDebuggee == NULL) || (pDebuggee->pSession == NULL) ) {
		return 1u;
	}

	pDebuggee->iRc = __xlDapRunProgram(
		pDebuggee->pSession,
		pDebuggee->pSession->pDiag,
		pDebuggee->sProgram,
		&pDebuggee->result,
		&pDebuggee->sTraceText
	);
	pDebuggee->bDone = 1;
	if ( pDebuggee->hDoneEvent != NULL ) {
		SetEvent(pDebuggee->hDoneEvent);
	}
	return (DWORD)((pDebuggee->iRc == XL_RC_OK) ? 0u : 1u);
}
#endif

static void __xlDapDebuggeeClose(XL_DapSession* pSession)
{
	if ( pSession == NULL ) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	if ( pSession->debuggee.hThread != NULL ) {
		WaitForSingleObject(pSession->debuggee.hThread, INFINITE);
		CloseHandle(pSession->debuggee.hThread);
		pSession->debuggee.hThread = NULL;
	}
	if ( pSession->debuggee.hDoneEvent != NULL ) {
		CloseHandle(pSession->debuggee.hDoneEvent);
		pSession->debuggee.hDoneEvent = NULL;
	}
#endif
	free(pSession->debuggee.sTraceText);
	memset(&pSession->debuggee, 0, sizeof(pSession->debuggee));
}

static int __xlDapDebuggeeStart(XL_DapSession* pSession, const char* sProgram)
{
	if ( (pSession == NULL) || (sProgram == NULL) || (sProgram[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	__xlDapDebuggeeClose(pSession);
	memset(&pSession->debuggee, 0, sizeof(pSession->debuggee));
	pSession->debuggee.pSession = pSession;
	snprintf(pSession->debuggee.sProgram, sizeof(pSession->debuggee.sProgram), "%s", sProgram);
	pSession->debuggee.iRc = XL_RC_FAIL;
#if defined(_WIN32) || defined(_WIN64)
	pSession->debuggee.hDoneEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
	if ( pSession->debuggee.hDoneEvent == NULL ) {
		__xlDapDebuggeeClose(pSession);
		return XL_RC_IO_ERROR;
	}
	pSession->debuggee.hThread = CreateThread(NULL, 0, __xlDapDebuggeeThreadProc, &pSession->debuggee, 0, NULL);
	if ( pSession->debuggee.hThread == NULL ) {
		__xlDapDebuggeeClose(pSession);
		return XL_RC_IO_ERROR;
	}
	pSession->debuggee.bStarted = 1;
	return XL_RC_OK;
#else
	pSession->debuggee.iRc = __xlDapRunProgram(
		pSession,
		pSession->pDiag,
		sProgram,
		&pSession->debuggee.result,
		&pSession->debuggee.sTraceText
	);
	pSession->debuggee.bStarted = 1;
	pSession->debuggee.bDone = 1;
	return XL_RC_OK;
#endif
}

static int __xlDapWaitForDebuggee(XL_DapSession* pSession, int* pbStopped, int* pbDone)
{
	if ( (pSession == NULL) || (pbStopped == NULL) || (pbDone == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	*pbStopped = 0;
	*pbDone = 0;

#if defined(_WIN32) || defined(_WIN64)
	if ( !pSession->debuggee.bStarted ) {
		return XL_RC_FAIL;
	}
	{
		HANDLE arrWait[2];
		DWORD iWait;

		arrWait[0] = (HANDLE)pSession->controller.hStoppedEvent;
		arrWait[1] = pSession->debuggee.hDoneEvent;
		if ( (arrWait[0] == NULL) || (arrWait[1] == NULL) ) {
			return XL_RC_IO_ERROR;
		}
		iWait = WaitForMultipleObjects(2, arrWait, FALSE, 50u);
		if ( iWait == WAIT_OBJECT_0 ) {
			*pbStopped = 1;
		} else if ( iWait == (WAIT_OBJECT_0 + 1u) ) {
			*pbDone = 1;
			pSession->bDebuggeeCompleted = 1;
			pSession->iLastExitCode = pSession->debuggee.result.iExitCode;
			WaitForSingleObject(pSession->debuggee.hThread, INFINITE);
			CloseHandle(pSession->debuggee.hThread);
			pSession->debuggee.hThread = NULL;
		} else if ( iWait == WAIT_TIMEOUT ) {
			return XL_RC_OK;
		} else {
			return XL_RC_IO_ERROR;
		}
	}
#else
	*pbDone = 1;
	pSession->bDebuggeeCompleted = 1;
#endif
	return XL_RC_OK;
}

static int __xlDapSendDebuggeeWaitEvent(XL_DapSession* pSession, int* pbQuit)
{
	int bStopped;
	int bDone;
	int iRc;

	if ( (pSession == NULL) || (pbQuit == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( ;; ) {
		iRc = __xlDapWaitForDebuggee(pSession, &bStopped, &bDone);
		if ( iRc != XL_RC_OK ) {
			__xlDapStdoutRestore(&pSession->stdoutGuard);
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg debuggee wait failed\\n\"}");
			return iRc;
		}
		if ( bStopped ) {
			__xlDapStdoutRestore(&pSession->stdoutGuard);
			return __xlDapSendStoppedEvent(pSession, pSession->controller.sStopReason);
		}
		if ( bDone ) {
			__xlDapStdoutRestore(&pSession->stdoutGuard);
			if ( pSession->debuggee.result.bHadException ) {
				xlDebugControllerStopAtException(
					&pSession->controller,
					pSession->debuggee.result.sExceptionFile,
					pSession->debuggee.result.iExceptionLine,
					pSession->debuggee.result.iExceptionColumn
				);
				return __xlDapSendStoppedEvent(pSession, "exception");
			}
			xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
			if ( pSession->debuggee.iRc != XL_RC_OK ) {
				(void)__xlDapSendDebuggeeFailureOutput(pSession);
			} else {
				char sExitedBody[64];

				snprintf(sExitedBody, sizeof(sExitedBody), "{\"exitCode\":%d}", pSession->iLastExitCode);
				(void)__xlDapSendEvent(pSession, "exited", sExitedBody);
			}
			(void)__xlDapSendEvent(pSession, "terminated", "{}");
			return XL_RC_OK;
		}
		if ( __xlDapStdinHasData() ) {
			char* sRequest;
			int bNestedQuit;
			int iReadRc;

			sRequest = NULL;
			iReadRc = __xlDapReadMessage(&sRequest);
			if ( iReadRc <= 0 ) {
				free(sRequest);
				*pbQuit = 1;
				return (iReadRc == 0) ? XL_RC_OK : XL_RC_IO_ERROR;
			}
			bNestedQuit = 0;
			iRc = __xlDapHandleRequest(pSession, sRequest, &bNestedQuit);
			free(sRequest);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
			if ( bNestedQuit ) {
				*pbQuit = 1;
				return XL_RC_OK;
			}
		}
	}
}

static int __xlDapSendResponse(
	XL_DapSession* pSession,
	int iRequestSeq,
	const char* sCommand,
	int bSuccess,
	const char* sBody,
	const char* sMessage
)
{
	char* sJson;
	char sCommandEscaped[192];
	char sMessageEscaped[1024];
	size_t iCap;
	int iSeq;
	int iRc;

	if ( (pSession == NULL) || (sCommand == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iSeq = ++pSession->iNextSeq;
	__xlDapJsonEscapeString(sCommandEscaped, sizeof(sCommandEscaped), sCommand);
	__xlDapJsonEscapeString(sMessageEscaped, sizeof(sMessageEscaped), (sMessage != NULL) ? sMessage : "request failed");
	iCap = strlen(sCommandEscaped) + ((sBody != NULL) ? strlen(sBody) : 0u) + strlen(sMessageEscaped) + 256u;
	sJson = (char*)malloc(iCap);
	if ( sJson == NULL ) {
		return XL_RC_FAIL;
	}

	if ( bSuccess ) {
		if ( (sBody != NULL) && (sBody[0] != '\0') ) {
			snprintf(
				sJson,
				iCap,
				"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,\"command\":\"%s\",\"body\":%s}",
				iSeq,
				iRequestSeq,
				sCommandEscaped,
				sBody
			);
		} else {
			snprintf(
				sJson,
				iCap,
				"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,\"command\":\"%s\"}",
				iSeq,
				iRequestSeq,
				sCommandEscaped
			);
		}
	} else {
		snprintf(
			sJson,
			iCap,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":false,\"command\":\"%s\",\"message\":\"%s\"}",
			iSeq,
			iRequestSeq,
			sCommandEscaped,
			sMessageEscaped
		);
	}

	iRc = __xlDapSendJson(pSession, sJson);
	free(sJson);
	return iRc;
}

static int __xlDapSendEvent(XL_DapSession* pSession, const char* sEvent, const char* sBody)
{
	char sJson[4096];
	int iSeq;

	if ( (pSession == NULL) || (sEvent == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iSeq = ++pSession->iNextSeq;
	if ( (sBody != NULL) && (sBody[0] != '\0') ) {
		snprintf(
			sJson,
			sizeof(sJson),
			"{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\",\"body\":%s}",
			iSeq,
			sEvent,
			sBody
		);
	} else {
		snprintf(
			sJson,
			sizeof(sJson),
			"{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\"}",
			iSeq,
			sEvent
		);
	}
	return __xlDapSendJson(pSession, sJson);
}

static void __xlDapJsonEscapeString(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iOut;
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sOut[0] = '\0';
	if ( sText == NULL ) {
		return;
	}

	iOut = 0u;
	for ( i = 0u; (sText[i] != '\0') && ((iOut + 1u) < iOutSize); ++i ) {
		char ch = sText[i];

		if ( (ch == '\\') || (ch == '"') ) {
			if ( (iOut + 2u) >= iOutSize ) {
				break;
			}
			sOut[iOut++] = '\\';
			sOut[iOut++] = ch;
		} else if ( ch == '\n' ) {
			if ( (iOut + 2u) >= iOutSize ) {
				break;
			}
			sOut[iOut++] = '\\';
			sOut[iOut++] = 'n';
		} else if ( ch == '\r' ) {
			if ( (iOut + 2u) >= iOutSize ) {
				break;
			}
			sOut[iOut++] = '\\';
			sOut[iOut++] = 'r';
		} else if ( ch == '\t' ) {
			if ( (iOut + 2u) >= iOutSize ) {
				break;
			}
			sOut[iOut++] = '\\';
			sOut[iOut++] = 't';
		} else {
			sOut[iOut++] = ch;
		}
	}
	sOut[iOut] = '\0';
}

static const char* __xlDapTraceTail(const char* sTraceText)
{
	const size_t iMaxTail = 1200u;
	size_t iLen;
	const char* sTail;
	const char* sLine;

	if ( sTraceText == NULL ) {
		return "";
	}

	iLen = strlen(sTraceText);
	if ( iLen <= iMaxTail ) {
		return sTraceText;
	}

	sTail = sTraceText + iLen - iMaxTail;
	sLine = strchr(sTail, '\n');
	if ( (sLine != NULL) && (sLine[1] != '\0') ) {
		return sLine + 1;
	}
	return sTail;
}

static int __xlDapSendDebuggeeFailureOutput(XL_DapSession* pSession)
{
	char sRaw[1800];
	char sEscaped[3600];
	char sBody[3900];
	const char* sKind;
	const char* sTraceTail;

	if ( pSession == NULL ) {
		return XL_RC_BAD_ARG;
	}

	sKind = pSession->debuggee.result.bHadRuntimeError ? "host runtime failure" : "host launch failure";
	sTraceTail = __xlDapTraceTail(pSession->debuggee.sTraceText);
	if ( sTraceTail[0] != '\0' ) {
		snprintf(
			sRaw,
			sizeof(sRaw),
			"xldbg launch failed: %s\ntrace tail:\n%s",
			sKind,
			sTraceTail
		);
	} else {
		snprintf(sRaw, sizeof(sRaw), "xldbg launch failed: %s\n", sKind);
	}

	__xlDapJsonEscapeString(sEscaped, sizeof(sEscaped), sRaw);
	snprintf(sBody, sizeof(sBody), "{\"category\":\"stderr\",\"output\":\"%s\"}", sEscaped);
	return __xlDapSendEvent(pSession, "output", sBody);
}

static int __xlDapSendStoppedEvent(XL_DapSession* pSession, const char* sReason)
{
	char sBody[256];
	char sText[XL_DIAG_TEXT_MAX * 2];

	if ( sReason == NULL ) {
		sReason = "breakpoint";
	}
	if ( (pSession != NULL) &&
		(strcmp(sReason, "exception") == 0) &&
		pSession->debuggee.result.bHadException &&
		(pSession->debuggee.result.sExceptionText[0] != '\0') ) {
		__xlDapJsonEscapeString(sText, sizeof(sText), pSession->debuggee.result.sExceptionText);
		snprintf(
			sBody,
			sizeof(sBody),
			"{\"reason\":\"exception\",\"threadId\":1,\"allThreadsStopped\":true,\"text\":\"uncaught exception: %s\"}",
			sText
		);
		return __xlDapSendEvent(pSession, "stopped", sBody);
	}
	snprintf(
		sBody,
		sizeof(sBody),
		"{\"reason\":\"%s\",\"threadId\":1,\"allThreadsStopped\":true}",
		sReason
	);
	return __xlDapSendEvent(pSession, "stopped", sBody);
}

static int __xlDapResumeDebuggee(
	XL_DapSession* pSession,
	int iRequestSeq,
	const char* sCommand,
	const char* sResponseBody,
	XL_DebugStepMode eStepMode,
	int* pbQuit
)
{
	int iRc;

	if ( (pSession == NULL) || (sCommand == NULL) || (pbQuit == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( pSession->bDebuggeeCompleted &&
		xlDebugControllerHasStop(&pSession->controller) &&
		(strcmp(pSession->controller.sStopReason, "exception") == 0) ) {
		if ( strcmp(sCommand, "continue") != 0 ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is stopped at a completed exception");
		}
		xlDebugControllerClearStop(&pSession->controller);
		xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sResponseBody, NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		(void)__xlDapSendEvent(pSession, "terminated", "{}");
		*pbQuit = 1;
		return XL_RC_OK;
	}
	if ( pSession->bDebuggeeCompleted || !xlDebugControllerHasStop(&pSession->controller) ) {
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is not stopped");
	}
	xlDebugControllerSetStepMode(&pSession->controller, eStepMode);
	xlDebugControllerClearStop(&pSession->controller);
	iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sResponseBody, NULL);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	iRc = __xlDapStdoutSuppress(&pSession->stdoutGuard);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	xlDebugControllerResume(&pSession->controller);
	iRc = __xlDapSendDebuggeeWaitEvent(pSession, pbQuit);
	if ( pSession->bDebuggeeCompleted && !xlDebugControllerHasStop(&pSession->controller) ) {
		*pbQuit = 1;
	}
	return iRc;
}

static void __xlDapBuildStackTraceBody(const XL_DebugController* pController, char* sOut, size_t iOutSize)
{
	const XL_DebugFrame* arrFrames;
	size_t iFrameCount;
	size_t iOutLen;
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sOut[0] = '\0';
	if ( pController == NULL ) {
		snprintf(sOut, iOutSize, "{\"stackFrames\":[],\"totalFrames\":0}");
		return;
	}

	arrFrames = pController->bStopped ? pController->arrStopFrames : pController->arrFrames;
	iFrameCount = pController->bStopped ? pController->iStopFrameCount : pController->iFrameCount;
	if ( iFrameCount == 0u ) {
		char sPath[XL_PATH_MAX_LEN * 2];
		char sFunc[256];
		int iLine;
		int iColumn;

		iLine = pController->bStopped ? pController->iStopLine : pController->iCurrentLine;
		iColumn = pController->bStopped ? pController->iStopColumn : pController->iCurrentColumn;
		__xlDapJsonEscapeString(
			sPath,
			sizeof(sPath),
			pController->bStopped ? pController->sStopSource : pController->sCurrentSource
		);
		__xlDapJsonEscapeString(
			sFunc,
			sizeof(sFunc),
			pController->bStopped ? pController->sStopFunc : pController->sCurrentFunc
		);
		if ( iLine <= 0 ) {
			snprintf(sOut, iOutSize, "{\"stackFrames\":[],\"totalFrames\":0}");
			return;
		}
		if ( sFunc[0] == '\0' ) {
			snprintf(sFunc, sizeof(sFunc), "main");
		}

		snprintf(
			sOut,
			iOutSize,
			"{\"stackFrames\":[{\"id\":1,\"name\":\"%s\",\"line\":%d,\"column\":%d,\"source\":{\"name\":\"%s\",\"path\":\"%s\"}}],\"totalFrames\":1}",
			sFunc,
			iLine,
			(iColumn > 0) ? iColumn : 1,
			sPath,
			sPath
		);
		return;
	}

	snprintf(sOut, iOutSize, "{\"stackFrames\":[");
	for ( i = 0u; i < iFrameCount; ++i ) {
		const XL_DebugFrame* pFrame;
		char sPath[XL_PATH_MAX_LEN * 2];
		char sFunc[256];
		int iWritten;

		pFrame = &arrFrames[iFrameCount - 1u - i];
		__xlDapJsonEscapeString(sPath, sizeof(sPath), pFrame->sSource);
		__xlDapJsonEscapeString(sFunc, sizeof(sFunc), pFrame->sFunc);
		if ( sFunc[0] == '\0' ) {
			snprintf(sFunc, sizeof(sFunc), "frame");
		}
		iOutLen = strlen(sOut);
		if ( iOutLen + strlen(sPath) + strlen(sFunc) + 160u >= iOutSize ) {
			break;
		}
		iWritten = snprintf(
			sOut + iOutLen,
			iOutSize - iOutLen,
			"%s{\"id\":%llu,\"name\":\"%s\",\"line\":%d,\"column\":%d,\"source\":{\"name\":\"%s\",\"path\":\"%s\"}}",
			(i == 0u) ? "" : ",",
			(unsigned long long)(i + 1u),
			sFunc,
			(pFrame->iLine > 0) ? pFrame->iLine : 1,
			(pFrame->iColumn > 0) ? pFrame->iColumn : 1,
			sPath,
			sPath
		);
		if ( iWritten < 0 ) {
			break;
		}
	}
	iOutLen = strlen(sOut);
	if ( iOutLen + 64u >= iOutSize ) {
		snprintf(sOut, iOutSize, "{\"stackFrames\":[],\"totalFrames\":0}");
		return;
	}
	snprintf(sOut + iOutLen, iOutSize - iOutLen, "],\"totalFrames\":%llu}", (unsigned long long)iFrameCount);
}

static size_t __xlDapFrameCount(const XL_DebugController* pController)
{
	if ( pController == NULL ) {
		return 0u;
	}
	return pController->bStopped ? pController->iStopFrameCount : pController->iFrameCount;
}

static int __xlDapFrameIdToFrameIndex(const XL_DebugController* pController, int iFrameId, size_t* piFrameIndex)
{
	size_t iFrameCount;

	if ( piFrameIndex != NULL ) {
		*piFrameIndex = 0u;
	}
	if ( (pController == NULL) || (iFrameId <= 0) ) {
		return 0;
	}
	iFrameCount = __xlDapFrameCount(pController);
	if ( (iFrameCount == 0u) || ((size_t)iFrameId > iFrameCount) ) {
		return 0;
	}
	if ( piFrameIndex != NULL ) {
		*piFrameIndex = iFrameCount - (size_t)iFrameId;
	}
	return 1;
}

static int __xlDapBuildFrameVariablesReference(const XL_DebugController* pController, int iBase, int iFrameId)
{
	size_t iStopSequence;

	if ( (pController == NULL) || (iFrameId <= 0) ) {
		return iBase + iFrameId;
	}
	iStopSequence = pController->bStopped ? pController->iStopSequence : 0u;
	if ( iStopSequence > 1000000u ) {
		iStopSequence = 1000000u;
	}
	return iBase + ((int)iStopSequence * XL_DAP_VARIABLE_REF_STRIDE) + iFrameId;
}

static int __xlDapDecodeFrameVariablesReference(
	const XL_DebugController* pController,
	int iVariablesReference,
	int iBase,
	int* piFrameId,
	int* pbStale
)
{
	int iOffset;
	int iRefSequence;
	int iFrameId;
	int iCurrentSequence;

	if ( piFrameId != NULL ) {
		*piFrameId = 0;
	}
	if ( pbStale != NULL ) {
		*pbStale = 0;
	}
	if ( (pController == NULL) || (iVariablesReference <= iBase) ) {
		return 0;
	}
	iOffset = iVariablesReference - iBase;
	iRefSequence = iOffset / XL_DAP_VARIABLE_REF_STRIDE;
	iFrameId = iOffset % XL_DAP_VARIABLE_REF_STRIDE;
	if ( (iFrameId <= 0) || (iFrameId > (int)XL_DEBUG_FRAME_MAX) ) {
		return 0;
	}
	iCurrentSequence = pController->bStopped ? (int)pController->iStopSequence : 0;
	if ( iRefSequence != iCurrentSequence ) {
		if ( pbStale != NULL ) {
			*pbStale = 1;
		}
		return 0;
	}
	if ( piFrameId != NULL ) {
		*piFrameId = iFrameId;
	}
	return 1;
}

static void __xlDapBuildScopesBody(const XL_DebugController* pController, int iFrameId, char* sOut, size_t iOutSize)
{
	size_t iFrameIndex;
	int iUseFrameId;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	iUseFrameId = (__xlDapFrameIdToFrameIndex(pController, iFrameId, &iFrameIndex) ? iFrameId : 1);
	XL_UNUSED(iFrameIndex);
	snprintf(
		sOut,
		iOutSize,
		"{\"scopes\":[{\"name\":\"Arguments\",\"variablesReference\":%d,\"expensive\":false},{\"name\":\"Locals\",\"variablesReference\":%d,\"expensive\":false}]}",
		__xlDapBuildFrameVariablesReference(pController, XL_DAP_ARGUMENTS_REF_BASE, iUseFrameId),
		__xlDapBuildFrameVariablesReference(pController, XL_DAP_LOCALS_REF_BASE, iUseFrameId)
	);
}

static const char* __xlDapValueTypeName(xvalue value)
{
	if ( value == NULL ) {
		return "null";
	}
	switch ( value->Type ) {
		case XVO_DT_NULL: return "null";
		case XVO_DT_BOOL: return "bool";
		case XVO_DT_INT: return "int";
		case XVO_DT_FLOAT: return "float";
		case XVO_DT_TEXT: return "string";
		case XVO_DT_TIME: return "time";
		case XVO_DT_POINT: return "point";
		case XVO_DT_FUNC: return "function";
		case XVO_DT_ARRAY: return "array";
		case XVO_DT_LIST: return "list";
		case XVO_DT_COLL: return "coll";
		case XVO_DT_TABLE: return "table";
		case XVO_DT_CLASS: return "class";
		case XVO_DT_CUSTOM: return "custom";
		default: return "var";
	}
}

static void __xlDapValueText(char* sOut, size_t iOutSize, xvalue value)
{
	const char* sText;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	if ( value == NULL ) {
		sOut[0] = '\0';
		return;
	}
	switch ( value->Type ) {
		case XVO_DT_NULL:
		case XVO_DT_BOOL:
		case XVO_DT_INT:
		case XVO_DT_FLOAT:
		case XVO_DT_TEXT:
		case XVO_DT_TIME:
		case XVO_DT_POINT:
			sText = xl_value_to_string(value);
			snprintf(sOut, iOutSize, "%s", (sText != NULL) ? sText : "");
			break;
		case XVO_DT_ARRAY:
			snprintf(sOut, iOutSize, "array[%u]", (unsigned)xvoArrayItemCount(value));
			break;
		case XVO_DT_LIST:
			snprintf(sOut, iOutSize, "list[%u]", (unsigned)xvoListItemCount(value));
			break;
		default:
			snprintf(sOut, iOutSize, "[var:%p]", (void*)value);
			break;
	}
}

static int __xlDapValueHasChildren(xvalue value)
{
	if ( value == NULL ) {
		return 0;
	}
	switch ( value->Type ) {
		case XVO_DT_ARRAY:
			return xvoArrayItemCount(value) != 0u;
		case XVO_DT_LIST:
			return xvoListItemCount(value) != 0u;
		default:
			return 0;
	}
}

static int __xlDapRegisterVariableChildrenRef(XL_DapSession* pSession, void* pValue)
{
	XL_DapVariableChildrenRef* pRef;
	size_t i;
	size_t iIndex;

	if ( (pSession == NULL) || (pValue == NULL) || !__xlDapValueHasChildren((xvalue)pValue) ) {
		return 0;
	}
	for ( i = 0u; i < XL_DAP_CHILDREN_REF_MAX; ++i ) {
		pRef = &pSession->arrVariableChildrenRefs[i];
		if ( (pRef->pValue == pValue) && (pRef->iStopSequence == pSession->controller.iStopSequence) ) {
			return pRef->iReference;
		}
	}
	iIndex = pSession->iVariableChildrenRefNext % XL_DAP_CHILDREN_REF_MAX;
	pSession->iVariableChildrenRefNext = (iIndex + 1u) % XL_DAP_CHILDREN_REF_MAX;
	pRef = &pSession->arrVariableChildrenRefs[iIndex];
	pRef->iReference = XL_DAP_CHILDREN_REF_BASE + (int)iIndex + 1;
	pRef->iStopSequence = pSession->controller.iStopSequence;
	pRef->pValue = pValue;
	return pRef->iReference;
}

static int __xlDapFindVariableChildrenRef(XL_DapSession* pSession, int iVariablesReference, void** ppValue, int* pbStale)
{
	size_t iIndex;
	XL_DapVariableChildrenRef* pRef;

	if ( ppValue != NULL ) {
		*ppValue = NULL;
	}
	if ( pbStale != NULL ) {
		*pbStale = 0;
	}
	if ( (pSession == NULL) ||
		(iVariablesReference <= XL_DAP_CHILDREN_REF_BASE) ||
		(iVariablesReference > XL_DAP_CHILDREN_REF_BASE + (int)XL_DAP_CHILDREN_REF_MAX) ) {
		return 0;
	}
	iIndex = (size_t)(iVariablesReference - XL_DAP_CHILDREN_REF_BASE - 1);
	pRef = &pSession->arrVariableChildrenRefs[iIndex];
	if ( pRef->iReference != iVariablesReference ) {
		return 0;
	}
	if ( pRef->iStopSequence != pSession->controller.iStopSequence ) {
		if ( pbStale != NULL ) {
			*pbStale = 1;
		}
		return 0;
	}
	if ( ppValue != NULL ) {
		*ppValue = pRef->pValue;
	}
	return pRef->pValue != NULL;
}

static void __xlDapBuildValueChildrenBody(XL_DapSession* pSession, xvalue value, char* sOut, size_t iOutSize)
{
	uint32 iCount;
	uint32 i;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	snprintf(sOut, iOutSize, "{\"variables\":[");
	if ( value == NULL ) {
		goto finish;
	}
	if ( value->Type == XVO_DT_ARRAY ) {
		iCount = xvoArrayItemCount(value);
	} else if ( value->Type == XVO_DT_LIST ) {
		iCount = xvoListItemCount(value);
	} else {
		iCount = 0u;
	}
	if ( iCount > 64u ) {
		iCount = 64u;
	}
	for ( i = 0u; i < iCount; ++i ) {
		xvalue child;
		char sName[32];
		char sType[80];
		char sValue[512];
		char sEscapedValue[512];
		int iChildRef;

		child = (value->Type == XVO_DT_ARRAY) ?
			(xvalue)xrtPtrArrayGet_Unsafe(value->vArray, i + 1u) :
			xvoListGetValue(value, (int64)i);
		snprintf(sName, sizeof(sName), "[%u]", (unsigned)i);
		__xlDapJsonEscapeString(sType, sizeof(sType), __xlDapValueTypeName(child));
		__xlDapValueText(sValue, sizeof(sValue), child);
		__xlDapJsonEscapeString(sEscapedValue, sizeof(sEscapedValue), sValue);
		iChildRef = __xlDapRegisterVariableChildrenRef(pSession, child);
		iUsed = strlen(sOut);
		if ( iUsed + strlen(sName) + strlen(sType) + strlen(sEscapedValue) + 96u >= iOutSize ) {
			break;
		}
		snprintf(
			sOut + iUsed,
			iOutSize - iUsed,
			"%s{\"name\":\"%s\",\"value\":\"%s\",\"type\":\"%s\",\"variablesReference\":%d}",
			(i == 0u) ? "" : ",",
			sName,
			sEscapedValue,
			sType,
			iChildRef
		);
	}
finish:
	iUsed = strlen(sOut);
	if ( iUsed + 3u < iOutSize ) {
		snprintf(sOut + iUsed, iOutSize - iUsed, "]}");
	}
}

static void __xlDapBuildVariablesBody(XL_DapSession* pSession, int iVariablesReference, char* sOut, size_t iOutSize)
{
	const XL_DebugController* pController;
	const XL_DebugVariable* arrVariables;
	size_t iVariableCount;
	size_t iFrameIndex;
	int iFrameId;
	void* pChildrenValue;
	size_t i;
	size_t iUsed;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	sOut[0] = '\0';
	pController = (pSession != NULL) ? &pSession->controller : NULL;
	if ( pController == NULL ) {
		snprintf(sOut, iOutSize, "{\"variables\":[]}");
		return;
	}
	pChildrenValue = NULL;
	if ( __xlDapFindVariableChildrenRef(pSession, iVariablesReference, &pChildrenValue, NULL) ) {
		__xlDapBuildValueChildrenBody(pSession, (xvalue)pChildrenValue, sOut, iOutSize);
		return;
	}
	if ( iVariablesReference == 1001 ) {
		arrVariables = pController->arrStopArguments;
		iVariableCount = pController->iStopArgumentCount;
	} else if ( iVariablesReference == 1002 ) {
		arrVariables = pController->arrStopLocals;
		iVariableCount = pController->iStopLocalCount;
	} else if ( __xlDapDecodeFrameVariablesReference(pController, iVariablesReference, XL_DAP_ARGUMENTS_REF_BASE, &iFrameId, NULL) ) {
		if ( !__xlDapFrameIdToFrameIndex(pController, iFrameId, &iFrameIndex) ) {
			snprintf(sOut, iOutSize, "{\"variables\":[]}");
			return;
		}
		if ( pController->bStopped ) {
			const XL_DebugFrame* pFrame = &pController->arrStopFrames[iFrameIndex];

			if ( pFrame->iArgumentStart >= pController->iStopFrameArgumentCount ) {
				arrVariables = pController->arrStopFrameArguments;
				iVariableCount = 0u;
			} else {
				arrVariables = &pController->arrStopFrameArguments[pFrame->iArgumentStart];
				iVariableCount = pFrame->iArgumentCount;
			}
			if ( (pFrame->iArgumentStart + iVariableCount) > pController->iStopFrameArgumentCount ) {
				iVariableCount = (pFrame->iArgumentStart < pController->iStopFrameArgumentCount) ?
					(pController->iStopFrameArgumentCount - pFrame->iArgumentStart) : 0u;
			}
		} else {
			const XL_DebugFrame* pFrame = &pController->arrFrames[iFrameIndex];

			if ( pFrame->iArgumentStart >= pController->iFrameArgumentCount ) {
				arrVariables = pController->arrFrameArguments;
				iVariableCount = 0u;
			} else {
				arrVariables = &pController->arrFrameArguments[pFrame->iArgumentStart];
				iVariableCount = pFrame->iArgumentCount;
			}
			if ( (pFrame->iArgumentStart + iVariableCount) > pController->iFrameArgumentCount ) {
				iVariableCount = (pFrame->iArgumentStart < pController->iFrameArgumentCount) ?
					(pController->iFrameArgumentCount - pFrame->iArgumentStart) : 0u;
			}
		}
	} else if ( __xlDapDecodeFrameVariablesReference(pController, iVariablesReference, XL_DAP_LOCALS_REF_BASE, &iFrameId, NULL) ) {
		if ( !__xlDapFrameIdToFrameIndex(pController, iFrameId, &iFrameIndex) ) {
			snprintf(sOut, iOutSize, "{\"variables\":[]}");
			return;
		}
		if ( pController->bStopped ) {
			const XL_DebugFrame* pFrame = &pController->arrStopFrames[iFrameIndex];

			if ( pFrame->iLocalStart >= pController->iStopFrameLocalCount ) {
				arrVariables = pController->arrStopFrameLocals;
				iVariableCount = 0u;
			} else {
				arrVariables = &pController->arrStopFrameLocals[pFrame->iLocalStart];
				iVariableCount = pFrame->iLocalCount;
			}
			if ( (pFrame->iLocalStart + iVariableCount) > pController->iStopFrameLocalCount ) {
				iVariableCount = (pFrame->iLocalStart < pController->iStopFrameLocalCount) ?
					(pController->iStopFrameLocalCount - pFrame->iLocalStart) : 0u;
			}
		} else {
			const XL_DebugFrame* pFrame = &pController->arrFrames[iFrameIndex];

			if ( pFrame->iLocalStart >= pController->iFrameLocalCount ) {
				arrVariables = pController->arrFrameLocals;
				iVariableCount = 0u;
			} else {
				arrVariables = &pController->arrFrameLocals[pFrame->iLocalStart];
				iVariableCount = pFrame->iLocalCount;
			}
			if ( (pFrame->iLocalStart + iVariableCount) > pController->iFrameLocalCount ) {
				iVariableCount = (pFrame->iLocalStart < pController->iFrameLocalCount) ?
					(pController->iFrameLocalCount - pFrame->iLocalStart) : 0u;
			}
		}
	} else {
		snprintf(sOut, iOutSize, "{\"variables\":[]}");
		return;
	}

	snprintf(sOut, iOutSize, "{\"variables\":[");
	for ( i = 0u; i < iVariableCount; ++i ) {
		const XL_DebugVariable* pVariable;
		char sName[160];
		char sType[80];
		char sValue[512];
		int iChildRef;

		pVariable = &arrVariables[i];
		__xlDapJsonEscapeString(sName, sizeof(sName), pVariable->sName);
		__xlDapJsonEscapeString(sType, sizeof(sType), pVariable->sType);
		__xlDapJsonEscapeString(sValue, sizeof(sValue), pVariable->sValue);
		iChildRef = __xlDapRegisterVariableChildrenRef(pSession, pVariable->pValue);
		iUsed = strlen(sOut);
		if ( iUsed + strlen(sName) + strlen(sValue) + strlen(sType) + 96u >= iOutSize ) {
			break;
		}
		snprintf(
			sOut + iUsed,
			iOutSize - iUsed,
			"%s{\"name\":\"%s\",\"value\":\"%s\",\"type\":\"%s\",\"variablesReference\":%d}",
			(i == 0u) ? "" : ",",
			sName,
			sValue,
			sType,
			iChildRef
		);
	}
	iUsed = strlen(sOut);
	if ( iUsed + 3u < iOutSize ) {
		snprintf(sOut + iUsed, iOutSize - iUsed, "]}");
	}
}

static int __xlDapHandleRequest(XL_DapSession* pSession, const char* sRequest, int* pbQuit)
{
	char sCommand[80];
	int iRequestSeq;

	if ( (pSession == NULL) || (sRequest == NULL) || (pbQuit == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*pbQuit = 0;
	iRequestSeq = 0;
	(void)__xlDapJsonGetInt(sRequest, "seq", &iRequestSeq);
	if ( !__xlDapJsonValidate(sRequest) ) {
		*pbQuit = 1;
		return __xlDapSendResponse(pSession, iRequestSeq, "unknown", 0, NULL, "invalid JSON");
	}
	if ( !__xlDapJsonGetString(sRequest, "command", sCommand, sizeof(sCommand)) ) {
		return __xlDapSendResponse(pSession, iRequestSeq, "unknown", 0, NULL, "missing command");
	}

	if ( strcmp(sCommand, "initialize") == 0 ) {
		int iRc;

		iRc = __xlDapSendResponse(
			pSession,
			iRequestSeq,
			sCommand,
			1,
			"{\"supportsConfigurationDoneRequest\":true,\"supportsConditionalBreakpoints\":false,\"supportsEvaluateForHovers\":false,\"supportsSetVariable\":false,\"supportsTerminateRequest\":false}",
			NULL
		);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		return __xlDapSendEvent(pSession, "initialized", "{}");
	}

	if ( strcmp(sCommand, "setBreakpoints") == 0 ) {
		char sSource[XL_PATH_MAX_LEN];
		char sBody[65536];
		int arrLines[XL_DEBUG_BREAKPOINT_MAX];
		int arrColumns[XL_DEBUG_BREAKPOINT_MAX];
		int arrUnsupported[XL_DEBUG_BREAKPOINT_MAX];
		size_t iBreakpointCount;
		int iRc;

		if ( !__xlDapJsonGetString(sRequest, "path", sSource, sizeof(sSource)) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "setBreakpoints requires source.path");
		}
		memset(arrLines, 0, sizeof(arrLines));
		memset(arrColumns, 0, sizeof(arrColumns));
		memset(arrUnsupported, 0, sizeof(arrUnsupported));
		iBreakpointCount = __xlDapCollectBreakpointLines(sRequest, arrLines, arrColumns, arrUnsupported, XL_COUNTOF(arrLines));
		iRc = xlDebugControllerSetBreakpoints(&pSession->controller, sSource, arrLines, arrColumns, arrUnsupported, iBreakpointCount);
		if ( iRc != XL_RC_OK ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "failed to set breakpoints");
		}
		xlDebugControllerBindBreakpoints(&pSession->controller);
		iRc = __xlDapBuildBreakpointResponseBody(&pSession->controller, sSource, sBody, sizeof(sBody));
		if ( iRc != XL_RC_OK ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "failed to build breakpoint response");
		}
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
	}

	if ( strcmp(sCommand, "setExceptionBreakpoints") == 0 ) {
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, "{\"breakpoints\":[]}", NULL);
	}

	if ( strcmp(sCommand, "configurationDone") == 0 ) {
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
	}

	if ( strcmp(sCommand, "threads") == 0 ) {
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, "{\"threads\":[{\"id\":1,\"name\":\"main\"}]}", NULL);
	}

	if ( strcmp(sCommand, "xldbg/executableLines") == 0 ) {
		char* sBody;
		int iRc;

		sBody = xlDebugControllerDumpExecutableLinesJson(&pSession->controller);
		if ( sBody == NULL ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "failed to dump executable lines");
		}
		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
		free(sBody);
		return iRc;
	}

	if ( strcmp(sCommand, "stackTrace") == 0 ) {
		char sBody[65536];

		__xlDapBuildStackTraceBody(&pSession->controller, sBody, sizeof(sBody));
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
	}

	if ( strcmp(sCommand, "scopes") == 0 ) {
		int iFrameId;
		char sBody[512];

		iFrameId = 1;
		(void)__xlDapJsonGetInt(sRequest, "frameId", &iFrameId);
		__xlDapBuildScopesBody(&pSession->controller, iFrameId, sBody, sizeof(sBody));
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
	}

	if ( strcmp(sCommand, "variables") == 0 ) {
		int iVariablesReference;
		int iFrameId;
		int bStale;
		char sBody[65536];

		iVariablesReference = 0;
		iFrameId = 0;
		bStale = 0;
		(void)__xlDapJsonGetInt(sRequest, "variablesReference", &iVariablesReference);
		if ( (iVariablesReference != 1001) &&
			(iVariablesReference != 1002) &&
			!__xlDapFindVariableChildrenRef(pSession, iVariablesReference, NULL, &bStale) &&
			!__xlDapDecodeFrameVariablesReference(&pSession->controller, iVariablesReference, XL_DAP_ARGUMENTS_REF_BASE, &iFrameId, &bStale) &&
			!__xlDapDecodeFrameVariablesReference(&pSession->controller, iVariablesReference, XL_DAP_LOCALS_REF_BASE, &iFrameId, &bStale) ) {
			if ( bStale ) {
				return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "stale variablesReference");
			}
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "unknown variablesReference");
		}
		__xlDapBuildVariablesBody(pSession, iVariablesReference, sBody, sizeof(sBody));
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
	}

	if ( strcmp(sCommand, "launch") == 0 ) {
		char sProgram[XL_PATH_MAX_LEN];
		int bStopOnEntry;
		int iRc;

		if ( pSession->debuggee.bStarted && !pSession->bDebuggeeCompleted ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is already running");
		}
		if ( !__xlDapJsonGetString(sRequest, "program", sProgram, sizeof(sProgram)) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "launch requires program");
		}
		bStopOnEntry = 0;
		(void)__xlDapJsonGetBool(sRequest, "stopOnEntry", &bStopOnEntry);
		iRc = __xlDapCheckProgramCompiles(pSession, pSession->pDiag, sProgram);
		if ( iRc != XL_RC_OK ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "launch failed");
		}
		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		iRc = __xlDapSendBreakpointChangedEvents(pSession);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pSession->bDebuggeeCompleted = 0;
		xlDebugControllerClearStop(&pSession->controller);
		xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_RUNNING);
		xlDebugControllerSetStepMode(&pSession->controller, XL_DEBUG_STEP_NONE);
		xlDebugControllerSetStopOnEntry(&pSession->controller, bStopOnEntry);
		xlDebugControllerSetBlocking(&pSession->controller, 1);
		iRc = __xlDapStdoutSuppress(&pSession->stdoutGuard);
		if ( iRc != XL_RC_OK ) {
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg launch failed\\n\"}");
			xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
			(void)__xlDapSendEvent(pSession, "terminated", "{}");
			*pbQuit = 1;
			return XL_RC_OK;
		}
		iRc = __xlDapDebuggeeStart(pSession, sProgram);
		if ( iRc != XL_RC_OK ) {
			__xlDapStdoutRestore(&pSession->stdoutGuard);
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg launch failed\\n\"}");
			xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
			(void)__xlDapSendEvent(pSession, "terminated", "{}");
			*pbQuit = 1;
			return XL_RC_OK;
		}
		iRc = __xlDapSendDebuggeeWaitEvent(pSession, pbQuit);
		if ( pSession->bDebuggeeCompleted && !xlDebugControllerHasStop(&pSession->controller) ) {
			*pbQuit = 1;
		}
		return iRc;
	}

	if ( strcmp(sCommand, "continue") == 0 ) {
		return __xlDapResumeDebuggee(
			pSession,
			iRequestSeq,
			sCommand,
			"{\"allThreadsContinued\":true}",
			XL_DEBUG_STEP_NONE,
			pbQuit
		);
	}

	if ( strcmp(sCommand, "pause") == 0 ) {
		if ( pSession->bDebuggeeCompleted || !pSession->debuggee.bStarted ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is not running");
		}
		if ( xlDebugControllerHasStop(&pSession->controller) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is already stopped");
		}
		xlDebugControllerRequestPause(&pSession->controller);
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
	}

	if ( strcmp(sCommand, "next") == 0 ) {
		return __xlDapResumeDebuggee(pSession, iRequestSeq, sCommand, NULL, XL_DEBUG_STEP_NEXT, pbQuit);
	}

	if ( strcmp(sCommand, "stepIn") == 0 ) {
		return __xlDapResumeDebuggee(pSession, iRequestSeq, sCommand, NULL, XL_DEBUG_STEP_IN, pbQuit);
	}

	if ( strcmp(sCommand, "stepOut") == 0 ) {
		return __xlDapResumeDebuggee(pSession, iRequestSeq, sCommand, NULL, XL_DEBUG_STEP_OUT, pbQuit);
	}

	if ( strcmp(sCommand, "disconnect") == 0 ) {
		int iRc;

		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		xlDebugControllerClearBreakpoints(&pSession->controller);
		xlDebugControllerSetBlocking(&pSession->controller, 0);
		if ( xlDebugControllerHasStop(&pSession->controller) ) {
			xlDebugControllerClearStop(&pSession->controller);
			(void)__xlDapStdoutSuppress(&pSession->stdoutGuard);
			xlDebugControllerResume(&pSession->controller);
			__xlDapStdoutRestore(&pSession->stdoutGuard);
		}
		__xlDapDebuggeeClose(pSession);
		xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
		(void)__xlDapSendEvent(pSession, "terminated", "{}");
		*pbQuit = 1;
		return XL_RC_OK;
	}

	return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "unsupported request");
}

static char* __xlDbgTrim(char* sText)
{
	char* sEnd;

	if ( sText == NULL ) {
		return NULL;
	}
	while ( (*sText == ' ') || (*sText == '\t') || (*sText == '\r') || (*sText == '\n') ) {
		++sText;
	}
	sEnd = sText + strlen(sText);
	while ( (sEnd > sText) &&
		((sEnd[-1] == ' ') || (sEnd[-1] == '\t') || (sEnd[-1] == '\r') || (sEnd[-1] == '\n')) ) {
		--sEnd;
	}
	*sEnd = '\0';
	return sText;
}

static int __xlDbgCommandEquals(const char* sCommand, const char* sLong, const char* sShort)
{
	if ( sCommand == NULL ) {
		return 0;
	}
	if ( (sLong != NULL) && (strcmp(sCommand, sLong) == 0) ) {
		return 1;
	}
	return (sShort != NULL) && (strcmp(sCommand, sShort) == 0);
}

static void __xlDbgPrintHelp(void)
{
	fputs("commands:\n", stdout);
	fputs("  break <line>     set a line breakpoint in the current program\n", stdout);
	fputs("  clear            clear all breakpoints\n", stdout);
	fputs("  run              start or restart the program\n", stdout);
	fputs("  continue         resume after a stop\n", stdout);
	fputs("  next             step over one source statement\n", stdout);
	fputs("  step             step into the next source statement\n", stdout);
	fputs("  out              step out of the current frame\n", stdout);
	fputs("  bt               print the current stack\n", stdout);
	fputs("  locals           print locals for the current frame\n", stdout);
	fputs("  quit             exit the debugger\n", stdout);
}

static void __xlDbgPrintStopped(const XL_DapSession* pSession)
{
	const XL_DebugController* pController;
	const char* sReason;
	const char* sSource;
	const char* sFunc;
	int iLine;
	int iColumn;

	if ( pSession == NULL ) {
		return;
	}
	pController = &pSession->controller;
	sReason = (pController->sStopReason[0] != '\0') ? pController->sStopReason : "breakpoint";
	sSource = (pController->sStopSource[0] != '\0') ? pController->sStopSource : pController->sCurrentSource;
	sFunc = (pController->sStopFunc[0] != '\0') ? pController->sStopFunc : pController->sCurrentFunc;
	iLine = (pController->iStopLine > 0) ? pController->iStopLine : pController->iCurrentLine;
	iColumn = (pController->iStopColumn > 0) ? pController->iStopColumn : pController->iCurrentColumn;

	fprintf(
		stdout,
		"stopped reason=%s at %s:%d:%d in %s\n",
		sReason,
		(sSource != NULL) ? sSource : "",
		(iLine > 0) ? iLine : 1,
		(iColumn > 0) ? iColumn : 1,
		(sFunc != NULL && sFunc[0] != '\0') ? sFunc : "main"
	);
	if ( (strcmp(sReason, "exception") == 0) && pSession->debuggee.result.bHadException ) {
		fprintf(stdout, "exception: %s\n", pSession->debuggee.result.sExceptionText);
	}
}

static void __xlDbgPrintStack(const XL_DebugController* pController)
{
	const XL_DebugFrame* arrFrames;
	size_t iFrameCount;
	size_t i;

	if ( (pController == NULL) || !pController->bStopped ) {
		fputs("debuggee is not stopped\n", stdout);
		return;
	}
	arrFrames = pController->arrStopFrames;
	iFrameCount = pController->iStopFrameCount;
	if ( iFrameCount == 0u ) {
		fprintf(
			stdout,
			"#0 %s at %s:%d:%d\n",
			(pController->sStopFunc[0] != '\0') ? pController->sStopFunc : "main",
			pController->sStopSource,
			(pController->iStopLine > 0) ? pController->iStopLine : 1,
			(pController->iStopColumn > 0) ? pController->iStopColumn : 1
		);
		return;
	}
	for ( i = 0u; i < iFrameCount; ++i ) {
		const XL_DebugFrame* pFrame;

		pFrame = &arrFrames[iFrameCount - 1u - i];
		fprintf(
			stdout,
			"#%llu %s at %s:%d:%d\n",
			(unsigned long long)i,
			(pFrame->sFunc[0] != '\0') ? pFrame->sFunc : "frame",
			pFrame->sSource,
			(pFrame->iLine > 0) ? pFrame->iLine : 1,
			(pFrame->iColumn > 0) ? pFrame->iColumn : 1
		);
	}
}

static void __xlDbgPrintLocals(const XL_DebugController* pController)
{
	const XL_DebugVariable* arrVariables;
	size_t iVariableCount;
	size_t i;

	if ( (pController == NULL) || !pController->bStopped ) {
		fputs("debuggee is not stopped\n", stdout);
		return;
	}
	arrVariables = pController->arrStopLocals;
	iVariableCount = pController->iStopLocalCount;
	if ( iVariableCount == 0u ) {
		fputs("no locals\n", stdout);
		return;
	}
	for ( i = 0u; i < iVariableCount; ++i ) {
		fprintf(
			stdout,
			"%s = %s (%s)\n",
			arrVariables[i].sName,
			arrVariables[i].sValue,
			arrVariables[i].sType
		);
	}
}

static int __xlDbgWaitForStopOrExit(XL_DapSession* pSession)
{
	int bStopped;
	int bDone;
	int iRc;

	if ( pSession == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( ;; ) {
		iRc = __xlDapWaitForDebuggee(pSession, &bStopped, &bDone);
		if ( iRc != XL_RC_OK ) {
			fputs("debuggee wait failed\n", stderr);
			return iRc;
		}
		if ( bStopped ) {
			__xlDbgPrintStopped(pSession);
			return XL_RC_OK;
		}
		if ( bDone ) {
			if ( pSession->debuggee.result.bHadException ) {
				xlDebugControllerStopAtException(
					&pSession->controller,
					pSession->debuggee.result.sExceptionFile,
					pSession->debuggee.result.iExceptionLine,
					pSession->debuggee.result.iExceptionColumn
				);
				__xlDbgPrintStopped(pSession);
				return XL_RC_OK;
			}
			xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
			if ( pSession->debuggee.iRc != XL_RC_OK ) {
				fputs("debuggee failed\n", stdout);
				return pSession->debuggee.iRc;
			}
			fprintf(stdout, "exited %d\n", pSession->iLastExitCode);
			__xlDapDebuggeeClose(pSession);
			return XL_RC_OK;
		}
	}
}

static int __xlDbgResume(XL_DapSession* pSession, XL_DebugStepMode eStepMode)
{
	if ( pSession == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pSession->bDebuggeeCompleted &&
		xlDebugControllerHasStop(&pSession->controller) &&
		(strcmp(pSession->controller.sStopReason, "exception") == 0) ) {
		xlDebugControllerClearStop(&pSession->controller);
		xlDebugControllerSetState(&pSession->controller, XL_DEBUG_STATE_TERMINATED);
		__xlDapDebuggeeClose(pSession);
		fputs("terminated\n", stdout);
		return XL_RC_OK;
	}
	if ( pSession->bDebuggeeCompleted || !xlDebugControllerHasStop(&pSession->controller) ) {
		fputs("debuggee is not stopped\n", stdout);
		return XL_RC_OK;
	}
	xlDebugControllerSetStepMode(&pSession->controller, eStepMode);
	xlDebugControllerClearStop(&pSession->controller);
	xlDebugControllerResume(&pSession->controller);
	return __xlDbgWaitForStopOrExit(pSession);
}

int xlDriverRunInteractiveDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_DapSession session;
	int arrBreakpointLines[XL_DEBUG_BREAKPOINT_MAX];
	int arrBreakpointColumns[XL_DEBUG_BREAKPOINT_MAX];
	int arrBreakpointUnsupported[XL_DEBUG_BREAKPOINT_MAX];
	size_t iBreakpointCount;
	char sLine[512];
	int iRc;

	XL_UNUSED(pDump);

	if ( (pOpts == NULL) || (pOpts->sInputFile == NULL) || (pOpts->sInputFile[0] == '\0') ) {
		XL_DIAG_ERROR(pDiag, "debugger", "missing input file");
		return XL_RC_BAD_ARG;
	}

	memset(&session, 0, sizeof(session));
	memset(arrBreakpointLines, 0, sizeof(arrBreakpointLines));
	memset(arrBreakpointColumns, 0, sizeof(arrBreakpointColumns));
	memset(arrBreakpointUnsupported, 0, sizeof(arrBreakpointUnsupported));
	session.pDiag = pDiag;
	__xlDapStdoutGuardInit(&session.stdoutGuard);
	xlDebugControllerInit(&session.controller);
	iBreakpointCount = 0u;
	iRc = XL_RC_OK;

	fprintf(stdout, "xldbg dbg %s\n", pOpts->sInputFile);
	__xlDbgPrintHelp();

	while ( fgets(sLine, sizeof(sLine), stdin) != NULL ) {
		char* sInput;
		char* sArg;
		char* sSpace;

		sInput = __xlDbgTrim(sLine);
		if ( (sInput == NULL) || (sInput[0] == '\0') ) {
			continue;
		}
		sSpace = sInput;
		while ( (*sSpace != '\0') && (*sSpace != ' ') && (*sSpace != '\t') ) {
			++sSpace;
		}
		sArg = sSpace;
		if ( *sSpace != '\0' ) {
			*sSpace = '\0';
			sArg = __xlDbgTrim(sSpace + 1);
		}

		if ( __xlDbgCommandEquals(sInput, "help", "h") ) {
			__xlDbgPrintHelp();
		} else if ( __xlDbgCommandEquals(sInput, "quit", "q") || __xlDbgCommandEquals(sInput, "exit", NULL) ) {
			break;
		} else if ( __xlDbgCommandEquals(sInput, "break", "b") ) {
			char* sEnd;
			long iLine;

			if ( (sArg == NULL) || (sArg[0] == '\0') ) {
				fputs("usage: break <line>\n", stdout);
				continue;
			}
			errno = 0;
			iLine = strtol(sArg, &sEnd, 10);
			if ( (errno != 0) || (sEnd == sArg) || (iLine <= 0) || (iBreakpointCount >= XL_DEBUG_BREAKPOINT_MAX) ) {
				fputs("invalid breakpoint line\n", stdout);
				continue;
			}
			arrBreakpointLines[iBreakpointCount] = (int)iLine;
			arrBreakpointColumns[iBreakpointCount] = 1;
			arrBreakpointUnsupported[iBreakpointCount] = 0;
			iBreakpointCount += 1u;
			iRc = xlDebugControllerSetBreakpoints(
				&session.controller,
				pOpts->sInputFile,
				arrBreakpointLines,
				arrBreakpointColumns,
				arrBreakpointUnsupported,
				iBreakpointCount
			);
			if ( iRc != XL_RC_OK ) {
				break;
			}
			fprintf(stdout, "breakpoint %llu at %s:%ld\n", (unsigned long long)iBreakpointCount, pOpts->sInputFile, iLine);
		} else if ( __xlDbgCommandEquals(sInput, "clear", NULL) ) {
			iBreakpointCount = 0u;
			xlDebugControllerClearBreakpoints(&session.controller);
			fputs("breakpoints cleared\n", stdout);
		} else if ( __xlDbgCommandEquals(sInput, "run", "r") ) {
			if ( xlDebugControllerHasStop(&session.controller) ) {
				xlDebugControllerClearStop(&session.controller);
				xlDebugControllerResume(&session.controller);
			}
			__xlDapDebuggeeClose(&session);
			xlDebugControllerSetBlocking(&session.controller, 1);
			xlDebugControllerSetState(&session.controller, XL_DEBUG_STATE_RUNNING);
			iRc = __xlDapCheckProgramCompiles(&session, pDiag, pOpts->sInputFile);
			if ( iRc != XL_RC_OK ) {
				fputs("launch failed\n", stdout);
				break;
			}
			iRc = __xlDapDebuggeeStart(&session, pOpts->sInputFile);
			if ( iRc != XL_RC_OK ) {
				fputs("launch failed\n", stdout);
				break;
			}
			iRc = __xlDbgWaitForStopOrExit(&session);
			if ( iRc != XL_RC_OK ) {
				break;
			}
		} else if ( __xlDbgCommandEquals(sInput, "continue", "c") ) {
			iRc = __xlDbgResume(&session, XL_DEBUG_STEP_NONE);
			if ( iRc != XL_RC_OK ) {
				break;
			}
		} else if ( __xlDbgCommandEquals(sInput, "next", "n") ) {
			iRc = __xlDbgResume(&session, XL_DEBUG_STEP_NEXT);
			if ( iRc != XL_RC_OK ) {
				break;
			}
		} else if ( __xlDbgCommandEquals(sInput, "step", "s") ) {
			iRc = __xlDbgResume(&session, XL_DEBUG_STEP_IN);
			if ( iRc != XL_RC_OK ) {
				break;
			}
		} else if ( __xlDbgCommandEquals(sInput, "out", NULL) || __xlDbgCommandEquals(sInput, "finish", NULL) ) {
			iRc = __xlDbgResume(&session, XL_DEBUG_STEP_OUT);
			if ( iRc != XL_RC_OK ) {
				break;
			}
		} else if ( __xlDbgCommandEquals(sInput, "bt", NULL) || __xlDbgCommandEquals(sInput, "backtrace", NULL) ) {
			__xlDbgPrintStack(&session.controller);
		} else if ( __xlDbgCommandEquals(sInput, "locals", "l") ) {
			__xlDbgPrintLocals(&session.controller);
		} else {
			fprintf(stdout, "unknown command: %s\n", sInput);
		}
	}

	if ( xlDebugControllerHasStop(&session.controller) ) {
		xlDebugControllerClearStop(&session.controller);
		xlDebugControllerResume(&session.controller);
	}
	__xlDapDebuggeeClose(&session);
	xlDebugControllerFree(&session.controller);
	return iRc;
}

int xlDriverRunDapDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_DapSession session;
	int bQuit;

	XL_UNUSED(pDump);

	if ( (pOpts != NULL) && (pOpts->dbg.sDapListen != NULL) ) {
		XL_DIAG_ERROR(pDiag, "dap", "TCP DAP listen mode is reserved but not implemented yet: %s", pOpts->dbg.sDapListen);
		return XL_RC_NOT_IMPLEMENTED;
	}

	memset(&session, 0, sizeof(session));
	session.pDiag = pDiag;
	__xlDapStdoutGuardInit(&session.stdoutGuard);
	xlDebugControllerInit(&session.controller);
	bQuit = 0;
	__xlDapSetBinaryMode();

	while ( !bQuit ) {
		char* sRequest;
		int iReadRc;
		int iHandleRc;

		sRequest = NULL;
		iReadRc = __xlDapReadMessage(&sRequest);
		if ( iReadRc == 0 ) {
			break;
		}
		if ( iReadRc < 0 ) {
			XL_DIAG_ERROR(pDiag, "dap", "failed to read DAP message");
			free(sRequest);
			if ( xlDebugControllerHasStop(&session.controller) ) {
				xlDebugControllerClearStop(&session.controller);
				xlDebugControllerResume(&session.controller);
			}
			__xlDapDebuggeeClose(&session);
			__xlDapStdoutRestore(&session.stdoutGuard);
			xlDebugControllerFree(&session.controller);
			return XL_RC_IO_ERROR;
		}

		iHandleRc = __xlDapHandleRequest(&session, sRequest, &bQuit);
		free(sRequest);
		if ( iHandleRc != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "dap", "failed to write DAP response");
			if ( xlDebugControllerHasStop(&session.controller) ) {
				xlDebugControllerClearStop(&session.controller);
				xlDebugControllerResume(&session.controller);
			}
			__xlDapDebuggeeClose(&session);
			__xlDapStdoutRestore(&session.stdoutGuard);
			xlDebugControllerFree(&session.controller);
			return iHandleRc;
		}
	}

	if ( xlDebugControllerHasStop(&session.controller) ) {
		xlDebugControllerClearStop(&session.controller);
		xlDebugControllerResume(&session.controller);
	}
	__xlDapDebuggeeClose(&session);
	__xlDapStdoutRestore(&session.stdoutGuard);
	xlDebugControllerFree(&session.controller);
	return XL_RC_OK;
}

#endif
