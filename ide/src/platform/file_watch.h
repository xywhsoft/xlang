#ifndef XLANG_IDE_FILE_WATCH_H
#define XLANG_IDE_FILE_WATCH_H
#include "model/workspace.h"
#include <stdint.h>
typedef struct xide_file_watch xide_file_watch;
typedef struct xide_watch_target { uint64_t token; const char* path; size_t limit; } xide_watch_target;
enum { XIDE_WATCH_READY=1,XIDE_WATCH_MISSING,XIDE_WATCH_UNREADABLE,XIDE_WATCH_TOO_LARGE,XIDE_WATCH_DIRECTORY,XIDE_WATCH_UNSTABLE };
typedef struct xide_watch_event {
    uint64_t epoch,token,sequence,size,hash,writeTime,fileId;
    unsigned volume,error; int state;
    char* path; unsigned char* bytes; size_t length;
} xide_watch_event;
typedef struct xide_watch_stats {
    uint64_t epoch,readyEpoch,notifications,sweeps,dropped;
    size_t targets,directories,queued,queuedBytes;
} xide_watch_stats;
xide_file_watch* xideFileWatchCreate(void (*wake)(void*),void* user,xide_model_error* error);
/* Copies every target. limit=0 observes metadata; body reads are capped at 16 MiB. */
uint64_t xideFileWatchSetTargets(xide_file_watch* watch,const xide_watch_target* targets,size_t count,xide_model_error* error);
void xideFileWatchRescan(xide_file_watch* watch);
int xideFileWatchTake(xide_file_watch* watch,xide_watch_event* event);
void xideFileWatchEventClear(xide_watch_event* event);
void xideFileWatchStats(xide_file_watch* watch,xide_watch_stats* stats);
void xideFileWatchDestroy(xide_file_watch* watch);
#endif
