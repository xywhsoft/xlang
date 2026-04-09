#include "xl_ast_dump.h"

typedef struct XL_AstTextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_AstTextBuffer;

static int __xlAstBufGrow(XL_AstTextBuffer* pBuf, size_t iNeed)
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

static void __xlAstBufAppend(XL_AstTextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return;
	}

	iLen = strlen(sText);
	if ( __xlAstBufGrow(pBuf, iLen) != XL_RC_OK ) {
		return;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
}

static void __xlAstBufAppendFormat(XL_AstTextBuffer* pBuf, const char* sFormat, ...)
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

	if ( __xlAstBufGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(args2);
		return;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, args2);
	va_end(args2);
	pBuf->iLen += (size_t)iNeed;
}

static void __xlAstAppendEscaped(XL_AstTextBuffer* pBuf, const char* sText)
{
	const unsigned char* pScan;

	if ( sText == NULL ) {
		return;
	}

	for ( pScan = (const unsigned char*)sText; *pScan != 0; ++pScan ) {
		switch ( *pScan ) {
			case '\\':
				__xlAstBufAppend(pBuf, "\\\\");
				break;
			case '\"':
				__xlAstBufAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlAstBufAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlAstBufAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlAstBufAppend(pBuf, "\\t");
				break;
			default:
				if ( *pScan < 0x20 ) {
					__xlAstBufAppendFormat(pBuf, "\\u%04x", (unsigned int)(*pScan));
				} else {
					char sTmp[2];
					sTmp[0] = (char)(*pScan);
					sTmp[1] = '\0';
					__xlAstBufAppend(pBuf, sTmp);
				}
				break;
		}
	}
}

static void __xlAstDumpTextNode(XL_AstTextBuffer* pBuf, const XL_AstNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		return;
	}

	for ( i = 0; i < (size_t)iIndent; ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(
		pBuf,
		"%s @ %d:%d-%d:%d",
		xlAstKindName(pNode->eKind),
		pNode->iLine,
		pNode->iColumn,
		pNode->iEndLine,
		pNode->iEndColumn
	);
	if ( pNode->sText != NULL ) {
		__xlAstBufAppend(pBuf, " text=\"");
		__xlAstAppendEscaped(pBuf, pNode->sText);
		__xlAstBufAppend(pBuf, "\"");
	}
	if ( pNode->sText2 != NULL ) {
		__xlAstBufAppend(pBuf, " text2=\"");
		__xlAstAppendEscaped(pBuf, pNode->sText2);
		__xlAstBufAppend(pBuf, "\"");
	}
	if ( pNode->iIntValue != 0 ) {
		__xlAstBufAppendFormat(pBuf, " int=%lld", (long long)pNode->iIntValue);
	}
	if ( pNode->fFloatValue != 0.0 ) {
		__xlAstBufAppendFormat(pBuf, " float=%0.17g", pNode->fFloatValue);
	}
	if ( pNode->bFlag != 0 ) {
		__xlAstBufAppendFormat(pBuf, " flag=%d", pNode->bFlag);
	}
	__xlAstBufAppend(pBuf, "\n");

	for ( i = 0; i < pNode->iChildCount; ++i ) {
		__xlAstDumpTextNode(pBuf, pNode->ppChildren[i], iIndent + 1);
	}
}

static void __xlAstDumpJsonNode(XL_AstTextBuffer* pBuf, const XL_AstNode* pNode, int iIndent)
{
	size_t i;

	if ( pNode == NULL ) {
		__xlAstBufAppend(pBuf, "null");
		return;
	}

	__xlAstBufAppend(pBuf, "{\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(pBuf, "\"kind\": \"%s\",\n", xlAstKindName(pNode->eKind));
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(pBuf, "\"line\": %d,\n", pNode->iLine);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(pBuf, "\"column\": %d,\n", pNode->iColumn);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppend(pBuf, "\"text\": ");
	if ( pNode->sText != NULL ) {
		__xlAstBufAppend(pBuf, "\"");
		__xlAstAppendEscaped(pBuf, pNode->sText);
		__xlAstBufAppend(pBuf, "\"");
	} else {
		__xlAstBufAppend(pBuf, "null");
	}
	__xlAstBufAppend(pBuf, ",\n");
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(pBuf, "\"intValue\": %lld,\n", (long long)pNode->iIntValue);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppendFormat(pBuf, "\"flag\": %d,\n", pNode->bFlag);
	for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppend(pBuf, "\"children\": [");
	if ( pNode->iChildCount != 0 ) {
		__xlAstBufAppend(pBuf, "\n");
	}
	for ( i = 0; i < pNode->iChildCount; ++i ) {
		size_t j;
		for ( j = 0; j < (size_t)(iIndent + 2); ++j ) {
			__xlAstBufAppend(pBuf, "\t");
		}
		__xlAstDumpJsonNode(pBuf, pNode->ppChildren[i], iIndent + 2);
		if ( (i + 1) != pNode->iChildCount ) {
			__xlAstBufAppend(pBuf, ",");
		}
		__xlAstBufAppend(pBuf, "\n");
	}
	if ( pNode->iChildCount != 0 ) {
		for ( i = 0; i < (size_t)(iIndent + 1); ++i ) {
			__xlAstBufAppend(pBuf, "\t");
		}
	}
	__xlAstBufAppend(pBuf, "]\n");
	for ( i = 0; i < (size_t)iIndent; ++i ) {
		__xlAstBufAppend(pBuf, "\t");
	}
	__xlAstBufAppend(pBuf, "}");
}

char* xlAstDumpText(const XL_AstNode* pNode)
{
	XL_AstTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlAstDumpTextNode(&buf, pNode, 0);
	return buf.sData;
}

char* xlAstDumpJson(const XL_AstNode* pNode)
{
	XL_AstTextBuffer buf;

	memset(&buf, 0, sizeof(buf));
	__xlAstDumpJsonNode(&buf, pNode, 0);
	__xlAstBufAppend(&buf, "\n");
	return buf.sData;
}
