#ifndef XIDE_WORKSPACE_SYMBOLS_H
#define XIDE_WORKSPACE_SYMBOLS_H
#include "services/language_check.h"
#include "services/workspace_symbol_index.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/module_store.h"
typedef struct xide_workspace_symbols xide_workspace_symbols;
typedef struct xide_workspace_symbol_query xide_workspace_symbol_query;
typedef struct xide_workspace_symbol_work {
 size_t roots,captured,reused,covered,sourceRejections,guardBytes,overlayHashBytes;
 uint64_t coordinatorThread,analysisThread;size_t analysisBuilds,analysisHits;
 xide_workspace_symbol_stats index;
} xide_workspace_symbol_work;
enum xide_workspace_symbol_state {XIDE_WORKSPACE_SYMBOL_FAILED=-1,XIDE_WORKSPACE_SYMBOL_PENDING=0,
 XIDE_WORKSPACE_SYMBOL_READY=1,XIDE_WORKSPACE_SYMBOL_CANCELLED=2};
/* Independent single-owner XRT coordinator + one bounded semantic actor;
 * foreground completion/diagnostics do not wait in this queue. Borrowed cache
 * outlives Destroy;module assets use the existing worker ownership contract.
 * Eight pending jobs;8 semantic sessions/64MiB known inputs;128MiB index and
 *128MiB compact guard/overlay-topology records per current cohort;one shared
 * original input-batch history additionally retains up to64MiB input storage.
 * Admitted input history may advance when a later workspace member fails;
 * index facts are published only after complete coverage. Active
 * work,queued input snapshots,old caller-held results and allocator cost are
 * extra. Destroy cancels/joins;retained result rows outlive this service. */
xide_workspace_symbols* xideWorkspaceSymbolsCreate(xide_toolchains* cache,const char* compiler,xls_module_store* modules);
void xideWorkspaceSymbolsDestroy(xide_workspace_symbols* store);
/* Prepared contexts,not manifests: at most64 full project/file scopes and
 *65536 total entry/member bindings. Every .xl member is covered by its own
 * checked entry or a checked importer. Configured primary entries always
 * keep their program check;uncovered source failures never yield partial
 * success. Zero scopes/overlays is valid (empty workspace).
 * Submit retains immutable inputs without filesystem/compiler/body hashing.
 * Background work reuses guarded shards,verifies actual overlay digests/BOM
 * and negative import candidates,then publishes the complete cohort once.
 * Caller must still gate replies on overlay/catalog/watcher epochs. This is
 * not filesystem discovery,protocol serialization or a stable resolve ID. */
xide_workspace_symbol_query* xideWorkspaceSymbolsSubmit(xide_workspace_symbols* store,
 xls_context* const* contexts,size_t contextCount,const xls_analysis_document* documents,size_t documentCount,
 const char* query,size_t limit);
enum xide_workspace_symbol_state xideWorkspaceSymbolsResult(xide_workspace_symbol_query* query,
 const xide_workspace_symbol_result** result,xide_workspace_symbol_work* work,char* error,size_t capacity);
/* Transfers once;destroy with xideWorkspaceSymbolResultDestroy. */
xide_workspace_symbol_result* xideWorkspaceSymbolsTake(xide_workspace_symbol_query* query);
int xideWorkspaceSymbolsSetWake(xide_workspace_symbol_query* query,void (*wake)(void*),void* user);
void xideWorkspaceSymbolsCancel(xide_workspace_symbol_query* query);
void xideWorkspaceSymbolsQueryDestroy(xide_workspace_symbol_query* query);
#endif
#endif
