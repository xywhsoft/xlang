#ifndef XL6_TOKEN_H
#define XL6_TOKEN_H

#include "src/common/xl6_common.h"

typedef enum XL6_TokenKind {
    XL6_TK_EOF = 0,
    XL6_TK_ERROR,
    XL6_TK_IDENT,
    XL6_TK_INT,
    XL6_TK_FLOAT,
    XL6_TK_STRING,
    XL6_TK_FSTRING,
    XL6_TK_TIME,
    XL6_TK_DOC,
    XL6_TK_SEP,

    XL6_KW_NULL,
    XL6_KW_TRUE,
    XL6_KW_FALSE,
    XL6_KW_FUNC,
    XL6_KW_CONST,
    XL6_KW_VAR,
    XL6_KW_AUTO,
    XL6_KW_TYPEDEF,
    XL6_KW_AS,
    XL6_KW_IF,
    XL6_KW_THEN,
    XL6_KW_ELSE,
    XL6_KW_ELSEIF,
    XL6_KW_END,
    XL6_KW_FOR,
    XL6_KW_IN,
    XL6_KW_IS,
    XL6_KW_WHILE,
    XL6_KW_DO,
    XL6_KW_BREAK,
    XL6_KW_CONTINUE,
    XL6_KW_RETURN,
    XL6_KW_ASYNC,
    XL6_KW_AWAIT,
    XL6_KW_YIELD,
    XL6_KW_TRY,
    XL6_KW_CATCH,
    XL6_KW_FINALLY,
    XL6_KW_THROW,
    XL6_KW_ASSERT,
    XL6_KW_IMPORT,
    XL6_KW_EXPORT,
    XL6_KW_EXTERN,
    XL6_KW_INCLUDE,
    XL6_KW_DEFINE,
    XL6_KW_CLASS,
    XL6_KW_EXTENDS,
    XL6_KW_UNION,
    XL6_KW_DERIVE,
    XL6_KW_GET,
    XL6_KW_LET,
    XL6_KW_PUBLIC,
    XL6_KW_PRIVATE,
    XL6_KW_PROTECTED,
    XL6_KW_STATIC,
    XL6_KW_OVERRIDE,
    XL6_KW_FINAL,
    XL6_KW_VIRTUAL,
    XL6_KW_NATIVE,
    XL6_KW_AND,
    XL6_KW_OR,
    XL6_KW_NOT,

    XL6_TK_LPAREN,
    XL6_TK_RPAREN,
    XL6_TK_LBRACKET,
    XL6_TK_RBRACKET,
    XL6_TK_LBRACE,
    XL6_TK_RBRACE,
    XL6_TK_COMMA,
    XL6_TK_COLON,
    XL6_TK_DOT,
    XL6_TK_AT,
    XL6_TK_SCOPE,
    XL6_TK_ARROW,
    XL6_TK_FAT_ARROW,
    XL6_TK_ELLIPSIS,
    XL6_TK_TRIPLE_STAR,

    XL6_TK_ASSIGN,
    XL6_TK_PLUS_ASSIGN,
    XL6_TK_MINUS_ASSIGN,
    XL6_TK_STAR_ASSIGN,
    XL6_TK_SLASH_ASSIGN,
    XL6_TK_PERCENT_ASSIGN,
    XL6_TK_POW_ASSIGN,
    XL6_TK_BIT_AND_ASSIGN,
    XL6_TK_BIT_OR_ASSIGN,
    XL6_TK_BIT_XOR_ASSIGN,
    XL6_TK_NULL_ASSIGN,

    XL6_TK_PLUS,
    XL6_TK_MINUS,
    XL6_TK_STAR,
    XL6_TK_SLASH,
    XL6_TK_PERCENT,
    XL6_TK_POW,
    XL6_TK_EQ,
    XL6_TK_NE,
    XL6_TK_LT,
    XL6_TK_LE,
    XL6_TK_GT,
    XL6_TK_GE,
    XL6_TK_APPROX,
    XL6_TK_QUESTION,
    XL6_TK_NULL_COALESCE,
    XL6_TK_SAFE_DOT,
    XL6_TK_SAFE_LBRACKET,
    XL6_TK_BANG,
    XL6_TK_BIT_NOT,
    XL6_TK_BIT_AND,
    XL6_TK_BIT_OR,
    XL6_TK_BIT_XOR,
    XL6_TK_SHL,
    XL6_TK_SHR
} XL6_TokenKind;

typedef struct XL6_Token {
    XL6_TokenKind kind;
    const char* text;
    size_t len;
    size_t startOffset;
    size_t endOffset;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    char message[128];
} XL6_Token;

const char* xl6TokenKindName(XL6_TokenKind kind);
int xl6TokenIsKeyword(XL6_TokenKind kind);

#endif
