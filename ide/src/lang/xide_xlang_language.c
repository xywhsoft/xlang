#include "lang/xide_xlang_language.h"

#include <string.h>

#define XIDE_FOLD_STACK_MAX 256

typedef struct xide_fold_stack_t {
	int iLine;
	int iLevel;
	uint32_t iFlags;
} xide_fold_stack_t;

static int xideIsIdentStart(int c)
{
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80);
}

static int xideIsIdent(int c)
{
	return xideIsIdentStart(c) || (c >= '0' && c <= '9');
}

static int xideIsDigit(int c)
{
	return c >= '0' && c <= '9';
}

static int xideTextEquals(const char* sText, int iStart, int iEnd, const char* sWord)
{
	int len = iEnd - iStart;
	return sWord != NULL && (int)strlen(sWord) == len && strncmp(sText + iStart, sWord, (size_t)len) == 0;
}

static int xideIsKeyword(const char* sText, int iStart, int iEnd)
{
	static const char* keywords[] = {
		"null", "true", "false", "func", "const", "var", "auto", "typedef", "as",
		"if", "then", "else", "elseif", "end", "for", "in", "while", "do",
		"break", "continue", "return", "async", "await", "yield",
		"try", "catch", "finally", "throw", "assert",
		"import", "export", "extern", "include", "define",
		"class", "extends", "union", "get", "let", "public", "private", "protected",
		"static", "virtual", "override", "final", "native",
		"and", "or", "not", "is", "self", "base"
	};
	static const char* types[] = {
		"bool", "int", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
		"float", "float32", "float64", "string", "time", "point", "type",
		"array", "list", "set", "dict", "object",
		"c_char", "c_schar", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
		"c_long", "c_ulong", "c_longlong", "c_ulonglong", "c_size", "c_ssize",
		"c_ptrdiff", "c_intptr", "c_uintptr", "c_bool", "c_float", "c_double", "c_wchar"
	};
	int i;
	int len = iEnd - iStart;
	for ( i = 0; i < (int)(sizeof(keywords) / sizeof(keywords[0])); i++ ) {
		if ( (int)strlen(keywords[i]) == len && strncmp(sText + iStart, keywords[i], (size_t)len) == 0 ) return XUI_CODE_TOKEN_KEYWORD;
	}
	for ( i = 0; i < (int)(sizeof(types) / sizeof(types[0])); i++ ) {
		if ( (int)strlen(types[i]) == len && strncmp(sText + iStart, types[i], (size_t)len) == 0 ) return XUI_CODE_TOKEN_TYPE;
	}
	return XUI_CODE_TOKEN_IDENTIFIER;
}

static int xideAddToken(xui_code_token_t* pTokens, int iCapacity, int* pCount, int iStart, int iEnd, int iKind)
{
	xui_code_token_t* token;
	if ( pCount == NULL || iStart >= iEnd ) return XUI_OK;
	if ( pTokens == NULL || *pCount >= iCapacity ) {
		(*pCount)++;
		return XUI_OK;
	}
	token = &pTokens[*pCount];
	memset(token, 0, sizeof(*token));
	token->iStartOffset = iStart;
	token->iEndOffset = iEnd;
	token->iKind = iKind;
	token->iStyle = iKind;
	(*pCount)++;
	return XUI_OK;
}

static int xideHashDirective(const char* sText, int iTextSize, int iStart)
{
	int i;
	if ( iStart < 0 || iStart >= iTextSize || sText[iStart] != '#' ) return 0;
	i = iStart + 1;
	if ( i + 3 <= iTextSize && strncmp(sText + i, "doc", 3) == 0 &&
	     (i + 3 >= iTextSize || !xideIsIdent((unsigned char)sText[i + 3])) ) {
		return 1;
	}
	if ( i + 3 <= iTextSize && strncmp(sText + i, "end", 3) == 0 &&
	     (i + 3 >= iTextSize || !xideIsIdent((unsigned char)sText[i + 3])) ) {
		return 2;
	}
	return 0;
}

static int xideIsLineFirstNonSpace(const char* sText, int iLineStart, int iOffset)
{
	int i;
	for ( i = iLineStart; i < iOffset; i++ ) {
		if ( sText[i] != ' ' && sText[i] != '\t' ) return 0;
	}
	return 1;
}

static int xideTryStringPrefix(const char* sText, int iTextSize, int iStart, int* pQuote)
{
	if ( iStart + 2 < iTextSize && sText[iStart] == 'r' && sText[iStart + 1] == 'f' &&
	     (sText[iStart + 2] == '\'' || sText[iStart + 2] == '"') ) {
		*pQuote = iStart + 2;
		return 1;
	}
	if ( iStart + 1 < iTextSize && (sText[iStart] == 'r' || sText[iStart] == 'f') &&
	     (sText[iStart + 1] == '\'' || sText[iStart + 1] == '"') ) {
		*pQuote = iStart + 1;
		return 1;
	}
	return 0;
}

static int xideScanString(const char* sText, int iTextSize, int iStart)
{
	int quoteAt;
	int i;
	int triple;
	char q;
	if ( sText[iStart] == '\'' || sText[iStart] == '"' ) {
		quoteAt = iStart;
	} else if ( !xideTryStringPrefix(sText, iTextSize, iStart, &quoteAt) ) {
		return iStart;
	}
	q = sText[quoteAt];
	triple = quoteAt + 2 < iTextSize && sText[quoteAt + 1] == q && sText[quoteAt + 2] == q;
	i = quoteAt + (triple ? 3 : 1);
	while ( i < iTextSize ) {
		if ( !triple && sText[i] == '\\' ) {
			i += i + 1 < iTextSize ? 2 : 1;
			continue;
		}
		if ( triple ) {
			if ( i + 2 < iTextSize && sText[i] == q && sText[i + 1] == q && sText[i + 2] == q ) return i + 3;
		} else if ( sText[i] == q ) {
			return i + 1;
		} else if ( sText[i] == '\n' || sText[i] == '\r' ) {
			return i;
		}
		i++;
	}
	return iTextSize;
}

static int xideScanNumber(const char* sText, int iTextSize, int iStart)
{
	int i = iStart;
	if ( i + 1 < iTextSize && sText[i] == '0' && (sText[i + 1] == 'x' || sText[i + 1] == 'b' || sText[i + 1] == 'o') ) {
		i += 2;
		while ( i < iTextSize && (xideIsIdent((unsigned char)sText[i]) || sText[i] == '_') ) i++;
		return i;
	}
	while ( i < iTextSize && (xideIsDigit((unsigned char)sText[i]) || sText[i] == '_') ) i++;
	if ( i < iTextSize && sText[i] == '.' && !(i + 1 < iTextSize && sText[i + 1] == '.') ) {
		i++;
		while ( i < iTextSize && (xideIsDigit((unsigned char)sText[i]) || sText[i] == '_') ) i++;
	}
	if ( i < iTextSize && (sText[i] == 'e' || sText[i] == 'E') ) {
		i++;
		if ( i < iTextSize && (sText[i] == '+' || sText[i] == '-') ) i++;
		while ( i < iTextSize && (xideIsDigit((unsigned char)sText[i]) || sText[i] == '_') ) i++;
	}
	return i;
}

static int xideScanLineEnd(const char* sText, int iTextSize, int iStart)
{
	int i = iStart;
	while ( i < iTextSize && sText[i] != '\n' && sText[i] != '\r' ) i++;
	return i;
}

static int xideScanBlockComment(const char* sText, int iTextSize, int iStart)
{
	int depth = 1;
	int i = iStart + 2;
	while ( i < iTextSize && depth > 0 ) {
		if ( i + 1 < iTextSize && sText[i] == '/' && sText[i + 1] == '*' ) {
			depth++;
			i += 2;
		} else if ( i + 1 < iTextSize && sText[i] == '*' && sText[i + 1] == '/' ) {
			depth--;
			i += 2;
		} else {
			i++;
		}
	}
	return i;
}

static int xideAddFoldRange(xui_code_fold_range_t* pRanges, int iCapacity, int* pCount, int iStartLine, int iEndLine, int iLevel, uint32_t iFlags)
{
	xui_code_fold_range_t* pRange;
	if ( pCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iEndLine <= iStartLine ) return XUI_OK;
	if ( pRanges == NULL || *pCount >= iCapacity ) {
		(*pCount)++;
		return XUI_OK;
	}
	pRange = &pRanges[*pCount];
	memset(pRange, 0, sizeof(*pRange));
	pRange->iSize = sizeof(*pRange);
	pRange->iStartLine = iStartLine;
	pRange->iEndLine = iEndLine;
	pRange->iLevel = iLevel;
	pRange->iFlags = iFlags | XUI_CODE_FOLD_HEADER;
	(*pCount)++;
	return XUI_OK;
}

static int xideFoldScanString(const char* sText, int iTextSize, int iStart, int* pLine, int* pLineStart)
{
	int quoteAt;
	int i;
	int triple;
	char q;
	if ( sText[iStart] == '\'' || sText[iStart] == '"' ) {
		quoteAt = iStart;
	} else if ( !xideTryStringPrefix(sText, iTextSize, iStart, &quoteAt) ) {
		return iStart;
	}
	q = sText[quoteAt];
	triple = quoteAt + 2 < iTextSize && sText[quoteAt + 1] == q && sText[quoteAt + 2] == q;
	i = quoteAt + (triple ? 3 : 1);
	while ( i < iTextSize ) {
		if ( !triple && sText[i] == '\\' ) {
			i += i + 1 < iTextSize ? 2 : 1;
			continue;
		}
		if ( sText[i] == '\n' ) {
			(*pLine)++;
			*pLineStart = i + 1;
			i++;
			continue;
		}
		if ( triple ) {
			if ( i + 2 < iTextSize && sText[i] == q && sText[i + 1] == q && sText[i + 2] == q ) return i + 3;
		} else if ( sText[i] == q ) {
			return i + 1;
		} else if ( sText[i] == '\r' ) {
			return i;
		}
		i++;
	}
	return iTextSize;
}

static int xideFoldScanBlockComment(const char* sText, int iTextSize, int iStart, int* pLine, int* pLineStart, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	int depth = 1;
	int i = iStart + 2;
	int startLine = *pLine;
	int ret;
	while ( i < iTextSize && depth > 0 ) {
		if ( i + 1 < iTextSize && sText[i] == '/' && sText[i + 1] == '*' ) {
			depth++;
			i += 2;
		} else if ( i + 1 < iTextSize && sText[i] == '*' && sText[i + 1] == '/' ) {
			depth--;
			i += 2;
		} else {
			if ( sText[i] == '\n' ) {
				(*pLine)++;
				*pLineStart = i + 1;
			}
			i++;
		}
	}
	ret = xideAddFoldRange(pRanges, iRangeCapacity, pRangeCount, startLine, *pLine, 0, XUI_CODE_FOLD_COMMENT);
	return ret == XUI_OK ? i : -1;
}

static int xideFoldIsModifier(const char* sText, int iStart, int iEnd)
{
	return xideTextEquals(sText, iStart, iEnd, "public") ||
	       xideTextEquals(sText, iStart, iEnd, "private") ||
	       xideTextEquals(sText, iStart, iEnd, "protected") ||
	       xideTextEquals(sText, iStart, iEnd, "static") ||
	       xideTextEquals(sText, iStart, iEnd, "virtual") ||
	       xideTextEquals(sText, iStart, iEnd, "override") ||
	       xideTextEquals(sText, iStart, iEnd, "final") ||
	       xideTextEquals(sText, iStart, iEnd, "native") ||
	       xideTextEquals(sText, iStart, iEnd, "extern") ||
	       xideTextEquals(sText, iStart, iEnd, "async");
}

static int xideFoldHasFollowingName(const char* sText, int iTextSize, int iEnd)
{
	int i = iEnd;
	while ( i < iTextSize && (sText[i] == ' ' || sText[i] == '\t') ) i++;
	return i < iTextSize && xideIsIdentStart((unsigned char)sText[i]);
}

static int xideFoldIsBlockStart(const char* sText, int iTextSize, int iStart, int iEnd)
{
	if ( xideTextEquals(sText, iStart, iEnd, "func") ) return xideFoldHasFollowingName(sText, iTextSize, iEnd);
	if ( xideTextEquals(sText, iStart, iEnd, "class") ) return xideFoldHasFollowingName(sText, iTextSize, iEnd);
	if ( xideTextEquals(sText, iStart, iEnd, "union") ) return 1;
	if ( xideTextEquals(sText, iStart, iEnd, "get") ) return xideFoldHasFollowingName(sText, iTextSize, iEnd);
	if ( xideTextEquals(sText, iStart, iEnd, "let") ) return xideFoldHasFollowingName(sText, iTextSize, iEnd);
	if ( xideTextEquals(sText, iStart, iEnd, "if") ) return 1;
	if ( xideTextEquals(sText, iStart, iEnd, "for") ) return 1;
	if ( xideTextEquals(sText, iStart, iEnd, "while") ) return 1;
	if ( xideTextEquals(sText, iStart, iEnd, "try") ) return 1;
	return 0;
}

static int xideFoldCloseBlock(xide_fold_stack_t* pStack, int* pStackCount, int iLine, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount)
{
	if ( pStack == NULL || pStackCount == NULL || *pStackCount <= 0 ) return XUI_OK;
	(*pStackCount)--;
	return xideAddFoldRange(pRanges, iRangeCapacity, pRangeCount,
		pStack[*pStackCount].iLine, iLine, pStack[*pStackCount].iLevel, pStack[*pStackCount].iFlags);
}

static int xideFold(xui_code_document_t* pDocument, xui_code_fold_range_t* pRanges, int iRangeCapacity, int* pRangeCount, void* pUser)
{
	xide_fold_stack_t stack[XIDE_FOLD_STACK_MAX];
	const char* sText;
	int size;
	int stackCount = 0;
	int line = 0;
	int lineStart = 0;
	int lineState = 0;
	int docLine = -1;
	int i = 0;
	int start;
	int end;
	int directive;
	int ret;
	(void)pUser;
	if ( pDocument == NULL || pRangeCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pRangeCount = 0;
	sText = xuiCodeDocumentGetText(pDocument);
	size = xuiCodeDocumentGetLength(pDocument);
	if ( sText == NULL || size <= 0 ) return XUI_OK;
	while ( i < size ) {
		unsigned char c = (unsigned char)sText[i];
		if ( c == '\n' ) {
			line++;
			lineStart = i + 1;
			lineState = 0;
			i++;
			continue;
		}
		if ( c == ' ' || c == '\t' || c == '\r' ) {
			i++;
			continue;
		}
		if ( c == '#' && xideIsLineFirstNonSpace(sText, lineStart, i) ) {
			directive = xideHashDirective(sText, size, i);
			if ( directive == 1 ) {
				docLine = line;
			} else if ( directive == 2 && docLine >= 0 ) {
				ret = xideAddFoldRange(pRanges, iRangeCapacity, pRangeCount, docLine, line, 0, XUI_CODE_FOLD_REGION | XUI_CODE_FOLD_CUSTOM);
				if ( ret != XUI_OK ) return ret;
				docLine = -1;
			}
			i = xideScanLineEnd(sText, size, i);
			continue;
		}
		if ( i + 1 < size && sText[i] == '/' && sText[i + 1] == '/' ) {
			i = xideScanLineEnd(sText, size, i);
			continue;
		}
		if ( i + 1 < size && sText[i] == '/' && sText[i + 1] == '*' ) {
			i = xideFoldScanBlockComment(sText, size, i, &line, &lineStart, pRanges, iRangeCapacity, pRangeCount);
			if ( i < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
			continue;
		}
		if ( c == '\'' || c == '"' || xideTryStringPrefix(sText, size, i, &start) ) {
			i = xideFoldScanString(sText, size, i, &line, &lineStart);
			lineState = 2;
			continue;
		}
		if ( xideIsIdentStart(c) ) {
			start = i++;
			while ( i < size && xideIsIdent((unsigned char)sText[i]) ) i++;
			end = i;
			if ( lineState == 0 || lineState == 1 ) {
				if ( xideTextEquals(sText, start, end, "end") ) {
					ret = xideFoldCloseBlock(stack, &stackCount, line, pRanges, iRangeCapacity, pRangeCount);
					if ( ret != XUI_OK ) return ret;
					lineState = 2;
				} else if ( xideFoldIsBlockStart(sText, size, start, end) ) {
					if ( stackCount < XIDE_FOLD_STACK_MAX ) {
						stack[stackCount].iLine = line;
						stack[stackCount].iLevel = stackCount;
						stack[stackCount].iFlags = XUI_CODE_FOLD_REGION;
						stackCount++;
					}
					lineState = 2;
				} else if ( xideFoldIsModifier(sText, start, end) ) {
					lineState = 1;
				} else {
					lineState = 2;
				}
			}
			continue;
		}
		lineState = 2;
		i++;
	}
	return XUI_OK;
}

static int xideLex(xui_code_document_t* pDocument, int iStartLine, int iEndLine, xui_code_token_t* pTokens, int iTokenCapacity, int* pTokenCount, void* pUser)
{
	const char* sText;
	int size;
	int i;
	int start;
	int end;
	int quoteAt;
	(void)iStartLine;
	(void)iEndLine;
	(void)pUser;
	if ( pDocument == NULL || pTokenCount == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pTokenCount = 0;
	sText = xuiCodeDocumentGetText(pDocument);
	size = xuiCodeDocumentGetLength(pDocument);
	if ( sText == NULL || size <= 0 ) return XUI_OK;
	i = 0;
	while ( i < size ) {
		unsigned char c = (unsigned char)sText[i];
		if ( i + 1 < size && sText[i] == '/' && sText[i + 1] == '/' ) {
			start = i;
			i += 2;
			while ( i < size && sText[i] != '\n' && sText[i] != '\r' ) i++;
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, XUI_CODE_TOKEN_COMMENT);
		} else if ( i + 1 < size && sText[i] == '/' && sText[i + 1] == '*' ) {
			start = i;
			i = xideScanBlockComment(sText, size, i);
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, XUI_CODE_TOKEN_COMMENT);
		} else if ( c == '#' ) {
			start = i;
			if ( xideHashDirective(sText, size, i) != 0 ) {
				i = xideScanLineEnd(sText, size, i);
				(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, XUI_CODE_TOKEN_PREPROCESSOR);
			} else {
				i++;
				while ( i < size && sText[i] != '#' && sText[i] != '\n' && sText[i] != '\r' ) i++;
				if ( i < size && sText[i] == '#' ) i++;
				(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, XUI_CODE_TOKEN_NUMBER);
			}
		} else if ( c == '\'' || c == '"' || xideTryStringPrefix(sText, size, i, &quoteAt) ) {
			end = xideScanString(sText, size, i);
			if ( end > i ) {
				(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, i, end, XUI_CODE_TOKEN_STRING);
				i = end;
			} else {
				(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, i, i + 1, XUI_CODE_TOKEN_TEXT);
				i++;
			}
		} else if ( xideIsDigit(c) ) {
			start = i;
			i = xideScanNumber(sText, size, i);
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, XUI_CODE_TOKEN_NUMBER);
		} else if ( xideIsIdentStart(c) ) {
			start = i++;
			while ( i < size && xideIsIdent((unsigned char)sText[i]) ) i++;
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, start, i, xideIsKeyword(sText, start, i));
		} else if ( c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ) {
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, i, i + 1, XUI_CODE_TOKEN_BRACE);
			i++;
		} else if ( strchr("+-*/%=<>!&|^~:,.@;", c) != NULL ) {
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, i, i + 1, XUI_CODE_TOKEN_OPERATOR);
			i++;
		} else {
			(void)xideAddToken(pTokens, iTokenCapacity, pTokenCount, i, i + 1, XUI_CODE_TOKEN_TEXT);
			i++;
		}
	}
	return XUI_OK;
}

int xideXlangLanguageRegister(xui_widget pCodeEdit)
{
	static const char* extensions[] = { "xl", "xlh" };
	xui_code_language_t lang;
	if ( pCodeEdit == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&lang, 0, sizeof(lang));
	lang.iSize = sizeof(lang);
	lang.sId = "xlang";
	lang.sName = "xlang";
	lang.arrExtensions = extensions;
	lang.iExtensionCount = 2;
	lang.sLineComment = "//";
	lang.sBlockCommentStart = "/*";
	lang.sBlockCommentEnd = "*/";
	lang.iLexerType = XUI_CODE_LEXER_CALLBACK;
	lang.onLex = xideLex;
	lang.onFold = xideFold;
	return xuiCodeLanguageRegistryRegister(xuiCodeEditGetLanguageRegistry(pCodeEdit), &lang);
}
