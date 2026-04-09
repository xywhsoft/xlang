#ifndef XL_LEXER_API_H
#define XL_LEXER_API_H

#include "xl_token.h"

typedef struct XL_Lexer XL_Lexer;

XL_Lexer* xlLexerCreateFromString(const char* sSource, size_t iLen, const char* sFileName);
XL_Lexer* xlLexerCreateFromFile(const char* sFilePath);
void xlLexerDestroy(XL_Lexer* pLexer);

int xlLexerTokenizeAll(XL_Lexer* pLexer, XL_TokenList* pList);
void xlTokenListInit(XL_TokenList* pList);
void xlTokenListFree(XL_TokenList* pList);

char* xlLexerTokensToText(const XL_TokenList* pList);
char* xlLexerTokensToJson(const XL_TokenList* pList);
const char* xlLexerGetError(const XL_Lexer* pLexer);
int xlLexerHadError(const XL_Lexer* pLexer);

#endif
