#include "xl_driver.h"

#include "../host/xl_host_symbols.h"
#include "../../test/xl_test.h"

static int __xlWriteTextFile(const char* sFilePath, const char* sText)
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

static char* __xlWrapJsonText(const char* sKey, const char* sText)
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

static char* __xlConcatText(const char* sText1, const char* sText2)
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

static char* __xlCommentText(const char* sText)
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

static void __xlBuildDumpPath(
	char* sOut,
	size_t iOutSize,
	const char* sGroup,
	const char* sInputFile,
	const char* sSuffix
)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;

	sName = sInputFile;
	if ( sName == NULL ) {
		sName = "stdin";
	}

	sScan = sName;
	while ( *sScan != '\0' ) {
		if ( xlPathIsSep((unsigned char)*sScan) ) {
			sName = sScan + 1;
		}
		++sScan;
	}

	sDot = strrchr(sName, '.');
	iLen = (sDot != NULL) ? (size_t)(sDot - sName) : strlen(sName);
	snprintf(sOut, iOutSize, "dump\\%s\\%.*s%s", (sGroup != NULL) ? sGroup : "misc", (int)iLen, sName, sSuffix);
}

static void __xlBuildDefaultOutputPath(
	char* sOut,
	size_t iOutSize,
	const XL_DumpContext* pDump,
	const char* sInputFile,
	const char* sExt
)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;
	char sBaseName[XL_PATH_MAX_LEN];
	char sBinDir[XL_PATH_MAX_LEN];

	sName = (sInputFile != NULL) ? sInputFile : "output";
	sScan = sName;
	while ( *sScan != '\0' ) {
		if ( xlPathIsSep((unsigned char)*sScan) ) {
			sName = sScan + 1;
		}
		++sScan;
	}

	sDot = strrchr(sName, '.');
	iLen = (sDot != NULL) ? (size_t)(sDot - sName) : strlen(sName);
	snprintf(sBaseName, sizeof(sBaseName), "%.*s%s", (int)iLen, sName, (sExt != NULL) ? sExt : ".exe");
	xlPathJoin(sBinDir, sizeof(sBinDir), (pDump != NULL) ? pDump->sRoot : "release", "bin");
	xlPathJoin(sOut, iOutSize, sBinDir, sBaseName);
}

static int __xlDriverWriteStubDump(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	char sRelPath[XL_PATH_MAX_LEN];
	char sText[XL_DIAG_TEXT_MAX];
	const char* sCommandName;
	const char* sDumpGroup;

	if ( (pOpts == NULL) || (pDump == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	sCommandName = xlCliCommandName(pOpts->eCommand);
	sDumpGroup = sCommandName;
	if ( pOpts->eCommand == XL_CMD_PARSE ) {
		sDumpGroup = "parser";
	}

	snprintf(sRelPath, sizeof(sRelPath), "dump\\%s\\stage0.txt", sDumpGroup);
	snprintf(
		sText,
		sizeof(sText),
		"demo5 stage0 stub\ncommand=%s\ninput=%s\n",
		sCommandName,
		(pOpts->sInputFile != NULL) ? pOpts->sInputFile : "<none>"
	);

	if ( xlDumpWriteText(pDump, sRelPath, sText) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "driver", "failed to write dump file: %s", sRelPath);
		return XL_RC_IO_ERROR;
	}

	return XL_RC_OK;
}

static int __xlDriverRunLex(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
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

	if ( pOpts->bDumpJson ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "lexer", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( pOpts->bDumpToken || pOpts->bDumpJson ) {
		__xlBuildDumpPath(
			sDefaultPath,
			sizeof(sDefaultPath),
			"lexer",
			pOpts->sInputFile,
			pOpts->bDumpJson ? ".tokens.json" : ".tokens.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
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

static int __xlDriverRunParse(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
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

	iRc = xlParserParseLexer(pLexer, pDiag, pOpts->bTraceParser ? 1 : 0, &result);
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

	if ( pOpts->bDumpJson ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "parser", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( pOpts->bDumpAst || pOpts->bDumpJson ) {
		__xlBuildDumpPath(
			sDefaultPath,
			sizeof(sDefaultPath),
			"parser",
			pOpts->sInputFile,
			pOpts->bDumpJson ? ".ast.json" : ".ast.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
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

static int __xlDriverRunSem(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	char* sTextDump;
	char* sJsonDump;
	char* sSymbolsJson;
	char* sTypesJson;
	char* sDiagText;
	char sDefaultPath[XL_PATH_MAX_LEN];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	sTextDump = NULL;
	sJsonDump = NULL;
	sSymbolsJson = NULL;
	sTypesJson = NULL;
	sDiagText = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, pOpts->bTraceParser ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, pOpts->bTraceSem ? 1 : 0, &semResult);
	sTextDump = xlSemDumpText(&semResult);
	sJsonDump = xlSemDumpJson(&semResult);
	sSymbolsJson = xlSemDumpSymbolsJson(&semResult);
	sTypesJson = xlSemDumpTypesJson(&semResult);
	sDiagText = xlSemDumpDiagText(&semResult);
	if ( (sTextDump == NULL) || (sJsonDump == NULL) || (sSymbolsJson == NULL) || (sTypesJson == NULL) || (sDiagText == NULL) ) {
		XL_DIAG_ERROR(pDiag, "sem", "failed to build semantic dump output");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( pOpts->bDumpJson ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "sem", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( pOpts->bDumpSem || pOpts->bDumpJson ) {
		__xlBuildDumpPath(
			sDefaultPath,
			sizeof(sDefaultPath),
			"sem",
			pOpts->sInputFile,
			pOpts->bDumpJson ? ".sem.json" : ".sem.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write semantic dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".symbols.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSymbolsJson) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write symbol dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".types.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sTypesJson) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write type dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".diag.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, sDiagText) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write diagnostic dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write module dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "sem", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "sem", "failed to write module json dump: %s", sDefaultPath);
		}
	}

cleanup:
	free(sTextDump);
	free(sJsonDump);
	free(sSymbolsJson);
	free(sTypesJson);
	free(sDiagText);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDriverRunLower(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
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
	sTextDump = NULL;
	sJsonDump = NULL;
	sHirJson = NULL;
	sMirJson = NULL;
	sSmText = NULL;
	sSmJson = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, pOpts->bTraceParser ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, pOpts->bTraceSem ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, pOpts->bTraceLower ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

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

	if ( pOpts->bDumpJson ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sTextDump, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "lower", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( pOpts->bDumpHir || pOpts->bDumpMir || pOpts->bDumpJson ) {
		__xlBuildDumpPath(
			sDefaultPath,
			sizeof(sDefaultPath),
			"lower",
			pOpts->sInputFile,
			pOpts->bDumpJson ? ".lower.json" : ".lower.txt"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, pOpts->bDumpJson ? sJsonDump : sTextDump) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write lower dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".hir.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sHirJson) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write HIR dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".mir.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sMirJson) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write MIR dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".sm.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSmText) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write async state-machine dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".sm.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, sSmJson) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write async state-machine json dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write module dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "lower", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "lower", "failed to write module json dump: %s", sDefaultPath);
		}
	}

cleanup:
	free(sTextDump);
	free(sJsonDump);
	free(sHirJson);
	free(sMirJson);
	free(sSmText);
	free(sSmJson);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDriverRunCgen(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
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
	sCode = NULL;
	sJsonDump = NULL;
	sModuleComment = NULL;
	sCodeWithModules = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, pOpts->bTraceParser ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, pOpts->bTraceSem ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, pOpts->bTraceLower ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlCgenMir(lowerResult.pMirRoot, pDiag, &sCode);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	sModuleComment = __xlCommentText((moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "");
	sCodeWithModules = __xlConcatText((sModuleComment != NULL) ? sModuleComment : "", sCode);
	sJsonDump = __xlWrapJsonText("code", (sCodeWithModules != NULL) ? sCodeWithModules : sCode);
	if ( (sCode == NULL) || (sJsonDump == NULL) || (sCodeWithModules == NULL) ) {
		XL_DIAG_ERROR(pDiag, "cgen", "failed to build C code dump");
		iRc = XL_RC_IO_ERROR;
		goto cleanup;
	}

	if ( pOpts->bDumpJson ) {
		fputs(sJsonDump, stdout);
	} else {
		fputs(sCodeWithModules, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? sJsonDump : sCodeWithModules) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "cgen", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

	if ( pOpts->bDumpC || pOpts->bDumpJson ) {
		__xlBuildDumpPath(
			sDefaultPath,
			sizeof(sDefaultPath),
			"cgen",
			pOpts->sInputFile,
			pOpts->bDumpJson ? ".c.json" : ".c"
		);
		if ( xlDumpWriteText(pDump, sDefaultPath, pOpts->bDumpJson ? sJsonDump : sCodeWithModules) != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "cgen", "failed to write C dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "cgen", pOpts->sInputFile, ".modules.txt");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "cgen", "failed to write module dump: %s", sDefaultPath);
		}

		__xlBuildDumpPath(sDefaultPath, sizeof(sDefaultPath), "cgen", pOpts->sInputFile, ".modules.json");
		if ( xlDumpWriteText(pDump, sDefaultPath, (moduleResult.sJsonDump != NULL) ? moduleResult.sJsonDump : "{}\n") != XL_RC_OK ) {
			XL_DIAG_WARN(pDiag, "cgen", "failed to write module json dump: %s", sDefaultPath);
		}
	}

cleanup:
	free(sCode);
	free(sJsonDump);
	free(sModuleComment);
	free(sCodeWithModules);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDriverRunProgram(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_ExecCtx execCtx;
	char* sCode;
	char* sTraceText;
	char* sJsonDump;
	char* sTraceWithModules;
	char sCodePath[XL_PATH_MAX_LEN];
	char sTracePath[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlExecCtxInit(&execCtx, pOpts->sInputFile, pDump->sRoot);
	sCode = NULL;
	sTraceText = NULL;
	sJsonDump = NULL;
	sTraceWithModules = NULL;

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, pOpts->bTraceParser ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, pOpts->bTraceSem ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, pOpts->bTraceLower ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlCgenMir(lowerResult.pMirRoot, pDiag, &sCode);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	__xlBuildDumpPath(sCodePath, sizeof(sCodePath), "cgen", pOpts->sInputFile, ".run.c");
	if ( xlDumpWriteText(pDump, sCodePath, sCode) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run", "failed to write run C dump: %s", sCodePath);
	}

	iRc = xlExecRunCode(&execCtx, pDiag, sCode, &sTraceText);
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "runtime", pOpts->sInputFile, ".trace.txt");
	sTraceWithModules = __xlConcatText((moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "", (sTraceText != NULL) ? sTraceText : "");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceWithModules != NULL) ? sTraceWithModules : ((sTraceText != NULL) ? sTraceText : "")) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run", "failed to write runtime trace: %s", sTracePath);
	}
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	snprintf(
		sSummary,
		sizeof(sSummary),
		"RunResult exitCode=%d entry=%s\n",
		execCtx.iExitCode,
		execCtx.sEntrySymbol
	);
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( pOpts->bDumpJson ) {
		if ( sJsonDump != NULL ) {
			fputs(sJsonDump, stdout);
		}
	} else {
		fputs(sSummary, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? ((sJsonDump != NULL) ? sJsonDump : "") : sSummary) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "run", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

cleanup:
	free(sCode);
	free(sTraceText);
	free(sTraceWithModules);
	free(sJsonDump);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDriverRunCProgram(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	XL_HostContext hostCtx;
	XL_HostRunResult result;
	char* sTraceText;
	char sTracePath[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	char* sJsonDump;
	int iRc;

	xlHostContextInit(&hostCtx, pOpts->sInputFile, pDump->sRoot);
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
	sJsonDump = NULL;

	iRc = xlHostRunCFile(&hostCtx, pDiag, pOpts->sInputFile, &sTraceText, &result);
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run-c", "failed to write host trace: %s", sTracePath);
	}
	if ( iRc != XL_RC_OK ) {
		free(sTraceText);
		return iRc;
	}

	snprintf(
		sSummary,
		sizeof(sSummary),
		"RunCResult exitCode=%d entry=%s\n",
		result.iExitCode,
		(result.sEntrySymbol[0] != '\0') ? result.sEntrySymbol : "main"
	);
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( pOpts->bDumpJson ) {
		if ( sJsonDump != NULL ) {
			fputs(sJsonDump, stdout);
		}
	} else {
		fputs(sSummary, stdout);
	}

	if ( pOpts->sEmitPath != NULL ) {
		if ( __xlWriteTextFile(pOpts->sEmitPath, pOpts->bDumpJson ? ((sJsonDump != NULL) ? sJsonDump : "") : sSummary) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "run-c", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
		}
	}

	free(sTraceText);
	free(sJsonDump);
	return iRc;
}

static int __xlDriverBuildProgram(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	XL_ModuleLinkResult moduleResult;
	XL_SemResult semResult;
	XL_LowerResult lowerResult;
	XL_HostContext hostCtx;
	XL_HostBuildResult result;
	char* sCode;
	char* sTraceText;
	char* sSymbolsJson;
	char* sJsonDump;
	char sCodePath[XL_PATH_MAX_LEN];
	char sTracePath[XL_PATH_MAX_LEN];
	char sSymbolsPath[XL_PATH_MAX_LEN];
	char sOutputFile[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlHostContextInit(&hostCtx, pOpts->sInputFile, pDump->sRoot);
	memset(&result, 0, sizeof(result));
	sCode = NULL;
	sTraceText = NULL;
	sSymbolsJson = NULL;
	sJsonDump = NULL;
	if ( pOpts->sEmitPath != NULL ) {
		snprintf(sOutputFile, sizeof(sOutputFile), "%s", pOpts->sEmitPath);
	} else {
		__xlBuildDefaultOutputPath(sOutputFile, sizeof(sOutputFile), pDump, pOpts->sInputFile, ".exe");
	}

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, pOpts->bTraceParser ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, pOpts->bTraceSem ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, pOpts->bTraceLower ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlCgenMir(lowerResult.pMirRoot, pDiag, &sCode);
	if ( iRc != XL_RC_OK ) goto cleanup;

	__xlBuildDumpPath(sCodePath, sizeof(sCodePath), "cgen", pOpts->sInputFile, ".build.c");
	if ( xlDumpWriteText(pDump, sCodePath, sCode) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write build C dump: %s", sCodePath);
	}

	iRc = xlHostBuildCText(&hostCtx, pDiag, pOpts->sInputFile, sCode, "xl_generated_entry", sOutputFile, &sTraceText, &result);
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".build.trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write host build trace: %s", sTracePath);
	}
	sSymbolsJson = xlHostDumpSymbolsJson();
	__xlBuildDumpPath(sSymbolsPath, sizeof(sSymbolsPath), "host", pOpts->sInputFile, ".symbols.json");
	if ( xlDumpWriteText(pDump, sSymbolsPath, (sSymbolsJson != NULL) ? sSymbolsJson : "{\n\t\"symbols\": []\n}\n") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write symbol dump: %s", sSymbolsPath);
	}
	if ( iRc != XL_RC_OK ) goto cleanup;

	snprintf(sSummary, sizeof(sSummary), "BuildResult output=%s kind=%s entry=%s\n", result.sOutputFile, result.sOutputKind, result.sEntrySymbol);
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( pOpts->bDumpJson ) {
		if ( sJsonDump != NULL ) fputs(sJsonDump, stdout);
	} else {
		fputs(sSummary, stdout);
	}

cleanup:
	free(sCode);
	free(sTraceText);
	free(sSymbolsJson);
	free(sJsonDump);
	xlLowerResultFree(&lowerResult);
	xlSemResultFree(&semResult);
	xlModuleLinkResultFree(&moduleResult);
	return iRc;
}

static int __xlDriverBuildCProgram(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	XL_HostContext hostCtx;
	XL_HostBuildResult result;
	char* sTraceText;
	char* sSymbolsJson;
	char* sJsonDump;
	char sTracePath[XL_PATH_MAX_LEN];
	char sSymbolsPath[XL_PATH_MAX_LEN];
	char sOutputFile[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;

	xlHostContextInit(&hostCtx, pOpts->sInputFile, pDump->sRoot);
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
	sSymbolsJson = NULL;
	sJsonDump = NULL;
	if ( pOpts->sEmitPath != NULL ) {
		snprintf(sOutputFile, sizeof(sOutputFile), "%s", pOpts->sEmitPath);
	} else {
		__xlBuildDefaultOutputPath(sOutputFile, sizeof(sOutputFile), pDump, pOpts->sInputFile, ".exe");
	}

	iRc = xlHostBuildCFile(&hostCtx, pDiag, pOpts->sInputFile, sOutputFile, &sTraceText, &result);
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".build.trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build-c", "failed to write host build trace: %s", sTracePath);
	}
	sSymbolsJson = xlHostDumpSymbolsJson();
	__xlBuildDumpPath(sSymbolsPath, sizeof(sSymbolsPath), "host", pOpts->sInputFile, ".symbols.json");
	if ( xlDumpWriteText(pDump, sSymbolsPath, (sSymbolsJson != NULL) ? sSymbolsJson : "{\n\t\"symbols\": []\n}\n") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build-c", "failed to write symbol dump: %s", sSymbolsPath);
	}
	if ( iRc != XL_RC_OK ) {
		free(sTraceText);
		free(sSymbolsJson);
		return iRc;
	}

	snprintf(sSummary, sizeof(sSummary), "BuildCResult output=%s kind=%s entry=%s\n", result.sOutputFile, result.sOutputKind, result.sEntrySymbol);
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( pOpts->bDumpJson ) {
		if ( sJsonDump != NULL ) fputs(sJsonDump, stdout);
	} else {
		fputs(sSummary, stdout);
	}

	free(sTraceText);
	free(sSymbolsJson);
	free(sJsonDump);
	return iRc;
}

static int __xlDriverRunTests(
	const XL_CliOptions* pOpts,
	XL_DiagContext* pDiag,
	XL_DumpContext* pDump
)
{
	int iRc;

	XL_UNUSED(pDiag);
	XL_UNUSED(pDump);

	iRc = xlTestRunSelector(pOpts->sInputFile);
	return (iRc == 0) ? XL_RC_OK : XL_RC_FAIL;
}

int xlDriverRun(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	int iRc;

	if ( (pOpts == NULL) || (pDiag == NULL) || (pDump == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iRc = xlDumpEnsureLayout(pDump);
	if ( iRc != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "driver", "failed to create release layout");
		return iRc;
	}

	if ( pOpts->eCommand == XL_CMD_LEX ) {
		return __xlDriverRunLex(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_PARSE ) {
		return __xlDriverRunParse(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_SEM ) {
		return __xlDriverRunSem(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_LOWER ) {
		return __xlDriverRunLower(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_CGEN ) {
		return __xlDriverRunCgen(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_BUILD ) {
		return __xlDriverBuildProgram(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_RUN ) {
		return __xlDriverRunProgram(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_BUILD_C ) {
		return __xlDriverBuildCProgram(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_RUN_C ) {
		return __xlDriverRunCProgram(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_TEST ) {
		return __xlDriverRunTests(pOpts, pDiag, pDump);
	}

	iRc = __xlDriverWriteStubDump(pOpts, pDiag, pDump);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	XL_DIAG_INFO(
		pDiag,
		"driver",
		"stage0 ready: command '%s' accepted, pipeline body not implemented yet",
		xlCliCommandName(pOpts->eCommand)
	);

	if ( pOpts->sEmitPath != NULL ) {
		XL_DIAG_INFO(pDiag, "driver", "emit path reserved: %s", pOpts->sEmitPath);
	}
	if ( pOpts->sStopAfter != NULL ) {
		XL_DIAG_INFO(pDiag, "driver", "stop-after reserved: %s", pOpts->sStopAfter);
	}

	return XL_RC_NOT_IMPLEMENTED;
}
