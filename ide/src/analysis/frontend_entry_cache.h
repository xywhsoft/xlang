#ifndef XLANG_ANALYSIS_FRONTEND_ENTRY_CACHE_H
#define XLANG_ANALYSIS_FRONTEND_ENTRY_CACHE_H
#include "analysis/semantic.h"
#include "src/parser/xl6_parser_api.h"
struct XL6_CompletionQuery;
typedef struct xls_entry_cache xls_entry_cache;
typedef struct xls_entry_cache_data xls_entry_cache_data;
/* Private one-owner aid for an ordinary entry parse, before import/semantic
 * mutation. Shared source loading/reporting always runs before this hook.
 * No disk proof or semantic identity is cached here. Record budget includes
 * the full context, exact text/path and independently owned cloned AST. */
xls_entry_cache* xlsEntryCacheCreate(size_t bytes);
void xlsEntryCacheDestroy(xls_entry_cache* cache);
size_t xlsEntryCacheBytes(const xls_entry_cache* cache);
typedef struct xls_entry_cache_scope {
    xls_entry_cache* cache;
    xls_entry_cache_data* pending;
    struct xls_entry_cache_scope* previous;
    const xls_context* context;
    const char* entry;
    char* canonicalEntry;
    xls_completion_entry_work work;
} xls_entry_cache_scope;
/* Nested/default calls shadow the outer cache. A cancelled/failed query drops
 * only its pending capture. A cache cannot be reentered or shared by owners. */
void xlsFrontendEntryBegin(xls_entry_cache_scope* scope,xls_entry_cache* cache,const xls_context* context,const char* entry);
xls_completion_entry_work xlsFrontendEntryEnd(xls_entry_cache_scope* scope,int commit);
/* Audited source-overlay adapter calls this after the ORIGINAL disk/overlay
 * report capture. Cursor targets and other modules use the original parser.
 * A limit/copy/allocation failure falls back without shrinking the language. */
int xlsFrontendEntryParse(const char* text,const char* path,XL6_ParseResult* result,struct XL6_CompletionQuery* query);
#endif
