#ifndef XL_TOKEN_H
#define XL_TOKEN_H

#include "../common/xl_common.h"

typedef enum XL_TokenKind {
	XL_TK_EOF = 0,
	XL_TK_ERROR,
	XL_TK_NEWLINE,
	XL_TK_IDENT,
	XL_TK_INT,
	XL_TK_INT_HEX,
	XL_TK_INT_OCT,
	XL_TK_INT_BIN,
	XL_TK_FLOAT,
	XL_TK_STRING,
	XL_TK_STRING_RAW,
	XL_TK_TEMPLATE,
	XL_TK_TIME,
	XL_KW_NULL,
	XL_KW_TRUE,
	XL_KW_FALSE,
	XL_KW_FUNC,
	XL_KW_FUNCTION,
	XL_KW_CONST,
	XL_KW_VAR,
	XL_KW_TYPEDEF,
	XL_KW_AS,
	XL_KW_IF,
	XL_KW_THEN,
	XL_KW_ELSE,
	XL_KW_ELSEIF,
	XL_KW_END,
	XL_KW_FOR,
	XL_KW_IN,
	XL_KW_WHILE,
	XL_KW_DO,
	XL_KW_BREAK,
	XL_KW_CONTINUE,
	XL_KW_RETURN,
	XL_KW_ASYNC,
	XL_KW_AWAIT,
	XL_KW_NEW,
	XL_KW_DELETE,
	XL_KW_YIELD,
	XL_KW_TRY,
	XL_KW_CATCH,
	XL_KW_FINALLY,
	XL_KW_THROW,
	XL_KW_ASSERT,
	XL_KW_IMPORT,
	XL_KW_EXPORT,
	XL_KW_EXTERN,
	XL_KW_INCLUDE,
	XL_KW_CLASS,
	XL_KW_CUSTOM,
	XL_KW_EXTENDS,
	XL_KW_UNION,
	XL_KW_CREATE,
	XL_KW_DESTROY,
	XL_KW_GET,
	XL_KW_LET,
	XL_KW_NOCOPY,
	XL_KW_AND,
	XL_KW_OR,
	XL_KW_NOT,
	XL_OP_GT,
	XL_OP_LT,
	XL_OP_GENERIC_LT,
	XL_OP_GE,
	XL_OP_LE,
	XL_OP_NE,
	XL_OP_EQ,
	XL_OP_APPROX,
	XL_OP_PLUS,
	XL_OP_MINUS,
	XL_OP_STAR,
	XL_OP_SLASH,
	XL_OP_PERCENT,
	XL_OP_POWER,
	XL_OP_AMP,
	XL_OP_PIPE,
	XL_OP_BANG,
	XL_OP_CARET,
	XL_OP_SHR,
	XL_OP_SHL,
	XL_OP_ASSIGN,
	XL_OP_PLUS_ASSIGN,
	XL_OP_MINUS_ASSIGN,
	XL_OP_STAR_ASSIGN,
	XL_OP_SLASH_ASSIGN,
	XL_OP_PERCENT_ASSIGN,
	XL_OP_POWER_ASSIGN,
	XL_OP_FAT_ARROW,
	XL_OP_QUESTION,
	XL_OP_COLON,
	XL_OP_NULL_COALESCE,
	XL_OP_NULL_ASSIGN,
	XL_OP_DOT,
	XL_OP_SAFE_DOT,
	XL_OP_SAFE_LBRACKET,
	XL_OP_RANGE,
	XL_OP_ELLIPSIS,
	XL_TK_LPAREN,
	XL_TK_RPAREN,
	XL_TK_LBRACKET,
	XL_TK_RBRACKET,
	XL_TK_LBRACE,
	XL_TK_RBRACE,
	XL_TK_COMMA,
	XL_TK_SEMICOLON,
	XL_TK_COUNT
} XL_TokenKind;

typedef enum XL_TokenValueKind {
	XL_TV_NONE = 0,
	XL_TV_INT = 1,
	XL_TV_FLOAT = 2
} XL_TokenValueKind;

typedef struct XL_Token {
	XL_TokenKind eKind;
	const char* sText;
	size_t iTextLen;
	size_t iStartOffset;
	size_t iEndOffset;
	int iStartLine;
	int iStartColumn;
	int iEndLine;
	int iEndColumn;
	XL_TokenValueKind eValueKind;
	int64_t iIntValue;
	double fFloatValue;
	char sMessage[128];
} XL_Token;

typedef struct XL_TokenList {
	XL_Token* pItems;
	size_t iCount;
	size_t iCapacity;
} XL_TokenList;

typedef struct XL_LexerExtra {
	const char* sFileName;
	size_t iOffset;
	size_t iTokenStartOffset;
	int iLine;
	int iColumn;
	int iTokenStartLine;
	int iTokenStartColumn;
	int bHadError;
	char sErrorText[128];
	XL_Token currentToken;
} XL_LexerExtra;

static inline const char* xlTokenKindName(XL_TokenKind eKind)
{
	static const char* arrNames[] = {
		"EOF", "ERROR", "NEWLINE",
		"IDENT", "INT", "INT_HEX", "INT_OCT", "INT_BIN", "FLOAT", "STRING", "STRING_RAW", "TEMPLATE", "TIME",
		"null", "true", "false", "func", "function", "const", "var", "typedef", "as",
		"if", "then", "else", "elseif", "end", "for", "in", "while", "do", "break", "continue", "return",
		"async", "await", "new", "delete", "yield", "try", "catch", "finally", "throw", "assert", "import", "export", "extern", "include",
		"class", "custom", "extends", "union", "create", "destroy", "get", "let",
		"and", "or", "not",
		">", "<", "GENERIC_LT", ">=", "<=", "!=", "==", "~=", "+", "-", "*", "/", "%", "**",
		"&", "|", "!", "^", ">>", "<<", "=", "+=", "-=", "*=", "/=", "%=", "**=", "=>",
		"?", ":", "??", "?=", ".", "?.", "?[", "..", "...",
		"(", ")", "[", "]", "{", "}", ",", ";"
	};

	if ( ((int)eKind >= 0) && (eKind < XL_TK_COUNT) ) {
		return arrNames[eKind];
	}
	return "UNKNOWN";
}

static inline int xlIsSoftTypeName(const char* sText, size_t iLen)
{
	static const char* arrNames[] = {
		"bool", "int", "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
		"float", "f32", "f64", "string", "time", "point",
		"array", "list", "set", "dict", "range",
		"var", "auto", "type", "function",
		"error", "module", "future", "task", "channel", "generator"
	};
	size_t i;

	if ( sText == NULL ) {
		return 0;
	}

	for ( i = 0; i < XL_COUNTOF(arrNames); ++i ) {
		if ( strlen(arrNames[i]) != iLen ) {
			continue;
		}
		if ( memcmp(arrNames[i], sText, iLen) == 0 ) {
			return 1;
		}
	}

	return 0;
}

#endif
