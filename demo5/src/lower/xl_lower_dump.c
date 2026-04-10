#include "xl_lower.h"

typedef struct XL_LowerTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_LowerTextBuffer;

static int __xlLowerBufGrow(XL_LowerTextBuffer* pBuf, size_t iNeed)
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

static void __xlLowerBufAppend(XL_LowerTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlLowerBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlLowerBufAppendFormat(XL_LowerTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlLowerBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlLowerAppendEscaped(XL_LowerTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlLowerBufAppend(pBuf, "\\\\");
				break;
			case '"':
				__xlLowerBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlLowerBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlLowerBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlLowerBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlLowerBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];
					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlLowerBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static void __xlDumpHirText(XL_LowerTextBuffer* pBuf, const XL_HirNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < (size_t)iIndent; ++i ) {
		__xlLowerBufAppend(pBuf, "\t");
	}
	__xlLowerBufAppendFormat(pBuf, "%s @ %d:%d", xlHirKindName(pNode->eKind), pNode->iLine, pNode->iColumn);
	if ( pNode->sText != NULL ) {
		__xlLowerBufAppend(pBuf, " text=\"");
		__xlLowerAppendEscaped(pBuf, pNode->sText);
		__xlLowerBufAppend(pBuf, "\"");
	}
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		__xlLowerBufAppend(pBuf, " type=\"");
		__xlLowerAppendEscaped(pBuf, pNode->sTypeName);
		__xlLowerBufAppend(pBuf, "\"");
	}
	if ( pNode->iScopeId >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " scope=%d", pNode->iScopeId);
	}
	if ( pNode->iSymbolIndex >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " symbol=%d", pNode->iSymbolIndex);
	}
	if ( pNode->iFuncIndex >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " func=%d", pNode->iFuncIndex);
	}
	if ( pNode->iIntValue != 0 ) {
		__xlLowerBufAppendFormat(pBuf, " int=%lld", (long long)pNode->iIntValue);
	}
	if ( pNode->bFlag != 0 ) {
		__xlLowerBufAppendFormat(pBuf, " flag=%d", pNode->bFlag);
	}
	__xlLowerBufAppend(pBuf, "\n");

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlDumpHirText(pBuf, pNode->ppChildren[i], iIndent + 1);
	}
}

static void __xlDumpMirText(XL_LowerTextBuffer* pBuf, const XL_MirNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < (size_t)iIndent; ++i ) {
		__xlLowerBufAppend(pBuf, "\t");
	}
	__xlLowerBufAppendFormat(pBuf, "%s @ %d:%d", xlMirKindName(pNode->eKind), pNode->iLine, pNode->iColumn);
	if ( pNode->sText != NULL ) {
		__xlLowerBufAppend(pBuf, " text=\"");
		__xlLowerAppendEscaped(pBuf, pNode->sText);
		__xlLowerBufAppend(pBuf, "\"");
	}
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		__xlLowerBufAppend(pBuf, " type=\"");
		__xlLowerAppendEscaped(pBuf, pNode->sTypeName);
		__xlLowerBufAppend(pBuf, "\"");
	}
	if ( pNode->iScopeId >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " scope=%d", pNode->iScopeId);
	}
	if ( pNode->iSymbolIndex >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " symbol=%d", pNode->iSymbolIndex);
	}
	if ( pNode->iFuncIndex >= 0 ) {
		__xlLowerBufAppendFormat(pBuf, " func=%d", pNode->iFuncIndex);
	}
	__xlLowerBufAppend(pBuf, "\n");

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlDumpMirText(pBuf, pNode->ppChildren[i], iIndent + 1);
	}
}

static void __xlDumpHirJson(XL_LowerTextBuffer* pBuf, const XL_HirNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		__xlLowerBufAppend(pBuf, "null");
		return;
	}

	__xlLowerBufAppend(pBuf, "{\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"kind\": \"%s\",\n", xlHirKindName(pNode->eKind));
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"line\": %d,\n", pNode->iLine);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"column\": %d,\n", pNode->iColumn);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"text\": ");
	if ( pNode->sText != NULL ) {
		__xlLowerBufAppend(pBuf, "\"");
		__xlLowerAppendEscaped(pBuf, pNode->sText);
		__xlLowerBufAppend(pBuf, "\"");
	} else {
		__xlLowerBufAppend(pBuf, "null");
	}
	__xlLowerBufAppend(pBuf, ",\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"type\": ");
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		__xlLowerBufAppend(pBuf, "\"");
		__xlLowerAppendEscaped(pBuf, pNode->sTypeName);
		__xlLowerBufAppend(pBuf, "\"");
	} else {
		__xlLowerBufAppend(pBuf, "null");
	}
	__xlLowerBufAppend(pBuf, ",\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"scopeId\": %d,\n", pNode->iScopeId);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"symbolIndex\": %d,\n", pNode->iSymbolIndex);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"funcIndex\": %d,\n", pNode->iFuncIndex);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"children\": [");
	if ( pNode->iChildCount != 0 ) {
		__xlLowerBufAppend(pBuf, "\n");
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		size_t j;
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) { __xlLowerBufAppend(pBuf, "\t"); }
		__xlDumpHirJson(pBuf, pNode->ppChildren[i], iIndent + 2);
		if ( (i + 1) != pNode->iChildCount ) {
			__xlLowerBufAppend(pBuf, ",");
		}
		__xlLowerBufAppend(pBuf, "\n");
	}
	if ( pNode->iChildCount != 0 ) {
		for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	}
	__xlLowerBufAppend(pBuf, "]\n");
	for ( i = 0; i < (size_t)iIndent; ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "}");
}

static void __xlDumpMirJson(XL_LowerTextBuffer* pBuf, const XL_MirNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		__xlLowerBufAppend(pBuf, "null");
		return;
	}

	__xlLowerBufAppend(pBuf, "{\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"kind\": \"%s\",\n", xlMirKindName(pNode->eKind));
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"line\": %d,\n", pNode->iLine);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"column\": %d,\n", pNode->iColumn);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"text\": ");
	if ( pNode->sText != NULL ) {
		__xlLowerBufAppend(pBuf, "\"");
		__xlLowerAppendEscaped(pBuf, pNode->sText);
		__xlLowerBufAppend(pBuf, "\"");
	} else {
		__xlLowerBufAppend(pBuf, "null");
	}
	__xlLowerBufAppend(pBuf, ",\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"type\": ");
	if ( (pNode->sTypeName != NULL) && (pNode->sTypeName[0] != '\0') ) {
		__xlLowerBufAppend(pBuf, "\"");
		__xlLowerAppendEscaped(pBuf, pNode->sTypeName);
		__xlLowerBufAppend(pBuf, "\"");
	} else {
		__xlLowerBufAppend(pBuf, "null");
	}
	__xlLowerBufAppend(pBuf, ",\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"scopeId\": %d,\n", pNode->iScopeId);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"symbolIndex\": %d,\n", pNode->iSymbolIndex);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppendFormat(pBuf, "\"funcIndex\": %d,\n", pNode->iFuncIndex);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "\"children\": [");
	if ( pNode->iChildCount != 0 ) {
		__xlLowerBufAppend(pBuf, "\n");
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		size_t j;
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) { __xlLowerBufAppend(pBuf, "\t"); }
		__xlDumpMirJson(pBuf, pNode->ppChildren[i], iIndent + 2);
		if ( (i + 1) != pNode->iChildCount ) {
			__xlLowerBufAppend(pBuf, ",");
		}
		__xlLowerBufAppend(pBuf, "\n");
	}
	if ( pNode->iChildCount != 0 ) {
		for ( i = 0; i < (size_t)(iIndent + 1); ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	}
	__xlLowerBufAppend(pBuf, "]\n");
	for ( i = 0; i < (size_t)iIndent; ++i ) { __xlLowerBufAppend(pBuf, "\t"); }
	__xlLowerBufAppend(pBuf, "}");
}

char* xlHirDumpJson(const XL_HirNode* pNode)
{
	XL_LowerTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlDumpHirJson(&buf, pNode, 0);
	__xlLowerBufAppend(&buf, "\n");
	return buf.sData;
}

char* xlMirDumpJson(const XL_MirNode* pNode)
{
	XL_LowerTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlDumpMirJson(&buf, pNode, 0);
	__xlLowerBufAppend(&buf, "\n");
	return buf.sData;
}

char* xlLowerDumpStateMachinesText(const XL_LowerResult* pResult)
{
	XL_LowerTextBuffer buf;
	char* sAsyncText;
	char* sGeneratorText;

	memset(&buf, 0, sizeof(buf));
	sAsyncText = xlAsyncSmDumpText((pResult != NULL) ? &pResult->asyncSm : NULL);
	sGeneratorText = xlGeneratorSmDumpText((pResult != NULL) ? &pResult->generatorSm : NULL);
	if ( sAsyncText != NULL ) {
		__xlLowerBufAppend(&buf, sAsyncText);
		free(sAsyncText);
	}
	if ( sGeneratorText != NULL ) {
		__xlLowerBufAppend(&buf, sGeneratorText);
		free(sGeneratorText);
	}
	return buf.sData;
}

char* xlLowerDumpStateMachinesJson(const XL_LowerResult* pResult)
{
	XL_LowerTextBuffer buf;
	char* sAsyncJson;
	char* sGeneratorJson;

	memset(&buf, 0, sizeof(buf));
	__xlLowerBufAppend(&buf, "{\n\t\"asyncSm\": ");
	sAsyncJson = xlAsyncSmDumpJson((pResult != NULL) ? &pResult->asyncSm : NULL);
	if ( sAsyncJson != NULL ) {
		__xlLowerBufAppend(&buf, sAsyncJson);
		free(sAsyncJson);
	} else {
		__xlLowerBufAppend(&buf, "null");
	}

	__xlLowerBufAppend(&buf, ",\n\t\"generatorSm\": ");
	sGeneratorJson = xlGeneratorSmDumpJson((pResult != NULL) ? &pResult->generatorSm : NULL);
	if ( sGeneratorJson != NULL ) {
		__xlLowerBufAppend(&buf, sGeneratorJson);
		free(sGeneratorJson);
	} else {
		__xlLowerBufAppend(&buf, "null");
	}
	__xlLowerBufAppend(&buf, "\n}\n");
	return buf.sData;
}

char* xlLowerDumpText(const XL_LowerResult* pResult)
{
	XL_LowerTextBuffer buf;
	char* sStateText;

	memset(&buf, 0, sizeof(buf));
	__xlLowerBufAppend(&buf, "HIR\n");
	__xlDumpHirText(&buf, (pResult != NULL) ? pResult->pHirRoot : NULL, 0);
	__xlLowerBufAppend(&buf, "MIR\n");
	__xlDumpMirText(&buf, (pResult != NULL) ? pResult->pMirRoot : NULL, 0);
	sStateText = xlLowerDumpStateMachinesText(pResult);
	if ( sStateText != NULL ) {
		__xlLowerBufAppend(&buf, sStateText);
		free(sStateText);
	}
	return buf.sData;
}

char* xlLowerDumpJson(const XL_LowerResult* pResult)
{
	XL_LowerTextBuffer buf;
	char* sStateJson;

	memset(&buf, 0, sizeof(buf));
	__xlLowerBufAppend(&buf, "{\n\t\"hir\": ");
	__xlDumpHirJson(&buf, (pResult != NULL) ? pResult->pHirRoot : NULL, 1);
	__xlLowerBufAppend(&buf, ",\n\t\"mir\": ");
	__xlDumpMirJson(&buf, (pResult != NULL) ? pResult->pMirRoot : NULL, 1);
	__xlLowerBufAppend(&buf, ",\n\t\"stateMachines\": ");
	sStateJson = xlLowerDumpStateMachinesJson(pResult);
	if ( sStateJson != NULL ) {
		__xlLowerBufAppend(&buf, sStateJson);
		free(sStateJson);
	} else {
		__xlLowerBufAppend(&buf, "null");
	}
	__xlLowerBufAppend(&buf, "\n}\n");
	return buf.sData;
}
