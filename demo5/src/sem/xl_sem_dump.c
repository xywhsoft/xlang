#include "xl_sem_dump.h"

typedef struct XL_SemTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_SemTextBuffer;

static int __xlSemBufGrow(XL_SemTextBuffer* pBuf, size_t iNeed)
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

static void __xlSemBufAppend(XL_SemTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlSemBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlSemBufAppendFormat(XL_SemTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlSemBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlSemAppendEscaped(XL_SemTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlSemBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlSemBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlSemBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlSemBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlSemBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlSemBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];
					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlSemBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static void __xlSemDumpTypesJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	const XL_TypeInfo* pType;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->types.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->types.iCount); ++i ) {
		pType = xlTypeTableGet(&pResult->types, i);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"name\": \"");
		__xlSemAppendEscaped(pBuf, pType->sName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"builtin\": %d,\n", pType->bBuiltin);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"line\": %d,\n", pType->iLine);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"column\": %d\n", pType->iColumn);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->types.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->types.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

static void __xlSemDumpFuncsJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	size_t k;
	const XL_FuncInfo* pFunc;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->funcs.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->funcs.iCount); ++i ) {
		pFunc = xlFuncTableGet(&pResult->funcs, i);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"name\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"overloadGroup\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sOverloadGroup);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"signatureId\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sSignatureId);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"dispatchKey\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sDispatchKey);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"abiSymbol\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sAbiSymbol);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"languageAbi\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sLanguageAbi);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"nativeAbi\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sNativeAbi);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"returnType\": \"");
		__xlSemAppendEscaped(pBuf, pFunc->sReturnTypeName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"scopeId\": %d,\n", pFunc->iScopeId);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"async\": %d,\n", pFunc->bAsync);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"generator\": %d,\n", pFunc->bGenerator);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"line\": %d,\n", pFunc->iLine);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"column\": %d,\n", pFunc->iColumn);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"params\": [");
		if ( pFunc->iParamCount != 0 ) {
			__xlSemBufAppend(pBuf, "\n");
		}
		for ( k = 0; k < pFunc->iParamCount; ++k ) {
			for ( j = 0; j < (size_t)(iIndent + 3); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppend(pBuf, "{\n");
			for ( j = 0; j < (size_t)(iIndent + 4); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppend(pBuf, "\"name\": \"");
			__xlSemAppendEscaped(pBuf, pFunc->arrParamNames[k]);
			__xlSemBufAppend(pBuf, "\",\n");
			for ( j = 0; j < (size_t)(iIndent + 4); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppend(pBuf, "\"label\": \"");
			__xlSemAppendEscaped(pBuf, pFunc->arrParamNames[k]);
			__xlSemBufAppend(pBuf, "\",\n");
			for ( j = 0; j < (size_t)(iIndent + 4); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppend(pBuf, "\"type\": \"");
			__xlSemAppendEscaped(pBuf, pFunc->arrParamTypes[k]);
			__xlSemBufAppend(pBuf, "\",\n");
			for ( j = 0; j < (size_t)(iIndent + 4); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppendFormat(pBuf, "\"hasDefault\": %d\n", ((pFunc->arrParamHasDefault != NULL) && pFunc->arrParamHasDefault[k]) ? 1 : 0);
			for ( j = 0; j < (size_t)(iIndent + 3); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
			__xlSemBufAppend(pBuf, "}");
			if ( (k + 1) != pFunc->iParamCount ) {
				__xlSemBufAppend(pBuf, ",");
			}
			__xlSemBufAppend(pBuf, "\n");
		}
		if ( pFunc->iParamCount != 0 ) {
			for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
				__xlSemBufAppend(pBuf, "\t");
			}
		}
		__xlSemBufAppend(pBuf, "]\n");
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->funcs.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->funcs.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

static void __xlSemDumpClassesJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	const XL_ClassInfo* pClass;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->classes.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->classes.iCount); ++i ) {
		pClass = &pResult->classes.pItems[i];
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"name\": \"");
		__xlSemAppendEscaped(pBuf, pClass->sName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"baseName\": \"");
		__xlSemAppendEscaped(pBuf, (pClass->sBaseName != NULL) ? pClass->sBaseName : "");
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"isObject\": %d,\n", pClass->bIsObject ? 1 : 0);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"isCustom\": %d,\n", pClass->bIsCustom ? 1 : 0);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"size\": %d,\n", pClass->iSize);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"align\": %d,\n", pClass->iAlign);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"hasCreate\": %d,\n", pClass->bHasCreate ? 1 : 0);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"hasDestroy\": %d,\n", pClass->bHasDestroy ? 1 : 0);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"fieldCount\": %llu,\n", (unsigned long long)pClass->iFieldCount);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"createCount\": %llu\n", (unsigned long long)pClass->iCreateCount);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->classes.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->classes.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

static void __xlSemDumpScopesJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	const XL_ScopeInfo* pScope;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->scopes.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->scopes.iCount); ++i ) {
		pScope = xlScopeTableGet(&pResult->scopes, (int)i);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"id\": %d,\n", pScope->iId);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"parentId\": %d,\n", pScope->iParentId);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"kind\": \"%s\",\n", xlScopeKindName(pScope->eKind));
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"name\": \"");
		__xlSemAppendEscaped(pBuf, pScope->sName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"depth\": %d,\n", pScope->iDepth);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"line\": %d,\n", pScope->iLine);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"column\": %d\n", pScope->iColumn);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->scopes.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->scopes.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

static void __xlSemDumpSymbolsJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	const XL_SymbolInfo* pSymbol;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->symbols.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->symbols.iCount); ++i ) {
		pSymbol = xlSymbolTableGet(&pResult->symbols, i);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"kind\": \"%s\",\n", xlSymbolKindName(pSymbol->eKind));
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"scopeId\": %d,\n", pSymbol->iScopeId);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"name\": \"");
		__xlSemAppendEscaped(pBuf, pSymbol->sName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"typeName\": \"");
		__xlSemAppendEscaped(pBuf, pSymbol->sTypeName);
		__xlSemBufAppend(pBuf, "\",\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"line\": %d,\n", pSymbol->iLine);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"column\": %d,\n", pSymbol->iColumn);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"async\": %d,\n", pSymbol->bAsync);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"generator\": %d\n", pSymbol->bGenerator);
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->symbols.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->symbols.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

static void __xlSemDumpDiagsJsonTo(const XL_SemResult* pResult, XL_SemTextBuffer* pBuf, int iIndent)
{
	size_t i;
	size_t j;
	const XL_SemDiagItem* pDiag;

	__xlSemBufAppend(pBuf, "[");
	if ( (pResult != NULL) && (pResult->diags.iCount != 0) ) {
		__xlSemBufAppend(pBuf, "\n");
	}

	for ( i = 0; (pResult != NULL) && (i < pResult->diags.iCount); ++i ) {
		pDiag = &pResult->diags.pItems[i];
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "{\n");
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"level\": \"%s\",\n", xlSemDiagLevelName(pDiag->eLevel));
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"line\": %d,\n", pDiag->iLine);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppendFormat(pBuf, "\"column\": %d,\n", pDiag->iColumn);
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "\"message\": \"");
		__xlSemAppendEscaped(pBuf, pDiag->sMessage);
		__xlSemBufAppend(pBuf, "\"\n");
		for ( j = 0; j < (size_t)(iIndent + 1); ++j ) {
			__xlSemBufAppend(pBuf, "\t");
		}
		__xlSemBufAppend(pBuf, "}");
		if ( (i + 1) != pResult->diags.iCount ) {
			__xlSemBufAppend(pBuf, ",");
		}
		__xlSemBufAppend(pBuf, "\n");
	}

	if ( (pResult != NULL) && (pResult->diags.iCount != 0) ) {
		for ( i = 0; i < (size_t)iIndent; ++i ) {
			__xlSemBufAppend(pBuf, "\t");
		}
	}
	__xlSemBufAppend(pBuf, "]");
}

char* xlSemDumpText(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;
	size_t i;
	const XL_TypeInfo* pType;
	const XL_ClassInfo* pClass;
	const XL_ScopeInfo* pScope;
	const XL_SymbolInfo* pSymbol;
	const XL_SemDiagItem* pDiag;

	memset(&buf, 0, sizeof(buf));
	__xlSemBufAppendFormat(&buf, "SemanticResult hadError=%d\n", (pResult != NULL) ? pResult->bHadError : 0);

	__xlSemBufAppendFormat(&buf, "Types (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->types.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->types.iCount); ++i ) {
		pType = xlTypeTableGet(&pResult->types, i);
		__xlSemBufAppendFormat(&buf, "\t- %s builtin=%d @ %d:%d\n", pType->sName, pType->bBuiltin, pType->iLine, pType->iColumn);
	}

	__xlSemBufAppendFormat(&buf, "Classes (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->classes.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->classes.iCount); ++i ) {
		size_t j;

		pClass = &pResult->classes.pItems[i];
		__xlSemBufAppendFormat(
			&buf,
			"\t- %s kind=%s extends=%s size=%d align=%d create=%d destroy=%d fields=%llu creates=%llu\n",
			pClass->sName,
			pClass->bIsCustom ? "custom" : (pClass->bIsObject ? "object" : "plain"),
			(pClass->sBaseName != NULL) ? pClass->sBaseName : "",
			pClass->iSize,
			pClass->iAlign,
			pClass->bHasCreate,
			pClass->bHasDestroy,
			(unsigned long long)pClass->iFieldCount,
			(unsigned long long)pClass->iCreateCount
		);
		for ( j = 0; j < pClass->iFieldCount; ++j ) {
			__xlSemBufAppendFormat(
				&buf,
				"\t\t- %s type=%s offset=%d size=%d align=%d union=%s\n",
				pClass->pFields[j].sName,
				pClass->pFields[j].sTypeName,
				pClass->pFields[j].iOffset,
				pClass->pFields[j].iSize,
				pClass->pFields[j].iAlign,
				(pClass->pFields[j].sUnionName != NULL) ? pClass->pFields[j].sUnionName : ""
			);
		}
	}

	__xlSemBufAppendFormat(&buf, "Functions (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->funcs.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->funcs.iCount); ++i ) {
		const XL_FuncInfo* pFunc;
		size_t j;

		pFunc = xlFuncTableGet(&pResult->funcs, i);
		__xlSemBufAppendFormat(
			&buf,
			"\t- %s(",
			pFunc->sName
		);
		for ( j = 0; j < pFunc->iParamCount; ++j ) {
			__xlSemBufAppendFormat(
				&buf,
				"%s%s %s",
				(j == 0) ? "" : ", ",
				pFunc->arrParamTypes[j],
				pFunc->arrParamNames[j]
			);
		}
		__xlSemBufAppendFormat(
			&buf,
			") as %s scope=%d async=%d generator=%d abi=%s sig=%s @ %d:%d\n",
			pFunc->sReturnTypeName,
			pFunc->iScopeId,
			pFunc->bAsync,
			pFunc->bGenerator,
			(pFunc->sAbiSymbol != NULL) ? pFunc->sAbiSymbol : "",
			(pFunc->sSignatureId != NULL) ? pFunc->sSignatureId : "",
			pFunc->iLine,
			pFunc->iColumn
		);
	}

	__xlSemBufAppendFormat(&buf, "Scopes (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->scopes.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->scopes.iCount); ++i ) {
		pScope = xlScopeTableGet(&pResult->scopes, (int)i);
		__xlSemBufAppendFormat(
			&buf,
			"\t- #%d %s \"%s\" parent=%d depth=%d @ %d:%d\n",
			pScope->iId,
			xlScopeKindName(pScope->eKind),
			pScope->sName,
			pScope->iParentId,
			pScope->iDepth,
			pScope->iLine,
			pScope->iColumn
		);
	}

	__xlSemBufAppendFormat(&buf, "Symbols (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->symbols.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->symbols.iCount); ++i ) {
		pSymbol = xlSymbolTableGet(&pResult->symbols, i);
		__xlSemBufAppendFormat(
			&buf,
			"\t- %s %s scope=%d type=%s async=%d generator=%d @ %d:%d\n",
			xlSymbolKindName(pSymbol->eKind),
			pSymbol->sName,
			pSymbol->iScopeId,
			pSymbol->sTypeName,
			pSymbol->bAsync,
			pSymbol->bGenerator,
			pSymbol->iLine,
			pSymbol->iColumn
		);
	}

	__xlSemBufAppendFormat(&buf, "Diagnostics (%llu)\n", (unsigned long long)((pResult != NULL) ? pResult->diags.iCount : 0));
	for ( i = 0; (pResult != NULL) && (i < pResult->diags.iCount); ++i ) {
		pDiag = &pResult->diags.pItems[i];
		__xlSemBufAppendFormat(&buf, "\t- %s @ %d:%d %s\n", xlSemDiagLevelName(pDiag->eLevel), pDiag->iLine, pDiag->iColumn, pDiag->sMessage);
	}

	return buf.sData;
}

char* xlSemDumpJson(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlSemBufAppend(&buf, "{\n");
	__xlSemBufAppendFormat(&buf, "\t\"hadError\": %d,\n", (pResult != NULL) ? pResult->bHadError : 0);
	__xlSemBufAppend(&buf, "\t\"types\": ");
	__xlSemDumpTypesJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, ",\n");
	__xlSemBufAppend(&buf, "\t\"classes\": ");
	__xlSemDumpClassesJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, ",\n");
	__xlSemBufAppend(&buf, "\t\"funcs\": ");
	__xlSemDumpFuncsJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, ",\n");
	__xlSemBufAppend(&buf, "\t\"scopes\": ");
	__xlSemDumpScopesJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, ",\n");
	__xlSemBufAppend(&buf, "\t\"symbols\": ");
	__xlSemDumpSymbolsJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, ",\n");
	__xlSemBufAppend(&buf, "\t\"diagnostics\": ");
	__xlSemDumpDiagsJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, "\n}\n");
	return buf.sData;
}

char* xlSemDumpSymbolsJson(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlSemDumpSymbolsJsonTo(pResult, &buf, 0);
	__xlSemBufAppend(&buf, "\n");
	return buf.sData;
}

char* xlSemDumpFuncsJson(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlSemDumpFuncsJsonTo(pResult, &buf, 0);
	__xlSemBufAppend(&buf, "\n");
	return buf.sData;
}

char* xlSemDumpXleFunctionsJson(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlSemBufAppend(&buf, "{\n\t\"functions\": ");
	__xlSemDumpFuncsJsonTo(pResult, &buf, 1);
	__xlSemBufAppend(&buf, "\n}\n");
	return buf.sData;
}

char* xlSemDumpTypesJson(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlSemDumpTypesJsonTo(pResult, &buf, 0);
	__xlSemBufAppend(&buf, "\n");
	return buf.sData;
}

char* xlSemDumpDiagText(const XL_SemResult* pResult)
{
	XL_SemTextBuffer buf;
	size_t i;
	const XL_SemDiagItem* pDiag;

	memset(&buf, 0, sizeof(buf));
	for ( i = 0; (pResult != NULL) && (i < pResult->diags.iCount); ++i ) {
		pDiag = &pResult->diags.pItems[i];
		__xlSemBufAppendFormat(&buf, "%s @ %d:%d %s\n", xlSemDiagLevelName(pDiag->eLevel), pDiag->iLine, pDiag->iColumn, pDiag->sMessage);
	}

	if ( buf.sData == NULL ) {
		__xlSemBufAppend(&buf, "");
	}
	return buf.sData;
}
