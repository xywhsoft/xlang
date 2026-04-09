#ifndef XL_PARSER_API_H
#define XL_PARSER_API_H

#include "../ast/xl_ast.h"
#include "../diag/xl_diag.h"
#include "../lexer/xl_lexer_api.h"

typedef struct XL_ParseResult {
	XL_AstNode* pRoot;
	int bHadError;
	int iErrorCount;
} XL_ParseResult;

void xlParseResultInit(XL_ParseResult* pResult);
void xlParseResultFree(XL_ParseResult* pResult);
int xlParserParseLexer(XL_Lexer* pLexer, XL_DiagContext* pDiag, int bTraceParser, XL_ParseResult* pResult);

#endif
