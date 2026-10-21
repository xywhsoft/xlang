#ifndef XLANG_IDE_TASKS_H
#define XLANG_IDE_TASKS_H
#include "model/workspace.h"
#include "platform/process.h"
#include "services/toolchain.h"
#include <stdint.h>
typedef struct xide_tasks xide_tasks;
typedef struct xide_task xide_task;
enum xide_task_state { XIDE_TASK_QUEUED, XIDE_TASK_EXECUTING, XIDE_TASK_CANCELLING, XIDE_TASK_SUCCEEDED, XIDE_TASK_FAILED, XIDE_TASK_CANCELLED };
enum xide_task_phase { XIDE_TASK_PREPARE, XIDE_TASK_PROBE, XIDE_TASK_BUILD, XIDE_TASK_RUN, XIDE_TASK_DONE, XIDE_TASK_TEST };
enum xide_task_kind { XIDE_TASK_KIND_BUILD, XIDE_TASK_KIND_RUN, XIDE_TASK_KIND_TEST };
enum { XIDE_TASK_PENDING_LIMIT=32, XIDE_TASK_HISTORY_LIMIT=64 };
typedef struct xide_task_info {
    uint64_t id,revision,durationMs;
    enum xide_task_state state;
    enum xide_task_phase phase;
    enum xide_task_kind kind;
    xprocessstatus exit;
    int built,ran,tested,invalidated,inputReport,inputsVerified;
    char error[512];
} xide_task_info;
enum xide_diagnostic_kind { XIDE_DIAGNOSTIC_COMPILER, XIDE_DIAGNOSTIC_UNSAVED_INPUT };
typedef struct xide_diagnostic {
    char *path,*message;
    int line,column;
    /* An input-state notice has no compiler location (line/column are zero). */
    enum xide_diagnostic_kind kind;
} xide_diagnostic;
/* One owner/UI thread calls these APIs; workers own copied request data only.
   Queued requests execute serially. Duplicate outstanding requests coalesce. */
xide_tasks* xideTasksCreate(void);
void xideTasksDestroy(xide_tasks* tasks);
xide_toolchains* xideTasksToolchains(xide_tasks* tasks);
int xideTaskToolchain(xide_task* task,xide_toolchain_profile* out);
xide_task* xideTasksBuild(xide_tasks* tasks,const xide_project* project,const char* configurationId,const char* compiler,int run,xide_model_error* error);
/* Copy named dirty-document paths before starting any worker. Those matching
   discovered inputs invalidate this request; unrelated documents are untouched. */
xide_task* xideTasksBuildGuarded(xide_tasks* tasks,const xide_project* project,const char* configurationId,const char* compiler,int run,const xide_strings* pendingInputs,xide_model_error* error);
/* A real .c/.xl entry, without a project/workspace manifest. Outputs and logs
   live under the source directory's private .xlang-ide cache. */
xide_task* xideTasksBuildFileGuarded(xide_tasks* tasks,const char* path,const char* compiler,int run,const xide_strings* pendingInputs,xide_model_error* error);
xide_task* xideTasksTest(xide_tasks* tasks,const xide_project* project,const char* configurationId,xide_model_error* error);
void xideTasksPoll(xide_tasks* tasks);
int xideTasksBusy(const xide_tasks* tasks);
size_t xideTasksCount(const xide_tasks* tasks);
xide_task* xideTasksAt(xide_tasks* tasks,size_t index);
int xideTasksContains(const xide_tasks* tasks,const xide_task* task);
/* Owner-thread operation: keep the newest completed entries plus explicitly
   protected handles. Active/queued tasks and workers still exiting are kept.
   Removed handles become invalid; retained IDs never change. No files deleted. */
size_t xideTasksTrimHistory(xide_tasks* tasks,size_t keepCompleted,const xide_task* const* protectedTasks,size_t protectedCount);
void xideTaskInfo(xide_task* task,xide_task_info* info);
const char* xideTaskProjectId(const xide_task* task);
const char* xideTaskConfigurationId(const xide_task* task);
const char* xideTaskProjectName(const xide_task* task);
const char* xideTaskArtifact(const xide_task* task);
const char* xideTaskOutput(const xide_task* task);
const char* xideTaskBuildLog(const xide_task* task);
const char* xideTaskRunLog(const xide_task* task);
const char* xideTaskTestLog(const xide_task* task);
const char* xideTaskInputReceipt(const xide_task* task);
/* Owned path copy (free), available after completion; input count
   includes resource assets and actual compiler inputs/search candidates.
   Never exposes the worker's array. */
size_t xideTaskInputCount(xide_task* task);
char* xideTaskInputPath(xide_task* task,size_t index);
/* Cursor is an absolute byte offset. If output was dropped, dropped reports
   the gap; raw per-phase files retain the complete original process output. */
size_t xideTaskReadLog(xide_task* task,uint64_t* cursor,void* bytes,size_t capacity,uint64_t* dropped);
size_t xideTaskReadTerminal(xide_task* task,uint64_t* cursor,void* bytes,size_t capacity,uint64_t* dropped);
int xideTaskWrite(xide_task* task,const void* bytes,size_t size);
void xideTaskCloseInput(xide_task* task);
int xideTaskInputOpen(xide_task* task);
void xideTaskCancel(xide_task* task);
/* An edited in-memory input invalidates build/run requests even before save. */
void xideTasksInvalidateInput(xide_tasks* tasks,const char* path);
size_t xideTaskDiagnosticCount(xide_task* task);
/* Caller owns copied strings, released with xideDiagnosticClear. */
int xideTaskDiagnostic(xide_task* task,size_t index,xide_diagnostic* diagnostic);
void xideDiagnosticClear(xide_diagnostic* diagnostic);
#endif
