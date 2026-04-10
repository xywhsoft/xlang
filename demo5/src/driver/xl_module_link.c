#include "xl_module_link.h"
#include "xl_xlp_load.h"

#include "../common/xl_path.h"
#include "../lexer/xl_lexer_api.h"
#include "../parser/xl_parser_api.h"
#include "../runtime/xl_native_registry.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif

typedef struct XL_ModuleTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_ModuleTextBuffer;

typedef struct XL_ModuleNameMap {
	char* sOriginalName;
	char* sMangledName;
} XL_ModuleNameMap;

typedef struct XL_ModuleExportInfo {
	char* sName;
	char* sMangledName;
} XL_ModuleExportInfo;

typedef struct XL_ModuleInfo {
	char* sModuleKey;
	char* sFilePath;
	char* sLeafAlias;
	char* sBindingName;
	char* sManifestPath;
	char* sPackageName;
	char* sPackageVersion;
	char* sPackageAbiVersion;
	char* sPackageAbiId;
	char* sArtifactKind;
	char* sArtifactPath;
	char* sArtifactFile;
	char* sConsumeMode;
	XL_AstNode* pRoot;
	char** arrImportKeys;
	char** arrResolvedImportFiles;
	size_t iImportCount;
	size_t iImportCap;
	XL_ModuleNameMap* arrNames;
	size_t iNameCount;
	size_t iNameCap;
	XL_ModuleExportInfo* arrExports;
	size_t iExportCount;
	size_t iExportCap;
	int bIsRoot;
	int bIsNative;
	int iState;
} XL_ModuleInfo;

typedef struct XL_ModuleScope {
	char** arrNames;
	size_t iCount;
	size_t iCap;
	struct XL_ModuleScope* pPrev;
} XL_ModuleScope;

typedef struct XL_ModuleLinkContext {
	XL_DiagContext* pDiag;
	int bTraceParser;
	XL_ModuleInfo* pModules;
	size_t iModuleCount;
	size_t iModuleCap;
	size_t* pOrderIndices;
	size_t iOrderCount;
	size_t iOrderCap;
} XL_ModuleLinkContext;

static int __xlModuleBufGrow(XL_ModuleTextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 256 : pBuf->iCap;
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

static void __xlModuleBufAppend(XL_ModuleTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlModuleBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlModuleBufAppendFormat(XL_ModuleTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlModuleBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlModuleBufAppendEscaped(XL_ModuleTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlModuleBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlModuleBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlModuleBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlModuleBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlModuleBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlModuleBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlModuleBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static char* __xlModuleDupText(const char* sText)
{
	size_t iLen;
	char* sCopy;

	if ( sText == NULL ) {
		sText = "";
	}

	iLen = strlen(sText);
	sCopy = (char*)malloc(iLen + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}

	memcpy(sCopy, sText, iLen + 1);
	return sCopy;
}

static void __xlModuleTrace(XL_DiagContext* pDiag, const char* sFormat, ...)
{
	va_list args;
	char sText[XL_DIAG_TEXT_MAX];

	if ( (pDiag == NULL) || (sFormat == NULL) ) {
		return;
	}

	va_start(args, sFormat);
	vsnprintf(sText, sizeof(sText), sFormat, args);
	va_end(args);
	xlDiagEmit(pDiag, XL_DIAG_LEVEL_INFO, "module", __FILE__, __LINE__, "%s", sText);
}

static void __xlModuleScopeFree(XL_ModuleScope* pScope)
{
	size_t i;

	if ( pScope == NULL ) {
		return;
	}

	for ( i = 0; i < pScope->iCount; ++i ) {
		free(pScope->arrNames[i]);
	}
	free(pScope->arrNames);
	free(pScope);
}

static int __xlFileExists(const char* sFilePath)
{
	FILE* pFile;

	if ( sFilePath == NULL ) {
		return 0;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return 0;
	}

	fclose(pFile);
	return 1;
}

static XL_ModuleScope* __xlModuleScopePush(XL_ModuleScope* pPrev)
{
	XL_ModuleScope* pScope;

	pScope = (XL_ModuleScope*)calloc(1, sizeof(*pScope));
	if ( pScope == NULL ) {
		return NULL;
	}

	pScope->pPrev = pPrev;
	return pScope;
}

static int __xlModuleScopeAdd(XL_ModuleScope* pScope, const char* sName)
{
	char** arrNew;
	size_t iNewCap;

	if ( (pScope == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pScope->iCount >= pScope->iCap ) {
		iNewCap = (pScope->iCap == 0) ? 4 : (pScope->iCap * 2);
		arrNew = (char**)realloc(pScope->arrNames, sizeof(char*) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pScope->arrNames = arrNew;
		pScope->iCap = iNewCap;
	}

	pScope->arrNames[pScope->iCount] = __xlModuleDupText(sName);
	if ( pScope->arrNames[pScope->iCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pScope->iCount += 1;
	return XL_RC_OK;
}

static int __xlModuleScopeHas(const XL_ModuleScope* pScope, const char* sName)
{
	size_t i;

	while ( pScope != NULL ) {
		for ( i = 0; i < pScope->iCount; ++i ) {
			if ( strcmp(pScope->arrNames[i], sName) == 0 ) {
				return 1;
			}
		}
		pScope = pScope->pPrev;
	}

	return 0;
}

static void __xlModuleNormalizePath(char* sOut, size_t iOutSize, const char* sPath)
{
	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	if ( sPath == NULL ) {
		sPath = "";
	}

#if defined(_WIN32) || defined(_WIN64)
	if ( GetFullPathNameA(sPath, (DWORD)iOutSize, sOut, NULL) == 0 ) {
		snprintf(sOut, iOutSize, "%s", sPath);
	}
#else
	snprintf(sOut, iOutSize, "%s", sPath);
#endif
}

static void __xlModuleStripFileName(char* sPath)
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

static const XL_AstNode* __xlAstChild(const XL_AstNode* pNode, size_t iIndex)
{
	if ( (pNode == NULL) || (iIndex >= pNode->iChildCount) ) {
		return NULL;
	}

	return pNode->ppChildren[iIndex];
}

static char* __xlBuildImportKey(const XL_AstNode* pNode)
{
	char* sLeft;
	char* sText;
	size_t iNeed;

	if ( pNode == NULL ) {
		return __xlModuleDupText("");
	}

	if ( pNode->eKind == XL_AST_IDENT ) {
		return __xlModuleDupText(pNode->sText);
	}
	if ( (pNode->eKind != XL_AST_MEMBER) || (pNode->sText == NULL) ) {
		return __xlModuleDupText("");
	}

	sLeft = __xlBuildImportKey(__xlAstChild(pNode, 0));
	if ( sLeft == NULL ) {
		return NULL;
	}

	iNeed = strlen(sLeft) + 1 + strlen(pNode->sText) + 1;
	sText = (char*)malloc(iNeed);
	if ( sText == NULL ) {
		free(sLeft);
		return NULL;
	}

	if ( sLeft[0] == '\0' ) {
		snprintf(sText, iNeed, "%s", pNode->sText);
	} else {
		snprintf(sText, iNeed, "%s.%s", sLeft, pNode->sText);
	}
	free(sLeft);
	return sText;
}

static int __xlResolveImportFile(char* sOut, size_t iOutSize, const char* sCurrentFile, const char* sImportKey)
{
	char sBaseDir[XL_PATH_MAX_LEN];
	char sImportRel[XL_PATH_MAX_LEN];
	char sJoined[XL_PATH_MAX_LEN];
	size_t i;

	if ( (sOut == NULL) || (iOutSize == 0) || (sCurrentFile == NULL) || (sImportKey == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	snprintf(sBaseDir, sizeof(sBaseDir), "%s", sCurrentFile);
	__xlModuleStripFileName(sBaseDir);

	snprintf(sImportRel, sizeof(sImportRel), "%s", sImportKey);
	for ( i = 0; sImportRel[i] != '\0'; ++i ) {
		if ( sImportRel[i] == '.' ) {
			sImportRel[i] = '\\';
		}
	}
	if ( strlen(sImportRel) + 4 < sizeof(sImportRel) ) {
		strcat(sImportRel, ".xl");
	}

	xlPathJoin(sJoined, sizeof(sJoined), sBaseDir, sImportRel);
	__xlModuleNormalizePath(sOut, iOutSize, sJoined);
	return XL_RC_OK;
}

static int __xlModuleInfoAddImport(XL_ModuleInfo* pModule, const char* sImportKey)
{
	char** arrNew;
	char** arrResolvedNew;
	size_t iNewCap;

	if ( (pModule == NULL) || (sImportKey == NULL) || (sImportKey[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pModule->iImportCount >= pModule->iImportCap ) {
		iNewCap = (pModule->iImportCap == 0) ? 4 : (pModule->iImportCap * 2);
		arrNew = (char**)realloc(pModule->arrImportKeys, sizeof(char*) * iNewCap);
		arrResolvedNew = (char**)realloc(pModule->arrResolvedImportFiles, sizeof(char*) * iNewCap);
		if ( (arrNew == NULL) || (arrResolvedNew == NULL) ) {
			if ( arrNew != NULL ) {
				pModule->arrImportKeys = arrNew;
			}
			if ( arrResolvedNew != NULL ) {
				pModule->arrResolvedImportFiles = arrResolvedNew;
			}
			return XL_RC_IO_ERROR;
		}
		pModule->arrImportKeys = arrNew;
		pModule->arrResolvedImportFiles = arrResolvedNew;
		memset(pModule->arrResolvedImportFiles + pModule->iImportCap, 0, sizeof(char*) * (iNewCap - pModule->iImportCap));
		pModule->iImportCap = iNewCap;
	}

	pModule->arrImportKeys[pModule->iImportCount] = __xlModuleDupText(sImportKey);
	if ( pModule->arrImportKeys[pModule->iImportCount] == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pModule->iImportCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoResolveImport(XL_ModuleInfo* pModule, size_t iImportIndex, const char* sResolvedFile)
{
	char* sCopy;

	if ( (pModule == NULL) || (iImportIndex >= pModule->iImportCount) ) {
		return XL_RC_BAD_ARG;
	}

	sCopy = __xlModuleDupText(sResolvedFile);
	if ( (sResolvedFile != NULL) && (sCopy == NULL) ) {
		return XL_RC_IO_ERROR;
	}

	free(pModule->arrResolvedImportFiles[iImportIndex]);
	pModule->arrResolvedImportFiles[iImportIndex] = sCopy;
	return XL_RC_OK;
}

static int __xlModuleInfoAddName(XL_ModuleInfo* pModule, const char* sOriginalName, const char* sMangledName)
{
	XL_ModuleNameMap* arrNew;
	size_t iNewCap;

	if ( (pModule == NULL) || (sOriginalName == NULL) || (sOriginalName[0] == '\0') ) {
		return XL_RC_OK;
	}

	if ( pModule->iNameCount >= pModule->iNameCap ) {
		iNewCap = (pModule->iNameCap == 0) ? 4 : (pModule->iNameCap * 2);
		arrNew = (XL_ModuleNameMap*)realloc(pModule->arrNames, sizeof(XL_ModuleNameMap) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pModule->arrNames = arrNew;
		pModule->iNameCap = iNewCap;
	}

	pModule->arrNames[pModule->iNameCount].sOriginalName = __xlModuleDupText(sOriginalName);
	pModule->arrNames[pModule->iNameCount].sMangledName = __xlModuleDupText(sMangledName);
	if ( (pModule->arrNames[pModule->iNameCount].sOriginalName == NULL) || (pModule->arrNames[pModule->iNameCount].sMangledName == NULL) ) {
		free(pModule->arrNames[pModule->iNameCount].sOriginalName);
		free(pModule->arrNames[pModule->iNameCount].sMangledName);
		return XL_RC_IO_ERROR;
	}
	pModule->iNameCount += 1;
	return XL_RC_OK;
}

static int __xlModuleInfoAddExport(XL_ModuleInfo* pModule, const char* sName, const char* sMangledName)
{
	XL_ModuleExportInfo* arrNew;
	size_t iNewCap;

	if ( (pModule == NULL) || (sName == NULL) || (sName[0] == '\0') || (sMangledName == NULL) ) {
		return XL_RC_OK;
	}

	if ( pModule->iExportCount >= pModule->iExportCap ) {
		iNewCap = (pModule->iExportCap == 0) ? 4 : (pModule->iExportCap * 2);
		arrNew = (XL_ModuleExportInfo*)realloc(pModule->arrExports, sizeof(XL_ModuleExportInfo) * iNewCap);
		if ( arrNew == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pModule->arrExports = arrNew;
		pModule->iExportCap = iNewCap;
	}

	pModule->arrExports[pModule->iExportCount].sName = __xlModuleDupText(sName);
	pModule->arrExports[pModule->iExportCount].sMangledName = __xlModuleDupText(sMangledName);
	if ( (pModule->arrExports[pModule->iExportCount].sName == NULL) || (pModule->arrExports[pModule->iExportCount].sMangledName == NULL) ) {
		free(pModule->arrExports[pModule->iExportCount].sName);
		free(pModule->arrExports[pModule->iExportCount].sMangledName);
		return XL_RC_IO_ERROR;
	}
	pModule->iExportCount += 1;
	return XL_RC_OK;
}

static void __xlModuleInfoFree(XL_ModuleInfo* pModule)
{
	size_t i;

	if ( pModule == NULL ) {
		return;
	}

	free(pModule->sModuleKey);
	free(pModule->sFilePath);
	free(pModule->sLeafAlias);
	free(pModule->sBindingName);
	free(pModule->sManifestPath);
	free(pModule->sPackageName);
	free(pModule->sPackageVersion);
	free(pModule->sPackageAbiVersion);
	free(pModule->sPackageAbiId);
	free(pModule->sArtifactKind);
	free(pModule->sArtifactPath);
	free(pModule->sArtifactFile);
	free(pModule->sConsumeMode);
	xlAstFree(pModule->pRoot);

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		free(pModule->arrImportKeys[i]);
		free(pModule->arrResolvedImportFiles[i]);
	}
	free(pModule->arrImportKeys);
	free(pModule->arrResolvedImportFiles);

	for ( i = 0; i < pModule->iNameCount; ++i ) {
		free(pModule->arrNames[i].sOriginalName);
		free(pModule->arrNames[i].sMangledName);
	}
	free(pModule->arrNames);

	for ( i = 0; i < pModule->iExportCount; ++i ) {
		free(pModule->arrExports[i].sName);
		free(pModule->arrExports[i].sMangledName);
	}
	free(pModule->arrExports);

	memset(pModule, 0, sizeof(*pModule));
}

static XL_ModuleInfo* __xlFindModuleByFile(XL_ModuleLinkContext* pCtx, const char* sFilePath)
{
	size_t i;

	if ( (pCtx == NULL) || (sFilePath == NULL) || (sFilePath[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pCtx->iModuleCount; ++i ) {
		if ( (pCtx->pModules[i].sFilePath != NULL) && (strcmp(pCtx->pModules[i].sFilePath, sFilePath) == 0) ) {
			return &pCtx->pModules[i];
		}
	}

	return NULL;
}

static XL_ModuleInfo* __xlFindModuleByKey(const XL_ModuleLinkContext* pCtx, const char* sModuleKey)
{
	size_t i;

	if ( (pCtx == NULL) || (sModuleKey == NULL) || (sModuleKey[0] == '\0') ) {
		return NULL;
	}

	for ( i = 0; i < pCtx->iModuleCount; ++i ) {
		if ( (pCtx->pModules[i].sModuleKey != NULL) && (strcmp(pCtx->pModules[i].sModuleKey, sModuleKey) == 0) ) {
			return &pCtx->pModules[i];
		}
	}

	return NULL;
}

static int __xlAppendOrder(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	size_t* pNewIndices;
	size_t iNewCap;
	size_t iModuleIndex;

	if ( (pCtx == NULL) || (pModule == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( pCtx->iOrderCount >= pCtx->iOrderCap ) {
		iNewCap = (pCtx->iOrderCap == 0) ? 4 : (pCtx->iOrderCap * 2);
		pNewIndices = (size_t*)realloc(pCtx->pOrderIndices, sizeof(size_t) * iNewCap);
		if ( pNewIndices == NULL ) {
			return XL_RC_IO_ERROR;
		}
		pCtx->pOrderIndices = pNewIndices;
		pCtx->iOrderCap = iNewCap;
	}

	iModuleIndex = (size_t)(pModule - pCtx->pModules);
	pCtx->pOrderIndices[pCtx->iOrderCount++] = iModuleIndex;
	return XL_RC_OK;
}

static XL_ModuleInfo* __xlAddModuleRecord(XL_ModuleLinkContext* pCtx)
{
	XL_ModuleInfo* pNewItems;
	size_t iNewCap;

	if ( pCtx->iModuleCount >= pCtx->iModuleCap ) {
		iNewCap = (pCtx->iModuleCap == 0) ? 4 : (pCtx->iModuleCap * 2);
		pNewItems = (XL_ModuleInfo*)realloc(pCtx->pModules, sizeof(XL_ModuleInfo) * iNewCap);
		if ( pNewItems == NULL ) {
			return NULL;
		}
		memset(pNewItems + pCtx->iModuleCap, 0, sizeof(XL_ModuleInfo) * (iNewCap - pCtx->iModuleCap));
		pCtx->pModules = pNewItems;
		pCtx->iModuleCap = iNewCap;
	}

	return &pCtx->pModules[pCtx->iModuleCount++];
}

static const char* __xlFindMangledName(const XL_ModuleInfo* pModule, const char* sOriginalName)
{
	size_t i;

	if ( (pModule == NULL) || (sOriginalName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iNameCount; ++i ) {
		if ( strcmp(pModule->arrNames[i].sOriginalName, sOriginalName) == 0 ) {
			return pModule->arrNames[i].sMangledName;
		}
	}

	return NULL;
}

static const char* __xlFindExportName(const XL_ModuleInfo* pModule, const char* sExportName)
{
	size_t i;

	if ( (pModule == NULL) || (sExportName == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iExportCount; ++i ) {
		if ( strcmp(pModule->arrExports[i].sName, sExportName) == 0 ) {
			return pModule->arrExports[i].sMangledName;
		}
	}

	return NULL;
}

static int __xlModuleHasImportKey(const XL_ModuleInfo* pModule, const char* sModuleKey)
{
	size_t i;

	if ( (pModule == NULL) || (sModuleKey == NULL) ) {
		return 0;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( strcmp(pModule->arrImportKeys[i], sModuleKey) == 0 ) {
			return 1;
		}
	}

	return 0;
}

static XL_ModuleInfo* __xlFindImportedModule(const XL_ModuleLinkContext* pCtx, const XL_ModuleInfo* pModule, const char* sModuleKey)
{
	size_t i;
	XL_ModuleInfo* pImported;

	if ( (pCtx == NULL) || (pModule == NULL) || (sModuleKey == NULL) ) {
		return NULL;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( strcmp(pModule->arrImportKeys[i], sModuleKey) != 0 ) {
			continue;
		}
		if ( pModule->arrResolvedImportFiles[i] != NULL ) {
			pImported = __xlFindModuleByFile((XL_ModuleLinkContext*)pCtx, pModule->arrResolvedImportFiles[i]);
			if ( pImported != NULL ) {
				return pImported;
			}
		}
	}

	return __xlFindModuleByKey(pCtx, sModuleKey);
}

static void __xlReplaceNodeWithIdent(XL_AstNode* pNode, const char* sName)
{
	size_t i;

	if ( (pNode == NULL) || (sName == NULL) ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		xlAstFree(pNode->ppChildren[i]);
	}
	free(pNode->ppChildren);
	pNode->ppChildren = NULL;
	pNode->iChildCount = 0;
	pNode->iChildCapacity = 0;
	free(pNode->sText2);
	pNode->sText2 = NULL;
	pNode->eKind = XL_AST_IDENT;
	pNode->iIntValue = 0;
	pNode->fFloatValue = 0.0;
	pNode->bFlag = 0;
	xlAstSetText(pNode, sName, strlen(sName));
}

static int __xlCollectImports(XL_ModuleInfo* pModule)
{
	const XL_AstNode* pStmt;
	char* sImportKey;
	size_t i;
	int iRc;

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_IMPORT) ) {
			continue;
		}

		sImportKey = __xlBuildImportKey(__xlAstChild(pStmt, 0));
		if ( sImportKey == NULL ) {
			return XL_RC_IO_ERROR;
		}
		iRc = __xlModuleInfoAddImport(pModule, sImportKey);
		free(sImportKey);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	return XL_RC_OK;
}

static const char* __xlModuleMangleKey(const XL_ModuleInfo* pModule, char* sBuffer, size_t iBufferSize)
{
	if ( (sBuffer == NULL) || (iBufferSize == 0) ) {
		return "";
	}

	if ( pModule == NULL ) {
		sBuffer[0] = '\0';
		return sBuffer;
	}

	if ( (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') && !pModule->bIsRoot ) {
		snprintf(sBuffer, iBufferSize, "%s__%s", pModule->sPackageAbiId, (pModule->sModuleKey != NULL) ? pModule->sModuleKey : "module");
	} else {
		snprintf(sBuffer, iBufferSize, "%s", (pModule->sModuleKey != NULL) ? pModule->sModuleKey : "module");
	}

	return sBuffer;
}

static char* __xlBuildMangledName(const char* sModuleKey, const char* sName)
{
	char sPrefix[XL_PATH_MAX_LEN];
	size_t i;
	size_t iLen;
	char* sResult;

	snprintf(sPrefix, sizeof(sPrefix), "xl_mod_%s__", (sModuleKey != NULL) ? sModuleKey : "module");
	for ( i = 0; sPrefix[i] != '\0'; ++i ) {
		if ( !( (sPrefix[i] >= '0') && (sPrefix[i] <= '9') ) &&
			!( (sPrefix[i] >= 'A') && (sPrefix[i] <= 'Z') ) &&
			!( (sPrefix[i] >= 'a') && (sPrefix[i] <= 'z') ) &&
			(sPrefix[i] != '_') ) {
			sPrefix[i] = '_';
		}
	}

	iLen = strlen(sPrefix) + strlen((sName != NULL) ? sName : "") + 1;
	sResult = (char*)malloc(iLen);
	if ( sResult == NULL ) {
		return NULL;
	}

	snprintf(sResult, iLen, "%s%s", sPrefix, (sName != NULL) ? sName : "");
	return sResult;
}

static int __xlCollectTopLevelNamesAndExports(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	XL_AstNode* pStmt;
	const XL_AstNode* pValue;
	char* sMangledName;
	char sMangleKey[XL_PATH_MAX_LEN];
	const char* sResolvedName;
	size_t i;
	size_t j;
	int iRc;

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( pStmt == NULL ) {
			continue;
		}

		if ( (pStmt->eKind == XL_AST_FUNC_DECL) || (pStmt->eKind == XL_AST_CONST_DECL) || (pStmt->eKind == XL_AST_VAR_DECL) ) {
			sMangledName = pModule->bIsRoot ? __xlModuleDupText(pStmt->sText) : __xlBuildMangledName(__xlModuleMangleKey(pModule, sMangleKey, sizeof(sMangleKey)), pStmt->sText);
			if ( sMangledName == NULL ) {
				return XL_RC_IO_ERROR;
			}
			iRc = __xlModuleInfoAddName(pModule, pStmt->sText, sMangledName);
			free(sMangledName);
			if ( iRc != XL_RC_OK ) {
				return iRc;
			}
		}
	}

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_EXPORT) ) {
			continue;
		}

		pValue = __xlAstChild(pStmt, 0);
		if ( (pValue == NULL) || (pValue->eKind != XL_AST_IDENT) || (pValue->sText == NULL) ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "export only supports top-level identifiers in demo5 stage 7");
			return XL_RC_FAIL;
		}

		sResolvedName = __xlFindMangledName(pModule, pValue->sText);
		if ( sResolvedName == NULL ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "export target not found in module '%s': %s", pModule->sModuleKey, pValue->sText);
			return XL_RC_FAIL;
		}

		iRc = __xlModuleInfoAddExport(pModule, pValue->sText, sResolvedName);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	if ( pModule->bIsRoot ) {
		return XL_RC_OK;
	}

	for ( i = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (i < pModule->pRoot->iChildCount); ++i ) {
		pStmt = pModule->pRoot->ppChildren[i];
		if ( pStmt == NULL ) {
			continue;
		}
		if ( (pStmt->eKind != XL_AST_FUNC_DECL) && (pStmt->eKind != XL_AST_CONST_DECL) && (pStmt->eKind != XL_AST_VAR_DECL) ) {
			continue;
		}
		for ( j = 0; j < pModule->iNameCount; ++j ) {
			if ( strcmp(pStmt->sText, pModule->arrNames[j].sOriginalName) == 0 ) {
				xlAstSetText(pStmt, pModule->arrNames[j].sMangledName, strlen(pModule->arrNames[j].sMangledName));
				break;
			}
		}
	}

	return XL_RC_OK;
}

static void __xlRewriteModuleNode(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope);

static void __xlRewriteSequentialChildren(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope)
{
	XL_AstNode* pChild;
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		pChild = pNode->ppChildren[i];
		__xlRewriteModuleNode(pCtx, pModule, pChild, pScope);
		if ( (pScope != NULL) && (pChild != NULL) &&
			((pChild->eKind == XL_AST_CONST_DECL) || (pChild->eKind == XL_AST_VAR_DECL) || (pChild->eKind == XL_AST_FUNC_DECL)) &&
			(pChild->sText != NULL) ) {
			__xlModuleScopeAdd(pScope, pChild->sText);
		}
	}
}

static void __xlRewriteMemberImportRef(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode)
{
	char* sChain;
	char* sDot;
	char* sImportKey;
	XL_ModuleInfo* pImported;
	const char* sMangledName;
	size_t i;

	if ( (pModule == NULL) || (pNode == NULL) ) {
		return;
	}

	sChain = __xlBuildImportKey(pNode);
	if ( sChain == NULL ) {
		return;
	}

	sDot = strrchr(sChain, '.');
	if ( sDot == NULL ) {
		free(sChain);
		return;
	}

	*sDot = '\0';
	sImportKey = sChain;
	if ( !__xlModuleHasImportKey(pModule, sImportKey) ) {
		for ( i = 0; i < pModule->iImportCount; ++i ) {
			pImported = __xlFindImportedModule(pCtx, pModule, pModule->arrImportKeys[i]);
			if ( (pImported != NULL) && (pImported->sLeafAlias != NULL) && (strcmp(pImported->sLeafAlias, sImportKey) == 0) ) {
				sImportKey = pImported->sModuleKey;
				break;
			}
		}
	}

	pImported = __xlFindImportedModule(pCtx, pModule, sImportKey);
	if ( (pImported == NULL) || !__xlModuleHasImportKey(pModule, sImportKey) ) {
		if ( strcmp(sImportKey, sChain) != 0 ) {
			/* alias path already resolved */
		} else {
			free(sChain);
			return;
		}
	}

	sMangledName = __xlFindExportName(pImported, sDot + 1);
	if ( sMangledName != NULL ) {
		__xlReplaceNodeWithIdent(pNode, sMangledName);
	}
	free(sChain);
}

static void __xlRewriteModuleNode(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule, XL_AstNode* pNode, XL_ModuleScope* pScope)
{
	XL_ModuleScope* pFuncScope;
	const XL_AstNode* pParams;
	const XL_AstNode* pParam;
	size_t i;
	const char* sMangledName;

	if ( pNode == NULL ) {
		return;
	}

	switch ( pNode->eKind ) {
		case XL_AST_MEMBER:
			__xlRewriteMemberImportRef(pCtx, pModule, pNode);
			if ( pNode->eKind == XL_AST_IDENT ) {
				return;
			}
			__xlRewriteSequentialChildren(pCtx, pModule, pNode, pScope);
			return;
		case XL_AST_IDENT:
			if ( (pNode->sText != NULL) && !__xlModuleScopeHas(pScope, pNode->sText) ) {
				sMangledName = __xlFindMangledName(pModule, pNode->sText);
				if ( (sMangledName != NULL) && (!pModule->bIsRoot) ) {
					xlAstSetText(pNode, sMangledName, strlen(sMangledName));
				}
			}
			return;
		case XL_AST_FUNC_DECL:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			pParams = __xlAstChild(pNode, 0);
			for ( i = 0; (pParams != NULL) && (i < pParams->iChildCount); ++i ) {
				pParam = pParams->ppChildren[i];
				if ( (pParam != NULL) && (pParam->sText != NULL) ) {
					__xlModuleScopeAdd(pFuncScope, pParam->sText);
				}
			}
			if ( __xlAstChild(pNode, 2) != NULL ) {
				__xlRewriteSequentialChildren(pCtx, pModule, pNode->ppChildren[2], pFuncScope);
			}
			__xlModuleScopeFree(pFuncScope);
			return;
		case XL_AST_BLOCK:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			__xlRewriteSequentialChildren(pCtx, pModule, pNode, pFuncScope);
			__xlModuleScopeFree(pFuncScope);
			return;
		case XL_AST_FORIN:
			pFuncScope = __xlModuleScopePush(pScope);
			if ( pFuncScope == NULL ) {
				return;
			}
			if ( __xlAstChild(pNode, 1) != NULL ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[1], pScope);
			}
			if ( __xlAstChild(pNode, 0) != NULL ) {
				for ( i = 0; i < pNode->ppChildren[0]->iChildCount; ++i ) {
					pParam = pNode->ppChildren[0]->ppChildren[i];
					if ( (pParam != NULL) && (pParam->sText != NULL) ) {
						__xlModuleScopeAdd(pFuncScope, pParam->sText);
					}
				}
			}
			if ( __xlAstChild(pNode, 2) != NULL ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[2], pFuncScope);
			}
			__xlModuleScopeFree(pFuncScope);
			return;
		default:
			for ( i = 0; i < pNode->iChildCount; ++i ) {
				__xlRewriteModuleNode(pCtx, pModule, pNode->ppChildren[i], pScope);
			}
			return;
	}
}

static int __xlLinkAndRewriteModule(XL_ModuleLinkContext* pCtx, XL_ModuleInfo* pModule)
{
	int iRc;

	iRc = __xlCollectTopLevelNamesAndExports(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	__xlRewriteSequentialChildren(pCtx, pModule, pModule->pRoot, NULL);
	return XL_RC_OK;
}

static int __xlParseFileToAst(const char* sFilePath, XL_DiagContext* pDiag, int bTraceParser, XL_AstNode** ppRoot)
{
	XL_Lexer* pLexer;
	XL_ParseResult result;
	int iRc;

	if ( (sFilePath == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppRoot = NULL;
	pLexer = xlLexerCreateFromFile(sFilePath);
	if ( pLexer == NULL ) {
		return XL_RC_IO_ERROR;
	}

	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, pDiag, bTraceParser, &result);
	xlLexerDestroy(pLexer);
	if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
		xlParseResultFree(&result);
		return (iRc != XL_RC_OK) ? iRc : XL_RC_FAIL;
	}

	*ppRoot = result.pRoot;
	result.pRoot = NULL;
	xlParseResultFree(&result);
	return XL_RC_OK;
}

static int __xlParseSourceToAst(const char* sSourceText, const char* sVirtualPath, XL_DiagContext* pDiag, int bTraceParser, XL_AstNode** ppRoot)
{
	XL_Lexer* pLexer;
	XL_ParseResult result;
	int iRc;

	if ( (sSourceText == NULL) || (ppRoot == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	*ppRoot = NULL;
	pLexer = xlLexerCreateFromString(sSourceText, strlen(sSourceText), (sVirtualPath != NULL) ? sVirtualPath : "<native>");
	if ( pLexer == NULL ) {
		return XL_RC_IO_ERROR;
	}

	xlParseResultInit(&result);
	iRc = xlParserParseLexer(pLexer, pDiag, bTraceParser, &result);
	xlLexerDestroy(pLexer);
	if ( (iRc != XL_RC_OK) || (result.pRoot == NULL) ) {
		xlParseResultFree(&result);
		return (iRc != XL_RC_OK) ? iRc : XL_RC_FAIL;
	}

	*ppRoot = result.pRoot;
	result.pRoot = NULL;
	xlParseResultFree(&result);
	return XL_RC_OK;
}

static void __xlSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
{
	size_t iOut;
	const unsigned char* pScan;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	iOut = 0;
	for ( pScan = (const unsigned char*)((sText != NULL) ? sText : ""); (*pScan != 0) && (iOut + 1 < iOutSize); ++pScan ) {
		if ( ((*pScan >= 'A') && (*pScan <= 'Z')) ||
			((*pScan >= 'a') && (*pScan <= 'z')) ||
			((*pScan >= '0') && (*pScan <= '9')) ||
			(*pScan == '_') ) {
			sOut[iOut++] = (char)(*pScan);
		} else {
			sOut[iOut++] = '_';
		}
	}
	sOut[iOut] = '\0';
}

static void __xlBuildXlpNativeVirtualPath(char* sOut, size_t iOutSize, const char* sManifestPath, const char* sModuleKey)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(sOut, iOutSize, "<xlp-native:%s:%s>", (sManifestPath != NULL) ? sManifestPath : "", sModuleKeySafe);
}

static void __xlBuildXlpNativeHelperName(char* sOut, size_t iOutSize, const char* sModuleKey, size_t iFuncIndex)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(sOut, iOutSize, "__xlp_extern__%s__%llu", sModuleKeySafe, (unsigned long long)iFuncIndex);
}

static char* __xlBuildXlpNativeSource(const char* sModuleKey, const XL_XlpFunctionInfo* pFuncs, size_t iFuncCount)
{
	XL_ModuleTextBuffer buf;
	size_t i;
	size_t j;
	int bHasExport;

	memset(&buf, 0, sizeof(buf));
	bHasExport = 0;
	for ( i = 0; i < iFuncCount; ++i ) {
		char sHelperName[256];
		const XL_XlpFunctionInfo* pFunc;

		pFunc = &pFuncs[i];
		if ( pFunc->bAsync || pFunc->bGenerator ) {
			continue;
		}
		if ( (pFunc->sExportName[0] == '\0') || (pFunc->sSymbol[0] == '\0') ) {
			continue;
		}

		__xlBuildXlpNativeHelperName(sHelperName, sizeof(sHelperName), sModuleKey, i);
		__xlModuleBufAppendFormat(&buf, "func %s(", sHelperName);
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppendFormat(&buf, "%s %s", (pFunc->pParams[j].sType[0] != '\0') ? pFunc->pParams[j].sType : "int",
				(pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")");
		if ( (pFunc->sReturnType[0] != '\0') && (strcmp(pFunc->sReturnType, "void") != 0) ) {
			__xlModuleBufAppendFormat(&buf, " as %s", pFunc->sReturnType);
		}
		__xlModuleBufAppend(&buf, "\nend\n");

		__xlModuleBufAppendFormat(&buf, "func %s(", pFunc->sExportName);
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppendFormat(&buf, "%s %s", (pFunc->pParams[j].sType[0] != '\0') ? pFunc->pParams[j].sType : "int",
				(pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")");
		if ( (pFunc->sReturnType[0] != '\0') && (strcmp(pFunc->sReturnType, "void") != 0) ) {
			__xlModuleBufAppendFormat(&buf, " as %s", pFunc->sReturnType);
		}
		__xlModuleBufAppend(&buf, "\n");
		if ( (pFunc->sReturnType[0] == '\0') || (strcmp(pFunc->sReturnType, "void") == 0) ) {
			__xlModuleBufAppend(&buf, "\t");
			__xlModuleBufAppendFormat(&buf, "%s(", sHelperName);
		} else {
			__xlModuleBufAppend(&buf, "\treturn ");
			__xlModuleBufAppendFormat(&buf, "%s(", sHelperName);
		}
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			if ( j != 0 ) {
				__xlModuleBufAppend(&buf, ", ");
			}
			__xlModuleBufAppend(&buf, (pFunc->pParams[j].sName[0] != '\0') ? pFunc->pParams[j].sName : "arg");
		}
		__xlModuleBufAppend(&buf, ")\nend\n");

		if ( (i == 0) || (strcmp(pFunc->sExportName, pFuncs[i - 1].sExportName) != 0) ) {
			__xlModuleBufAppendFormat(&buf, "export %s\n", pFunc->sExportName);
			bHasExport = 1;
		}
	}

	if ( !bHasExport ) {
		free(buf.sData);
		return NULL;
	}
	return buf.sData;
}

static void __xlAnnotateXlpNativeExterns(XL_AstNode* pRoot, const char* sModuleKey, const XL_XlpFunctionInfo* pFuncs, size_t iFuncCount)
{
	size_t i;
	size_t j;

	if ( (pRoot == NULL) || (pFuncs == NULL) ) {
		return;
	}

	for ( i = 0; i < iFuncCount; ++i ) {
		char sHelperName[256];

		if ( pFuncs[i].sSymbol[0] == '\0' ) {
			continue;
		}
		__xlBuildXlpNativeHelperName(sHelperName, sizeof(sHelperName), sModuleKey, i);
		for ( j = 0; j < pRoot->iChildCount; ++j ) {
			XL_AstNode* pStmt;

			pStmt = pRoot->ppChildren[j];
			if ( (pStmt == NULL) || (pStmt->eKind != XL_AST_FUNC_DECL) || (pStmt->sText == NULL) ) {
				continue;
			}
			if ( strcmp(pStmt->sText, sHelperName) == 0 ) {
				xlAstSetText2(pStmt, pFuncs[i].sSymbol, strlen(pFuncs[i].sSymbol));
				break;
			}
		}
	}
}

static int __xlLoadModule(
	XL_ModuleLinkContext* pCtx,
	const char* sFilePath,
	const char* sModuleKey,
	int bIsRoot,
	const char* sImporterManifestPath,
	XL_ModuleInfo** ppModule
)
{
	XL_ModuleInfo* pModule;
	XL_ModuleInfo* pImportedModule;
	XL_NativeModuleInfo nativeInfo;
	char sImportFile[XL_PATH_MAX_LEN];
	char sCurrentModuleFile[XL_PATH_MAX_LEN];
	char sNativeVirtualPath[XL_PATH_MAX_LEN];
	XL_XlpResolveInfo xlpInfo;
	XL_XlpFunctionInfo* pXlpFuncs;
	size_t iXlpFuncCount;
	char* sImportKey;
	size_t i;
	int iRc;

	pModule = __xlFindModuleByFile(pCtx, sFilePath);
	if ( (pModule == NULL) && !bIsRoot && (sModuleKey != NULL) && !__xlFileExists(sFilePath) ) {
		xlXlpResolveInfoInit(&xlpInfo);
		if ( xlXlpFindModuleSource(sFilePath, sImporterManifestPath, sModuleKey, &xlpInfo) == XL_RC_OK ) {
			if ( xlpInfo.bIsNative ) {
				pXlpFuncs = NULL;
				iXlpFuncCount = 0;
				if ( xlXlpReadFunctions(xlpInfo.sManifestPath, sModuleKey, &pXlpFuncs, &iXlpFuncCount) == XL_RC_OK && (iXlpFuncCount != 0) ) {
					__xlBuildXlpNativeVirtualPath(sNativeVirtualPath, sizeof(sNativeVirtualPath), xlpInfo.sManifestPath, sModuleKey);
					pModule = __xlFindModuleByFile(pCtx, sNativeVirtualPath);
				} else {
					const char* sNativeKey;

					sNativeKey = (xlpInfo.sBindingName[0] != '\0') ? xlpInfo.sBindingName : sModuleKey;
					memset(&nativeInfo, 0, sizeof(nativeInfo));
					if ( xlNativeRegistryFind(sNativeKey, &nativeInfo) == XL_RC_OK ) {
						pModule = __xlFindModuleByFile(pCtx, nativeInfo.sVirtualPath);
					}
				}
				xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
			} else {
				pModule = __xlFindModuleByFile(pCtx, xlpInfo.sModuleFile);
			}
		}
	}
	if ( pModule != NULL ) {
		if ( pModule->iState == 1 ) {
			XL_DIAG_ERROR(pCtx->pDiag, "module", "cyclic import detected: %s", sFilePath);
			return XL_RC_FAIL;
		}
		*ppModule = pModule;
		return XL_RC_OK;
	}

	pModule = __xlAddModuleRecord(pCtx);
	if ( pModule == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pModule->sModuleKey = __xlModuleDupText((sModuleKey != NULL) ? sModuleKey : "");
	pModule->bIsRoot = bIsRoot;
	pModule->iState = 1;
	if ( (!bIsRoot) && (sModuleKey != NULL) ) {
		const char* sDot;

		sDot = strrchr(sModuleKey, '.');
		pModule->sLeafAlias = __xlModuleDupText((sDot != NULL) ? (sDot + 1) : sModuleKey);
	}

	memset(&nativeInfo, 0, sizeof(nativeInfo));
	xlXlpResolveInfoInit(&xlpInfo);
	pXlpFuncs = NULL;
	iXlpFuncCount = 0;
	if ( bIsRoot || __xlFileExists(sFilePath) ) {
		pModule->sFilePath = __xlModuleDupText(sFilePath);
		iRc = __xlParseFileToAst(sFilePath, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
	} else if ( (sModuleKey != NULL) &&
		(xlXlpFindModuleSource(sFilePath, sImporterManifestPath, sModuleKey, &xlpInfo) == XL_RC_OK) ) {
		pModule->sManifestPath = __xlModuleDupText(xlpInfo.sManifestPath);
		pModule->sPackageName = __xlModuleDupText(xlpInfo.sPackageName);
		pModule->sPackageVersion = __xlModuleDupText(xlpInfo.sPackageVersion);
		pModule->sPackageAbiVersion = __xlModuleDupText(xlpInfo.sPackageAbiVersion);
		pModule->sPackageAbiId = __xlModuleDupText(xlpInfo.sPackageAbiId);
		pModule->sArtifactKind = __xlModuleDupText(xlpInfo.sArtifactKind);
		pModule->sArtifactPath = __xlModuleDupText(xlpInfo.sArtifactPath);
		pModule->sArtifactFile = __xlModuleDupText(xlpInfo.sArtifactFile);
		pModule->sConsumeMode = __xlModuleDupText(xlpInfo.sConsumeMode);
		if ( (pModule->sManifestPath == NULL) || (pModule->sPackageName == NULL) || (pModule->sPackageVersion == NULL) ||
			(pModule->sPackageAbiVersion == NULL) || (pModule->sPackageAbiId == NULL) ||
			(pModule->sArtifactKind == NULL) || (pModule->sArtifactPath == NULL) ||
			(pModule->sArtifactFile == NULL) || (pModule->sConsumeMode == NULL) ) {
			return XL_RC_IO_ERROR;
		}
		if ( xlpInfo.bIsNative ) {
			if ( (strcmp(xlpInfo.sConsumeMode, "native-registry") != 0) &&
				(strcmp(xlpInfo.sConsumeMode, "shared-bridge") != 0) &&
				(strcmp(xlpInfo.sConsumeMode, "static-bridge") != 0) ) {
				XL_DIAG_ERROR(pCtx->pDiag, "module", "unsupported xlp native consume mode: %s", xlpInfo.sConsumeMode);
				return XL_RC_IO_ERROR;
			}
			pModule->bIsNative = 1;
			pModule->sBindingName = __xlModuleDupText(xlpInfo.sBindingName);
			iRc = xlXlpReadFunctions(xlpInfo.sManifestPath, sModuleKey, &pXlpFuncs, &iXlpFuncCount);
			if ( (iRc == XL_RC_OK) && (iXlpFuncCount != 0) ) {
				char* sNativeSource;

				__xlBuildXlpNativeVirtualPath(sNativeVirtualPath, sizeof(sNativeVirtualPath), xlpInfo.sManifestPath, sModuleKey);
				pModule->sFilePath = __xlModuleDupText(sNativeVirtualPath);
				sNativeSource = __xlBuildXlpNativeSource(sModuleKey, pXlpFuncs, iXlpFuncCount);
				if ( sNativeSource == NULL ) {
					xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
					return XL_RC_IO_ERROR;
				}
				iRc = __xlParseSourceToAst(sNativeSource, sNativeVirtualPath, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
				if ( iRc == XL_RC_OK ) {
					__xlAnnotateXlpNativeExterns(pModule->pRoot, sModuleKey, pXlpFuncs, iXlpFuncCount);
				}
				free(sNativeSource);
			} else {
				const char* sNativeKey;

				sNativeKey = (xlpInfo.sBindingName[0] != '\0') ? xlpInfo.sBindingName : sModuleKey;
				if ( xlNativeRegistryFind(sNativeKey, &nativeInfo) != XL_RC_OK ) {
					xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
					XL_DIAG_ERROR(pCtx->pDiag, "module", "xlp native module not found in registry: %s", sNativeKey);
					return XL_RC_IO_ERROR;
				}
				pModule->sFilePath = __xlModuleDupText(nativeInfo.sVirtualPath);
				free(pModule->sBindingName);
				pModule->sBindingName = __xlModuleDupText(nativeInfo.sBindingName);
				iRc = __xlParseSourceToAst(nativeInfo.sSourceText, nativeInfo.sVirtualPath, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
			}
			xlXlpFunctionInfoFree(pXlpFuncs, iXlpFuncCount);
		} else {
			pModule->sFilePath = __xlModuleDupText(xlpInfo.sModuleFile);
			pModule->sBindingName = __xlModuleDupText(xlpInfo.sBindingName);
			iRc = __xlParseFileToAst(xlpInfo.sModuleFile, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
		}
	} else if ( (sModuleKey != NULL) && (xlNativeRegistryFind(sModuleKey, &nativeInfo) == XL_RC_OK) ) {
		pModule->bIsNative = 1;
		pModule->sFilePath = __xlModuleDupText(nativeInfo.sVirtualPath);
		pModule->sBindingName = __xlModuleDupText(nativeInfo.sBindingName);
		iRc = __xlParseSourceToAst(nativeInfo.sSourceText, nativeInfo.sVirtualPath, pCtx->pDiag, pCtx->bTraceParser, &pModule->pRoot);
	} else {
		XL_DIAG_ERROR(pCtx->pDiag, "module", "module not found: %s", (sModuleKey != NULL) ? sModuleKey : sFilePath);
		return XL_RC_IO_ERROR;
	}
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}
	snprintf(sCurrentModuleFile, sizeof(sCurrentModuleFile), "%s", (pModule->sFilePath != NULL) ? pModule->sFilePath : "");
	if ( pCtx->bTraceParser && (pCtx->pDiag != NULL) ) {
		__xlModuleTrace(
			pCtx->pDiag,
			"load module key='%s' file='%s' native=%d binding='%s' artifactKind='%s' consumeMode='%s'",
			pModule->sModuleKey,
			pModule->sFilePath,
			pModule->bIsNative,
			(pModule->sBindingName != NULL) ? pModule->sBindingName : "",
			(pModule->sArtifactKind != NULL) ? pModule->sArtifactKind : "",
			(pModule->sConsumeMode != NULL) ? pModule->sConsumeMode : ""
		);
	}

	iRc = __xlCollectImports(pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		sImportKey = pModule->arrImportKeys[i];
		iRc = __xlResolveImportFile(sImportFile, sizeof(sImportFile), pModule->sFilePath, sImportKey);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pImportedModule = NULL;
		iRc = __xlLoadModule(pCtx, sImportFile, sImportKey, 0, pModule->sManifestPath, &pImportedModule);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
		pModule = __xlFindModuleByFile(pCtx, sCurrentModuleFile);
		if ( pModule == NULL ) {
			return XL_RC_FAIL;
		}
		iRc = __xlModuleInfoResolveImport(pModule, i, (pImportedModule != NULL) ? pImportedModule->sFilePath : NULL);
		if ( iRc != XL_RC_OK ) {
			return iRc;
		}
	}

	iRc = __xlLinkAndRewriteModule(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	pModule->iState = 2;
	iRc = __xlAppendOrder(pCtx, pModule);
	if ( iRc != XL_RC_OK ) {
		return iRc;
	}

	*ppModule = pModule;
	return XL_RC_OK;
}

static void __xlDetachRootChildren(XL_AstNode* pRoot)
{
	if ( pRoot == NULL ) {
		return;
	}

	free(pRoot->ppChildren);
	pRoot->ppChildren = NULL;
	pRoot->iChildCount = 0;
	pRoot->iChildCapacity = 0;
}

static int __xlBuildLinkedRoot(XL_ModuleLinkContext* pCtx, XL_AstNode** ppRoot)
{
	XL_AstNode* pProgram;
	XL_ModuleInfo* pModule;
	XL_AstNode* pStmt;
	size_t i;
	size_t j;

	pProgram = xlAstCreateProgram();
	if ( pProgram == NULL ) {
		return XL_RC_IO_ERROR;
	}

	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		for ( j = 0; (pModule != NULL) && (pModule->pRoot != NULL) && (j < pModule->pRoot->iChildCount); ++j ) {
			pStmt = pModule->pRoot->ppChildren[j];
			if ( (pStmt == NULL) || (pStmt->eKind == XL_AST_IMPORT) || (pStmt->eKind == XL_AST_EXPORT) ) {
				pModule->pRoot->ppChildren[j] = NULL;
				continue;
			}
			xlAstAppendChild(pProgram, pStmt);
			pModule->pRoot->ppChildren[j] = NULL;
		}
	}

	*ppRoot = pProgram;
	return XL_RC_OK;
}

static void __xlBuildModuleTextDump(const XL_ModuleLinkContext* pCtx, char** psText)
{
	XL_ModuleTextBuffer buf;
	XL_ModuleInfo* pModule;
	char* sNativeRegistryText;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlModuleBufAppend(&buf, "ModuleLoadOrder\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlModuleBufAppendFormat(
			&buf,
			"- [%zu] %s -> %s native=%d binding=%s\n",
			i,
			pModule->sModuleKey,
			pModule->sFilePath,
			pModule->bIsNative,
			(pModule->sBindingName != NULL) ? pModule->sBindingName : ""
		);
	}
	__xlModuleBufAppend(&buf, "ModuleExports\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlModuleBufAppendFormat(&buf, "- module=%s\n", pModule->sModuleKey);
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			__xlModuleBufAppendFormat(&buf, "  export %s -> %s\n", pModule->arrExports[j].sName, pModule->arrExports[j].sMangledName);
		}
	}
	sNativeRegistryText = xlNativeRegistryDumpText();
	if ( sNativeRegistryText != NULL ) {
		__xlModuleBufAppend(&buf, sNativeRegistryText);
		free(sNativeRegistryText);
	}
	*psText = buf.sData;
}

static void __xlBuildModuleJsonDump(const XL_ModuleLinkContext* pCtx, char** psJson)
{
	XL_ModuleTextBuffer buf;
	XL_ModuleInfo* pModule;
	char* sNativeRegistryJson;
	size_t i;
	size_t j;

	memset(&buf, 0, sizeof(buf));
	__xlModuleBufAppend(&buf, "{\n\t\"modules\": [\n");
	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		__xlModuleBufAppend(&buf, "\t\t{\n");
		__xlModuleBufAppend(&buf, "\t\t\t\"key\": \"");
		__xlModuleBufAppendEscaped(&buf, pModule->sModuleKey);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"file\": \"");
		__xlModuleBufAppendEscaped(&buf, pModule->sFilePath);
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"native\": ");
		__xlModuleBufAppend(&buf, pModule->bIsNative ? "1" : "0");
		__xlModuleBufAppend(&buf, ",\n\t\t\t\"binding\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sBindingName != NULL) ? pModule->sBindingName : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactKind\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactKind != NULL) ? pModule->sArtifactKind : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactPath\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactPath != NULL) ? pModule->sArtifactPath : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"artifactFile\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sArtifactFile != NULL) ? pModule->sArtifactFile : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"consumeMode\": \"");
		__xlModuleBufAppendEscaped(&buf, (pModule->sConsumeMode != NULL) ? pModule->sConsumeMode : "");
		__xlModuleBufAppend(&buf, "\",\n\t\t\t\"exports\": [");
		if ( pModule->iExportCount != 0 ) {
			__xlModuleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			__xlModuleBufAppend(&buf, "\t\t\t\t{\n\t\t\t\t\t\"name\": \"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrExports[j].sName);
			__xlModuleBufAppend(&buf, "\",\n\t\t\t\t\t\"symbol\": \"");
			__xlModuleBufAppendEscaped(&buf, pModule->arrExports[j].sMangledName);
			__xlModuleBufAppend(&buf, "\"\n\t\t\t\t}");
			if ( (j + 1) != pModule->iExportCount ) {
				__xlModuleBufAppend(&buf, ",");
			}
			__xlModuleBufAppend(&buf, "\n");
		}
		if ( pModule->iExportCount != 0 ) {
			__xlModuleBufAppend(&buf, "\t\t\t");
		}
		__xlModuleBufAppend(&buf, "]\n\t\t}");
		if ( (i + 1) != pCtx->iOrderCount ) {
			__xlModuleBufAppend(&buf, ",");
		}
		__xlModuleBufAppend(&buf, "\n");
	}
	__xlModuleBufAppend(&buf, "\t]");
	sNativeRegistryJson = xlNativeRegistryDumpJson();
	if ( sNativeRegistryJson != NULL ) {
		__xlModuleBufAppend(&buf, ",\n\t\"registry\": ");
		__xlModuleBufAppend(&buf, sNativeRegistryJson);
		free(sNativeRegistryJson);
		__xlModuleBufAppend(&buf, "\n");
	} else {
		__xlModuleBufAppend(&buf, "\n");
	}
	__xlModuleBufAppend(&buf, "}\n");
	*psJson = buf.sData;
}

static int __xlCopyModuleSummaries(const XL_ModuleLinkContext* pCtx, XL_ModuleLinkResult* pResult)
{
	size_t i;
	size_t j;

	if ( (pCtx == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}
	if ( pCtx->iOrderCount == 0 ) {
		return XL_RC_OK;
	}

	pResult->pModules = (XL_ModuleSummary*)calloc(pCtx->iOrderCount, sizeof(*pResult->pModules));
	if ( pResult->pModules == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pResult->iModuleCount = pCtx->iOrderCount;

	for ( i = 0; i < pCtx->iOrderCount; ++i ) {
		XL_ModuleInfo* pModule;
		XL_ModuleSummary* pSummary;

		pModule = &pCtx->pModules[pCtx->pOrderIndices[i]];
		pSummary = &pResult->pModules[i];
		pSummary->sKey = __xlModuleDupText((pModule != NULL) ? pModule->sModuleKey : "");
		pSummary->sFilePath = __xlModuleDupText((pModule != NULL) ? pModule->sFilePath : "");
		pSummary->sBindingName = __xlModuleDupText((pModule != NULL) ? pModule->sBindingName : "");
		pSummary->sManifestPath = __xlModuleDupText((pModule != NULL) ? pModule->sManifestPath : "");
		pSummary->sPackageName = __xlModuleDupText((pModule != NULL) ? pModule->sPackageName : "");
		pSummary->sPackageVersion = __xlModuleDupText((pModule != NULL) ? pModule->sPackageVersion : "");
		pSummary->sPackageAbiVersion = __xlModuleDupText((pModule != NULL) ? pModule->sPackageAbiVersion : "");
		pSummary->sPackageAbiId = __xlModuleDupText((pModule != NULL) ? pModule->sPackageAbiId : "");
		pSummary->sArtifactKind = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactKind : "");
		pSummary->sArtifactPath = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactPath : "");
		pSummary->sArtifactFile = __xlModuleDupText((pModule != NULL) ? pModule->sArtifactFile : "");
		pSummary->sConsumeMode = __xlModuleDupText((pModule != NULL) ? pModule->sConsumeMode : "");
		pSummary->bIsRoot = (pModule != NULL) ? pModule->bIsRoot : 0;
		pSummary->bIsNative = (pModule != NULL) ? pModule->bIsNative : 0;
		pSummary->iImportCount = (pModule != NULL) ? pModule->iImportCount : 0;
		pSummary->iExportCount = (pModule != NULL) ? pModule->iExportCount : 0;
		if ( (pSummary->sKey == NULL) || (pSummary->sFilePath == NULL) || (pSummary->sBindingName == NULL) ||
			(pSummary->sManifestPath == NULL) || (pSummary->sPackageName == NULL) || (pSummary->sPackageVersion == NULL) ||
			(pSummary->sPackageAbiVersion == NULL) || (pSummary->sPackageAbiId == NULL) ||
			(pSummary->sArtifactKind == NULL) || (pSummary->sArtifactPath == NULL) ||
			(pSummary->sArtifactFile == NULL) || (pSummary->sConsumeMode == NULL) ) {
			return XL_RC_IO_ERROR;
		}
		if ( pSummary->iImportCount != 0 ) {
			pSummary->arrImports = (char**)calloc(pSummary->iImportCount, sizeof(*pSummary->arrImports));
			pSummary->arrResolvedImports = (char**)calloc(pSummary->iImportCount, sizeof(*pSummary->arrResolvedImports));
			if ( (pSummary->arrImports == NULL) || (pSummary->arrResolvedImports == NULL) ) {
				return XL_RC_IO_ERROR;
			}
			for ( j = 0; j < pSummary->iImportCount; ++j ) {
				pSummary->arrImports[j] = __xlModuleDupText(pModule->arrImportKeys[j]);
				pSummary->arrResolvedImports[j] = __xlModuleDupText(pModule->arrResolvedImportFiles[j]);
				if ( (pSummary->arrImports[j] == NULL) || ((pModule->arrResolvedImportFiles[j] != NULL) && (pSummary->arrResolvedImports[j] == NULL)) ) {
					return XL_RC_IO_ERROR;
				}
			}
		}
		if ( pSummary->iExportCount == 0 ) {
			continue;
		}

		pSummary->pExports = (XL_ModuleExportSummary*)calloc(pSummary->iExportCount, sizeof(*pSummary->pExports));
		if ( pSummary->pExports == NULL ) {
			return XL_RC_IO_ERROR;
		}

		for ( j = 0; j < pSummary->iExportCount; ++j ) {
			pSummary->pExports[j].sName = __xlModuleDupText(pModule->arrExports[j].sName);
			pSummary->pExports[j].sSymbol = __xlModuleDupText(pModule->arrExports[j].sMangledName);
			if ( (pSummary->pExports[j].sName == NULL) || (pSummary->pExports[j].sSymbol == NULL) ) {
				return XL_RC_IO_ERROR;
			}
		}
	}

	return XL_RC_OK;
}

void xlModuleLinkResultInit(XL_ModuleLinkResult* pResult)
{
	if ( pResult != NULL ) {
		memset(pResult, 0, sizeof(*pResult));
	}
}

void xlModuleLinkResultFree(XL_ModuleLinkResult* pResult)
{
	size_t i;
	size_t j;

	if ( pResult == NULL ) {
		return;
	}

	xlAstFree(pResult->pLinkedRoot);
	free(pResult->sTextDump);
	free(pResult->sJsonDump);
	for ( i = 0; i < pResult->iModuleCount; ++i ) {
		for ( j = 0; j < pResult->pModules[i].iImportCount; ++j ) {
			free(pResult->pModules[i].arrImports[j]);
			free(pResult->pModules[i].arrResolvedImports[j]);
		}
		free(pResult->pModules[i].arrImports);
		free(pResult->pModules[i].arrResolvedImports);
		for ( j = 0; j < pResult->pModules[i].iExportCount; ++j ) {
			free(pResult->pModules[i].pExports[j].sName);
			free(pResult->pModules[i].pExports[j].sSymbol);
		}
		free(pResult->pModules[i].pExports);
		free(pResult->pModules[i].sKey);
		free(pResult->pModules[i].sFilePath);
		free(pResult->pModules[i].sBindingName);
		free(pResult->pModules[i].sManifestPath);
		free(pResult->pModules[i].sPackageName);
		free(pResult->pModules[i].sPackageVersion);
		free(pResult->pModules[i].sPackageAbiVersion);
		free(pResult->pModules[i].sPackageAbiId);
		free(pResult->pModules[i].sArtifactKind);
		free(pResult->pModules[i].sArtifactPath);
		free(pResult->pModules[i].sArtifactFile);
		free(pResult->pModules[i].sConsumeMode);
	}
	free(pResult->pModules);
	memset(pResult, 0, sizeof(*pResult));
}

int xlModuleLinkFromFile(const char* sInputFile, XL_DiagContext* pDiag, int bTraceParser, XL_ModuleLinkResult* pResult)
{
	XL_ModuleLinkContext ctx;
	XL_ModuleInfo* pRootModule;
	char sFullPath[XL_PATH_MAX_LEN];
	size_t i;
	int iRc;

	if ( (sInputFile == NULL) || (pResult == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	xlModuleLinkResultInit(pResult);
	memset(&ctx, 0, sizeof(ctx));
	ctx.pDiag = pDiag;
	ctx.bTraceParser = bTraceParser;

	__xlModuleNormalizePath(sFullPath, sizeof(sFullPath), sInputFile);
	iRc = __xlLoadModule(&ctx, sFullPath, "<root>", 1, NULL, &pRootModule);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	iRc = __xlBuildLinkedRoot(&ctx, &pResult->pLinkedRoot);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}

	__xlBuildModuleTextDump(&ctx, &pResult->sTextDump);
	__xlBuildModuleJsonDump(&ctx, &pResult->sJsonDump);
	iRc = __xlCopyModuleSummaries(&ctx, pResult);
	if ( iRc != XL_RC_OK ) {
		goto cleanup;
	}
	iRc = XL_RC_OK;

cleanup:
	for ( i = 0; i < ctx.iModuleCount; ++i ) {
		__xlDetachRootChildren(ctx.pModules[i].pRoot);
		__xlModuleInfoFree(&ctx.pModules[i]);
	}
	free(ctx.pModules);
	free(ctx.pOrderIndices);
	if ( iRc != XL_RC_OK ) {
		xlModuleLinkResultFree(pResult);
	}
	return iRc;
}
