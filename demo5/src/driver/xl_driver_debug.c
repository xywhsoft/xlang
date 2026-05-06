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
#include "xl_xlp_dump.h"

#if XLANG_DEBUGMODE_ENABLED
#if defined(_WIN32) || defined(_WIN64)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

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
	char* sXlpFuncsJson;
	char* sXlpJson;
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
	sXlpFuncsJson = NULL;
	sXlpJson = NULL;
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
	sXlpFuncsJson = xlSemDumpXlpFunctionsJson(&semResult);
	sXlpJson = xlXlpDumpPackageJson(&moduleResult, &semResult, pOpts->sInputFile);
	sDiagText = xlSemDumpDiagText(&semResult);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) || (sSymbolsJson == NULL) || (sTypesJson == NULL) ||
		(sFuncsJson == NULL) || (sXlpFuncsJson == NULL) || (sXlpJson == NULL) || (sDiagText == NULL) ) {
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
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".xlp.functions.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sXlpFuncsJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write xlp function dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".xlp.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sXlpJson) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write xlp package dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".diag.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, sDiagText) != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write diagnostic dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write module dump: %s", sDefaultPath);
		__xlBuildDumpPathDebug(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) XL_DIAG_WARN(pDiag, "sem", "failed to write module json dump: %s", sDefaultPath);
	}

cleanup:
	free(sTextDump); free(sJsonDump); free(sSymbolsJson); free(sTypesJson); free(sFuncsJson); free(sXlpFuncsJson); free(sXlpJson); free(sDiagText);
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
	int iRc;
	int bStarted;
	int bDone;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hThread;
	HANDLE hDoneEvent;
#endif
} XL_DapDebuggee;

struct XL_DapSession {
	int iNextSeq;
	int iLastExitCode;
	int bDebuggeeCompleted;
	XL_DiagContext* pDiag;
	XL_DebugController controller;
	XL_DapStdoutGuard stdoutGuard;
	XL_DapDebuggee debuggee;
};

static int __xlDapSendEvent(XL_DapSession* pSession, const char* sEvent, const char* sBody);
static int __xlDapSendStoppedEvent(XL_DapSession* pSession, const char* sReason);

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

static size_t __xlDapCollectBreakpointLines(const char* sJson, int* arrLines, int* arrColumns, size_t iCap)
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
			++iCount;
		}
		sScan = (sEnd != sValue) ? sEnd : (sScan + 6);
	}

	return iCount;
}

static int __xlDapBuildBreakpointResponseBody(const XL_DebugController* pController, char* sOut, size_t iOutSize)
{
	size_t i;
	size_t iLen;
	size_t iCount;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return XL_RC_BAD_ARG;
	}

	iLen = (size_t)snprintf(sOut, iOutSize, "{\"breakpoints\":[");
	iCount = xlDebugControllerBreakpointCount(pController);
	for ( i = 0u; i < iCount; ++i ) {
		const XL_DebugBreakpoint* pBreakpoint;
		int iNeed;

		pBreakpoint = xlDebugControllerBreakpointAt(pController, i);
		if ( pBreakpoint == NULL ) {
			continue;
		}
		if ( iLen >= iOutSize ) {
			return XL_RC_IO_ERROR;
		}
		iNeed = snprintf(
			sOut + iLen,
			iOutSize - iLen,
			"%s{\"verified\":%s,\"line\":%d,\"message\":\"%s\"}",
			(i == 0u) ? "" : ",",
			pBreakpoint->bVerified ? "true" : "false",
			(pBreakpoint->iActualLine > 0) ? pBreakpoint->iActualLine : pBreakpoint->iLine,
			pBreakpoint->sMessage
		);
		if ( iNeed < 0 ) {
			return XL_RC_IO_ERROR;
		}
		iLen += (size_t)iNeed;
	}
	if ( iLen >= iOutSize ) {
		return XL_RC_IO_ERROR;
	}
	snprintf(sOut + iLen, iOutSize - iLen, "]}");
	return XL_RC_OK;
}

static int __xlDapSendJson(const char* sJson)
{
	size_t iLen;

	if ( sJson == NULL ) {
		return XL_RC_BAD_ARG;
	}

	iLen = strlen(sJson);
	if ( fprintf(stdout, "Content-Length: %lu\r\n\r\n", (unsigned long)iLen) < 0 ) {
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

static int __xlDapRunProgram(XL_DapSession* pSession, XL_DiagContext* pDiag, const char* sProgram, XL_HostRunResult* pResult)
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
	iRc = xlHostRunCText(&hostCtx, pDiag, sProgram, sCode, "xl_generated_entry", NULL, pResult);
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
		&pDebuggee->result
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
	pSession->debuggee.iRc = __xlDapRunProgram(pSession, pSession->pDiag, sProgram, &pSession->debuggee.result);
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
		iWait = WaitForMultipleObjects(2, arrWait, FALSE, INFINITE);
		if ( iWait == WAIT_OBJECT_0 ) {
			*pbStopped = 1;
		} else if ( iWait == (WAIT_OBJECT_0 + 1u) ) {
			*pbDone = 1;
			pSession->bDebuggeeCompleted = 1;
			pSession->iLastExitCode = pSession->debuggee.result.iExitCode;
			WaitForSingleObject(pSession->debuggee.hThread, INFINITE);
			CloseHandle(pSession->debuggee.hThread);
			pSession->debuggee.hThread = NULL;
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

static int __xlDapSendDebuggeeWaitEvent(XL_DapSession* pSession)
{
	int bStopped;
	int bDone;
	int iRc;

	if ( pSession == NULL ) {
		return XL_RC_BAD_ARG;
	}

	iRc = __xlDapWaitForDebuggee(pSession, &bStopped, &bDone);
	__xlDapStdoutRestore(&pSession->stdoutGuard);
	if ( iRc != XL_RC_OK ) {
		(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg debuggee wait failed\\n\"}");
		return iRc;
	}
	if ( bStopped ) {
		return __xlDapSendStoppedEvent(pSession, pSession->controller.sStopReason);
	}
	if ( bDone ) {
		if ( pSession->debuggee.iRc != XL_RC_OK ) {
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg launch failed\\n\"}");
		}
		(void)__xlDapSendEvent(pSession, "terminated", "{}");
		return XL_RC_OK;
	}
	return XL_RC_OK;
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
	size_t iCap;
	int iSeq;
	int iRc;

	if ( (pSession == NULL) || (sCommand == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iSeq = ++pSession->iNextSeq;
	iCap = strlen(sCommand) + ((sBody != NULL) ? strlen(sBody) : 0u) + ((sMessage != NULL) ? strlen(sMessage) : 0u) + 256u;
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
				sCommand,
				sBody
			);
		} else {
			snprintf(
				sJson,
				iCap,
				"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":true,\"command\":\"%s\"}",
				iSeq,
				iRequestSeq,
				sCommand
			);
		}
	} else {
		if ( sMessage == NULL ) {
			sMessage = "request failed";
		}
		snprintf(
			sJson,
			iCap,
			"{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,\"success\":false,\"command\":\"%s\",\"message\":\"%s\"}",
			iSeq,
			iRequestSeq,
			sCommand,
			sMessage
		);
	}

	iRc = __xlDapSendJson(sJson);
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
	return __xlDapSendJson(sJson);
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

static int __xlDapSendStoppedEvent(XL_DapSession* pSession, const char* sReason)
{
	char sBody[256];

	if ( sReason == NULL ) {
		sReason = "breakpoint";
	}
	snprintf(
		sBody,
		sizeof(sBody),
		"{\"reason\":\"%s\",\"threadId\":1,\"allThreadsStopped\":true}",
		sReason
	);
	return __xlDapSendEvent(pSession, "stopped", sBody);
}

static void __xlDapBuildStackTraceBody(const XL_DebugController* pController, char* sOut, size_t iOutSize)
{
	char sPath[XL_PATH_MAX_LEN * 2];
	int iLine;
	int iColumn;

	if ( (sOut == NULL) || (iOutSize == 0u) ) {
		return;
	}
	if ( pController == NULL ) {
		snprintf(sOut, iOutSize, "{\"stackFrames\":[],\"totalFrames\":0}");
		return;
	}

	iLine = pController->bStopped ? pController->iStopLine : pController->iCurrentLine;
	iColumn = pController->bStopped ? pController->iStopColumn : pController->iCurrentColumn;
	__xlDapJsonEscapeString(
		sPath,
		sizeof(sPath),
		pController->bStopped ? pController->sStopSource : pController->sCurrentSource
	);
	if ( iLine <= 0 ) {
		snprintf(sOut, iOutSize, "{\"stackFrames\":[],\"totalFrames\":0}");
		return;
	}

	snprintf(
		sOut,
		iOutSize,
		"{\"stackFrames\":[{\"id\":1,\"name\":\"main\",\"line\":%d,\"column\":%d,\"source\":{\"name\":\"%s\",\"path\":\"%s\"}}],\"totalFrames\":1}",
		iLine,
		(iColumn > 0) ? iColumn : 1,
		sPath,
		sPath
	);
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
		size_t iBreakpointCount;
		int iRc;

		if ( !__xlDapJsonGetString(sRequest, "path", sSource, sizeof(sSource)) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "setBreakpoints requires source.path");
		}
		memset(arrLines, 0, sizeof(arrLines));
		memset(arrColumns, 0, sizeof(arrColumns));
		iBreakpointCount = __xlDapCollectBreakpointLines(sRequest, arrLines, arrColumns, XL_COUNTOF(arrLines));
		iRc = xlDebugControllerSetBreakpoints(&pSession->controller, sSource, arrLines, arrColumns, iBreakpointCount);
		if ( iRc != XL_RC_OK ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "failed to set breakpoints");
		}
		iRc = __xlDapBuildBreakpointResponseBody(&pSession->controller, sBody, sizeof(sBody));
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

	if ( strcmp(sCommand, "stackTrace") == 0 ) {
		char sBody[4096];

		__xlDapBuildStackTraceBody(&pSession->controller, sBody, sizeof(sBody));
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, sBody, NULL);
	}

	if ( strcmp(sCommand, "scopes") == 0 ) {
		return __xlDapSendResponse(
			pSession,
			iRequestSeq,
			sCommand,
			1,
			"{\"scopes\":[{\"name\":\"Arguments\",\"variablesReference\":1001,\"expensive\":false},{\"name\":\"Locals\",\"variablesReference\":1002,\"expensive\":false}]}",
			NULL
		);
	}

	if ( strcmp(sCommand, "variables") == 0 ) {
		int iVariablesReference;

		iVariablesReference = 0;
		(void)__xlDapJsonGetInt(sRequest, "variablesReference", &iVariablesReference);
		if ( (iVariablesReference != 1001) && (iVariablesReference != 1002) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "unknown variablesReference");
		}
		return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, "{\"variables\":[]}", NULL);
	}

	if ( strcmp(sCommand, "launch") == 0 ) {
		char sProgram[XL_PATH_MAX_LEN];
		int iRc;

		if ( !__xlDapJsonGetString(sRequest, "program", sProgram, sizeof(sProgram)) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "launch requires program");
		}
		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pSession->bDebuggeeCompleted = 0;
		xlDebugControllerClearStop(&pSession->controller);
		xlDebugControllerSetBlocking(&pSession->controller, 1);
		iRc = __xlDapStdoutSuppress(&pSession->stdoutGuard);
		if ( iRc != XL_RC_OK ) {
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg launch failed\\n\"}");
			(void)__xlDapSendEvent(pSession, "terminated", "{}");
			*pbQuit = 1;
			return XL_RC_OK;
		}
		iRc = __xlDapDebuggeeStart(pSession, sProgram);
		if ( iRc != XL_RC_OK ) {
			__xlDapStdoutRestore(&pSession->stdoutGuard);
			(void)__xlDapSendEvent(pSession, "output", "{\"category\":\"stderr\",\"output\":\"xldbg launch failed\\n\"}");
			(void)__xlDapSendEvent(pSession, "terminated", "{}");
			*pbQuit = 1;
			return XL_RC_OK;
		}
		iRc = __xlDapSendDebuggeeWaitEvent(pSession);
		if ( pSession->bDebuggeeCompleted && !xlDebugControllerHasStop(&pSession->controller) ) {
			*pbQuit = 1;
		}
		return iRc;
	}

	if ( strcmp(sCommand, "continue") == 0 ) {
		int iRc;

		if ( pSession->bDebuggeeCompleted || !xlDebugControllerHasStop(&pSession->controller) ) {
			return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "debuggee is not stopped");
		}
		xlDebugControllerClearStop(&pSession->controller);
		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, "{\"allThreadsContinued\":true}", NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		iRc = __xlDapStdoutSuppress(&pSession->stdoutGuard);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		xlDebugControllerResume(&pSession->controller);
		iRc = __xlDapSendDebuggeeWaitEvent(pSession);
		if ( pSession->bDebuggeeCompleted && !xlDebugControllerHasStop(&pSession->controller) ) {
			*pbQuit = 1;
		}
		return iRc;
	}

	if ( strcmp(sCommand, "disconnect") == 0 ) {
		int iRc;

		iRc = __xlDapSendResponse(pSession, iRequestSeq, sCommand, 1, NULL, NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		if ( xlDebugControllerHasStop(&pSession->controller) ) {
			xlDebugControllerClearStop(&pSession->controller);
			(void)__xlDapStdoutSuppress(&pSession->stdoutGuard);
			xlDebugControllerResume(&pSession->controller);
			__xlDapStdoutRestore(&pSession->stdoutGuard);
		}
		__xlDapDebuggeeClose(pSession);
		(void)__xlDapSendEvent(pSession, "terminated", "{}");
		*pbQuit = 1;
		return XL_RC_OK;
	}

	return __xlDapSendResponse(pSession, iRequestSeq, sCommand, 0, NULL, "unsupported request");
}

int xlDriverRunInteractiveDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	XL_UNUSED(pOpts);
	XL_UNUSED(pDump);

	XL_DIAG_ERROR(pDiag, "debugger", "interactive debugger is not implemented yet");
	return XL_RC_NOT_IMPLEMENTED;
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
