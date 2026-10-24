#ifndef XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#define XLANG_ANALYSIS_FRONTEND_INTERNAL_H
#include "analysis/session.h"
#include "analysis/check.h"
#include "src/parser/xl6_parser_api.h"
#include "src/ir/xl6_ir.h"
#include "analysis/frontend_sem.h"
#include "analysis/ast_materialize.h"
/* Compiler facts remain inside the analysis layer. Never mutate a published
 * parse tree; semantic passes require their own owned representation. */
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* syntax);
const XL6_IrModule* xlsSemanticCheckIr(const xls_semantic_check* result);
const XL6_AstNode* xlsSemanticCheckAst(const xls_semantic_check* result);
/* Immutable mask in the producing API's overlay order, captured by the
 * original frontend's real lookups. Unknown/incomplete/invalid owners reject.
 * Private sessions additionally prove unchanged membership and mandatory
 * entry/query bodies, then still validate all original disk/probe inputs. */
int xlsSemanticCheckInputUsage(const xls_semantic_check* result,uint64_t* used);
int xlsSemanticCompletionInputUsage(const xls_semantic_completion* result,uint64_t* used);
typedef struct xls_parse_work { uint64_t full,fragments,reused,bytes; } xls_parse_work;
xls_syntax* xlsSyntaxBuild(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work);
/* Private ordinary module-cache construction. Adopt consumes *root after
 * allocation (including a subsequent failure); leaves it intact if allocation
 * fails. No published AST is adopted. TryFunction never falls back to a full
 * parse. Failed/uncertain edits use the caller's original parser instead. */
xls_syntax* xlsSyntaxAdoptOrdinary(const char* path,xls_text* text,XL6_AstNode** root);
xls_syntax* xlsSyntaxTryFunction(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work);
/* Completed private cache data, including each retained chunk in full even
 * when another generation shares it. Excludes allocator/sort overhead and
 * transient compiler copies. No lazy AST view is permitted in this cache. */
int xlsSyntaxCacheBytes(const xls_syntax* syntax,size_t limit,size_t* bytes);
xls_parse_work xlsSemanticCheckModuleParseWork(const xls_semantic_check* result);
xls_ast_intern_work xlsSemanticCheckMemoTypes(const xls_semantic_check* result);
struct XL6_CompletionQuery;
/* Private cursor parse. Query starts empty except absolute path/offset and
 * owns the shared parser's prefix on return, including rejected source.
 * Previous cursor chunks are immutable; ordinary/cursor modes never mix. */
xls_syntax* xlsSyntaxBuildCursor(const char* path,xls_text* text,const xls_syntax* previous,
    struct XL6_CompletionQuery* query,xls_parse_work* work);
/* Signature projections have a distinct mode and the original call's LPAREN
 * marker. Reuse the same function updater; the caller supplies the shared
 * signature parser in local fragment coordinates. No second grammar. */
typedef int (*xls_syntax_parse_fn)(const char*,const char*,XL6_ParseResult*,struct XL6_CompletionQuery*,void*);
xls_syntax* xlsSyntaxTrySignatureFunction(const char* path,xls_text* text,const xls_syntax* previous,
    struct XL6_CompletionQuery* query,xls_syntax_parse_fn parser,void* user,xls_parse_work* work);
xls_syntax* xlsSyntaxCaptureSignature(const char* path,xls_text* text,const XL6_AstNode* root,
    const struct XL6_CompletionQuery* query,size_t bytes);
int xlsSyntaxSignatureMatches(const xls_syntax*,const char* path,size_t offset,size_t callOpen);
int xlsSyntaxCopySignatureParse(const xls_syntax*,struct XL6_CompletionQuery*,XL6_ParseResult*,size_t callOpen);
int xlsSyntaxSignatureCacheBytes(const xls_syntax*,size_t limit,size_t* bytes);
int xlsSyntaxIsCursor(const xls_syntax* syntax);
int xlsSyntaxCursorMatches(const xls_syntax* syntax,const char* path,size_t offset);
/* Query starts empty except matching path/offset; destination parse is empty.
 * Prefix is independently owned, error borrows syntax. Failure changes neither
 * output. Semantic results must copy the error before releasing syntax. */
int xlsSyntaxCopyCursorParse(const xls_syntax* syntax,struct XL6_CompletionQuery* query,XL6_ParseResult* result);
/* Exact query cursor snapshot. The result owns its mutable AST/query and
 * never borrows prefix/error/source state from the syntax snapshot. */
xls_semantic_completion* xlsSemanticCompletionCreateParsed(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_semantic_completion* xlsSemanticCompletionCreateParsedInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
struct xls_entry_cache;
/* Entry/source reporting remains the original shared frontend. This optional
 * one-owner cache only copies an ordinary pre-import entry AST. */
xls_semantic_completion* xlsSemanticCompletionCreateCachedInContext(xls_context* context,struct xls_entry_cache* cache,
    const char* entry,const char* queryPath,size_t offset,const xls_semantic_input* inputs,size_t count,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Retained cursor syntax for the opened query, including an imported query
 * with a closed entry. Imported projection use follows original source capture
 * and requires exact bytes/path/offset. Ordinary sources may use the cache. */
xls_semantic_completion* xlsSemanticCompletionCreateCachedParsedInContext(xls_context* context,struct xls_entry_cache* cache,
    const char* entry,const char* queryPath,size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
int xlsSemanticCompletionCursorSyntaxApplied(const xls_semantic_completion* result);
/* Session-owned opt-in. Fresh public completion APIs remain independent
 * oracles. Previous results/memos are immutable and retained by the caller. */
xls_semantic_completion* xlsSemanticCompletionCreateIncrementalInContext(xls_context* context,struct xls_entry_cache* cache,
    const char* entry,const char* queryPath,size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    xls_semantic_completion* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_sem_work xlsSemanticCompletionSemWork(const xls_semantic_completion* result);
xls_sem_call_work xlsSemanticCompletionCallWork(const xls_semantic_completion* result);
xls_sem_storage_work xlsSemanticCompletionStorageWork(const xls_semantic_completion* result);
xls_ast_materialize_work xlsSemanticCompletionMaterializationWork(const xls_semantic_completion* result);
/* One mutex-protected materialization precedes full AST exposure. Query JSON,
 * items and immutable semantic units never require or observe partial IR data. */
const XL6_AstNode* xlsSemanticCompletionAst(const xls_semantic_completion* result);
xls_sem_tail_work xlsSemanticCompletionTailWork(const xls_semantic_completion* result);
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
/* Ordinary entry/import cache after shared source loading/reporting. Parsed
 * overlay syntax bypasses entry parsing; ordinary imports may still reuse. */
xls_semantic_check* xlsSemanticCheckCreateCachedInContext(xls_context* context,struct xls_entry_cache* cache,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Nodes are immutable chunk facts, with original chunk coordinates. Use the
 * supplied current range; it includes edits before reused declarations.
 * Visitor >0 continues, 0 stops normally, <0 fails. No AST is materialized. */
typedef int (*xls_syntax_visit)(const XL6_AstNode* node,const XL6_AstRange* current,void* user);
int xlsSyntaxVisit(const xls_syntax* syntax,xls_syntax_visit visit,void* user,
    int (*cancelled)(void*),void* cancelUser);
#endif
