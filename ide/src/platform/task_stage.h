#ifndef XLANG_IDE_TASK_STAGE_H
#define XLANG_IDE_TASK_STAGE_H
#include "model/workspace.h"
#include <stdint.h>

typedef struct xide_task_stage xide_task_stage;
/* Worker-only creation of a fresh GUID directory beneath .xlang-ide-build.
   A retained task owns this lease until its history entry is destroyed.
   Unmarked legacy directories are never adopted. Release never deletes files. */
xide_task_stage* xideTaskStageBegin(const char* directory,xide_model_error* error);
/* Read-only shared lease for a file/directory within a marked stage. NULL
   without an error means ordinary/unmanaged storage, which cleanup ignores. */
xide_task_stage* xideTaskStageProtectPath(const char* path,xide_model_error* error);
const char* xideTaskStageDirectory(const xide_task_stage* stage);
/* Call only after all task file/process I/O has finished. A failed completion
   record leaves the stage protected and must not invalidate a built output. */
int xideTaskStageComplete(xide_task_stage* stage,xide_model_error* error);
void xideTaskStageRelease(xide_task_stage* stage);
typedef struct xide_task_stage_stats {
 size_t scanned,completed,retained,skipped,deletedStages,deletedFiles;
 uint64_t releasedBytes;
} xide_task_stage_stats;
/* Background-only. Caller supplies its protected input/file/directory paths.
   The default caller keeps 64 completed stages and a seven-day grace period.
   Enumeration and ownership validation precede deletion. Unknown children,
   nonordinary files, links and leases are preserved. Cancellation can leave
   a partially cleaned old stage; removed bytes are counted, never restored. */
int xideTaskStagePrune(const char* cacheRoot,size_t keepCompleted,uint64_t nowTicks,
 const xide_strings* protectedPaths,int (*cancelled)(void*),void* user,
 xide_task_stage_stats* stats,xide_model_error* error);
#define XIDE_TASK_STAGE_GRACE_TICKS (UINT64_C(7)*24*60*60*10000000)
#endif
