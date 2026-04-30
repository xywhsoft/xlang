#include "xl_xlp_dump.h"
#include "../ast/xl_ast_dump.h"
#include "xl_xlp_load.h"

#include "../sem/xl_func.h"

typedef struct XL_XlpTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_XlpTextBuffer;

static int __xlXlpBufGrow(XL_XlpTextBuffer* pBuf, size_t iNeed)
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

static void __xlXlpBufAppend(XL_XlpTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlXlpBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlXlpBufAppendFormat(XL_XlpTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlXlpBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlXlpBufFree(XL_XlpTextBuffer* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	free(pBuf->sData);
	pBuf->sData = NULL;
	pBuf->iLen = 0;
	pBuf->iCap = 0;
}

static void __xlXlpAppendEscaped(XL_XlpTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlXlpBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlXlpBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlXlpBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlXlpBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlXlpBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlXlpBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlXlpBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static const char* __xlXlpFileStem(const char* sInputFile, char* sOut, size_t iOutSize)
{
	const char* sName;
	const char* sScan;
	const char* sDot;
	size_t iLen;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return "";
	}

	sName = (sInputFile != NULL) ? sInputFile : "package";
	sScan = sName;
	while ( *sScan != '\0' ) {
		if ( (*sScan == '\\') || (*sScan == '/') ) {
			sName = sScan + 1;
		}
		++sScan;
	}

	sDot = strrchr(sName, '.');
	iLen = (sDot != NULL) ? (size_t)(sDot - sName) : strlen(sName);
	if ( iLen >= iOutSize ) {
		iLen = iOutSize - 1;
	}

	memcpy(sOut, sName, iLen);
	sOut[iLen] = '\0';
	return sOut;
}

static void __xlXlpBuildPackageAbiId(
	const char* sPackageName,
	const char* sPackageVersion,
	const char* sPackageAbiVersion,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s@%s#abi%s",
		((sPackageName != NULL) && (sPackageName[0] != '\0')) ? sPackageName : "package",
		((sPackageVersion != NULL) && (sPackageVersion[0] != '\0')) ? sPackageVersion : "0.0.0",
		((sPackageAbiVersion != NULL) && (sPackageAbiVersion[0] != '\0')) ? sPackageAbiVersion : "1"
	);
}

static void __xlXlpBuildDependencyAbiId(const XL_XlpDependencyInfo* pDep, char* sOut, size_t iOutSize)
{
	const char* sName;
	const char* sVersion;
	const char* sAbiVersion;

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}
	if ( (pDep != NULL) && (pDep->sAbiId[0] != '\0') ) {
		snprintf(sOut, iOutSize, "%s", pDep->sAbiId);
		return;
	}

	sName = ((pDep != NULL) && (pDep->sName[0] != '\0')) ? pDep->sName : "package";
	sVersion = ((pDep != NULL) && (pDep->sVersion[0] != '\0')) ? pDep->sVersion : "0.0.0";
	sAbiVersion = ((pDep != NULL) && (pDep->sAbiVersion[0] != '\0')) ? pDep->sAbiVersion : "1";
	snprintf(sOut, iOutSize, "%s@%s#abi%s", sName, sVersion, sAbiVersion);
}

static void __xlXlpBuildModuleAbiId(const char* sPackageAbiId, const char* sModuleKey, char* sOut, size_t iOutSize)
{
	snprintf(sOut, iOutSize, "%s::%s", (sPackageAbiId != NULL) ? sPackageAbiId : "package@0.0.0#abi1", (sModuleKey != NULL) ? sModuleKey : "<root>");
}

static void __xlXlpBuildFunctionLanguageAbiId(
	const char* sModuleAbiId,
	const char* sSignatureId,
	const char* sLanguageAbi,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s::%s#%s",
		(sModuleAbiId != NULL) ? sModuleAbiId : "package@0.0.0#abi1::<root>",
		(sSignatureId != NULL) ? sSignatureId : "",
		(sLanguageAbi != NULL) ? sLanguageAbi : "xlang@v1"
	);
}

static void __xlXlpBuildFunctionNativeAbiId(
	const char* sModuleAbiId,
	const char* sAbiSymbol,
	const char* sNativeAbi,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s::%s#%s",
		(sModuleAbiId != NULL) ? sModuleAbiId : "package@0.0.0#abi1::<root>",
		(sAbiSymbol != NULL) ? sAbiSymbol : "",
		(sNativeAbi != NULL) ? sNativeAbi : "c@v1"
	);
}

static void __xlXlpBuildExportAbiId(
	const char* sModuleAbiId,
	const char* sExportName,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s::%s",
		(sModuleAbiId != NULL) ? sModuleAbiId : "package@0.0.0#abi1::<root>",
		(sExportName != NULL) ? sExportName : ""
	);
}

static void __xlXlpBuildExportVersionedSymbol(
	const char* sModuleAbiId,
	const char* sSymbol,
	char* sOut,
	size_t iOutSize
)
{
	snprintf(
		sOut,
		iOutSize,
		"%s::%s",
		(sModuleAbiId != NULL) ? sModuleAbiId : "package@0.0.0#abi1::<root>",
		(sSymbol != NULL) ? sSymbol : ""
	);
}

static void __xlXlpSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
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

static void __xlXlpBuildModuleAbiSymbol(
	const char* sModuleKey,
	const char* sSuffix,
	char* sOut,
	size_t iOutSize
)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlXlpSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(
		sOut,
		iOutSize,
		"xl_mod_%s__%s",
		(sModuleKeySafe[0] != '\0') ? sModuleKeySafe : "module",
		(sSuffix != NULL) ? sSuffix : ""
	);
}

static int __xlXlpModuleKeyHasCollision(const XL_ModuleLinkResult* pModuleResult, const XL_ModuleSummary* pTarget)
{
	size_t i;
	size_t iMatchCount;
	const char* sTargetKey;

	if ( (pModuleResult == NULL) || (pModuleResult->pModules == NULL) || (pTarget == NULL) ) {
		return 0;
	}

	sTargetKey = ((pTarget->sKey != NULL) && (pTarget->sKey[0] != '\0')) ? pTarget->sKey : "<root>";
	iMatchCount = 0;
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		const XL_ModuleSummary* pModule;
		const char* sModuleKey;

		pModule = &pModuleResult->pModules[i];
		sModuleKey = ((pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "<root>";
		if ( strcmp(sModuleKey, sTargetKey) != 0 ) {
			continue;
		}
		++iMatchCount;
		if ( iMatchCount > 1u ) {
			return 1;
		}
	}

	return 0;
}

static void __xlXlpBuildUniqueModuleAbiSymbol(const XL_ModuleSummary* pModule, const char* sSuffix, char* sOut, size_t iOutSize)
{
	char sUniqueKey[XL_PATH_MAX_LEN];
	const char* sModuleKey;

	if ( (pModule != NULL) &&
		!pModule->bIsRoot &&
		(pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		sModuleKey = ((pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "module";
		snprintf(sUniqueKey, sizeof(sUniqueKey), "%s__%s", pModule->sPackageAbiId, sModuleKey);
		__xlXlpBuildModuleAbiSymbol(sUniqueKey, sSuffix, sOut, iOutSize);
		return;
	}

	__xlXlpBuildModuleAbiSymbol((pModule != NULL) ? pModule->sKey : NULL, sSuffix, sOut, iOutSize);
}

static void __xlXlpGetEffectiveModuleAbiSymbol(
	const XL_ModuleLinkResult* pModuleResult,
	const XL_ModuleSummary* pModule,
	const char* sExistingSymbol,
	const char* sSuffix,
	char* sOut,
	size_t iOutSize
)
{
	char sDefaultSymbol[XL_PATH_MAX_LEN];

	if ( (sOut == NULL) || (iOutSize == 0) ) {
		return;
	}

	__xlXlpBuildModuleAbiSymbol((pModule != NULL) ? pModule->sKey : NULL, sSuffix, sDefaultSymbol, sizeof(sDefaultSymbol));
	if ( __xlXlpModuleKeyHasCollision(pModuleResult, pModule) &&
		( ((sExistingSymbol == NULL) || (sExistingSymbol[0] == '\0')) || (strcmp(sExistingSymbol, sDefaultSymbol) == 0) ) ) {
		__xlXlpBuildUniqueModuleAbiSymbol(pModule, sSuffix, sOut, iOutSize);
		return;
	}

	if ( (sExistingSymbol != NULL) && (sExistingSymbol[0] != '\0') ) {
		snprintf(sOut, iOutSize, "%s", sExistingSymbol);
		return;
	}

	snprintf(sOut, iOutSize, "%s", sDefaultSymbol);
}

static const char* __xlXlpModulePackageName(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageName != NULL) && (pModule->sPackageName[0] != '\0') ) {
		return pModule->sPackageName;
	}
	return (pModule != NULL) ? pModule->sKey : "";
}

static const char* __xlXlpModulePackageVersion(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageVersion != NULL) && (pModule->sPackageVersion[0] != '\0') ) {
		return pModule->sPackageVersion;
	}
	return "0.0.0";
}

static const char* __xlXlpModulePackageAbiVersion(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageAbiVersion != NULL) && (pModule->sPackageAbiVersion[0] != '\0') ) {
		return pModule->sPackageAbiVersion;
	}
	return "1";
}

static const char* __xlXlpModulePackageAbiId(const XL_ModuleSummary* pModule, char* sFallback, size_t iFallbackSize)
{
	if ( (pModule != NULL) && (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		return pModule->sPackageAbiId;
	}
	__xlXlpBuildPackageAbiId(
		__xlXlpModulePackageName(pModule),
		__xlXlpModulePackageVersion(pModule),
		__xlXlpModulePackageAbiVersion(pModule),
		sFallback,
		iFallbackSize
	);
	return sFallback;
}

static const XL_ModuleSummary* __xlXlpFindRootModule(const XL_ModuleLinkResult* pModuleResult)
{
	size_t i;

	if ( pModuleResult == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pModuleResult->iModuleCount; ++i ) {
		if ( pModuleResult->pModules[i].bIsRoot ) {
			return &pModuleResult->pModules[i];
		}
	}
	return (pModuleResult->iModuleCount != 0) ? &pModuleResult->pModules[0] : NULL;
}

static int __xlXlpIsSameDependency(const XL_ModuleSummary* pLeft, const XL_ModuleSummary* pRight)
{
	char sLeftAbiId[XL_PATH_MAX_LEN];
	char sRightAbiId[XL_PATH_MAX_LEN];

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return 0;
	}
	return strcmp(
		__xlXlpModulePackageAbiId(pLeft, sLeftAbiId, sizeof(sLeftAbiId)),
		__xlXlpModulePackageAbiId(pRight, sRightAbiId, sizeof(sRightAbiId))
	) == 0;
}

static int __xlXlpShouldEmitDependency(const XL_ModuleLinkResult* pModuleResult, size_t iIndex)
{
	size_t i;
	const XL_ModuleSummary* pModule;

	if ( (pModuleResult == NULL) || (iIndex >= pModuleResult->iModuleCount) ) {
		return 0;
	}

	pModule = &pModuleResult->pModules[iIndex];
	if ( pModule->bIsRoot ) {
		return 0;
	}
	if ( (!pModule->bIsNative) && ((pModule->sPackageAbiId == NULL) || (pModule->sPackageAbiId[0] == '\0')) ) {
		return 0;
	}

	for ( i = 0; i < iIndex; ++i ) {
		if ( __xlXlpIsSameDependency(&pModuleResult->pModules[i], pModule) ) {
			return 0;
		}
	}
	return 1;
}

static int __xlXlpModuleImportsKey(const XL_ModuleSummary* pModule, const char* sImportKey)
{
	size_t i;

	if ( (pModule == NULL) || (sImportKey == NULL) ) {
		return 0;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( (pModule->arrImports[i] != NULL) && (strcmp(pModule->arrImports[i], sImportKey) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlXlpModuleImportsResolvedFile(const XL_ModuleSummary* pModule, const char* sResolvedFile)
{
	size_t i;

	if ( (pModule == NULL) || (sResolvedFile == NULL) || (sResolvedFile[0] == '\0') ) {
		return 0;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( (pModule->arrResolvedImports != NULL) &&
			(pModule->arrResolvedImports[i] != NULL) &&
			(strcmp(pModule->arrResolvedImports[i], sResolvedFile) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xlXlpModuleHasResolvedImports(const XL_ModuleSummary* pModule)
{
	size_t i;

	if ( (pModule == NULL) || (pModule->arrResolvedImports == NULL) ) {
		return 0;
	}

	for ( i = 0; i < pModule->iImportCount; ++i ) {
		if ( (pModule->arrResolvedImports[i] != NULL) && (pModule->arrResolvedImports[i][0] != '\0') ) {
			return 1;
		}
	}
	return 0;
}

static int __xlXlpModuleUsesDependency(const XL_ModuleSummary* pUserModule, const XL_ModuleSummary* pDepModule)
{
	char sVersion[64];

	if ( (pUserModule == NULL) || (pDepModule == NULL) ) {
		return 0;
	}

	if ( (pUserModule->sManifestPath != NULL) && (pUserModule->sManifestPath[0] != '\0') &&
		xlXlpFindDependencyVersion(pUserModule->sManifestPath, __xlXlpModulePackageName(pDepModule), sVersion, sizeof(sVersion)) == XL_RC_OK ) {
		return strcmp(sVersion, __xlXlpModulePackageVersion(pDepModule)) == 0;
	}

	if ( __xlXlpModuleImportsResolvedFile(pUserModule, pDepModule->sFilePath) ) {
		return 1;
	}
	if ( !__xlXlpModuleHasResolvedImports(pUserModule) ) {
		return __xlXlpModuleImportsKey(pUserModule, pDepModule->sKey);
	}
	return 0;
}

static size_t __xlXlpCountExportFuncs(const XL_SemResult* pSemResult, const char* sSymbol)
{
	size_t i;
	size_t iCount;
	const XL_FuncInfo* pFunc;

	iCount = 0;
	if ( (pSemResult == NULL) || (sSymbol == NULL) ) {
		return 0;
	}

	for ( i = 0; i < pSemResult->funcs.iCount; ++i ) {
		pFunc = xlFuncTableGet(&pSemResult->funcs, i);
		if ( (pFunc != NULL) && (pFunc->sName != NULL) && (strcmp(pFunc->sName, sSymbol) == 0) ) {
			iCount += 1;
		}
	}

	return iCount;
}

static int __xlXlpAppendDefaultExprText(XL_XlpTextBuffer* pBuf, const XL_AstNode* pNode)
{
	if ( (pBuf == NULL) || (pNode == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	switch ( pNode->eKind ) {
		case XL_AST_INT:
		case XL_AST_FLOAT:
		case XL_AST_BOOL:
		case XL_AST_NULL:
		case XL_AST_IDENT:
		case XL_AST_TIME:
			__xlXlpBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			return XL_RC_OK;
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
			__xlXlpBufAppend(pBuf, "\"");
			__xlXlpAppendEscaped(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			__xlXlpBufAppend(pBuf, "\"");
			return XL_RC_OK;
		case XL_AST_UNARY:
			__xlXlpBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			return __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL);
		case XL_AST_BINARY:
			__xlXlpBufAppend(pBuf, "(");
			if ( __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXlpBufAppend(pBuf, " ");
			__xlXlpBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			__xlXlpBufAppend(pBuf, " ");
			if ( __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXlpBufAppend(pBuf, ")");
			return XL_RC_OK;
		case XL_AST_TERNARY:
			__xlXlpBufAppend(pBuf, "(");
			if ( __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXlpBufAppend(pBuf, " ? ");
			if ( __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXlpBufAppend(pBuf, " : ");
			if ( __xlXlpAppendDefaultExprText(pBuf, (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXlpBufAppend(pBuf, ")");
			return XL_RC_OK;
		default:
			return XL_RC_FAIL;
	}
}

static char* __xlXlpBuildDefaultExprText(const XL_AstNode* pNode)
{
	XL_XlpTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	if ( __xlXlpAppendDefaultExprText(&buf, pNode) != XL_RC_OK ) {
		free(buf.sData);
		return NULL;
	}
	return buf.sData;
}

static void __xlXlpAppendParamsJson(XL_XlpTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXlpBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iParamCount != 0) ) {
		__xlXlpBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iParamCount); ++j ) {
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"name\": \"");
		__xlXlpAppendEscaped(pBuf, pFunc->arrParamNames[j]);
		__xlXlpBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"label\": \"");
		__xlXlpAppendEscaped(pBuf, pFunc->arrParamNames[j]);
		__xlXlpBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"type\": \"");
		__xlXlpAppendEscaped(pBuf, pFunc->arrParamTypes[j]);
		__xlXlpBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppendFormat(pBuf, "\"hasDefault\": %d\n", ((pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[j]) ? 1 : 0);
		if ( (pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[j] ) {
			char* sDefaultText;
			char* sDefaultJson;

			sDefaultText = ((pFunc->arrParamDefaultValues != NULL) && (pFunc->arrParamDefaultValues[j] != NULL)) ?
				__xlXlpBuildDefaultExprText(pFunc->arrParamDefaultValues[j]) : NULL;
			sDefaultJson = ((pFunc->arrParamDefaultValues != NULL) && (pFunc->arrParamDefaultValues[j] != NULL)) ?
				xlAstDumpJson(pFunc->arrParamDefaultValues[j]) : NULL;
			__xlXlpBufAppend(pBuf, ",\n");
			for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
				__xlXlpBufAppend(pBuf, "\t");
			}
			__xlXlpBufAppend(pBuf, "\"defaultValueText\": \"");
			__xlXlpAppendEscaped(pBuf, (sDefaultText != NULL) ? sDefaultText : "");
			__xlXlpBufAppend(pBuf, "\",\n");
			for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
				__xlXlpBufAppend(pBuf, "\t");
			}
			__xlXlpBufAppend(pBuf, "\"defaultValueJson\": \"");
			__xlXlpAppendEscaped(pBuf, (sDefaultJson != NULL) ? sDefaultJson : "");
			__xlXlpBufAppend(pBuf, "\"\n");
			free(sDefaultText);
			free(sDefaultJson);
		}
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iParamCount ) {
			__xlXlpBufAppend(pBuf, ",");
		}
		__xlXlpBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iParamCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
	}
	__xlXlpBufAppend(pBuf, "]");
}

static void __xlXlpAppendTypeParamsJson(XL_XlpTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXlpBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iTypeParamCount != 0) ) {
		__xlXlpBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iTypeParamCount); ++j ) {
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"name\": \"");
		__xlXlpAppendEscaped(pBuf, ((pFunc->arrTypeParamNames != NULL) && (pFunc->arrTypeParamNames[j] != NULL)) ? pFunc->arrTypeParamNames[j] : "");
		__xlXlpBufAppend(pBuf, "\"\n");
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iTypeParamCount ) {
			__xlXlpBufAppend(pBuf, ",");
		}
		__xlXlpBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iTypeParamCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
	}
	__xlXlpBufAppend(pBuf, "]");
}

static void __xlXlpAppendReturnsJson(XL_XlpTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXlpBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iReturnCount != 0) ) {
		__xlXlpBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iReturnCount); ++j ) {
		const char* sType;

		sType = ((pFunc->arrReturnTypeNames != NULL) && (pFunc->arrReturnTypeNames[j] != NULL)) ?
			pFunc->arrReturnTypeNames[j] : "";
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppendFormat(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"index\": ");
		__xlXlpBufAppendFormat(pBuf, "%llu,\n", (unsigned long long)j);
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "\"type\": \"");
		__xlXlpAppendEscaped(pBuf, sType);
		__xlXlpBufAppend(pBuf, "\"\n");
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
		__xlXlpBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iReturnCount ) {
			__xlXlpBufAppend(pBuf, ",");
		}
		__xlXlpBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iReturnCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXlpBufAppend(pBuf, "\t");
		}
	}
	__xlXlpBufAppend(pBuf, "]");
}

char* xlXlpDumpPackageJson(const XL_ModuleLinkResult* pModuleResult, const XL_SemResult* pSemResult, const char* sInputFile)
{
	XL_XlpTextBuffer buf;
	char sPackageName[XL_PATH_MAX_LEN];
	char sPackageVersion[32];
	char sPackageAbiVersion[32];
	char sPackageAbiId[XL_PATH_MAX_LEN];
	const XL_ModuleSummary* pRootModule;
	size_t i;
	size_t j;
	size_t k;

	memset(&buf, 0, sizeof(buf));
	__xlXlpFileStem(sInputFile, sPackageName, sizeof(sPackageName));
	pRootModule = __xlXlpFindRootModule(pModuleResult);
	if ( (pRootModule != NULL) && (pRootModule->sPackageName != NULL) && (pRootModule->sPackageName[0] != '\0') ) {
		snprintf(sPackageName, sizeof(sPackageName), "%s", pRootModule->sPackageName);
	}
	snprintf(sPackageVersion, sizeof(sPackageVersion), "%s", __xlXlpModulePackageVersion(pRootModule));
	snprintf(sPackageAbiVersion, sizeof(sPackageAbiVersion), "%s", __xlXlpModulePackageAbiVersion(pRootModule));
	if ( (pRootModule != NULL) && (pRootModule->sPackageAbiId != NULL) && (pRootModule->sPackageAbiId[0] != '\0') ) {
		snprintf(sPackageAbiId, sizeof(sPackageAbiId), "%s", pRootModule->sPackageAbiId);
	} else {
		__xlXlpBuildPackageAbiId(sPackageName, sPackageVersion, sPackageAbiVersion, sPackageAbiId, sizeof(sPackageAbiId));
	}

	__xlXlpBufAppend(&buf, "{\n");
	__xlXlpBufAppend(&buf, "\t\"format\": \"xlp-0\",\n");
	__xlXlpBufAppend(&buf, "\t\"package\": {\n");
	__xlXlpBufAppend(&buf, "\t\t\"name\": \"");
	__xlXlpAppendEscaped(&buf, sPackageName);
	__xlXlpBufAppend(&buf, "\",\n");
	__xlXlpBufAppend(&buf, "\t\t\"version\": \"");
	__xlXlpAppendEscaped(&buf, sPackageVersion);
	__xlXlpBufAppend(&buf, "\",\n");
	__xlXlpBufAppend(&buf, "\t\t\"abiVersion\": \"");
	__xlXlpAppendEscaped(&buf, sPackageAbiVersion);
	__xlXlpBufAppend(&buf, "\",\n");
	__xlXlpBufAppend(&buf, "\t\t\"xlangAbi\": \"1\",\n");
	__xlXlpBufAppend(&buf, "\t\t\"abiId\": \"");
	__xlXlpAppendEscaped(&buf, sPackageAbiId);
	__xlXlpBufAppend(&buf, "\"\n");
	__xlXlpBufAppend(&buf, "\t},\n");
	__xlXlpBufAppend(&buf, "\t\"dependencies\": [");
	for ( i = 0; (pModuleResult != NULL) && (i < pModuleResult->iModuleCount); ++i ) {
		const XL_ModuleSummary* pModule;
		char sDepAbiId[XL_PATH_MAX_LEN];
		XL_XlpDependencyInfo* pDepInfos;
		size_t iDepInfoCount;
		int bNeedComma;

		pModule = &pModuleResult->pModules[i];
		if ( !__xlXlpShouldEmitDependency(pModuleResult, i) ) {
			continue;
		}
		pDepInfos = NULL;
		iDepInfoCount = 0;
		if ( (pModule->sManifestPath != NULL) && (pModule->sManifestPath[0] != '\0') ) {
			xlXlpReadDependencies(pModule->sManifestPath, &pDepInfos, &iDepInfoCount);
		}
		__xlXlpBufAppend(&buf, "\n\t\t{\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"name\": \"");
		__xlXlpAppendEscaped(&buf, __xlXlpModulePackageName(pModule));
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppendFormat(&buf, "\t\t\t\"kind\": \"%s\",\n", pModule->bIsNative ? "native" : "source");
		__xlXlpBufAppend(&buf, "\t\t\t\"version\": \"");
		__xlXlpAppendEscaped(&buf, __xlXlpModulePackageVersion(pModule));
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"abiVersion\": \"");
		__xlXlpAppendEscaped(&buf, __xlXlpModulePackageAbiVersion(pModule));
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"abiId\": \"");
		__xlXlpAppendEscaped(&buf, __xlXlpModulePackageAbiId(pModule, sDepAbiId, sizeof(sDepAbiId)));
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"manifest\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sManifestPath);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"binding\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sBindingName);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactKind\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactKind);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactPath\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactPath);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactFile\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactFile);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"consumeMode\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sConsumeMode);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"usedBy\": [");
		bNeedComma = 0;
		for ( j = 0; j < pModuleResult->iModuleCount; ++j ) {
			const XL_ModuleSummary* pUserModule;

			pUserModule = &pModuleResult->pModules[j];
			if ( !__xlXlpModuleUsesDependency(pUserModule, pModule) ) {
				continue;
			}
			if ( bNeedComma ) {
				__xlXlpBufAppend(&buf, ", ");
			}
			__xlXlpBufAppend(&buf, "\"");
			__xlXlpAppendEscaped(&buf, pUserModule->sKey);
			__xlXlpBufAppend(&buf, "\"");
			bNeedComma = 1;
		}
		__xlXlpBufAppend(&buf, "],\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"dependsOn\": [");
		if ( iDepInfoCount != 0 ) {
			size_t m;
			for ( m = 0; m < iDepInfoCount; ++m ) {
				if ( m != 0 ) {
					__xlXlpBufAppend(&buf, ", ");
				}
				__xlXlpBufAppend(&buf, "\"");
				__xlXlpAppendEscaped(&buf, pDepInfos[m].sName);
				__xlXlpBufAppend(&buf, "@");
				__xlXlpAppendEscaped(&buf, (pDepInfos[m].sVersion[0] != '\0') ? pDepInfos[m].sVersion : "0.0.0");
				__xlXlpBufAppend(&buf, "\"");
			}
		}
		__xlXlpBufAppend(&buf, "],\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"dependsOnAbi\": [");
		if ( iDepInfoCount != 0 ) {
			size_t m;
			char sNestedAbiId[XL_PATH_MAX_LEN];
			for ( m = 0; m < iDepInfoCount; ++m ) {
				if ( m != 0 ) {
					__xlXlpBufAppend(&buf, ", ");
				}
				__xlXlpBuildDependencyAbiId(&pDepInfos[m], sNestedAbiId, sizeof(sNestedAbiId));
				__xlXlpBufAppend(&buf, "\"");
				__xlXlpAppendEscaped(&buf, sNestedAbiId);
				__xlXlpBufAppend(&buf, "\"");
			}
		}
		__xlXlpBufAppend(&buf, "]\n");
		xlXlpDependencyInfoFree(pDepInfos);
		__xlXlpBufAppend(&buf, "\t\t},");
	}
	if ( buf.iLen >= 1 ) {
		if ( buf.sData[buf.iLen - 1] == ',' ) {
			buf.sData[buf.iLen - 1] = '\n';
		}
	}
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) && (strstr(buf.sData, "\"dependencies\": [\n") != NULL) ) {
		__xlXlpBufAppend(&buf, "\t");
	}
	__xlXlpBufAppend(&buf, "],\n");
	__xlXlpBufAppend(&buf, "\t\"modules\": [");
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) ) {
		__xlXlpBufAppend(&buf, "\n");
	}

	for ( i = 0; (pModuleResult != NULL) && (i < pModuleResult->iModuleCount); ++i ) {
		const XL_ModuleSummary* pModule;
		const char* sModulePackageName;
		const char* sModulePackageVersion;
		const char* sModulePackageAbiVersion;
		const char* sModulePackageAbiIdText;
		char sModulePackageAbiFallback[XL_PATH_MAX_LEN];
		char sModuleAbiId[XL_PATH_MAX_LEN];
		char sInitSymbol[XL_PATH_MAX_LEN];
		char sExportsSymbol[XL_PATH_MAX_LEN];
		char sDescriptorSymbol[XL_PATH_MAX_LEN];

		pModule = &pModuleResult->pModules[i];
		if ( pModule->bIsRoot ) {
			sModulePackageName = __xlXlpModulePackageName(pModule);
			sModulePackageVersion = __xlXlpModulePackageVersion(pModule);
			sModulePackageAbiVersion = __xlXlpModulePackageAbiVersion(pModule);
			sModulePackageAbiIdText = __xlXlpModulePackageAbiId(pModule, sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
		} else {
			sModulePackageName = ((pModule->sPackageName != NULL) && (pModule->sPackageName[0] != '\0')) ? pModule->sPackageName :
				(pModule->bIsNative ? pModule->sKey : sPackageName);
			sModulePackageVersion = ((pModule->sPackageVersion != NULL) && (pModule->sPackageVersion[0] != '\0')) ? pModule->sPackageVersion : "0.0.0";
			sModulePackageAbiVersion = ((pModule->sPackageAbiVersion != NULL) && (pModule->sPackageAbiVersion[0] != '\0')) ? pModule->sPackageAbiVersion : "1";
			if ( (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
				sModulePackageAbiIdText = pModule->sPackageAbiId;
			} else if ( pModule->bIsNative ) {
				__xlXlpBuildPackageAbiId(pModule->sKey, "0.0.0", "1", sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
				sModulePackageAbiIdText = sModulePackageAbiFallback;
			} else {
				sModulePackageAbiIdText = sPackageAbiId;
			}
		}
		__xlXlpBuildModuleAbiId(sModulePackageAbiIdText, pModule->sKey, sModuleAbiId, sizeof(sModuleAbiId));
		__xlXlpGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sInitSymbol, "init", sInitSymbol, sizeof(sInitSymbol));
		__xlXlpGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sExportsSymbol, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		__xlXlpGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sDescriptorSymbol, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		__xlXlpBufAppend(&buf, "\t\t{\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"key\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sKey);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"packageName\": \"");
		__xlXlpAppendEscaped(&buf, sModulePackageName);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"packageVersion\": \"");
		__xlXlpAppendEscaped(&buf, sModulePackageVersion);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"packageAbiVersion\": \"");
		__xlXlpAppendEscaped(&buf, sModulePackageAbiVersion);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"abiId\": \"");
		__xlXlpAppendEscaped(&buf, sModuleAbiId);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"manifest\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sManifestPath);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"file\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sFilePath);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppendFormat(&buf, "\t\t\t\"root\": %d,\n", pModule->bIsRoot ? 1 : 0);
		__xlXlpBufAppendFormat(&buf, "\t\t\t\"native\": %d,\n", pModule->bIsNative ? 1 : 0);
		__xlXlpBufAppend(&buf, "\t\t\t\"binding\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sBindingName);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactKind\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactKind);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactPath\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactPath);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"artifactFile\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sArtifactFile);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"consumeMode\": \"");
		__xlXlpAppendEscaped(&buf, pModule->sConsumeMode);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"initSymbol\": \"");
		__xlXlpAppendEscaped(&buf, sInitSymbol);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"exportsSymbol\": \"");
		__xlXlpAppendEscaped(&buf, sExportsSymbol);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"descriptorSymbol\": \"");
		__xlXlpAppendEscaped(&buf, sDescriptorSymbol);
		__xlXlpBufAppend(&buf, "\",\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"imports\": [");
		for ( j = 0; j < pModule->iImportCount; ++j ) {
			if ( j != 0 ) {
				__xlXlpBufAppend(&buf, ", ");
			}
			__xlXlpBufAppend(&buf, "\"");
			__xlXlpAppendEscaped(&buf, pModule->arrImports[j]);
			__xlXlpBufAppend(&buf, "\"");
		}
		__xlXlpBufAppend(&buf, "],\n");
		__xlXlpBufAppend(&buf, "\t\t\t\"exports\": [");
		if ( pModule->iExportCount != 0 ) {
			__xlXlpBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			size_t iMatchCount;
			char sExportAbiId[XL_PATH_MAX_LEN];
			char sVersionedSymbol[XL_PATH_MAX_LEN];

			iMatchCount = __xlXlpCountExportFuncs(pSemResult, pModule->pExports[j].sSymbol);
			__xlXlpBuildExportAbiId(sModuleAbiId, pModule->pExports[j].sName, sExportAbiId, sizeof(sExportAbiId));
			__xlXlpBuildExportVersionedSymbol(sModuleAbiId, pModule->pExports[j].sSymbol, sVersionedSymbol, sizeof(sVersionedSymbol));
			__xlXlpBufAppend(&buf, "\t\t\t\t{\n");
			__xlXlpBufAppend(&buf, "\t\t\t\t\t\"name\": \"");
			__xlXlpAppendEscaped(&buf, pModule->pExports[j].sName);
			__xlXlpBufAppend(&buf, "\",\n");
			__xlXlpBufAppend(&buf, "\t\t\t\t\t\"symbol\": \"");
			__xlXlpAppendEscaped(&buf, pModule->pExports[j].sSymbol);
			__xlXlpBufAppend(&buf, "\",\n");
			__xlXlpBufAppend(&buf, "\t\t\t\t\t\"abiId\": \"");
			__xlXlpAppendEscaped(&buf, sExportAbiId);
			__xlXlpBufAppend(&buf, "\",\n");
			__xlXlpBufAppend(&buf, "\t\t\t\t\t\"versionedSymbol\": \"");
			__xlXlpAppendEscaped(&buf, sVersionedSymbol);
			__xlXlpBufAppend(&buf, "\",\n");
			__xlXlpBufAppendFormat(&buf, "\t\t\t\t\t\"kind\": \"%s\",\n", (iMatchCount != 0) ? "function" : "value");
			__xlXlpBufAppendFormat(&buf, "\t\t\t\t\t\"overloadCount\": %llu\n", (unsigned long long)iMatchCount);
			__xlXlpBufAppend(&buf, "\t\t\t\t}");
			if ( (j + 1) != pModule->iExportCount ) {
				__xlXlpBufAppend(&buf, ",");
			}
			__xlXlpBufAppend(&buf, "\n");
		}
		if ( pModule->iExportCount != 0 ) {
			__xlXlpBufAppend(&buf, "\t\t\t");
		}
		__xlXlpBufAppend(&buf, "]\n");
		__xlXlpBufAppend(&buf, "\t\t}");
		if ( (i + 1) != pModuleResult->iModuleCount ) {
			__xlXlpBufAppend(&buf, ",");
		}
		__xlXlpBufAppend(&buf, "\n");
	}
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) ) {
		__xlXlpBufAppend(&buf, "\t");
	}
	__xlXlpBufAppend(&buf, "],\n");
	__xlXlpBufAppend(&buf, "\t\"functions\": [");

	for ( i = 0; (pModuleResult != NULL) && (i < pModuleResult->iModuleCount); ++i ) {
		const XL_ModuleSummary* pModule;
		int bNeedModulePrefix;

		pModule = &pModuleResult->pModules[i];
		bNeedModulePrefix = 1;
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			for ( k = 0; (pSemResult != NULL) && (k < pSemResult->funcs.iCount); ++k ) {
				const XL_FuncInfo* pFunc;
				const char* sModulePackageAbiIdText;
				char sModulePackageAbiFallback[XL_PATH_MAX_LEN];
				char sModuleAbiId[XL_PATH_MAX_LEN];
				char sLanguageAbiId[XL_PATH_MAX_LEN];
				char sNativeAbiId[XL_PATH_MAX_LEN];

				pFunc = xlFuncTableGet(&pSemResult->funcs, k);
				if ( (pFunc == NULL) || (pFunc->sName == NULL) || (strcmp(pFunc->sName, pModule->pExports[j].sSymbol) != 0) ) {
					continue;
				}
				if ( (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
					sModulePackageAbiIdText = pModule->sPackageAbiId;
				} else if ( pModule->bIsNative ) {
					__xlXlpBuildPackageAbiId(pModule->sKey, "0.0.0", "1", sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
					sModulePackageAbiIdText = sModulePackageAbiFallback;
				} else {
					sModulePackageAbiIdText = sPackageAbiId;
				}
				__xlXlpBuildModuleAbiId(sModulePackageAbiIdText, pModule->sKey, sModuleAbiId, sizeof(sModuleAbiId));
				__xlXlpBuildFunctionLanguageAbiId(sModuleAbiId, pFunc->sSignatureId, pFunc->sLanguageAbi, sLanguageAbiId, sizeof(sLanguageAbiId));
				__xlXlpBuildFunctionNativeAbiId(sModuleAbiId, pFunc->sAbiSymbol, pFunc->sNativeAbi, sNativeAbiId, sizeof(sNativeAbiId));

				if ( bNeedModulePrefix ) {
					__xlXlpBufAppend(&buf, "\n");
					bNeedModulePrefix = 0;
				}
				__xlXlpBufAppend(&buf, "\t\t{\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"module\": \"");
				__xlXlpAppendEscaped(&buf, pModule->sKey);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"moduleAbiId\": \"");
				__xlXlpAppendEscaped(&buf, sModuleAbiId);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"exportName\": \"");
				__xlXlpAppendEscaped(&buf, pModule->pExports[j].sName);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"receiverType\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sReceiverType);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"methodName\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sMethodName);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"symbol\": \"");
				__xlXlpAppendEscaped(&buf, pModule->pExports[j].sSymbol);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"overloadGroup\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sOverloadGroup);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"signatureId\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sSignatureId);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"dispatchKey\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sDispatchKey);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"languageAbi\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sLanguageAbi);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"nativeAbi\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sNativeAbi);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"languageAbiId\": \"");
				__xlXlpAppendEscaped(&buf, sLanguageAbiId);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"nativeAbiId\": \"");
				__xlXlpAppendEscaped(&buf, sNativeAbiId);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"abiSymbol\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sAbiSymbol);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"returnType\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sReturnTypeName);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"storageReturnType\": \"");
				__xlXlpAppendEscaped(&buf, pFunc->sStorageReturnTypeName);
				__xlXlpBufAppend(&buf, "\",\n");
				__xlXlpBufAppendFormat(&buf, "\t\t\t\"returnCount\": %llu,\n", (unsigned long long)pFunc->iReturnCount);
				__xlXlpBufAppend(&buf, "\t\t\t\"returns\": ");
				__xlXlpAppendReturnsJson(&buf, pFunc, 3);
				__xlXlpBufAppend(&buf, ",\n");
				__xlXlpBufAppendFormat(&buf, "\t\t\t\"async\": %d,\n", pFunc->bAsync ? 1 : 0);
				__xlXlpBufAppendFormat(&buf, "\t\t\t\"generator\": %d,\n", pFunc->bGenerator ? 1 : 0);
				__xlXlpBufAppendFormat(&buf, "\t\t\t\"varargKind\": %d,\n", pFunc->eVarArgKind);
				__xlXlpBufAppendFormat(&buf, "\t\t\t\"typeParamCount\": %llu,\n", (unsigned long long)pFunc->iTypeParamCount);
				__xlXlpBufAppend(&buf, "\t\t\t\"typeParams\": ");
				__xlXlpAppendTypeParamsJson(&buf, pFunc, 3);
				__xlXlpBufAppend(&buf, ",\n");
				__xlXlpBufAppend(&buf, "\t\t\t\"params\": ");
				__xlXlpAppendParamsJson(&buf, pFunc, 3);
				__xlXlpBufAppend(&buf, "\n\t\t},\n");
			}
		}
	}

	if ( buf.iLen >= 2 ) {
		if ( (buf.sData[buf.iLen - 2] == ',') && (buf.sData[buf.iLen - 1] == '\n') ) {
			buf.sData[buf.iLen - 2] = '\n';
			buf.sData[buf.iLen - 1] = '\0';
			buf.iLen -= 1;
		}
	}
	if ( buf.iLen > 0 ) {
		if ( buf.sData[buf.iLen - 1] == '\n' ) {
			__xlXlpBufAppend(&buf, "\t");
		}
	}
	__xlXlpBufAppend(&buf, "]\n}\n");
	return buf.sData;
}
