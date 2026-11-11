#ifndef XLANG_IDE_FILE_WATCH_H
#define XLANG_IDE_FILE_WATCH_H
#include "model/workspace.h"
#include <stdint.h>
typedef struct xide_file_watch xide_file_watch;
typedef struct xide_watch_target { uint64_t token; const char* path; size_t limit; } xide_watch_target;
enum { XIDE_WATCH_READY=1,XIDE_WATCH_MISSING,XIDE_WATCH_UNREADABLE,XIDE_WATCH_TOO_LARGE,XIDE_WATCH_DIRECTORY,XIDE_WATCH_UNSTABLE,
       XIDE_WATCH_SOURCE_CHANGE,XIDE_WATCH_SOURCE_RESCAN };
typedef struct xide_watch_event {
    uint64_t epoch,token,sequence,size,hash,writeTime,fileId;
    uint64_t observedAt; /* Monotonic read-start time, not filesystem time. */
    unsigned volume,error; int state,changeType;
    char* path; unsigned char* bytes; size_t length;
} xide_watch_event;
typedef struct xide_watch_stats {
    uint64_t epoch,readyEpoch,notifications,sweeps,dropped;
    size_t targets,directories,queued,queuedBytes;
} xide_watch_stats;
xide_file_watch* xideFileWatchCreate(void (*wake)(void*),void* user,xide_model_error* error);
/* Copies every target. limit=0 observes metadata; body reads are capped at 16 MiB. */
uint64_t xideFileWatchSetTargets(xide_file_watch* watch,const xide_watch_target* targets,size_t count,xide_model_error* error);
/* Opt-in recursive source-name notifications below subscribed parent directories.
 * Known targets keep their stable snapshots. Other .xl names use token=0,
 * SOURCE_CHANGE/type1..3/path and no body. Directory-name changes or uncertain
 * notification coverage use SOURCE_RESCAN/token0/pathNULL. These are invalidation
 * hints,not source snapshots or additions to project membership. No tree scan. */
uint64_t xideFileWatchSetTargetsWithSources(xide_file_watch*,const xide_watch_target*,size_t count,int enabled,xide_model_error*);
/* Owner-side asynchronous source-reader lease. Suspend publishes an empty
 * epoch without walking/freeing queued bodies or waiting for worker I/O.
 * Suspended(token) becomes true ONLY after the worker closes prior readers and
 * native directory subscriptions and retires all queued snapshots. No Take or
 * subscription replacement is allowed meanwhile. One lease; no nesting.
 * Resume copies a new complete subscription set, requires the exact acknowledged
 * token, and forces fresh snapshots plus a source coverage invalidation when
 * enabled. A failed Resume preserves the held/empty state. Caller serializes
 * setters/lease calls; queries and worker wake follow the existing contracts.
 * This leases this monitor only, not arbitrary app/LSP/toolchain readers. */
uint64_t xideFileWatchSuspend(xide_file_watch*,xide_model_error*);
int xideFileWatchSuspended(xide_file_watch*,uint64_t token);
uint64_t xideFileWatchResume(xide_file_watch*,uint64_t token,const xide_watch_target*,size_t count,int enabled,xide_model_error*);
void xideFileWatchRescan(xide_file_watch* watch);
int xideFileWatchTake(xide_file_watch* watch,xide_watch_event* event);
void xideFileWatchEventClear(xide_watch_event* event);
void xideFileWatchStats(xide_file_watch* watch,xide_watch_stats* stats);
void xideFileWatchDestroy(xide_file_watch* watch);
#endif
