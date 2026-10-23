#ifndef XIDE_DIAGNOSTIC_STORE_H
#define XIDE_DIAGNOSTIC_STORE_H
#include "services/language_check.h"
#include <stdint.h>

typedef struct xide_diagnostic_store xide_diagnostic_store;
typedef struct xide_diagnostic_query xide_diagnostic_query;
typedef struct xide_diagnostic_publication xide_diagnostic_publication;
typedef struct xide_diagnostic_limits {
 size_t records,retainedReportBytes,publishedReportBytes,queue;
} xide_diagnostic_limits;
typedef struct xide_diagnostic_snapshot {
 const xvalue *documents,*invalidated; /* borrowed until query destruction */
 size_t records,retainedReportBytes; uint64_t workerThreadId;
 const char* conflictPath; /* borrowed, failed merge observed different inputs */
 char error[256];
} xide_diagnostic_snapshot;
enum xide_diagnostic_state { XIDE_DIAGNOSTIC_PENDING,XIDE_DIAGNOSTIC_READY,XIDE_DIAGNOSTIC_FAILED,XIDE_DIAGNOSTIC_CANCELLED };
typedef struct xide_diagnostic_load { size_t queued,running; uint64_t submitted,completed,cancelled,rejected; } xide_diagnostic_load;

/* One XRT actor owns all records, merges and disk sweeps. No timer or worker
   per entry. Default limits:64records,32MiB retained/4MiB publication encoded
   reports,8queued jobs. Retained limits charge JSON metadata and native
   immutable contexts, conservatively per guard, not heap/RSS. Contributions
   to one file must have equal complete contexts; conflicting context updates
   fail atomically and preserve previous records/publication. Disjoint files
   may come from different contexts. */
xide_diagnostic_store* xideDiagnosticStoreCreate(const xide_diagnostic_limits* limits);
/* Opt-in immutable path dictionaries for exact client invalidation. Work is
   confined to the same actor. Existing callers retain their original format. */
xide_diagnostic_store* xideDiagnosticStoreCreateWithDependencies(const xide_diagnostic_limits* limits,int enabled);
/* Cancels/joins actor; shutdown-only, never on an interactive owner path.
   Completed query snapshots can outlive the store. */
void xideDiagnosticStoreDestroy(xide_diagnostic_store* store);
int xideDiagnosticStoreLoad(xide_diagnostic_store* store,xide_diagnostic_load* load);
/* ALWAYS consumes checkedResult, including submission failure. It must be an
   exclusively owned, verified snapshot result from the same producing check
   as guard. Captures one guard reference and a key copy, without cloning JSON,
   merging, validating files or waiting on the caller. Source errors are valid
   diagnostic results. Superseded/cancelled work must be version-gated by host. */
xide_diagnostic_query* xideDiagnosticStorePutTake(xide_diagnostic_store* store,const char* key,xide_language_input_guard* guard,xvalue* checkedResult);
xide_diagnostic_query* xideDiagnosticStoreRemove(xide_diagnostic_store* store,const char* key);
/* Open/close/change of an overlay invalidates roots whose actual files or
   observed resolution candidates match path, without touching other roots. */
xide_diagnostic_query* xideDiagnosticStoreInvalidate(xide_diagnostic_store* store,const char* path);
/* Reuses original proof validation. Host chooses bounded polling cadence. */
xide_diagnostic_query* xideDiagnosticStoreSweep(xide_diagnostic_store* store);
xide_diagnostic_query* xideDiagnosticStoreSnapshot(xide_diagnostic_store* store);
xide_diagnostic_query* xideDiagnosticStoreClear(xide_diagnostic_store* store);
enum xide_diagnostic_state xideDiagnosticQueryResult(xide_diagnostic_query* query,xide_diagnostic_snapshot* result);
/* Single query owner, once after READY. Moves all output ownership without
   cloning or waiting; later Result/Take calls fail. Publication and invalidated
   keys outlive query/store. Outputs are cleared on failure. */
int xideDiagnosticQueryTake(xide_diagnostic_query* query,xide_diagnostic_publication** publication,xvalue** invalidated);
void xideDiagnosticPublicationDestroy(xide_diagnostic_publication* publication);
size_t xideDiagnosticPublicationCount(const xide_diagnostic_publication* publication);
const xvalue* xideDiagnosticPublicationGroup(const xide_diagnostic_publication* publication,size_t index);
size_t xideDiagnosticPublicationFind(const xide_diagnostic_publication* publication,const char* path);
/* Prepared sorted path comparison and exact group equality, no I/O or copies. */
int xideDiagnosticPublicationCompare(const xide_diagnostic_publication* left,size_t leftIndex,const xide_diagnostic_publication* right,size_t rightIndex);
int xideDiagnosticPublicationEqual(const xide_diagnostic_publication* left,size_t leftIndex,const xide_diagnostic_publication* right,size_t rightIndex);
/* Worker-prepared dictionary chunks are bounded to256KiB. IDs reference exact
   path sets shared by contributing entries; no report clone or I/O here. */
size_t xideDiagnosticPublicationDependencyCount(const xide_diagnostic_publication* publication);
const char* xideDiagnosticPublicationDependencyId(const xide_diagnostic_publication* publication,size_t index);
const char* xideDiagnosticPublicationGroupDependency(const xide_diagnostic_publication* publication,size_t index);
size_t xideDiagnosticPublicationFindDependency(const xide_diagnostic_publication* publication,const char* id);
const xvalue* xideDiagnosticPublicationDependencyChunks(const xide_diagnostic_publication* publication,size_t index);
int xideDiagnosticQuerySetWake(xide_diagnostic_query* query,void (*wake)(void*),void* user);
void xideDiagnosticQueryCancel(xide_diagnostic_query* query);
void xideDiagnosticQueryDestroy(xide_diagnostic_query* query);
#endif
