#ifndef XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#define XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#include "analysis/session.h"
#include "analysis/check.h"
#include "src/parser/xl6_parser_api.h"
#include "src/ir/xl6_ir.h"
/* Compiler facts remain inside the analysis layer. Never mutate a published
 * parse tree; semantic passes require their own owned representation. */
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* syntax);
const XL6_IrModule* xlsSemanticCheckIr(const xls_semantic_check* result);
const XL6_AstNode* xlsSemanticCheckAst(const xls_semantic_check* result);
typedef struct xls_parse_work { uint64_t full,fragments,reused,bytes; } xls_parse_work;
xls_syntax* xlsSyntaxBuild(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work);
struct XL6_CompletionQuery;
/* Private cursor parse. Query starts empty except absolute path/offset and
 * owns the shared parser's prefix on return, including rejected source.
 * Previous cursor chunks are immutable; ordinary/cursor modes never mix. */
xls_syntax* xlsSyntaxBuildCursor(const char* path,xls_text* text,const xls_syntax* previous,
    struct XL6_CompletionQuery* query,xls_parse_work* work);
int xlsSyntaxIsCursor(const xls_syntax* syntax);
/* Query starts empty except matching path/offset; destination parse is empty.
 * Prefix is independently owned, error borrows syntax. Failure changes neither
 * output. Semantic results must copy the error before releasing syntax. */
int xlsSyntaxCopyCursorParse(const xls_syntax* syntax,struct XL6_CompletionQuery* query,XL6_ParseResult* result);
/* Exact entry cursor snapshot only. The result owns its mutable AST/query and
 * never borrows prefix/error/source state from the syntax snapshot. */
xls_semantic_completion* xlsSemanticCompletionCreateParsed(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_semantic_completion* xlsSemanticCompletionCreateParsedInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Private mutable copy for compiler semantic passes. Destination must be
 * empty/initialized. Diagnostics and sources are independently owned. */
int xlsSyntaxCopyParse(const xls_syntax* syntax,XL6_ParseResult* result);
xls_semantic_check* xlsSemanticCheckCreateParsed(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_semantic_check* xlsSemanticCheckCreateIncremental(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_semantic_check* xlsSemanticCheckCreateIncrementalInContext(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Nodes are immutable chunk facts, with original chunk coordinates. Use the
 * supplied current range; it includes edits before reused declarations.
 * Visitor >0 continues, 0 stops normally, <0 fails. No AST is materialized. */
typedef int (*xls_syntax_visit)(const XL6_AstNode* node,const XL6_AstRange* current,void* user);
int xlsSyntaxVisit(const xls_syntax* syntax,xls_syntax_visit visit,void* user,
    int (*cancelled)(void*),void* cancelUser);
#endif
