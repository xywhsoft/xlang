#include "xl_driver_debug.h"
#include "../ast/xl_ast_dump.h"
#include "../codegen/c/xl_cgen.h"
#include "../diag/xl_dump.h"
#include "../host/xl_host.h"
#include "../lexer/xl_lexer_api.h"
#include "../lower/xl_lower.h"
#include "../parser/xl_parser_api.h"
#include "../sem/xl_sem_dump.h"
#include "xl_module_link.h"
#include "xl_xlp_dump.h"

#if XLANG_DEBUGMODE_ENABLED

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
	iRc = xlCgenMir(&lowerResult, &moduleResult, pDiag, &sCode);
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

#endif
