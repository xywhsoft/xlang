#include "xl_driver.h"
#include "../codegen/c/xl_cgen.h"
#include "../common/xl_app_path.h"
#include "../host/xl_host.h"
#include "../lower/xl_lower.h"
#include "../runtime/xl_exec_ctx.h"
#include "xl_module_link.h"
#include "xl_xlp_load.h"
#if XLANG_DEBUGMODE_ENABLED
#include "../ast/xl_ast_dump.h"
#include "../diag/xl_dump.h"
#include "../lexer/xl_lexer_api.h"
#include "../parser/xl_parser_api.h"
#include "../sem/xl_sem_dump.h"
#include "xl_driver_debug.h"
#include "../host/xl_host_symbols.h"
#include "xl_xlp_dump.h"
#endif

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

#if XLANG_DEBUGMODE_ENABLED
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
#endif

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

static const XL_AstNode* __xlDriverAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}
	return pNode->ppChildren[iIndex];
}

static const XL_AstNode* __xlDriverFuncReceiverNode(const XL_AstNode* pNode)
{
	if ( (pNode == NULL) || (pNode->eKind != XL_AST_FUNC_DECL) || (pNode->iChildCount <= 3) ) {
		return NULL;
	}
	return pNode->ppChildren[3];
}

static const char* __xlDriverFuncExternName(const XL_AstNode* pNode)
{
	const XL_AstNode* pBody;
	const XL_AstNode* pReceiver;
	const char* sExternName;

	if ( (pNode == NULL) || (pNode->eKind != XL_AST_FUNC_DECL) ) {
		return NULL;
	}
	if ( pNode->iIntValue == 0 ) {
		return NULL;
	}
	pReceiver = __xlDriverFuncReceiverNode(pNode);
	sExternName = pNode->sText2;
	if ( pReceiver != NULL ) {
		if ( (sExternName == NULL) || (sExternName[0] == '\0') ) {
			sExternName = pNode->sText;
		}
		if ( (sExternName == NULL) || (sExternName[0] == '\0') ) {
			return NULL;
		}
	} else if ( (sExternName == NULL) || (sExternName[0] == '\0') ) {
		return NULL;
	}
	pBody = __xlDriverAstChild(pNode, 2);
	if ( (pBody == NULL) || (pBody->eKind != XL_AST_BLOCK) || (pBody->iChildCount != 0) ) {
		return NULL;
	}
	return sExternName;
}

static const char* __xlDriverVarExternName(const XL_AstNode* pNode)
{
	const XL_AstNode* pTypeNode;
	const XL_AstNode* pValueNode;

	if ( (pNode == NULL) || (pNode->eKind != XL_AST_VAR_DECL) ) {
		return NULL;
	}
	if ( (pNode->sText2 == NULL) || (pNode->sText2[0] == '\0') ) {
		return NULL;
	}
	pTypeNode = __xlDriverAstChild(pNode, 0);
	pValueNode = __xlDriverAstChild(pNode, 1);
	if ( (pValueNode == NULL) && (pTypeNode != NULL) && (pTypeNode->eKind != XL_AST_TYPE_REF) ) {
		pValueNode = pTypeNode;
		pTypeNode = NULL;
	}
	if ( (pTypeNode == NULL) || (pValueNode != NULL) ) {
		return NULL;
	}
	return pNode->sText2;
}

static int __xlAddRootExternSymbolsGeneric(
	const XL_AstNode* pNode,
	int (*fnAddSymbol)(void* pCtx, const char* sName),
	void* pCtx
)
{
	size_t i;
	const char* sExternName;

	if ( fnAddSymbol == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pNode == NULL ) {
		return XL_RC_OK;
	}

	sExternName = __xlDriverFuncExternName(pNode);
	if ( sExternName == NULL ) {
		sExternName = __xlDriverVarExternName(pNode);
	}
	if ( (sExternName != NULL) && (sExternName[0] != '\0') ) {
		if ( fnAddSymbol(pCtx, sExternName) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlAddRootExternSymbolsGeneric(pNode->ppChildren[i], fnAddSymbol, pCtx) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

#if XLANG_DEBUGMODE_ENABLED
static void __xlBuildDumpPath(
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
		unsigned char ch;

		ch = (unsigned char)sInput[i];
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
#endif

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

static int __xlModuleNeedsSharedExternSymbols(const XL_ModuleSummary* pModule)
{
	if ( pModule == NULL ) {
		return 0;
	}
	return !pModule->bIsRoot &&
		pModule->bIsNative &&
		(pModule->sManifestPath != NULL) &&
		(pModule->sManifestPath[0] != '\0') &&
		(pModule->sConsumeMode != NULL) &&
		(strcmp(pModule->sConsumeMode, "shared-bridge") == 0);
}

static int __xlAddSharedExternSymbolsFromModule(
	const XL_ModuleSummary* pModule,
	int (*fnAddSymbol)(void* pCtx, const char* sName),
	void* pCtx
)
{
	XL_XlpFunctionInfo* pFuncs;
	size_t iFuncCount;
	size_t j;

	if ( (pModule == NULL) || (fnAddSymbol == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( !__xlModuleNeedsSharedExternSymbols(pModule) ) {
		return XL_RC_OK;
	}

	pFuncs = NULL;
	iFuncCount = 0;
	if ( xlXlpReadFunctions(pModule->sManifestPath, pModule->sKey, &pFuncs, &iFuncCount) != XL_RC_OK ) {
		return XL_RC_OK;
	}

	for ( j = 0; j < iFuncCount; ++j ) {
		/*
		 * functions[].symbol is the host-resolved external target exported by the
		 * shared artifact. abiSymbol stays inside generated code as the wrapper ABI name.
		 */
		if ( (pFuncs[j].sSymbol[0] != '\0') && (fnAddSymbol(pCtx, pFuncs[j].sSymbol) != XL_RC_OK) ) {
			xlXlpFunctionInfoFree(pFuncs, iFuncCount);
			return XL_RC_FAIL;
		}
	}

	xlXlpFunctionInfoFree(pFuncs, iFuncCount);
	return XL_RC_OK;
}

static int __xlExecCtxAddExternSymbolAdapter(void* pCtx, const char* sName)
{
	return xlExecCtxAddExternSymbol((XL_ExecCtx*)pCtx, sName);
}

static int __xlHostCtxAddExternSymbolAdapter(void* pCtx, const char* sName)
{
	return xlHostContextAddExternSymbol((XL_HostContext*)pCtx, sName);
}

static int __xlAddRootExternSymbolsToExecCtx(XL_ExecCtx* pExecCtx, const XL_AstNode* pRoot)
{
	return __xlAddRootExternSymbolsGeneric(pRoot, __xlExecCtxAddExternSymbolAdapter, pExecCtx);
}

static int __xlAddRootExternSymbolsToHostCtx(XL_HostContext* pHostCtx, const XL_AstNode* pRoot)
{
	return __xlAddRootExternSymbolsGeneric(pRoot, __xlHostCtxAddExternSymbolAdapter, pHostCtx);
}

static int __xlAddMirExternSymbolsGeneric(
	const XL_MirNode* pNode,
	int (*fnAddSymbol)(void* pCtx, const char* sName),
	void* pCtx
)
{
	size_t i;

	if ( fnAddSymbol == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pNode == NULL ) {
		return XL_RC_OK;
	}
	if ( (pNode->sExternName != NULL) && (pNode->sExternName[0] != '\0') ) {
		if ( fnAddSymbol(pCtx, pNode->sExternName) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		if ( __xlAddMirExternSymbolsGeneric(pNode->ppChildren[i], fnAddSymbol, pCtx) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}
	return XL_RC_OK;
}

static int __xlAddMirExternSymbolsToExecCtx(XL_ExecCtx* pExecCtx, const XL_MirNode* pRoot)
{
	return __xlAddMirExternSymbolsGeneric(pRoot, __xlExecCtxAddExternSymbolAdapter, pExecCtx);
}

static int __xlAddMirExternSymbolsToHostCtx(XL_HostContext* pHostCtx, const XL_MirNode* pRoot)
{
	return __xlAddMirExternSymbolsGeneric(pRoot, __xlHostCtxAddExternSymbolAdapter, pHostCtx);
}

static int __xlAddModuleExternSymbolsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		if ( __xlAddSharedExternSymbolsFromModule(pModule, __xlExecCtxAddExternSymbolAdapter, pExecCtx) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
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

		pModule = &pModuleResult->pModules[i];
		if ( __xlAddSharedExternSymbolsFromModule(pModule, __xlHostCtxAddExternSymbolAdapter, pHostCtx) != XL_RC_OK ) {
			return XL_RC_FAIL;
		}
	}

	return XL_RC_OK;
}

static int __xlAddModuleLinkLibrariesToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLinkLibCount; ++j ) {
			if ( xlExecCtxAddLinkLibrary(pExecCtx, pModule->arrLinkLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}

	return XL_RC_OK;
}

static int __xlAddModuleLinkLibrariesToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;

		pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLinkLibCount; ++j ) {
			if ( xlHostContextAddLinkLibrary(pHostCtx, pModule->arrLinkLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}

	return XL_RC_OK;
}

static int __xlAddModuleLibPathsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLibPathCount; ++j ) {
			if ( xlExecCtxAddLibraryPath(pExecCtx, pModule->arrLibPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static int __xlAddModuleLibPathsToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLibPathCount; ++j ) {
			if ( xlHostContextAddLibraryPath(pHostCtx, pModule->arrLibPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static int __xlAddModuleIncPathsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iIncPathCount; ++j ) {
			if ( xlExecCtxAddIncludePath(pExecCtx, pModule->arrIncPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static int __xlAddModuleIncPathsToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iIncPathCount; ++j ) {
			if ( xlHostContextAddIncludePath(pHostCtx, pModule->arrIncPaths[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static int __xlAddModuleLoadLibsToExecCtx(XL_ExecCtx* pExecCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pExecCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLoadLibCount; ++j ) {
			if ( xlExecCtxAddLoadLibrary(pExecCtx, pModule->arrLoadLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static int __xlAddModuleLoadLibsToHostCtx(XL_HostContext* pHostCtx, const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;
	size_t j;

	if ( (pHostCtx == NULL) || (pModuleResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule = &pModuleResult->pModules[i];
		for ( j = 0; j < pModule->iLoadLibCount; ++j ) {
			if ( xlHostContextAddLoadLibrary(pHostCtx, pModule->arrLoadLibs[j]) != XL_RC_OK ) {
				return XL_RC_FAIL;
			}
		}
	}
	return XL_RC_OK;
}

static const char* __xlDriverRootPath(const XL_DumpContext* pDump, char* sFallback, size_t iFallbackSize)
{
#if XLANG_DEBUGMODE_ENABLED
	if ( (pDump != NULL) && (pDump->sRoot[0] != '\0') ) {
		return pDump->sRoot;
	}
#else
	XL_UNUSED(pDump);
#endif

	if ( (sFallback != NULL) && (iFallbackSize > 0) ) {
		if ( xlAppPathGetDirectory(sFallback, iFallbackSize) == XL_RC_OK ) {
			return sFallback;
		}
		snprintf(sFallback, iFallbackSize, ".");
		return sFallback;
	}

	return ".";
}

static void __xlBuildDefaultOutputPath(
	char* sOut,
	size_t iOutSize,
	const char* sRoot,
	const char* sInputFile,
	const char* sExt
)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;
	char sBaseName[XL_PATH_MAX_LEN];

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
	xlPathJoin(sOut, iOutSize, (sRoot != NULL) ? sRoot : ".", sBaseName);
}

#if XLANG_DEBUGMODE_ENABLED
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
#endif

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
	char sRootPath[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;
#if XLANG_DEBUGMODE_ENABLED
	char* sTraceWithModules;
	char sCodePath[XL_PATH_MAX_LEN];
	char sTracePath[XL_PATH_MAX_LEN];
	char* sJsonDump;
#endif

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlExecCtxInit(&execCtx, pOpts->sInputFile, __xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)));
	sCode = NULL;
	sTraceText = NULL;
#if XLANG_DEBUGMODE_ENABLED
	sTraceWithModules = NULL;
	sJsonDump = NULL;
#endif

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) {
		goto cleanup;
	}

	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, XL_CLI_TRACE_SEM(pOpts) ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, XL_CLI_TRACE_LOWER(pOpts) ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = xlCgenMir(&lowerResult, &moduleResult, pDiag, &sCode);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}
	if ( __xlAddModuleArtifactsToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module artifacts");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleExternSymbolsToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddMirExternSymbolsToExecCtx(&execCtx, lowerResult.pMirRoot) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect lowered extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddRootExternSymbolsToExecCtx(&execCtx, moduleResult.pLinkedRoot) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect root extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLinkLibrariesToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module link libraries");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLibPathsToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module library paths");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleIncPathsToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module include paths");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLoadLibsToExecCtx(&execCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "run", "failed to collect module load libraries");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}

#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sCodePath, sizeof(sCodePath), "cgen", pOpts->sInputFile, ".run.c");
	if ( xlDumpWriteText(pDump, sCodePath, sCode) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run", "failed to write run C dump: %s", sCodePath);
	}
#endif

	iRc = xlExecRunCode(&execCtx, pDiag, sCode, &sTraceText);
#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "runtime", pOpts->sInputFile, ".trace.txt");
	sTraceWithModules = __xlConcatText((moduleResult.sTextDump != NULL) ? moduleResult.sTextDump : "", (sTraceText != NULL) ? sTraceText : "");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceWithModules != NULL) ? sTraceWithModules : ((sTraceText != NULL) ? sTraceText : "")) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run", "failed to write runtime trace: %s", sTracePath);
	}
#endif
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
#if XLANG_DEBUGMODE_ENABLED
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		if ( sJsonDump != NULL ) {
			fputs(sJsonDump, stdout);
		}
	} else {
		fputs(sSummary, stdout);
	}
#else
	fputs(sSummary, stdout);
#endif

	if ( pOpts->sEmitPath != NULL ) {
		const char* sEmitText = sSummary;
#if XLANG_DEBUGMODE_ENABLED
		if ( XL_CLI_DUMP_JSON(pOpts) ) {
			sEmitText = (sJsonDump != NULL) ? sJsonDump : "";
		}
#endif
		if ( __xlWriteTextFile(pOpts->sEmitPath, sEmitText) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "run", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
			goto cleanup;
		}
	}

cleanup:
	free(sCode);
	free(sTraceText);
#if XLANG_DEBUGMODE_ENABLED
	free(sTraceWithModules);
	free(sJsonDump);
#endif
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
	char sRootPath[XL_PATH_MAX_LEN];
	char sTracePath[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;
#if XLANG_DEBUGMODE_ENABLED
	char* sJsonDump;
#endif

	xlHostContextInit(&hostCtx, pOpts->sInputFile, __xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)));
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
#if XLANG_DEBUGMODE_ENABLED
	sJsonDump = NULL;
#endif

	iRc = xlHostRunCFile(&hostCtx, pDiag, pOpts->sInputFile, &sTraceText, &result);
#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "run-c", "failed to write host trace: %s", sTracePath);
	}
#endif
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
#if XLANG_DEBUGMODE_ENABLED
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		if ( sJsonDump != NULL ) {
			fputs(sJsonDump, stdout);
		}
	} else {
		fputs(sSummary, stdout);
	}
#else
	fputs(sSummary, stdout);
#endif

	if ( pOpts->sEmitPath != NULL ) {
		const char* sEmitText = sSummary;
#if XLANG_DEBUGMODE_ENABLED
		if ( XL_CLI_DUMP_JSON(pOpts) ) {
			sEmitText = (sJsonDump != NULL) ? sJsonDump : "";
		}
#endif
		if ( __xlWriteTextFile(pOpts->sEmitPath, sEmitText) != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "run-c", "failed to write emit file: %s", pOpts->sEmitPath);
			iRc = XL_RC_IO_ERROR;
		}
	}

	free(sTraceText);
#if XLANG_DEBUGMODE_ENABLED
	free(sJsonDump);
#endif
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
	char sRootPath[XL_PATH_MAX_LEN];
	char sOutputFile[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;
#if XLANG_DEBUGMODE_ENABLED
	char* sSymbolsJson;
	char sCodePath[XL_PATH_MAX_LEN];
	char sTracePath[XL_PATH_MAX_LEN];
	char sSymbolsPath[XL_PATH_MAX_LEN];
	char* sJsonDump;
#endif

	xlModuleLinkResultInit(&moduleResult);
	xlSemResultInit(&semResult);
	xlLowerResultInit(&lowerResult);
	xlHostContextInit(&hostCtx, pOpts->sInputFile, __xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)));
	memset(&result, 0, sizeof(result));
	sCode = NULL;
	sTraceText = NULL;
#if XLANG_DEBUGMODE_ENABLED
	sSymbolsJson = NULL;
	sJsonDump = NULL;
#endif
	if ( pOpts->sEmitPath != NULL ) {
		snprintf(sOutputFile, sizeof(sOutputFile), "%s", pOpts->sEmitPath);
	} else {
		__xlBuildDefaultOutputPath(
			sOutputFile,
			sizeof(sOutputFile),
			__xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)),
			pOpts->sInputFile,
			".exe"
		);
	}

	iRc = xlModuleLinkFromFile(pOpts->sInputFile, pDiag, XL_CLI_TRACE_PARSER(pOpts) ? 1 : 0, &moduleResult);
	if ( (iRc != XL_RC_OK) || (moduleResult.pLinkedRoot == NULL) ) goto cleanup;
	iRc = xlSemAnalyzeAst(moduleResult.pLinkedRoot, pDiag, XL_CLI_TRACE_SEM(pOpts) ? 1 : 0, &semResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlLowerAst(moduleResult.pLinkedRoot, &semResult, pDiag, XL_CLI_TRACE_LOWER(pOpts) ? 1 : 0, &lowerResult);
	if ( iRc != XL_RC_OK ) goto cleanup;
	iRc = xlCgenMir(&lowerResult, &moduleResult, pDiag, &sCode);
	if ( iRc != XL_RC_OK ) goto cleanup;
	if ( __xlAddModuleArtifactsToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module artifacts");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleExternSymbolsToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddMirExternSymbolsToHostCtx(&hostCtx, lowerResult.pMirRoot) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect lowered extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddRootExternSymbolsToHostCtx(&hostCtx, moduleResult.pLinkedRoot) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect root extern symbols");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLinkLibrariesToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module link libraries");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLibPathsToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module library paths");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleIncPathsToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module include paths");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}
	if ( __xlAddModuleLoadLibsToHostCtx(&hostCtx, &moduleResult) != XL_RC_OK ) {
		XL_DIAG_ERROR(pDiag, "build", "failed to collect module load libraries");
		iRc = XL_RC_FAIL;
		goto cleanup;
	}

#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sCodePath, sizeof(sCodePath), "cgen", pOpts->sInputFile, ".build.c");
	if ( xlDumpWriteText(pDump, sCodePath, sCode) != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write build C dump: %s", sCodePath);
	}
#endif

	iRc = xlHostBuildCText(&hostCtx, pDiag, pOpts->sInputFile, sCode, "xl_generated_entry", sOutputFile, &sTraceText, &result);
#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".build.trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write host build trace: %s", sTracePath);
	}
	sSymbolsJson = xlHostDumpSymbolsJson();
	__xlBuildDumpPath(sSymbolsPath, sizeof(sSymbolsPath), "host", pOpts->sInputFile, ".symbols.json");
	if ( xlDumpWriteText(pDump, sSymbolsPath, (sSymbolsJson != NULL) ? sSymbolsJson : "{\n\t\"symbols\": []\n}\n") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build", "failed to write symbol dump: %s", sSymbolsPath);
	}
#endif
	if ( iRc != XL_RC_OK ) goto cleanup;

	snprintf(sSummary, sizeof(sSummary), "BuildResult output=%s kind=%s entry=%s\n", result.sOutputFile, result.sOutputKind, result.sEntrySymbol);
#if XLANG_DEBUGMODE_ENABLED
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		if ( sJsonDump != NULL ) fputs(sJsonDump, stdout);
	} else {
		fputs(sSummary, stdout);
	}
#else
	fputs(sSummary, stdout);
#endif

cleanup:
	free(sCode);
	free(sTraceText);
#if XLANG_DEBUGMODE_ENABLED
	free(sSymbolsJson);
	free(sJsonDump);
#endif
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
	char sRootPath[XL_PATH_MAX_LEN];
	char sOutputFile[XL_PATH_MAX_LEN];
	char sSummary[XL_DIAG_TEXT_MAX];
	int iRc;
#if XLANG_DEBUGMODE_ENABLED
	char* sSymbolsJson;
	char sTracePath[XL_PATH_MAX_LEN];
	char sSymbolsPath[XL_PATH_MAX_LEN];
	char* sJsonDump;
#endif

	xlHostContextInit(&hostCtx, pOpts->sInputFile, __xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)));
	memset(&result, 0, sizeof(result));
	sTraceText = NULL;
#if XLANG_DEBUGMODE_ENABLED
	sSymbolsJson = NULL;
	sJsonDump = NULL;
#endif
	if ( pOpts->sEmitPath != NULL ) {
		snprintf(sOutputFile, sizeof(sOutputFile), "%s", pOpts->sEmitPath);
	} else {
		__xlBuildDefaultOutputPath(
			sOutputFile,
			sizeof(sOutputFile),
			__xlDriverRootPath(pDump, sRootPath, sizeof(sRootPath)),
			pOpts->sInputFile,
			".exe"
		);
	}

	iRc = xlHostBuildCFile(&hostCtx, pDiag, pOpts->sInputFile, sOutputFile, &sTraceText, &result);
#if XLANG_DEBUGMODE_ENABLED
	__xlBuildDumpPath(sTracePath, sizeof(sTracePath), "host", pOpts->sInputFile, ".build.trace.txt");
	if ( xlDumpWriteText(pDump, sTracePath, (sTraceText != NULL) ? sTraceText : "") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build-c", "failed to write host build trace: %s", sTracePath);
	}
	sSymbolsJson = xlHostDumpSymbolsJson();
	__xlBuildDumpPath(sSymbolsPath, sizeof(sSymbolsPath), "host", pOpts->sInputFile, ".symbols.json");
	if ( xlDumpWriteText(pDump, sSymbolsPath, (sSymbolsJson != NULL) ? sSymbolsJson : "{\n\t\"symbols\": []\n}\n") != XL_RC_OK ) {
		XL_DIAG_WARN(pDiag, "build-c", "failed to write symbol dump: %s", sSymbolsPath);
	}
#endif
	if ( iRc != XL_RC_OK ) {
		free(sTraceText);
#if XLANG_DEBUGMODE_ENABLED
		free(sSymbolsJson);
#endif
		return iRc;
	}

	snprintf(sSummary, sizeof(sSummary), "BuildCResult output=%s kind=%s entry=%s\n", result.sOutputFile, result.sOutputKind, result.sEntrySymbol);
#if XLANG_DEBUGMODE_ENABLED
	sJsonDump = __xlWrapJsonText("result", sSummary);
	if ( XL_CLI_DUMP_JSON(pOpts) ) {
		if ( sJsonDump != NULL ) fputs(sJsonDump, stdout);
	} else {
		fputs(sSummary, stdout);
	}
#else
	fputs(sSummary, stdout);
#endif

	free(sTraceText);
#if XLANG_DEBUGMODE_ENABLED
	free(sSymbolsJson);
	free(sJsonDump);
#endif
	return iRc;
}

static int __xlDriverRunInternal(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	int iRc;

	if ( (pOpts == NULL) || (pDiag == NULL) ) {
		return XL_RC_BAD_ARG;
	}

#if XLANG_DEBUGMODE_ENABLED
	if ( pDump != NULL ) {
		iRc = xlDumpEnsureLayout(pDump);
		if ( iRc != XL_RC_OK ) {
			XL_DIAG_ERROR(pDiag, "driver", "failed to create release layout");
			return iRc;
		}
	}
#endif
	if ( pOpts->sInputFile != NULL ) {
		xlDiagSetDefaultSourceFile(pDiag, pOpts->sInputFile);
	}

#if XLANG_DEBUGMODE_ENABLED
	if ( pOpts->eCommand == XL_CMD_LEX ) {
		return xlDriverRunLexDebug(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_PARSE ) {
		return xlDriverRunParseDebug(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_SEM ) {
		return xlDriverRunSemDebug(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_LOWER ) {
		return xlDriverRunLowerDebug(pOpts, pDiag, pDump);
	}
	if ( pOpts->eCommand == XL_CMD_CGEN ) {
		return xlDriverRunCgenDebug(pOpts, pDiag, pDump);
	}
#endif
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

#if XLANG_DEBUGMODE_ENABLED
	iRc = __xlDriverWriteStubDump(pOpts, pDiag, pDump);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
#endif

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

int xlDriverRunRelease(const XL_CliOptions* pOpts, XL_DiagContext* pDiag)
{
	return __xlDriverRunInternal(pOpts, pDiag, NULL);
}

#if XLANG_DEBUGMODE_ENABLED
int xlDriverRunDebug(const XL_CliOptions* pOpts, XL_DiagContext* pDiag, XL_DumpContext* pDump)
{
	return __xlDriverRunInternal(pOpts, pDiag, pDump);
}
#endif
