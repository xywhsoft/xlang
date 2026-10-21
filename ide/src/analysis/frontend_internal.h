#ifndef XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#define XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#include "analysis/session.h"
#include "src/parser/xl6_parser_api.h"
/* Compiler facts remain inside the analysis layer. Never mutate a published
 * parse tree; semantic passes require their own owned representation. */
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* syntax);
typedef struct xls_parse_work { uint64_t full,fragments,reused,bytes; } xls_parse_work;
xls_syntax* xlsSyntaxBuild(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work);
/* Nodes are immutable chunk facts, with original chunk coordinates. Use the
 * supplied current range; it includes edits before reused declarations.
 * Visitor >0 continues, 0 stops normally, <0 fails. No AST is materialized. */
typedef int (*xls_syntax_visit)(const XL6_AstNode* node,const XL6_AstRange* current,void* user);
int xlsSyntaxVisit(const xls_syntax* syntax,xls_syntax_visit visit,void* user,
    int (*cancelled)(void*),void* cancelUser);
#endif
