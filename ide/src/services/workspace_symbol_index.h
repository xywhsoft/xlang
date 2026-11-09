#ifndef XIDE_WORKSPACE_SYMBOL_INDEX_H
#define XIDE_WORKSPACE_SYMBOL_INDEX_H
#include "analysis/context.h"
#include "services/semantic_symbols.h"

typedef struct xide_workspace_symbol_index xide_workspace_symbol_index;
typedef struct xide_workspace_symbol_update xide_workspace_symbol_update;
typedef struct xide_workspace_symbol_snapshot xide_workspace_symbol_snapshot;
typedef struct xide_workspace_symbol_result xide_workspace_symbol_result;
typedef struct xide_workspace_symbol_limits {size_t bytes,parts,rows,results;} xide_workspace_symbol_limits;
typedef struct xide_workspace_symbol_stats {
 uint64_t generation;size_t parts,scopes,rows,duplicateRows,bytes;
} xide_workspace_symbol_stats;
typedef struct xide_workspace_symbol_view {
 const char *owner,*qualified;const xls_context* scope;
 uint64_t generation;size_t ticket;int score;xide_semantic_symbol_view declaration;
} xide_workspace_symbol_view;
/* Single submitting owner. Immutable acquired snapshots/results may be read
 * concurrently and survive index destruction. No parser, compiler, AST,
 * directory scan, filesystem I/O or protocol calls. Platform lexical source
 * keys follow the existing navigation path policy. Zero limits use defaults:
 * 128MiB known retained payload,65536 owners,1048576 rows,4096 query results.
 * This is not an RSS/transient budget;old snapshots and active updates/results
 * can retain their previous bounded cohorts. Freshness remains caller-owned. */
xide_workspace_symbol_index* xideWorkspaceSymbolIndexCreate(const xide_workspace_symbol_limits* limits);
void xideWorkspaceSymbolIndexDestroy(xide_workspace_symbol_index* index);
/* Begin retains the base revision and existing immutable owner shards.
 * Set prepares only the changed shard;scope is the full semantic project/file
 * context,not necessarily its selected compiler entry. Owner keys are stable
 * caller-prepared UTF8 identifiers. Invalid/OOM/cancel poisons the transaction.
 * Commit always consumes the update,including on conflict/failure. Earlier
 * concurrent updates or index destruction cannot be overwritten. */
xide_workspace_symbol_update* xideWorkspaceSymbolIndexBegin(xide_workspace_symbol_index* index,
 int (*stopped)(void*),void* user,xide_model_error* error);
int xideWorkspaceSymbolIndexSet(xide_workspace_symbol_update* update,const char* owner,
 xls_context* scope,xide_semantic_symbols* symbols,xide_model_error* error);
int xideWorkspaceSymbolIndexRemove(xide_workspace_symbol_update* update,const char* owner,xide_model_error* error);
int xideWorkspaceSymbolIndexCommit(xide_workspace_symbol_update* update,xide_model_error* error);
void xideWorkspaceSymbolIndexAbort(xide_workspace_symbol_update* update);
xide_workspace_symbol_snapshot* xideWorkspaceSymbolIndexAcquire(xide_workspace_symbol_index* index);
xide_workspace_symbol_snapshot* xideWorkspaceSymbolSnapshotRetain(xide_workspace_symbol_snapshot* snapshot);
void xideWorkspaceSymbolSnapshotRelease(xide_workspace_symbol_snapshot* snapshot);
xide_workspace_symbol_stats xideWorkspaceSymbolSnapshotStats(const xide_workspace_symbol_snapshot* snapshot);
/* Stable snapshot-local position;reject another generation or an invalid ID.
 * This resolves retained data only. The protocol host must validate source,
 * configuration and overlay epochs before publishing/navigating a location. */
int xideWorkspaceSymbolSnapshotResolve(const xide_workspace_symbol_snapshot* snapshot,
 uint64_t generation,size_t ticket,xide_workspace_symbol_view* out);
/* Strict UTF8 query <=512 bytes/128 scalars/16 whitespace-separated terms.
 * Every term must match. ASCII case folding,exact/prefix/substring bonuses,
 * UTF8 scalar subsequences and camel/separator boundaries;nonASCII matches
 * exact scalars. Empty query lists symbols. Bounded best-K heap,complete match
 * count and deterministic ties;no per-row query allocation. Shared declarations
 * in the same semantic scope/source/name/range are deduplicated. */
xide_workspace_symbol_result* xideWorkspaceSymbolSnapshotQuery(xide_workspace_symbol_snapshot* snapshot,
 const char* query,size_t limit,int (*stopped)(void*),void* user,xide_model_error* error);
size_t xideWorkspaceSymbolResultCount(const xide_workspace_symbol_result* result);
size_t xideWorkspaceSymbolResultMatches(const xide_workspace_symbol_result* result);
int xideWorkspaceSymbolResultAt(const xide_workspace_symbol_result* result,size_t at,xide_workspace_symbol_view* out);
void xideWorkspaceSymbolResultDestroy(xide_workspace_symbol_result* result);
#endif
