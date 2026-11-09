#ifndef XIDE_SYNC_PLAN_H
#define XIDE_SYNC_PLAN_H
#include "protocol/document_sync.h"
#include "services/language_client.h"

typedef struct xide_sync_input { const char* uri;xvalue* text;uint64_t identity,revision; } xide_sync_input;
typedef struct xide_watch_input {const char* uri;int type;uint64_t revision;} xide_watch_input;
enum {XIDE_SYNC_DOCUMENTS=0,XIDE_SYNC_WATCHED_FILES=1};
typedef struct xide_sync_operation {xvalue* uri;int close;uint64_t revision;} xide_sync_operation;
typedef struct xide_sync_result {
    uint64_t token,workerThreadId;
    int valid,kind;
    xls_sync* sync;
    xide_language_batch* batch;
    char digest[65];size_t count;int versions[64];
    size_t operationCount;xide_sync_operation operations[128];
} xide_sync_result;
typedef struct xide_sync_planner xide_sync_planner;
/* One owner, one XRT worker; wake never touches UI and outlives Destroy.
 * Text values must be immutable strings for the lifetime of retained refs. */
xide_sync_planner* xideSyncPlannerCreate(void (*wake)(void*),void* user);
/* Clones only published sync metadata/entry refs and retains input text values,
 * without scanning/copying their bodies. Inputs/base remain caller-owned.
 * Returns 1 accepted, 0 pending slot busy (retry), -1 invalid/allocation failure. */
int xideSyncPlannerSubmit(xide_sync_planner* planner,uint64_t token,const xls_sync* base,
                         const xide_sync_input* inputs,size_t count);
/* Shares the pending/result/retirement slots and worker. At most128 valid UTF8
 * URIs and1MiB combined URI bytes. Owner copies bounded path metadata only;
 * worker builds one watched-files notification. Result has no sync snapshot;
 * operations preserve URI/revision for exact send-time fence acknowledgement. */
int xideSyncPlannerSubmitWatches(xide_sync_planner*,uint64_t token,const xide_watch_input*,size_t count);
void xideSyncPlannerCancel(xide_sync_planner* planner);
xide_sync_result* xideSyncPlannerTake(xide_sync_planner* planner);
/* Retires a taken result, including an old sync swapped into it, on the worker.
 * Success TAKES it; a full retire slot leaves ownership with the caller. */
int xideSyncPlannerRetire(xide_sync_planner* planner,xide_sync_result* result);
/* Heavy cleanup, for the worker/final shutdown only. */
void xideSyncResultRelease(xide_sync_result* result);
void xideSyncPlannerDestroy(xide_sync_planner* planner);
#endif
