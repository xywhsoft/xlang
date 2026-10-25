#ifndef XLANG_ANALYSIS_MODULE_STORE_H
#define XLANG_ANALYSIS_MODULE_STORE_H
#include "analysis/session.h"
typedef struct xls_module_store xls_module_store;
typedef struct xls_module_snapshot xls_module_snapshot;
typedef struct xls_module_limits {size_t modules,versions,bytes;} xls_module_limits;
typedef struct xls_module_stats {
    uint64_t lookups,hits,misses,published,deduplicated,evictions,declined;
    size_t modules,versions,bytes,registryBytes,limit;
} xls_module_stats;
/* Process-lifetime ordinary syntax bank,not a semantic/configuration cache.
 * Paths are exact caller-canonical UTF-8 labels. Module IDs remain stable
 * across content changes and syntax eviction,never reused during this store.
 * A revision identifies an immutable cache publication,not an editor version
 * or an authoritative latest file. Original loading/input proofs still run.
 * Defaults:65536 names,1024 versions,64MiB store-owned known storage. Charges
 * include actual syntax capacities (shared chunks charged in full per view),
 * registry and version records;allocator/OS/transient/caller-held storage extra.
 * Registry admission may evict syntax;identity/LRU work is not rolled back on
 * query cancellation. Only valid ordinary syntax may be published. */
xls_module_store* xlsModuleStoreCreate(const xls_module_limits* limits);
xls_module_store* xlsModuleStoreRetain(xls_module_store* store);
void xlsModuleStoreRelease(xls_module_store* store);
/* Thread-safe. No file I/O. Allocations,source comparison,parse/copy and
 * destruction happen outside metadata locks. Caller retains store during calls.
 * Cache refusal never changes language diagnostics or the caller's syntax. */
uint64_t xlsModuleStoreIdentity(xls_module_store* store,const char* path);
/* Retained exact byte match,otherwise the newest resident incremental seed.
 * A miss may return NULL;the caller must check exact before treating it as a
 * hit. Concurrent publication/eviction may replace any resident version. */
xls_module_snapshot* xlsModuleStoreAcquire(xls_module_store* store,const char* path,
    const char* text,size_t length,int* exact);
int xlsModuleStorePublish(xls_module_store* store,const char* path,xls_syntax* syntax);
void xlsModuleStoreStats(xls_module_store* store,xls_module_stats* stats);
xls_module_snapshot* xlsModuleSnapshotRetain(xls_module_snapshot* snapshot);
void xlsModuleSnapshotRelease(xls_module_snapshot* snapshot);
uint64_t xlsModuleSnapshotIdentity(const xls_module_snapshot* snapshot);
uint64_t xlsModuleSnapshotRevision(const xls_module_snapshot* snapshot);
const char* xlsModuleSnapshotPath(const xls_module_snapshot* snapshot);
/* Borrowed ordinary chunks. Use private compiler copy/incremental syntax APIs;
 * never request a lazily materialized shared AST view or mutate these chunks.
 * Snapshot survives eviction/store destruction until its final release. */
const xls_syntax* xlsModuleSnapshotSyntax(const xls_module_snapshot* snapshot);
/* Synchronous worker-local binding;store borrowed from the owning worker.
 * Nested/default scopes shadow the outer bank and restore it on all exits. */
typedef struct xls_module_scope {struct xls_module_scope* previous;xls_module_store* store;} xls_module_scope;
void xlsModuleScopeBegin(xls_module_scope* scope,xls_module_store* store);
void xlsModuleScopeEnd(xls_module_scope* scope);
xls_module_store* xlsModuleScopeCurrent(void);
#endif
