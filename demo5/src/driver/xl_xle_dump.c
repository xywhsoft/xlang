#include "xl_xle_dump.h"
#include "../ast/xl_ast_dump.h"
#include "xl_xle_load.h"

#include "../sem/xl_func.h"

typedef struct XL_XleTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_XleTextBuffer;

static int __xlXleBufGrow(XL_XleTextBuffer* pBuf, size_t iNeed)
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

static void __xlXleBufAppend(XL_XleTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlXleBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlXleBufAppendFormat(XL_XleTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlXleBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlXleBufFree(XL_XleTextBuffer* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	free(pBuf->sData);
	pBuf->sData = NULL;
	pBuf->iLen = 0;
	pBuf->iCap = 0;
}

static void __xlXleAppendEscaped(XL_XleTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlXleBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlXleBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlXleBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlXleBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlXleBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlXleBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];

					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlXleBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static const char* __xlXleFileStem(const char* sInputFile, char* sOut, size_t iOutSize)
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

static void __xlXleBuildPackageAbiId(
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

static void __xlXleBuildDependencyAbiId(const XL_XleDependencyInfo* pDep, char* sOut, size_t iOutSize)
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

static void __xlXleBuildModuleAbiId(const char* sPackageAbiId, const char* sModuleKey, char* sOut, size_t iOutSize)
{
	snprintf(sOut, iOutSize, "%s::%s", (sPackageAbiId != NULL) ? sPackageAbiId : "package@0.0.0#abi1", (sModuleKey != NULL) ? sModuleKey : "<root>");
}

static void __xlXleBuildFunctionLanguageAbiId(
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

static void __xlXleBuildFunctionNativeAbiId(
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

static void __xlXleBuildExportAbiId(
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

static void __xlXleBuildExportVersionedSymbol(
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

static void __xlXleSanitizeIdentPart(char* sOut, size_t iOutSize, const char* sText)
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

static void __xlXleBuildModuleAbiSymbol(
	const char* sModuleKey,
	const char* sSuffix,
	char* sOut,
	size_t iOutSize
)
{
	char sModuleKeySafe[XL_PATH_MAX_LEN];

	__xlXleSanitizeIdentPart(sModuleKeySafe, sizeof(sModuleKeySafe), sModuleKey);
	snprintf(
		sOut,
		iOutSize,
		"xl_mod_%s__%s",
		(sModuleKeySafe[0] != '\0') ? sModuleKeySafe : "module",
		(sSuffix != NULL) ? sSuffix : ""
	);
}

static int __xlXleModuleKeyHasCollision(const XL_ModuleLinkResult* pModuleResult, const XL_ModuleSummary* pTarget)
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

static void __xlXleBuildUniqueModuleAbiSymbol(const XL_ModuleSummary* pModule, const char* sSuffix, char* sOut, size_t iOutSize)
{
	char sUniqueKey[XL_PATH_MAX_LEN];
	const char* sModuleKey;

	if ( (pModule != NULL) &&
		!pModule->bIsRoot &&
		(pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		sModuleKey = ((pModule->sKey != NULL) && (pModule->sKey[0] != '\0')) ? pModule->sKey : "module";
		snprintf(sUniqueKey, sizeof(sUniqueKey), "%s__%s", pModule->sPackageAbiId, sModuleKey);
		__xlXleBuildModuleAbiSymbol(sUniqueKey, sSuffix, sOut, iOutSize);
		return;
	}

	__xlXleBuildModuleAbiSymbol((pModule != NULL) ? pModule->sKey : NULL, sSuffix, sOut, iOutSize);
}

static void __xlXleGetEffectiveModuleAbiSymbol(
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

	__xlXleBuildModuleAbiSymbol((pModule != NULL) ? pModule->sKey : NULL, sSuffix, sDefaultSymbol, sizeof(sDefaultSymbol));
	if ( __xlXleModuleKeyHasCollision(pModuleResult, pModule) &&
		( ((sExistingSymbol == NULL) || (sExistingSymbol[0] == '\0')) || (strcmp(sExistingSymbol, sDefaultSymbol) == 0) ) ) {
		__xlXleBuildUniqueModuleAbiSymbol(pModule, sSuffix, sOut, iOutSize);
		return;
	}

	if ( (sExistingSymbol != NULL) && (sExistingSymbol[0] != '\0') ) {
		snprintf(sOut, iOutSize, "%s", sExistingSymbol);
		return;
	}

	snprintf(sOut, iOutSize, "%s", sDefaultSymbol);
}

static const char* __xlXleModulePackageName(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageName != NULL) && (pModule->sPackageName[0] != '\0') ) {
		return pModule->sPackageName;
	}
	return (pModule != NULL) ? pModule->sKey : "";
}

static const char* __xlXleModulePackageVersion(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageVersion != NULL) && (pModule->sPackageVersion[0] != '\0') ) {
		return pModule->sPackageVersion;
	}
	return "0.0.0";
}

static const char* __xlXleModulePackageAbiVersion(const XL_ModuleSummary* pModule)
{
	if ( (pModule != NULL) && (pModule->sPackageAbiVersion != NULL) && (pModule->sPackageAbiVersion[0] != '\0') ) {
		return pModule->sPackageAbiVersion;
	}
	return "1";
}

static const char* __xlXleModulePackageAbiId(const XL_ModuleSummary* pModule, char* sFallback, size_t iFallbackSize)
{
	if ( (pModule != NULL) && (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
		return pModule->sPackageAbiId;
	}
	__xlXleBuildPackageAbiId(
		__xlXleModulePackageName(pModule),
		__xlXleModulePackageVersion(pModule),
		__xlXleModulePackageAbiVersion(pModule),
		sFallback,
		iFallbackSize
	);
	return sFallback;
}

static const XL_ModuleSummary* __xlXleFindRootModule(const XL_ModuleLinkResult* pModuleResult)
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

static int __xlXleIsSameDependency(const XL_ModuleSummary* pLeft, const XL_ModuleSummary* pRight)
{
	char sLeftAbiId[XL_PATH_MAX_LEN];
	char sRightAbiId[XL_PATH_MAX_LEN];

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return 0;
	}
	return strcmp(
		__xlXleModulePackageAbiId(pLeft, sLeftAbiId, sizeof(sLeftAbiId)),
		__xlXleModulePackageAbiId(pRight, sRightAbiId, sizeof(sRightAbiId))
	) == 0;
}

static int __xlXleShouldEmitDependency(const XL_ModuleLinkResult* pModuleResult, size_t iIndex)
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
		if ( __xlXleIsSameDependency(&pModuleResult->pModules[i], pModule) ) {
			return 0;
		}
	}
	return 1;
}

static int __xlXleModuleImportsKey(const XL_ModuleSummary* pModule, const char* sImportKey)
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

static int __xlXleModuleImportsResolvedFile(const XL_ModuleSummary* pModule, const char* sResolvedFile)
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

static int __xlXleModuleHasResolvedImports(const XL_ModuleSummary* pModule)
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

static int __xlXleModuleUsesDependency(const XL_ModuleSummary* pUserModule, const XL_ModuleSummary* pDepModule)
{
	char sVersion[64];

	if ( (pUserModule == NULL) || (pDepModule == NULL) ) {
		return 0;
	}

	if ( (pUserModule->sManifestPath != NULL) && (pUserModule->sManifestPath[0] != '\0') &&
		xlXleFindDependencyVersion(pUserModule->sManifestPath, __xlXleModulePackageName(pDepModule), sVersion, sizeof(sVersion)) == XL_RC_OK ) {
		return strcmp(sVersion, __xlXleModulePackageVersion(pDepModule)) == 0;
	}

	if ( __xlXleModuleImportsResolvedFile(pUserModule, pDepModule->sFilePath) ) {
		return 1;
	}
	if ( !__xlXleModuleHasResolvedImports(pUserModule) ) {
		return __xlXleModuleImportsKey(pUserModule, pDepModule->sKey);
	}
	return 0;
}

static size_t __xlXleCountExportFuncs(const XL_SemResult* pSemResult, const char* sSymbol)
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

static int __xlXleAppendDefaultExprText(XL_XleTextBuffer* pBuf, const XL_AstNode* pNode)
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
			__xlXleBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			return XL_RC_OK;
		case XL_AST_STRING:
		case XL_AST_TEMPLATE:
			__xlXleBufAppend(pBuf, "\"");
			__xlXleAppendEscaped(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			__xlXleBufAppend(pBuf, "\"");
			return XL_RC_OK;
		case XL_AST_UNARY:
			__xlXleBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			return __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL);
		case XL_AST_BINARY:
			__xlXleBufAppend(pBuf, "(");
			if ( __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXleBufAppend(pBuf, " ");
			__xlXleBufAppend(pBuf, (pNode->sText != NULL) ? pNode->sText : "");
			__xlXleBufAppend(pBuf, " ");
			if ( __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXleBufAppend(pBuf, ")");
			return XL_RC_OK;
		case XL_AST_TERNARY:
			__xlXleBufAppend(pBuf, "(");
			if ( __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 0) ? pNode->ppChildren[0] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXleBufAppend(pBuf, " ? ");
			if ( __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 1) ? pNode->ppChildren[1] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXleBufAppend(pBuf, " : ");
			if ( __xlXleAppendDefaultExprText(pBuf, (pNode->iChildCount > 2) ? pNode->ppChildren[2] : NULL) != XL_RC_OK ) return XL_RC_IO_ERROR;
			__xlXleBufAppend(pBuf, ")");
			return XL_RC_OK;
		default:
			return XL_RC_FAIL;
	}
}

static char* __xlXleBuildDefaultExprText(const XL_AstNode* pNode)
{
	XL_XleTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	if ( __xlXleAppendDefaultExprText(&buf, pNode) != XL_RC_OK ) {
		free(buf.sData);
		return NULL;
	}
	return buf.sData;
}

static void __xlXleAppendParamsJson(XL_XleTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXleBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iParamCount != 0) ) {
		__xlXleBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iParamCount); ++j ) {
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"name\": \"");
		__xlXleAppendEscaped(pBuf, pFunc->arrParamNames[j]);
		__xlXleBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"label\": \"");
		__xlXleAppendEscaped(pBuf, pFunc->arrParamNames[j]);
		__xlXleBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"type\": \"");
		__xlXleAppendEscaped(pBuf, pFunc->arrParamTypes[j]);
		__xlXleBufAppend(pBuf, "\",\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppendFormat(pBuf, "\"hasDefault\": %d\n", ((pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[j]) ? 1 : 0);
		if ( (pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[j] ) {
			char* sDefaultText;
			char* sDefaultJson;

			sDefaultText = ((pFunc->arrParamDefaultValues != NULL) && (pFunc->arrParamDefaultValues[j] != NULL)) ?
				__xlXleBuildDefaultExprText(pFunc->arrParamDefaultValues[j]) : NULL;
			sDefaultJson = ((pFunc->arrParamDefaultValues != NULL) && (pFunc->arrParamDefaultValues[j] != NULL)) ?
				xlAstDumpJson(pFunc->arrParamDefaultValues[j]) : NULL;
			__xlXleBufAppend(pBuf, ",\n");
			for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
				__xlXleBufAppend(pBuf, "\t");
			}
			__xlXleBufAppend(pBuf, "\"defaultValueText\": \"");
			__xlXleAppendEscaped(pBuf, (sDefaultText != NULL) ? sDefaultText : "");
			__xlXleBufAppend(pBuf, "\",\n");
			for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
				__xlXleBufAppend(pBuf, "\t");
			}
			__xlXleBufAppend(pBuf, "\"defaultValueJson\": \"");
			__xlXleAppendEscaped(pBuf, (sDefaultJson != NULL) ? sDefaultJson : "");
			__xlXleBufAppend(pBuf, "\"\n");
			free(sDefaultText);
			free(sDefaultJson);
		}
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iParamCount ) {
			__xlXleBufAppend(pBuf, ",");
		}
		__xlXleBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iParamCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXleBufAppend(pBuf, "\t");
		}
	}
	__xlXleBufAppend(pBuf, "]");
}

static void __xlXleAppendTypeParamsJson(XL_XleTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXleBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iTypeParamCount != 0) ) {
		__xlXleBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iTypeParamCount); ++j ) {
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"name\": \"");
		__xlXleAppendEscaped(pBuf, ((pFunc->arrTypeParamNames != NULL) && (pFunc->arrTypeParamNames[j] != NULL)) ? pFunc->arrTypeParamNames[j] : "");
		__xlXleBufAppend(pBuf, "\"\n");
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iTypeParamCount ) {
			__xlXleBufAppend(pBuf, ",");
		}
		__xlXleBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iTypeParamCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXleBufAppend(pBuf, "\t");
		}
	}
	__xlXleBufAppend(pBuf, "]");
}

static void __xlXleAppendReturnsJson(XL_XleTextBuffer* pBuf, const XL_FuncInfo* pFunc, int iIndent)
{
	size_t j;
	size_t k;

	__xlXleBufAppend(pBuf, "[");
	if ( (pFunc != NULL) && (pFunc->iReturnCount != 0) ) {
		__xlXleBufAppend(pBuf, "\n");
	}

	for ( j = 0; (pFunc != NULL) && (j < pFunc->iReturnCount); ++j ) {
		const char* sType;

		sType = ((pFunc->arrReturnTypeNames != NULL) && (pFunc->arrReturnTypeNames[j] != NULL)) ?
			pFunc->arrReturnTypeNames[j] : "";
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppendFormat(pBuf, "{\n");
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"index\": ");
		__xlXleBufAppendFormat(pBuf, "%llu,\n", (unsigned long long)j);
		for ( k = 0; k < (size_t)(iIndent + 2); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "\"type\": \"");
		__xlXleAppendEscaped(pBuf, sType);
		__xlXleBufAppend(pBuf, "\"\n");
		for ( k = 0; k < (size_t)(iIndent + 1); ++k ) {
			__xlXleBufAppend(pBuf, "\t");
		}
		__xlXleBufAppend(pBuf, "}");
		if ( (j + 1) != pFunc->iReturnCount ) {
			__xlXleBufAppend(pBuf, ",");
		}
		__xlXleBufAppend(pBuf, "\n");
	}

	if ( (pFunc != NULL) && (pFunc->iReturnCount != 0) ) {
		for ( j = 0; j < (size_t)iIndent; ++j ) {
			__xlXleBufAppend(pBuf, "\t");
		}
	}
	__xlXleBufAppend(pBuf, "]");
}

char* xlXleDumpPackageJson(const XL_ModuleLinkResult* pModuleResult, const XL_SemResult* pSemResult, const char* sInputFile)
{
	XL_XleTextBuffer buf;
	char sPackageName[XL_PATH_MAX_LEN];
	char sPackageVersion[32];
	char sPackageAbiVersion[32];
	char sPackageAbiId[XL_PATH_MAX_LEN];
	const XL_ModuleSummary* pRootModule;
	size_t i;
	size_t j;
	size_t k;

	memset(&buf, 0, sizeof(buf));
	__xlXleFileStem(sInputFile, sPackageName, sizeof(sPackageName));
	pRootModule = __xlXleFindRootModule(pModuleResult);
	if ( (pRootModule != NULL) && (pRootModule->sPackageName != NULL) && (pRootModule->sPackageName[0] != '\0') ) {
		snprintf(sPackageName, sizeof(sPackageName), "%s", pRootModule->sPackageName);
	}
	snprintf(sPackageVersion, sizeof(sPackageVersion), "%s", __xlXleModulePackageVersion(pRootModule));
	snprintf(sPackageAbiVersion, sizeof(sPackageAbiVersion), "%s", __xlXleModulePackageAbiVersion(pRootModule));
	if ( (pRootModule != NULL) && (pRootModule->sPackageAbiId != NULL) && (pRootModule->sPackageAbiId[0] != '\0') ) {
		snprintf(sPackageAbiId, sizeof(sPackageAbiId), "%s", pRootModule->sPackageAbiId);
	} else {
		__xlXleBuildPackageAbiId(sPackageName, sPackageVersion, sPackageAbiVersion, sPackageAbiId, sizeof(sPackageAbiId));
	}

	__xlXleBufAppend(&buf, "{\n");
	__xlXleBufAppend(&buf, "\t\"format\": \"xle-1\",\n");
	__xlXleBufAppend(&buf, "\t\"package\": {\n");
	__xlXleBufAppend(&buf, "\t\t\"name\": \"");
	__xlXleAppendEscaped(&buf, sPackageName);
	__xlXleBufAppend(&buf, "\",\n");
	__xlXleBufAppend(&buf, "\t\t\"version\": \"");
	__xlXleAppendEscaped(&buf, sPackageVersion);
	__xlXleBufAppend(&buf, "\",\n");
	__xlXleBufAppend(&buf, "\t\t\"abiVersion\": \"");
	__xlXleAppendEscaped(&buf, sPackageAbiVersion);
	__xlXleBufAppend(&buf, "\",\n");
	__xlXleBufAppend(&buf, "\t\t\"xlangAbi\": \"1\",\n");
	__xlXleBufAppend(&buf, "\t\t\"abiId\": \"");
	__xlXleAppendEscaped(&buf, sPackageAbiId);
	__xlXleBufAppend(&buf, "\"\n");
	__xlXleBufAppend(&buf, "\t},\n");
	__xlXleBufAppend(&buf, "\t\"dependencies\": [");
	for ( i = 0; (pModuleResult != NULL) && (i < pModuleResult->iModuleCount); ++i ) {
		const XL_ModuleSummary* pModule;
		char sDepAbiId[XL_PATH_MAX_LEN];
		XL_XleDependencyInfo* pDepInfos;
		size_t iDepInfoCount;
		int bNeedComma;

		pModule = &pModuleResult->pModules[i];
		if ( !__xlXleShouldEmitDependency(pModuleResult, i) ) {
			continue;
		}
		pDepInfos = NULL;
		iDepInfoCount = 0;
		if ( (pModule->sManifestPath != NULL) && (pModule->sManifestPath[0] != '\0') ) {
			xlXleReadDependencies(pModule->sManifestPath, &pDepInfos, &iDepInfoCount);
		}
		__xlXleBufAppend(&buf, "\n\t\t{\n");
		__xlXleBufAppend(&buf, "\t\t\t\"name\": \"");
		__xlXleAppendEscaped(&buf, __xlXleModulePackageName(pModule));
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppendFormat(&buf, "\t\t\t\"kind\": \"%s\",\n", pModule->bIsNative ? "native" : "source");
		__xlXleBufAppend(&buf, "\t\t\t\"version\": \"");
		__xlXleAppendEscaped(&buf, __xlXleModulePackageVersion(pModule));
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"abiVersion\": \"");
		__xlXleAppendEscaped(&buf, __xlXleModulePackageAbiVersion(pModule));
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"abiId\": \"");
		__xlXleAppendEscaped(&buf, __xlXleModulePackageAbiId(pModule, sDepAbiId, sizeof(sDepAbiId)));
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"manifest\": \"");
		__xlXleAppendEscaped(&buf, pModule->sManifestPath);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"binding\": \"");
		__xlXleAppendEscaped(&buf, pModule->sBindingName);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactKind\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactKind);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactPath\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactPath);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactFile\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactFile);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"consumeMode\": \"");
		__xlXleAppendEscaped(&buf, pModule->sConsumeMode);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"usedBy\": [");
		bNeedComma = 0;
		for ( j = 0; j < pModuleResult->iModuleCount; ++j ) {
			const XL_ModuleSummary* pUserModule;

			pUserModule = &pModuleResult->pModules[j];
			if ( !__xlXleModuleUsesDependency(pUserModule, pModule) ) {
				continue;
			}
			if ( bNeedComma ) {
				__xlXleBufAppend(&buf, ", ");
			}
			__xlXleBufAppend(&buf, "\"");
			__xlXleAppendEscaped(&buf, pUserModule->sKey);
			__xlXleBufAppend(&buf, "\"");
			bNeedComma = 1;
		}
		__xlXleBufAppend(&buf, "],\n");
		__xlXleBufAppend(&buf, "\t\t\t\"dependsOn\": [");
		if ( iDepInfoCount != 0 ) {
			size_t m;
			for ( m = 0; m < iDepInfoCount; ++m ) {
				if ( m != 0 ) {
					__xlXleBufAppend(&buf, ", ");
				}
				__xlXleBufAppend(&buf, "\"");
				__xlXleAppendEscaped(&buf, pDepInfos[m].sName);
				__xlXleBufAppend(&buf, "@");
				__xlXleAppendEscaped(&buf, (pDepInfos[m].sVersion[0] != '\0') ? pDepInfos[m].sVersion : "0.0.0");
				__xlXleBufAppend(&buf, "\"");
			}
		}
		__xlXleBufAppend(&buf, "],\n");
		__xlXleBufAppend(&buf, "\t\t\t\"dependsOnAbi\": [");
		if ( iDepInfoCount != 0 ) {
			size_t m;
			char sNestedAbiId[XL_PATH_MAX_LEN];
			for ( m = 0; m < iDepInfoCount; ++m ) {
				if ( m != 0 ) {
					__xlXleBufAppend(&buf, ", ");
				}
				__xlXleBuildDependencyAbiId(&pDepInfos[m], sNestedAbiId, sizeof(sNestedAbiId));
				__xlXleBufAppend(&buf, "\"");
				__xlXleAppendEscaped(&buf, sNestedAbiId);
				__xlXleBufAppend(&buf, "\"");
			}
		}
		__xlXleBufAppend(&buf, "]\n");
		xlXleDependencyInfoFree(pDepInfos);
		__xlXleBufAppend(&buf, "\t\t},");
	}
	if ( buf.iLen >= 1 ) {
		if ( buf.sData[buf.iLen - 1] == ',' ) {
			buf.sData[buf.iLen - 1] = '\n';
		}
	}
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) && (strstr(buf.sData, "\"dependencies\": [\n") != NULL) ) {
		__xlXleBufAppend(&buf, "\t");
	}
	__xlXleBufAppend(&buf, "],\n");
	__xlXleBufAppend(&buf, "\t\"modules\": [");
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) ) {
		__xlXleBufAppend(&buf, "\n");
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
			sModulePackageName = __xlXleModulePackageName(pModule);
			sModulePackageVersion = __xlXleModulePackageVersion(pModule);
			sModulePackageAbiVersion = __xlXleModulePackageAbiVersion(pModule);
			sModulePackageAbiIdText = __xlXleModulePackageAbiId(pModule, sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
		} else {
			sModulePackageName = ((pModule->sPackageName != NULL) && (pModule->sPackageName[0] != '\0')) ? pModule->sPackageName :
				(pModule->bIsNative ? pModule->sKey : sPackageName);
			sModulePackageVersion = ((pModule->sPackageVersion != NULL) && (pModule->sPackageVersion[0] != '\0')) ? pModule->sPackageVersion : "0.0.0";
			sModulePackageAbiVersion = ((pModule->sPackageAbiVersion != NULL) && (pModule->sPackageAbiVersion[0] != '\0')) ? pModule->sPackageAbiVersion : "1";
			if ( (pModule->sPackageAbiId != NULL) && (pModule->sPackageAbiId[0] != '\0') ) {
				sModulePackageAbiIdText = pModule->sPackageAbiId;
			} else if ( pModule->bIsNative ) {
				__xlXleBuildPackageAbiId(pModule->sKey, "0.0.0", "1", sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
				sModulePackageAbiIdText = sModulePackageAbiFallback;
			} else {
				sModulePackageAbiIdText = sPackageAbiId;
			}
		}
		__xlXleBuildModuleAbiId(sModulePackageAbiIdText, pModule->sKey, sModuleAbiId, sizeof(sModuleAbiId));
		__xlXleGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sInitSymbol, "init", sInitSymbol, sizeof(sInitSymbol));
		__xlXleGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sExportsSymbol, "exports", sExportsSymbol, sizeof(sExportsSymbol));
		__xlXleGetEffectiveModuleAbiSymbol(pModuleResult, pModule, pModule->sDescriptorSymbol, "descriptor", sDescriptorSymbol, sizeof(sDescriptorSymbol));
		__xlXleBufAppend(&buf, "\t\t{\n");
		__xlXleBufAppend(&buf, "\t\t\t\"key\": \"");
		__xlXleAppendEscaped(&buf, pModule->sKey);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"packageName\": \"");
		__xlXleAppendEscaped(&buf, sModulePackageName);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"packageVersion\": \"");
		__xlXleAppendEscaped(&buf, sModulePackageVersion);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"packageAbiVersion\": \"");
		__xlXleAppendEscaped(&buf, sModulePackageAbiVersion);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"abiId\": \"");
		__xlXleAppendEscaped(&buf, sModuleAbiId);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"manifest\": \"");
		__xlXleAppendEscaped(&buf, pModule->sManifestPath);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"file\": \"");
		__xlXleAppendEscaped(&buf, pModule->sFilePath);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppendFormat(&buf, "\t\t\t\"root\": %d,\n", pModule->bIsRoot ? 1 : 0);
		__xlXleBufAppendFormat(&buf, "\t\t\t\"native\": %d,\n", pModule->bIsNative ? 1 : 0);
		__xlXleBufAppend(&buf, "\t\t\t\"binding\": \"");
		__xlXleAppendEscaped(&buf, pModule->sBindingName);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactKind\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactKind);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactPath\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactPath);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"artifactFile\": \"");
		__xlXleAppendEscaped(&buf, pModule->sArtifactFile);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"consumeMode\": \"");
		__xlXleAppendEscaped(&buf, pModule->sConsumeMode);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"initSymbol\": \"");
		__xlXleAppendEscaped(&buf, sInitSymbol);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"exportsSymbol\": \"");
		__xlXleAppendEscaped(&buf, sExportsSymbol);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"descriptorSymbol\": \"");
		__xlXleAppendEscaped(&buf, sDescriptorSymbol);
		__xlXleBufAppend(&buf, "\",\n");
		__xlXleBufAppend(&buf, "\t\t\t\"imports\": [");
		for ( j = 0; j < pModule->iImportCount; ++j ) {
			if ( j != 0 ) {
				__xlXleBufAppend(&buf, ", ");
			}
			__xlXleBufAppend(&buf, "\"");
			__xlXleAppendEscaped(&buf, pModule->arrImports[j]);
			__xlXleBufAppend(&buf, "\"");
		}
		__xlXleBufAppend(&buf, "],\n");
		__xlXleBufAppend(&buf, "\t\t\t\"exports\": [");
		if ( pModule->iExportCount != 0 ) {
			__xlXleBufAppend(&buf, "\n");
		}
		for ( j = 0; j < pModule->iExportCount; ++j ) {
			size_t iMatchCount;
			char sExportAbiId[XL_PATH_MAX_LEN];
			char sVersionedSymbol[XL_PATH_MAX_LEN];

			iMatchCount = __xlXleCountExportFuncs(pSemResult, pModule->pExports[j].sSymbol);
			__xlXleBuildExportAbiId(sModuleAbiId, pModule->pExports[j].sName, sExportAbiId, sizeof(sExportAbiId));
			__xlXleBuildExportVersionedSymbol(sModuleAbiId, pModule->pExports[j].sSymbol, sVersionedSymbol, sizeof(sVersionedSymbol));
			__xlXleBufAppend(&buf, "\t\t\t\t{\n");
			__xlXleBufAppend(&buf, "\t\t\t\t\t\"name\": \"");
			__xlXleAppendEscaped(&buf, pModule->pExports[j].sName);
			__xlXleBufAppend(&buf, "\",\n");
			__xlXleBufAppend(&buf, "\t\t\t\t\t\"symbol\": \"");
			__xlXleAppendEscaped(&buf, pModule->pExports[j].sSymbol);
			__xlXleBufAppend(&buf, "\",\n");
			__xlXleBufAppend(&buf, "\t\t\t\t\t\"abiId\": \"");
			__xlXleAppendEscaped(&buf, sExportAbiId);
			__xlXleBufAppend(&buf, "\",\n");
			__xlXleBufAppend(&buf, "\t\t\t\t\t\"versionedSymbol\": \"");
			__xlXleAppendEscaped(&buf, sVersionedSymbol);
			__xlXleBufAppend(&buf, "\",\n");
			__xlXleBufAppendFormat(&buf, "\t\t\t\t\t\"kind\": \"%s\",\n", (iMatchCount != 0) ? "function" : "value");
			__xlXleBufAppendFormat(&buf, "\t\t\t\t\t\"overloadCount\": %llu\n", (unsigned long long)iMatchCount);
			__xlXleBufAppend(&buf, "\t\t\t\t}");
			if ( (j + 1) != pModule->iExportCount ) {
				__xlXleBufAppend(&buf, ",");
			}
			__xlXleBufAppend(&buf, "\n");
		}
		if ( pModule->iExportCount != 0 ) {
			__xlXleBufAppend(&buf, "\t\t\t");
		}
		__xlXleBufAppend(&buf, "]\n");
		__xlXleBufAppend(&buf, "\t\t}");
		if ( (i + 1) != pModuleResult->iModuleCount ) {
			__xlXleBufAppend(&buf, ",");
		}
		__xlXleBufAppend(&buf, "\n");
	}
	if ( (pModuleResult != NULL) && (pModuleResult->iModuleCount != 0) ) {
		__xlXleBufAppend(&buf, "\t");
	}
	__xlXleBufAppend(&buf, "],\n");
	__xlXleBufAppend(&buf, "\t\"functions\": [");

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
					__xlXleBuildPackageAbiId(pModule->sKey, "0.0.0", "1", sModulePackageAbiFallback, sizeof(sModulePackageAbiFallback));
					sModulePackageAbiIdText = sModulePackageAbiFallback;
				} else {
					sModulePackageAbiIdText = sPackageAbiId;
				}
				__xlXleBuildModuleAbiId(sModulePackageAbiIdText, pModule->sKey, sModuleAbiId, sizeof(sModuleAbiId));
				__xlXleBuildFunctionLanguageAbiId(sModuleAbiId, pFunc->sSignatureId, pFunc->sLanguageAbi, sLanguageAbiId, sizeof(sLanguageAbiId));
				__xlXleBuildFunctionNativeAbiId(sModuleAbiId, pFunc->sAbiSymbol, pFunc->sNativeAbi, sNativeAbiId, sizeof(sNativeAbiId));

				if ( bNeedModulePrefix ) {
					__xlXleBufAppend(&buf, "\n");
					bNeedModulePrefix = 0;
				}
				__xlXleBufAppend(&buf, "\t\t{\n");
				__xlXleBufAppend(&buf, "\t\t\t\"module\": \"");
				__xlXleAppendEscaped(&buf, pModule->sKey);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"moduleAbiId\": \"");
				__xlXleAppendEscaped(&buf, sModuleAbiId);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"exportName\": \"");
				__xlXleAppendEscaped(&buf, pModule->pExports[j].sName);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"receiverType\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sReceiverType);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"methodName\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sMethodName);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"symbol\": \"");
				__xlXleAppendEscaped(&buf, pModule->pExports[j].sSymbol);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"overloadGroup\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sOverloadGroup);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"signatureId\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sSignatureId);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"dispatchKey\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sDispatchKey);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"languageAbi\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sLanguageAbi);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"nativeAbi\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sNativeAbi);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"languageAbiId\": \"");
				__xlXleAppendEscaped(&buf, sLanguageAbiId);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"nativeAbiId\": \"");
				__xlXleAppendEscaped(&buf, sNativeAbiId);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"abiSymbol\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sAbiSymbol);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"returnType\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sReturnTypeName);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"storageReturnType\": \"");
				__xlXleAppendEscaped(&buf, pFunc->sStorageReturnTypeName);
				__xlXleBufAppend(&buf, "\",\n");
				__xlXleBufAppendFormat(&buf, "\t\t\t\"returnCount\": %llu,\n", (unsigned long long)pFunc->iReturnCount);
				__xlXleBufAppend(&buf, "\t\t\t\"returns\": ");
				__xlXleAppendReturnsJson(&buf, pFunc, 3);
				__xlXleBufAppend(&buf, ",\n");
				__xlXleBufAppendFormat(&buf, "\t\t\t\"async\": %d,\n", pFunc->bAsync ? 1 : 0);
				__xlXleBufAppendFormat(&buf, "\t\t\t\"generator\": %d,\n", pFunc->bGenerator ? 1 : 0);
				__xlXleBufAppendFormat(&buf, "\t\t\t\"varargKind\": %d,\n", pFunc->eVarArgKind);
				__xlXleBufAppendFormat(&buf, "\t\t\t\"typeParamCount\": %llu,\n", (unsigned long long)pFunc->iTypeParamCount);
				__xlXleBufAppend(&buf, "\t\t\t\"typeParams\": ");
				__xlXleAppendTypeParamsJson(&buf, pFunc, 3);
				__xlXleBufAppend(&buf, ",\n");
				__xlXleBufAppend(&buf, "\t\t\t\"params\": ");
				__xlXleAppendParamsJson(&buf, pFunc, 3);
				__xlXleBufAppend(&buf, "\n\t\t},\n");
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
			__xlXleBufAppend(&buf, "\t");
		}
	}
	__xlXleBufAppend(&buf, "]\n}\n");
	return buf.sData;
}
