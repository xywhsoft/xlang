#ifndef XLANG_ANALYSIS_DEPENDENCY_GRAPH_H
#define XLANG_ANALYSIS_DEPENDENCY_GRAPH_H
#include <stddef.h>
#include <stdint.h>

typedef struct xls_dependency_graph xls_dependency_graph;
typedef struct xls_dependency_key { const void* data;size_t size; } xls_dependency_key;
typedef int (*xls_dependency_compare)(xls_dependency_key left,xls_dependency_key right,void* user);
typedef struct xls_dependency_limits { size_t roots,paths,edges,keyBytes,bytes; } xls_dependency_limits;
typedef struct xls_dependency_update { size_t root;const xls_dependency_key* keys;size_t count; } xls_dependency_update;
typedef struct xls_dependency_stats {
    uint64_t generation,updates,lookups,comparisons;
    size_t roots,paths,edges,bytes,peakBytes;
} xls_dependency_stats;
enum xls_dependency_error { XLS_DEPENDENCY_OK,XLS_DEPENDENCY_ARGUMENT,XLS_DEPENDENCY_LIMIT,XLS_DEPENDENCY_MEMORY,XLS_DEPENDENCY_CANCELLED };

/* One owner thread; no transport, path policy, filesystem or compiler AST.
 * Caller prepares canonical keys and a deterministic total-order comparator,
 * valid for the graph's lifetime. Comparator/cancel callbacks must not reenter
 * this graph. Keys may contain NUL and are copied; equality is the comparator's
 * equality, not a hash. Implementation uses the existing intrusive XRT AVL.
 * Default:64 root slots,163840 paths/edges,256KiB per key,64MiB charged storage.
 * Bytes include graph,nodes,keys,owner arrays and peak staging,not XRT metadata
 * or RSS. Root slots identify the caller's retained analysis owners/contexts. */
xls_dependency_graph* xlsDependencyGraphCreate(const xls_dependency_limits* limits,
    xls_dependency_compare compare,void* user,enum xls_dependency_error* error);
void xlsDependencyGraphDestroy(xls_dependency_graph* graph);
/* Replace multiple distinct owners atomically;count0 removes all that owner's
 * edges. Duplicate equivalent keys are accepted. Failed/cancelled staging
 * preserves owners,keys,edges,generation and existing lookup results. Only work
 * counters/peak charge may change. Successful commit allocates nothing. Old
 * keys shared by other owners retain their identity/storage. Inputs borrowed
 * only for this call; no result/guard references are retained by the graph. */
int xlsDependencyGraphUpdate(xls_dependency_graph* graph,const xls_dependency_update* updates,size_t count,
    int (*cancelled)(void*),void* user,enum xls_dependency_error* error);
/* Clears owners on invalid input. Success with owners0 is a proven miss.
 * Exact O(log(paths)) reverse lookup,without allocation or I/O. Bit63 valid. */
int xlsDependencyGraphFind(xls_dependency_graph* graph,xls_dependency_key key,uint64_t* owners);
void xlsDependencyGraphStats(const xls_dependency_graph* graph,xls_dependency_stats* stats);
#endif
