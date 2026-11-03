#ifndef XL6_PARSER_API_H
#define XL6_PARSER_API_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

typedef struct XL6_ParseResult {
    XL6_AstNode* root;
    XL6_DiagList diags;
} XL6_ParseResult;

typedef struct XL6_ParseContext {
    XL6_AstNode* root;
    XL6_DiagList* diags;
    int hadError;
} XL6_ParseContext;

void xl6ParseResultInit(XL6_ParseResult* result);
void xl6ParseResultFree(XL6_ParseResult* result);
int xl6ParseSource(const char* source, const char* fileName, XL6_ParseResult* result);
struct XL6_CompletionQuery;
int xl6ParseSourceWithCompletion(const char* source,const char* fileName,
    XL6_ParseResult* result,struct XL6_CompletionQuery* query);
int xl6ParseFile(const char* path, XL6_ParseResult* result);

#endif
