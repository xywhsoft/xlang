#include "xl_lexer_api.h"
#include "xl_lexer_gen.h"

typedef struct XL_TextBuffer {
	char* sData;
	size_t iLen;
	size_t iCap;
} XL_TextBuffer;

struct XL_Lexer {
	yyscan_t scanner;
	YY_BUFFER_STATE hBuffer;
	char* sSource;
	size_t iSourceLen;
	int bOwnsSource;
	XL_LexerExtra extra;
};

static int __xlTextBufferGrow(XL_TextBuffer* pBuf, size_t iNeed)
{
	char* sNewData;
	size_t iNewCap;

	if ( pBuf == NULL ) {
		return XL_RC_BAD_ARG;
	}
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

static int __xlTextBufferAppend(XL_TextBuffer* pBuf, const char* sText)
{
	size_t iLen;

	if ( (pBuf == NULL) || (sText == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	iLen = strlen(sText);
	if ( __xlTextBufferGrow(pBuf, iLen) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
	pBuf->iLen += iLen;
	pBuf->sData[pBuf->iLen] = '\0';
	return XL_RC_OK;
}

static int __xlTextBufferAppendN(XL_TextBuffer* pBuf, const char* sText, size_t iLen)
{
	if ( (pBuf == NULL) || ((sText == NULL) && (iLen != 0)) ) {
		return XL_RC_BAD_ARG;
	}

	if ( __xlTextBufferGrow(pBuf, iLen) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	if ( iLen != 0 ) {
		memcpy(pBuf->sData + pBuf->iLen, sText, iLen);
		pBuf->iLen += iLen;
	}
	pBuf->sData[pBuf->iLen] = '\0';
	return XL_RC_OK;
}

static int __xlTextBufferAppendFormat(XL_TextBuffer* pBuf, const char* sFormat, ...)
{
	va_list args;
	va_list argsCopy;
	int iNeed;

	if ( (pBuf == NULL) || (sFormat == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	va_start(args, sFormat);
	va_copy(argsCopy, args);
	iNeed = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if ( iNeed < 0 ) {
		va_end(argsCopy);
		return XL_RC_FAIL;
	}

	if ( __xlTextBufferGrow(pBuf, (size_t)iNeed) != XL_RC_OK ) {
		va_end(argsCopy);
		return XL_RC_IO_ERROR;
	}

	vsnprintf(pBuf->sData + pBuf->iLen, pBuf->iCap - pBuf->iLen, sFormat, argsCopy);
	va_end(argsCopy);
	pBuf->iLen += (size_t)iNeed;
	return XL_RC_OK;
}

static int __xlTokenListGrow(XL_TokenList* pList, size_t iNeed)
{
	XL_Token* pNewItems;
	size_t iNewCap;

	if ( pList == NULL ) {
		return XL_RC_BAD_ARG;
	}
	if ( (pList->iCount + iNeed) <= pList->iCapacity ) {
		return XL_RC_OK;
	}

	iNewCap = (pList->iCapacity == 0) ? 64 : pList->iCapacity;
	while ( iNewCap < (pList->iCount + iNeed) ) {
		iNewCap *= 2;
	}

	pNewItems = (XL_Token*)realloc(pList->pItems, sizeof(XL_Token) * iNewCap);
	if ( pNewItems == NULL ) {
		return XL_RC_IO_ERROR;
	}

	pList->pItems = pNewItems;
	pList->iCapacity = iNewCap;
	return XL_RC_OK;
}

static int __xlTokenListPush(XL_TokenList* pList, const XL_Token* pToken)
{
	if ( (pList == NULL) || (pToken == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	if ( __xlTokenListGrow(pList, 1) != XL_RC_OK ) {
		return XL_RC_IO_ERROR;
	}

	pList->pItems[pList->iCount++] = *pToken;
	return XL_RC_OK;
}

static int __xlReadAllBytes(const char* sFilePath, char** ppData, size_t* piLen)
{
	FILE* pFile;
	long iFileSize;
	char* sBuffer;
	size_t iReadLen;

	if ( (sFilePath == NULL) || (ppData == NULL) || (piLen == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	pFile = fopen(sFilePath, "rb");
	if ( pFile == NULL ) {
		return XL_RC_IO_ERROR;
	}

	if ( fseek(pFile, 0, SEEK_END) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iFileSize = ftell(pFile);
	if ( iFileSize < 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	if ( fseek(pFile, 0, SEEK_SET) != 0 ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	sBuffer = (char*)malloc((size_t)iFileSize + 1);
	if ( sBuffer == NULL ) {
		fclose(pFile);
		return XL_RC_IO_ERROR;
	}

	iReadLen = fread(sBuffer, 1, (size_t)iFileSize, pFile);
	fclose(pFile);
	sBuffer[iReadLen] = '\0';

	*ppData = sBuffer;
	*piLen = iReadLen;
	return XL_RC_OK;
}

static int __xlIsLinkSpace(int ch)
{
	return (ch == ' ') || (ch == '\t') || (ch == '\v') || (ch == '\f');
}

static int __xlIsLinkIdentStart(int ch)
{
	return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')) || (ch == '_');
}

static int __xlIsLinkIdentCont(int ch)
{
	return __xlIsLinkIdentStart(ch) || ((ch >= '0') && (ch <= '9'));
}

static char* __xlStripLinkLines(const char* sSource, size_t iLen, size_t* piOutLen)
{
	XL_TextBuffer buf;
	size_t iPos;

	if ( (sSource == NULL) || (piOutLen == NULL) ) {
		return NULL;
	}

	memset(&buf, 0, sizeof(buf));
	iPos = 0;
	while ( iPos < iLen ) {
		size_t iLineStart;
		size_t iCursor;
		size_t iLineEnd;
		size_t iNewlineLen;
		size_t iLibStart;
		size_t iLibEnd;

		iLineStart = iPos;
		iCursor = iPos;
		while ( (iCursor < iLen) && __xlIsLinkSpace((unsigned char)sSource[iCursor]) ) {
			++iCursor;
		}
		iLineEnd = iCursor;
		while ( (iLineEnd < iLen) && (sSource[iLineEnd] != '\r') && (sSource[iLineEnd] != '\n') ) {
			++iLineEnd;
		}

		iNewlineLen = 0;
		if ( (iLineEnd < iLen) && (sSource[iLineEnd] == '\r') ) {
			iNewlineLen = 1;
			if ( ((iLineEnd + 1) < iLen) && (sSource[iLineEnd + 1] == '\n') ) {
				iNewlineLen = 2;
			}
		} else if ( (iLineEnd < iLen) && (sSource[iLineEnd] == '\n') ) {
			iNewlineLen = 1;
		}

		if ( ((iCursor + 4) <= iLen) &&
			(memcmp(sSource + iCursor, "link", 4) == 0) &&
			(((iCursor + 4) == iLen) || __xlIsLinkSpace((unsigned char)sSource[iCursor + 4])) ) {
			iLibStart = iCursor + 4;
			while ( (iLibStart < iLineEnd) && __xlIsLinkSpace((unsigned char)sSource[iLibStart]) ) {
				++iLibStart;
			}
			iLibEnd = iLibStart;
			if ( (iLibStart < iLineEnd) && __xlIsLinkIdentStart((unsigned char)sSource[iLibStart]) ) {
				while ( (iLibEnd < iLineEnd) && __xlIsLinkIdentCont((unsigned char)sSource[iLibEnd]) ) {
					++iLibEnd;
				}
				while ( (iLibEnd < iLineEnd) && __xlIsLinkSpace((unsigned char)sSource[iLibEnd]) ) {
					++iLibEnd;
				}
				if ( (iLibEnd == iLineEnd) ||
					((sSource[iLibEnd] == '/') && ((iLibEnd + 1) < iLineEnd) && (sSource[iLibEnd + 1] == '/')) ) {
					if ( __xlTextBufferAppendN(&buf, sSource + iLineEnd, iNewlineLen) != XL_RC_OK ) {
						free(buf.sData);
						return NULL;
					}
					iPos = iLineEnd + iNewlineLen;
					continue;
				}
			}
		}

		if ( __xlTextBufferAppendN(&buf, sSource + iLineStart, (iLineEnd - iLineStart) + iNewlineLen) != XL_RC_OK ) {
			free(buf.sData);
			return NULL;
		}
		iPos = iLineEnd + iNewlineLen;
	}

	if ( buf.sData == NULL ) {
		buf.sData = (char*)malloc(1);
		if ( buf.sData == NULL ) {
			return NULL;
		}
		buf.sData[0] = '\0';
	}
	*piOutLen = buf.iLen;
	return buf.sData;
}

static void __xlAppendEscapedText(XL_TextBuffer* pBuf, const char* sText, size_t iLen)
{
	size_t i;

	for ( i = 0; i < iLen; ++i ) {
		unsigned char ch = (unsigned char)sText[i];
		switch ( ch ) {
			case '\\':
				__xlTextBufferAppend(pBuf, "\\\\");
				break;
			case '\"':
				__xlTextBufferAppend(pBuf, "\\\"");
				break;
			case '\r':
				__xlTextBufferAppend(pBuf, "\\r");
				break;
			case '\n':
				__xlTextBufferAppend(pBuf, "\\n");
				break;
			case '\t':
				__xlTextBufferAppend(pBuf, "\\t");
				break;
			default:
				if ( ch < 0x20 ) {
					__xlTextBufferAppendFormat(pBuf, "\\u%04x", (unsigned int)ch);
				} else {
					__xlTextBufferAppendN(pBuf, (const char*)&ch, 1);
				}
				break;
		}
	}
}

XL_Lexer* xlLexerCreateFromString(const char* sSource, size_t iLen, const char* sFileName)
{
	XL_Lexer* pLexer;
	char* sSanitized;
	char* sOwnedSource;
	size_t iSanitizedLen;

	if ( sSource == NULL ) {
		return NULL;
	}

	pLexer = (XL_Lexer*)calloc(1, sizeof(XL_Lexer));
	if ( pLexer == NULL ) {
		return NULL;
	}

	sSanitized = __xlStripLinkLines(sSource, iLen, &iSanitizedLen);
	if ( sSanitized == NULL ) {
		free(pLexer);
		return NULL;
	}
	sOwnedSource = (char*)realloc(sSanitized, iSanitizedLen + 2);
	if ( sOwnedSource == NULL ) {
		free(sSanitized);
		free(pLexer);
		return NULL;
	}
	pLexer->sSource = sOwnedSource;
	pLexer->sSource[iSanitizedLen] = '\0';
	pLexer->sSource[iSanitizedLen + 1] = '\0';
	pLexer->iSourceLen = iSanitizedLen;
	pLexer->bOwnsSource = 1;
	pLexer->extra.sFileName = (sFileName != NULL) ? sFileName : "<memory>";
	pLexer->extra.iLine = 1;
	pLexer->extra.iColumn = 1;

	if ( xllex_init_extra(&pLexer->extra, &pLexer->scanner) != 0 ) {
		free(pLexer->sSource);
		free(pLexer);
		return NULL;
	}

	pLexer->hBuffer = xl_scan_buffer(pLexer->sSource, iLen + 2, pLexer->scanner);
	if ( pLexer->hBuffer == NULL ) {
		xllex_destroy(pLexer->scanner);
		free(pLexer->sSource);
		free(pLexer);
		return NULL;
	}

	xl_switch_to_buffer(pLexer->hBuffer, pLexer->scanner);
	xlset_lineno(1, pLexer->scanner);
	return pLexer;
}

XL_Lexer* xlLexerCreateFromFile(const char* sFilePath)
{
	char* sData;
	size_t iLen;
	XL_Lexer* pLexer;

	if ( __xlReadAllBytes(sFilePath, &sData, &iLen) != XL_RC_OK ) {
		return NULL;
	}

	pLexer = xlLexerCreateFromString(sData, iLen, sFilePath);
	free(sData);
	return pLexer;
}

void xlLexerDestroy(XL_Lexer* pLexer)
{
	if ( pLexer == NULL ) {
		return;
	}
	if ( pLexer->hBuffer != NULL ) {
		xl_delete_buffer(pLexer->hBuffer, pLexer->scanner);
	}
	if ( pLexer->scanner != NULL ) {
		xllex_destroy(pLexer->scanner);
	}
	if ( pLexer->bOwnsSource && (pLexer->sSource != NULL) ) {
		free(pLexer->sSource);
	}
	free(pLexer);
}

const char* xlLexerFileName(const XL_Lexer* pLexer)
{
	if ( pLexer == NULL ) {
		return NULL;
	}
	return pLexer->extra.sFileName;
}

int xlLexerTokenizeAll(XL_Lexer* pLexer, XL_TokenList* pList)
{
	int iType;

	if ( (pLexer == NULL) || (pList == NULL) ) {
		return XL_RC_BAD_ARG;
	}

	while ( 1 ) {
		iType = xllex(pLexer->scanner);
		if ( __xlTokenListPush(pList, &pLexer->extra.currentToken) != XL_RC_OK ) {
			return XL_RC_IO_ERROR;
		}
		if ( iType == XL_TK_EOF ) {
			break;
		}
	}

	return pLexer->extra.bHadError ? XL_RC_FAIL : XL_RC_OK;
}

void xlTokenListInit(XL_TokenList* pList)
{
	if ( pList != NULL ) {
		memset(pList, 0, sizeof(*pList));
	}
}

void xlTokenListFree(XL_TokenList* pList)
{
	if ( pList != NULL ) {
		free(pList->pItems);
		pList->pItems = NULL;
		pList->iCount = 0;
		pList->iCapacity = 0;
	}
}

char* xlLexerTokensToText(const XL_TokenList* pList)
{
	XL_TextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	__xlTextBufferAppend(&buf, "line:col-end        kind                 text\n");
	__xlTextBufferAppend(&buf, "-----------------------------------------------\n");

	if ( pList != NULL ) {
		for ( i = 0; i < pList->iCount; ++i ) {
			const XL_Token* pToken = &pList->pItems[i];
			__xlTextBufferAppendFormat(
				&buf,
				"%4d:%-4d-%4d:%-4d  %-20s  \"",
				pToken->iStartLine,
				pToken->iStartColumn,
				pToken->iEndLine,
				pToken->iEndColumn,
				xlTokenKindName(pToken->eKind)
			);
			__xlAppendEscapedText(&buf, pToken->sText, pToken->iTextLen);
			__xlTextBufferAppend(&buf, "\"");
			if ( pToken->eValueKind == XL_TV_INT ) {
				__xlTextBufferAppendFormat(&buf, "  int=%lld", (long long)pToken->iIntValue);
			} else if ( pToken->eValueKind == XL_TV_FLOAT ) {
				__xlTextBufferAppendFormat(&buf, "  float=%0.17g", pToken->fFloatValue);
			}
			if ( pToken->sMessage[0] != '\0' ) {
				__xlTextBufferAppendFormat(&buf, "  msg=%s", pToken->sMessage);
			}
			__xlTextBufferAppend(&buf, "\n");
		}
	}

	return buf.sData;
}

char* xlLexerTokensToJson(const XL_TokenList* pList)
{
	XL_TextBuffer buf;
	size_t i;

	memset(&buf, 0, sizeof(buf));
	__xlTextBufferAppend(&buf, "[\n");

	if ( pList != NULL ) {
		for ( i = 0; i < pList->iCount; ++i ) {
			const XL_Token* pToken = &pList->pItems[i];
			if ( i != 0 ) {
				__xlTextBufferAppend(&buf, ",\n");
			}
			__xlTextBufferAppend(&buf, "  {\n");
			__xlTextBufferAppendFormat(&buf, "    \"kind\": \"%s\",\n", xlTokenKindName(pToken->eKind));
			__xlTextBufferAppend(&buf, "    \"text\": \"");
			__xlAppendEscapedText(&buf, pToken->sText, pToken->iTextLen);
			__xlTextBufferAppend(&buf, "\",\n");
			__xlTextBufferAppendFormat(&buf, "    \"startOffset\": %llu,\n", (unsigned long long)pToken->iStartOffset);
			__xlTextBufferAppendFormat(&buf, "    \"endOffset\": %llu,\n", (unsigned long long)pToken->iEndOffset);
			__xlTextBufferAppendFormat(&buf, "    \"startLine\": %d,\n", pToken->iStartLine);
			__xlTextBufferAppendFormat(&buf, "    \"startColumn\": %d,\n", pToken->iStartColumn);
			__xlTextBufferAppendFormat(&buf, "    \"endLine\": %d,\n", pToken->iEndLine);
			__xlTextBufferAppendFormat(&buf, "    \"endColumn\": %d,\n", pToken->iEndColumn);
			if ( pToken->eValueKind == XL_TV_INT ) {
				__xlTextBufferAppendFormat(&buf, "    \"intValue\": %lld,\n", (long long)pToken->iIntValue);
			} else if ( pToken->eValueKind == XL_TV_FLOAT ) {
				__xlTextBufferAppendFormat(&buf, "    \"floatValue\": %.17g,\n", pToken->fFloatValue);
			}
			__xlTextBufferAppend(&buf, "    \"message\": \"");
			__xlAppendEscapedText(&buf, pToken->sMessage, strlen(pToken->sMessage));
			__xlTextBufferAppend(&buf, "\"\n");
			__xlTextBufferAppend(&buf, "  }");
		}
	}

	__xlTextBufferAppend(&buf, "\n]\n");
	return buf.sData;
}

const char* xlLexerGetError(const XL_Lexer* pLexer)
{
	return (pLexer != NULL) ? pLexer->extra.sErrorText : NULL;
}

int xlLexerHadError(const XL_Lexer* pLexer)
{
	return (pLexer != NULL) ? pLexer->extra.bHadError : 0;
}
