#include "xl_func.h"

typedef struct XL_FuncTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_FuncTextBuffer;

static char* __xlFuncDupText(const char* sText)
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

static int __xlFuncBufGrow(XL_FuncTextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( (pBuf->iLen + iNeed + 1) <= pBuf->iCap ) {
		return XL_RC_OK;
	}

	iNewCap = (pBuf->iCap == 0) ? 64 : pBuf->iCap;
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

static int __xlFuncBufAppend(XL_FuncTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return XL_RC_OK;
	}

	iLen = strlen(sText);
	if ( __xlFuncBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return XL_RC_OK;
}

static int __xlFuncBufAppendFormat(XL_FuncTextBuffer* pBuf, const char* sFormat, ...)
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
		return XL_RC_IO_ERROR;
	}
	if ( __xlFuncBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return XL_RC_IO_ERROR;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
	return XL_RC_OK;
}

static int __xlFuncIsIdentByte(unsigned char ch)
{
	return ((ch >= 'a') && (ch <= 'z')) ||
		((ch >= 'A') && (ch <= 'Z')) ||
		((ch >= '0') && (ch <= '9')) ||
		(ch == '_');
}

static int __xlFuncBufAppendSanitized(XL_FuncTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return __xlFuncBufAppend(pBuf, "void");
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		if ( __xlFuncIsIdentByte(*pScan) ) {
			char sTmp[2];

			sTmp[0] = (char)(*pScan);
			sTmp[1] = '\0';
			if ( __xlFuncBufAppend(pBuf, sTmp) != XL_RC_OK ) {
				return XL_RC_IO_ERROR;
			}
		} else {
			if ( __xlFuncBufAppendFormat(pBuf, "_x%02X", (unsigned int)(*pScan)) != XL_RC_OK ) {
				return XL_RC_IO_ERROR;
			}
		}
	}

	return XL_RC_OK;
}

static char* __xlFuncBufTake(XL_FuncTextBuffer* pBuf)
{
	char* sData;

	if ( pBuf == NULL ) {
		return NULL;
	}

	sData = pBuf->sData;
	pBuf->sData = NULL;
	pBuf->iLen = 0;
	pBuf->iCap = 0;
	return sData;
}

static void __xlFuncBufFree(XL_FuncTextBuffer* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}

	free(pBuf->sData);
	memset(pBuf, 0, sizeof(*pBuf));
}

static char* __xlFuncBuildOverloadGroup(const char* sName)
{
	XL_FuncTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	if ( __xlFuncBufAppend(&buf, "global::") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	if ( __xlFuncBufAppend(&buf, (sName != NULL) ? sName : "") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	return __xlFuncBufTake(&buf);
}

static char* __xlFuncBuildSignatureId(const char* sName, const char* const* arrParamTypes, size_t iParamCount)
{
	XL_FuncTextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	if ( (__xlFuncBufAppend(&buf, (sName != NULL) ? sName : "") != XL_RC_OK) ||
		(__xlFuncBufAppend(&buf, "(") != XL_RC_OK) ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}

	for ( i = 0; i < iParamCount; ++i ) {
		if ( i != 0 ) {
			if ( __xlFuncBufAppend(&buf, ",") != XL_RC_OK ) {
				__xlFuncBufFree(&buf);
				return NULL;
			}
		}
		if ( __xlFuncBufAppend(&buf, ((arrParamTypes != NULL) && (arrParamTypes[i] != NULL)) ? arrParamTypes[i] : "") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
	}

	if ( __xlFuncBufAppend(&buf, ")") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	return __xlFuncBufTake(&buf);
}

static char* __xlFuncBuildDispatchKey(
	const char* sName,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount
)
{
	XL_FuncTextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	if ( __xlFuncBufAppend(&buf, (sName != NULL) ? sName : "") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	if ( __xlFuncBufAppendFormat(&buf, "$arity%llu", (unsigned long long)iParamCount) != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}

	for ( i = 0; i < iParamCount; ++i ) {
		if ( __xlFuncBufAppend(&buf, "|") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppend(&buf, ((arrParamNames != NULL) && (arrParamNames[i] != NULL)) ? arrParamNames[i] : "") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppend(&buf, ":") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppend(&buf, ((arrParamTypes != NULL) && (arrParamTypes[i] != NULL)) ? arrParamTypes[i] : "") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
	}

	return __xlFuncBufTake(&buf);
}

static char* __xlFuncBuildAbiSymbol(
	const char* sName,
	const char* sReturnTypeName,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount
)
{
	XL_FuncTextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	if ( __xlFuncBufAppend(&buf, "xl__") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	if ( __xlFuncBufAppendSanitized(&buf, (sName != NULL) ? sName : "") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	if ( __xlFuncBufAppendFormat(&buf, "__arity%llu", (unsigned long long)iParamCount) != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	for ( i = 0; i < iParamCount; ++i ) {
		if ( __xlFuncBufAppend(&buf, "__") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppendSanitized(&buf, ((arrParamNames != NULL) && (arrParamNames[i] != NULL)) ? arrParamNames[i] : "") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppend(&buf, "__") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
		if ( __xlFuncBufAppendSanitized(&buf, ((arrParamTypes != NULL) && (arrParamTypes[i] != NULL)) ? arrParamTypes[i] : "") != XL_RC_OK ) {
			__xlFuncBufFree(&buf);
			return NULL;
		}
	}
	if ( __xlFuncBufAppend(&buf, "__ret__") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	if ( __xlFuncBufAppendSanitized(&buf, (sReturnTypeName != NULL) ? sReturnTypeName : "void") != XL_RC_OK ) {
		__xlFuncBufFree(&buf);
		return NULL;
	}
	return __xlFuncBufTake(&buf);
}

static int __xlFuncTableGrow(XL_FuncTable* pTable)
{
	XL_FuncInfo* pNewItems;
	size_t iNewCapacity;

	if ( pTable == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( pTable->iCount < pTable->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCapacity = (pTable->iCapacity == 0) ? 8 : (pTable->iCapacity * 2);
	pNewItems = (XL_FuncInfo*)realloc(pTable->pItems, sizeof(XL_FuncInfo) * iNewCapacity);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pTable->pItems = pNewItems;
	pTable->iCapacity = iNewCapacity;
	return XL_RC_OK;
}

static void __xlFuncInfoFree(XL_FuncInfo* pInfo)
{
	size_t i;

	if ( pInfo == NULL ) {
		return;
	}

	free(pInfo->sName);
	free(pInfo->sReceiverType);
	free(pInfo->sMethodName);
	free(pInfo->sReturnTypeName);
	free(pInfo->sStorageReturnTypeName);
	free(pInfo->sGeneratorValueTypeName);
	free(pInfo->sOverloadGroup);
	free(pInfo->sSignatureId);
	free(pInfo->sDispatchKey);
	free(pInfo->sLanguageAbi);
	free(pInfo->sNativeAbi);
	free(pInfo->sAbiSymbol);
	for ( i = 0; i < pInfo->iParamCount; ++i ) {
		free(pInfo->arrParamNames[i]);
		free(pInfo->arrParamTypes[i]);
		if ( pInfo->arrGenericParamTypes != NULL ) {
			free(pInfo->arrGenericParamTypes[i]);
		}
	}
	for ( i = 0; i < pInfo->iReturnCount; ++i ) {
		free(pInfo->arrReturnTypeNames[i]);
		if ( pInfo->arrGenericReturnTypeNames != NULL ) {
			free(pInfo->arrGenericReturnTypeNames[i]);
		}
	}
	for ( i = 0; i < pInfo->iTypeParamCount; ++i ) {
		free(pInfo->arrTypeParamNames[i]);
	}
	for ( i = 0; i < pInfo->iGenericInstanceTypeArgCount; ++i ) {
		free(pInfo->arrGenericInstanceTypeArgs[i]);
	}
	for ( i = 0; i < pInfo->iCaptureCount; ++i ) {
		free(pInfo->arrCaptureNames[i]);
		free(pInfo->arrCaptureTypeNames[i]);
	}
	free(pInfo->arrParamNames);
	free(pInfo->arrParamTypes);
	free(pInfo->arrReturnTypeNames);
	free(pInfo->arrTypeParamNames);
	free(pInfo->arrGenericParamTypes);
	free(pInfo->arrGenericReturnTypeNames);
	free(pInfo->arrGenericInstanceTypeArgs);
	free(pInfo->arrCaptureNames);
	free(pInfo->arrCaptureTypeNames);
	free(pInfo->arrCaptureSymbolIndexes);
	free(pInfo->arrParamHasDefault);
	free((void*)pInfo->arrParamDefaultValues);
	memset(pInfo, 0, sizeof(*pInfo));
}

void xlFuncTableInit(XL_FuncTable* pTable)
{
	if ( pTable != NULL ) {
		memset(pTable, 0, sizeof(*pTable));
	}
}

void xlFuncTableFree(XL_FuncTable* pTable)
{
	size_t i;

	if ( pTable == NULL ) {
		return;
	}

	for ( i = 0; i < pTable->iCount; ++i ) {
		__xlFuncInfoFree(&pTable->pItems[i]);
	}
	free(pTable->pItems);
	memset(pTable, 0, sizeof(*pTable));
}

int xlFuncTableAddEx(
	XL_FuncTable* pTable,
	const char* sName,
	const char* sReceiverType,
	const char* sMethodName,
	const char* sReturnTypeName,
	const char* sStorageReturnTypeName,
	const char* const* arrReturnTypeNames,
	size_t iReturnCount,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount,
	const unsigned char* arrParamHasDefault,
	const XL_AstNode* const* arrParamDefaultValues,
	int iScopeId,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator,
	int eVarArgKind
)
{
	XL_FuncInfo* pInfo;
	size_t i;

	if ( (pTable == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}
	if ( __xlFuncTableGrow(pTable) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pInfo = &pTable->pItems[pTable->iCount];
	memset(pInfo, 0, sizeof(*pInfo));

	pInfo->sName = __xlFuncDupText(sName);
	pInfo->sReceiverType = __xlFuncDupText(sReceiverType);
	pInfo->sMethodName = __xlFuncDupText(sMethodName);
	pInfo->sReturnTypeName = __xlFuncDupText(sReturnTypeName);
	pInfo->sStorageReturnTypeName = __xlFuncDupText((sStorageReturnTypeName != NULL) ? sStorageReturnTypeName : sReturnTypeName);
	pInfo->sGeneratorValueTypeName = __xlFuncDupText(bGenerator ? ((sReturnTypeName != NULL) ? sReturnTypeName : "") : "");
	pInfo->sOverloadGroup = __xlFuncBuildOverloadGroup(sName);
	pInfo->sSignatureId = __xlFuncBuildSignatureId(sName, arrParamTypes, iParamCount);
	pInfo->sDispatchKey = __xlFuncBuildDispatchKey(sName, arrParamNames, arrParamTypes, iParamCount);
	pInfo->sLanguageAbi = __xlFuncDupText(XL_FUNC_LANGUAGE_ABI_V1);
	pInfo->sNativeAbi = __xlFuncDupText(XL_FUNC_NATIVE_ABI_V1);
	pInfo->sAbiSymbol = __xlFuncBuildAbiSymbol(sName, (sStorageReturnTypeName != NULL) ? sStorageReturnTypeName : sReturnTypeName, arrParamNames, arrParamTypes, iParamCount);
	pInfo->iScopeId = iScopeId;
	pInfo->iLine = iLine;
	pInfo->iColumn = iColumn;
	pInfo->bAsync = bAsync;
	pInfo->bGenerator = bGenerator;
	pInfo->eVarArgKind = eVarArgKind;
	pInfo->iGenericBaseFuncIndex = -1;
	pInfo->iParamCount = iParamCount;
	pInfo->iReturnCount = iReturnCount;

	if ( iParamCount != 0 ) {
		pInfo->arrParamNames = (char**)calloc(iParamCount, sizeof(char*));
		pInfo->arrParamTypes = (char**)calloc(iParamCount, sizeof(char*));
		pInfo->arrParamHasDefault = (unsigned char*)calloc(iParamCount, sizeof(unsigned char));
		pInfo->arrParamDefaultValues = (const XL_AstNode**)calloc(iParamCount, sizeof(XL_AstNode*));
		if ( (pInfo->arrParamNames == NULL) || (pInfo->arrParamTypes == NULL) ||
			(pInfo->arrParamHasDefault == NULL) || (pInfo->arrParamDefaultValues == NULL) ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}

		for ( i = 0; i < iParamCount; ++i ) {
			pInfo->arrParamNames[i] = __xlFuncDupText((arrParamNames != NULL) ? arrParamNames[i] : "");
			pInfo->arrParamTypes[i] = __xlFuncDupText((arrParamTypes != NULL) ? arrParamTypes[i] : "");
			pInfo->arrParamHasDefault[i] = (arrParamHasDefault != NULL) ? arrParamHasDefault[i] : 0;
			pInfo->arrParamDefaultValues[i] = (arrParamDefaultValues != NULL) ? arrParamDefaultValues[i] : NULL;
			if ( (pInfo->arrParamNames[i] == NULL) || (pInfo->arrParamTypes[i] == NULL) ) {
				__xlFuncInfoFree(pInfo);
				return XL_RC_IO_ERROR;
			}
		}
	}

	if ( iReturnCount != 0 ) {
		pInfo->arrReturnTypeNames = (char**)calloc(iReturnCount, sizeof(char*));
		if ( pInfo->arrReturnTypeNames == NULL ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}
		for ( i = 0; i < iReturnCount; ++i ) {
			pInfo->arrReturnTypeNames[i] = __xlFuncDupText((arrReturnTypeNames != NULL) ? arrReturnTypeNames[i] : "");
			if ( pInfo->arrReturnTypeNames[i] == NULL ) {
				__xlFuncInfoFree(pInfo);
				return XL_RC_IO_ERROR;
			}
		}
	}

	if ( (pInfo->sName == NULL) || (pInfo->sReceiverType == NULL) || (pInfo->sMethodName == NULL) ||
		(pInfo->sReturnTypeName == NULL) || (pInfo->sStorageReturnTypeName == NULL) ||
		(pInfo->sGeneratorValueTypeName == NULL) ||
		(pInfo->sOverloadGroup == NULL) || (pInfo->sSignatureId == NULL) ||
		(pInfo->sDispatchKey == NULL) || (pInfo->sLanguageAbi == NULL) ||
		(pInfo->sNativeAbi == NULL) || (pInfo->sAbiSymbol == NULL) ) {
		__xlFuncInfoFree(pInfo);
		return XL_RC_IO_ERROR;
	}

	pTable->iCount += 1;
	return XL_RC_OK;
}

int xlFuncTableAdd(
	XL_FuncTable* pTable,
	const char* sName,
	const char* sReceiverType,
	const char* sMethodName,
	const char* sReturnTypeName,
	const char* const* arrParamNames,
	const char* const* arrParamTypes,
	size_t iParamCount,
	const unsigned char* arrParamHasDefault,
	const XL_AstNode* const* arrParamDefaultValues,
	int iScopeId,
	int iLine,
	int iColumn,
	int bAsync,
	int bGenerator,
	int eVarArgKind
)
{
	const char* arrReturnTypes[1];
	size_t iReturnCount;

	iReturnCount = ((sReturnTypeName != NULL) && (sReturnTypeName[0] != '\0') && (strcmp(sReturnTypeName, "void") != 0)) ? 1 : 0;
	arrReturnTypes[0] = sReturnTypeName;
	return xlFuncTableAddEx(
		pTable,
		sName,
		sReceiverType,
		sMethodName,
		(iReturnCount != 0) ? sReturnTypeName : "",
		(iReturnCount != 0) ? sReturnTypeName : "void",
		(iReturnCount != 0) ? arrReturnTypes : NULL,
		iReturnCount,
		arrParamNames,
		arrParamTypes,
		iParamCount,
		arrParamHasDefault,
		arrParamDefaultValues,
		iScopeId,
		iLine,
		iColumn,
		bAsync,
		bGenerator,
		eVarArgKind
	);
}

const XL_FuncInfo* xlFuncTableGet(const XL_FuncTable* pTable, size_t iIndex)
{
	if ( (pTable == NULL) || (iIndex >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iIndex];
}

XL_FuncInfo* xlFuncTableGetMutable(XL_FuncTable* pTable, size_t iIndex)
{
	if ( (pTable == NULL) || (iIndex >= pTable->iCount) ) {
		return NULL;
	}

	return &pTable->pItems[iIndex];
}

int xlFuncInfoAddCapture(XL_FuncInfo* pInfo, const char* sName, const char* sTypeName, int iSymbolIndex)
{
	char** arrCaptureNames;
	char** arrCaptureTypes;
	int* arrCaptureSymbols;
	size_t i;

	if ( (pInfo == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pInfo->iCaptureCount; ++i ) {
		if ( (pInfo->arrCaptureNames[i] != NULL) && (strcmp(pInfo->arrCaptureNames[i], sName) == 0) ) {
			return XL_RC_OK;
		}
	}

	arrCaptureNames = (char**)realloc(pInfo->arrCaptureNames, sizeof(char*) * (pInfo->iCaptureCount + 1u));
	if ( arrCaptureNames == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pInfo->arrCaptureNames = arrCaptureNames;

	arrCaptureTypes = (char**)realloc(pInfo->arrCaptureTypeNames, sizeof(char*) * (pInfo->iCaptureCount + 1u));
	if ( arrCaptureTypes == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pInfo->arrCaptureTypeNames = arrCaptureTypes;

	arrCaptureSymbols = (int*)realloc(pInfo->arrCaptureSymbolIndexes, sizeof(int) * (pInfo->iCaptureCount + 1u));
	if ( arrCaptureSymbols == NULL ) {
		return XL_RC_IO_ERROR;
	}
	pInfo->arrCaptureSymbolIndexes = arrCaptureSymbols;

	pInfo->arrCaptureNames[pInfo->iCaptureCount] = __xlFuncDupText(sName);
	pInfo->arrCaptureTypeNames[pInfo->iCaptureCount] = __xlFuncDupText((sTypeName != NULL) ? sTypeName : "var");
	pInfo->arrCaptureSymbolIndexes[pInfo->iCaptureCount] = iSymbolIndex;
	if ( (pInfo->arrCaptureNames[pInfo->iCaptureCount] == NULL) || (pInfo->arrCaptureTypeNames[pInfo->iCaptureCount] == NULL) ) {
		free(pInfo->arrCaptureNames[pInfo->iCaptureCount]);
		free(pInfo->arrCaptureTypeNames[pInfo->iCaptureCount]);
		pInfo->arrCaptureNames[pInfo->iCaptureCount] = NULL;
		pInfo->arrCaptureTypeNames[pInfo->iCaptureCount] = NULL;
		return XL_RC_IO_ERROR;
	}

	pInfo->iCaptureCount += 1u;
	return XL_RC_OK;
}

int xlFuncInfoSetGenerics(
	XL_FuncInfo* pInfo,
	const char* const* arrTypeParamNames,
	size_t iTypeParamCount,
	const char* const* arrGenericParamTypes,
	size_t iGenericParamCount,
	const char* const* arrGenericReturnTypeNames,
	size_t iGenericReturnCount
)
{
	size_t i;

	if ( pInfo == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( iTypeParamCount == 0 ) {
		return XL_RC_OK;
	}
	if ( iGenericParamCount != pInfo->iParamCount || iGenericReturnCount != pInfo->iReturnCount ) {
		return XL_RC_BAD_ARG;
	}

	pInfo->arrTypeParamNames = (char**)calloc(iTypeParamCount, sizeof(char*));
	pInfo->arrGenericParamTypes = (char**)calloc((iGenericParamCount != 0) ? iGenericParamCount : 1, sizeof(char*));
	pInfo->arrGenericReturnTypeNames = (char**)calloc((iGenericReturnCount != 0) ? iGenericReturnCount : 1, sizeof(char*));
	if ( (pInfo->arrTypeParamNames == NULL) || (pInfo->arrGenericParamTypes == NULL) || (pInfo->arrGenericReturnTypeNames == NULL) ) {
		__xlFuncInfoFree(pInfo);
		return XL_RC_IO_ERROR;
	}

	for ( i = 0; i < iTypeParamCount; ++i ) {
		pInfo->arrTypeParamNames[i] = __xlFuncDupText((arrTypeParamNames != NULL) ? arrTypeParamNames[i] : "");
		if ( pInfo->arrTypeParamNames[i] == NULL ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}
	}
	for ( i = 0; i < iGenericParamCount; ++i ) {
		pInfo->arrGenericParamTypes[i] = __xlFuncDupText((arrGenericParamTypes != NULL) ? arrGenericParamTypes[i] : "");
		if ( pInfo->arrGenericParamTypes[i] == NULL ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}
	}
	for ( i = 0; i < iGenericReturnCount; ++i ) {
		pInfo->arrGenericReturnTypeNames[i] = __xlFuncDupText((arrGenericReturnTypeNames != NULL) ? arrGenericReturnTypeNames[i] : "");
		if ( pInfo->arrGenericReturnTypeNames[i] == NULL ) {
			__xlFuncInfoFree(pInfo);
			return XL_RC_IO_ERROR;
		}
	}
	pInfo->iTypeParamCount = iTypeParamCount;
	return XL_RC_OK;
}

int xlFuncInfoSetGenericInstance(
	XL_FuncInfo* pInfo,
	int iGenericBaseFuncIndex,
	const char* const* arrTypeArgs,
	size_t iTypeArgCount
)
{
	size_t i;

	if ( pInfo == NULL ) {
		return XL_RC_BAD_ARG;
	}

	for ( i = 0; i < pInfo->iGenericInstanceTypeArgCount; ++i ) {
		free(pInfo->arrGenericInstanceTypeArgs[i]);
	}
	free(pInfo->arrGenericInstanceTypeArgs);
	pInfo->arrGenericInstanceTypeArgs = NULL;
	pInfo->iGenericInstanceTypeArgCount = 0;
	pInfo->iGenericBaseFuncIndex = iGenericBaseFuncIndex;

	if ( iTypeArgCount == 0 ) {
		return XL_RC_OK;
	}

	pInfo->arrGenericInstanceTypeArgs = (char**)calloc(iTypeArgCount, sizeof(char*));
	if ( pInfo->arrGenericInstanceTypeArgs == NULL ) {
		return XL_RC_IO_ERROR;
	}

	for ( i = 0; i < iTypeArgCount; ++i ) {
		pInfo->arrGenericInstanceTypeArgs[i] = __xlFuncDupText((arrTypeArgs != NULL) ? arrTypeArgs[i] : "");
		if ( pInfo->arrGenericInstanceTypeArgs[i] == NULL ) {
			return XL_RC_IO_ERROR;
		}
	}

	pInfo->iGenericInstanceTypeArgCount = iTypeArgCount;
	return XL_RC_OK;
}
