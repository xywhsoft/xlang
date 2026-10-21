#ifndef XLANG_IDE_C_COMPLETION_JOB_H
#define XLANG_IDE_C_COMPLETION_JOB_H
#include "services/c_language.h"
#include "services/c_bridge.h"
#include "platform/file_rename.h"
typedef struct xide_c_completion_job xide_c_completion_job;
enum xide_c_job_state { XIDE_C_JOB_RUNNING,XIDE_C_JOB_READY,XIDE_C_JOB_FAILED,XIDE_C_JOB_INVALIDATED,XIDE_C_JOB_CANCELLED };
typedef struct xide_c_job_spec {
 const char *server,*directory; const xvalue* commands;
 const xide_c_document* documents; size_t count; uint64_t documentId; int offset;
 const char* const* extraFiles; size_t extraCount;
 void (*wake)(void*); void* user;
 const xide_c_bridge_spec* bridge;
} xide_c_job_spec;
/* Deep copies inputs. Worker owns the immutable C session and guards explicit
   compilation files, canonical -include/-imacros pair operands, and
   extraFiles/server by content and physical identity. Negotiated experimental
   compiler observations additionally guard actual physical reads and probes.
   This is not a claim of a complete C preprocessing dependency graph. */
xide_c_completion_job* xideCCompletionJobStart(const xide_c_job_spec* spec,xide_model_error* error);
enum xide_c_job_state xideCCompletionJobState(xide_c_completion_job* job,char* message,size_t capacity);
/* Owned copy only while READY and not cancelled. */
xvalue* xideCCompletionJobResult(xide_c_completion_job* job);
void xideCCompletionJobCancel(xide_c_completion_job* job);
int xideCCompletionJobDone(xide_c_completion_job* job);
void xideCCompletionJobDestroy(xide_c_completion_job* job);
/* Persistent analysis mode; standard LSP events retain their real wire JSON.
   Local tokens and input guards are separate consumer metadata. */
xide_c_completion_job* xideCAnalysisJobStart(const xide_c_job_spec* spec,xide_model_error* error);
int xideCAnalysisJobSupports(xide_c_completion_job* job,enum xide_c_query query);
uint64_t xideCAnalysisJobRequest(xide_c_completion_job* job,enum xide_c_query query,uint64_t documentId,int offset);
void xideCAnalysisJobCancelRequest(xide_c_completion_job* job,uint64_t token);
int xideCAnalysisJobTake(xide_c_completion_job* job,uint64_t token,xide_c_event* event);
/* Copy latest version-validated diagnostics if sequence changed. Initialize
   events to zero; caller clears each returned event. Partial batches are valid. */
int xideCAnalysisJobDiagnostics(xide_c_completion_job* job,uint64_t* sequence,xide_c_event* events,size_t capacity,size_t* count);
/* Copy explicit disk baselines or negotiated compiler-read file snapshots.
   No caller-side lazy enrollment or physical I/O. Available only while READY. */
int xideCAnalysisJobGuard(xide_c_completion_job* job,const char* path,xide_file_snapshot* snapshot);
#endif
