#ifndef XLANG_ANALYSIS_FRONTEND_ENTRY_CACHE_H
#define XLANG_ANALYSIS_FRONTEND_ENTRY_CACHE_H
#include "analysis/semantic.h"
#include "analysis/frontend_internal.h"
#include "src/parser/xl6_parser_api.h"
#include <xrt.h>
struct XL6_CompletionQuery;
typedef struct xls_entry_cache xls_entry_cache;
typedef struct xls_entry_cache_data xls_entry_cache_data;
/* One-owner ordinary entry/import AST aid, before import/semantic mutation.
 * Imports retain immutable ordinary syntax chunks; eligible function edits
 * reuse the shared syntax updater and always make a private compiler tree.
 * Original source loading/reporting always runs first. A shared completed-data
 * budget includes context, text/path and retained chunks/capacities. Each
 * generation charges shared chunks in full; allocator/transient work is extra.
 * Module keys use XRT and a 512-record LRU; no proof/semantic IDs are cached. */
xls_entry_cache* xlsEntryCacheCreate(size_t bytes);
void xlsEntryCacheDestroy(xls_entry_cache* cache);
size_t xlsEntryCacheBytes(const xls_entry_cache* cache);
size_t xlsModuleCacheBytes(const xls_entry_cache* cache);
typedef struct xls_entry_cache_scope {
    xls_entry_cache* cache;
    xls_entry_cache_data* pending;
    struct xls_entry_cache_scope* previous;
    const xls_context* context;
    const char* entry;
    char* canonicalEntry;
    xls_completion_entry_work work;
    xls_entry_parse_work moduleWork;
    xls_parse_work moduleSyntaxWork;
    xmap pendingModules;
    size_t pendingModuleBytes;
    int pendingModulesReady;
} xls_entry_cache_scope;
/* Nested/default calls shadow the outer cache. A cancelled/failed query drops
 * pending captures. Existing module hits may update LRU order, even on cancel.
 * Committed and pending generations each obey the logical byte budget.
 * A cache cannot be reentered or shared by owners. */
void xlsFrontendEntryBegin(xls_entry_cache_scope* scope,xls_entry_cache* cache,const xls_context* context,const char* entry);
xls_completion_entry_work xlsFrontendEntryEnd(xls_entry_cache_scope* scope,int commit);
/* Audited source-overlay adapter calls this after the ORIGINAL disk/overlay
 * report capture. Cursor targets always use the original cursor parser.
 * A limit/copy/allocation failure falls back without shrinking the language. */
int xlsFrontendEntryParse(const char* text,const char* path,XL6_ParseResult* result,struct XL6_CompletionQuery* query);
#endif
