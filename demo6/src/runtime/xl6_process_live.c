#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6ProcessDestroyV1(void* handle)
{
    XL6_ProcessLive* process=handle;
    if (!process) return;
    xerror* prior=xrtTakeError();
    int32 remaining=xrtRefRelease(&process->refs);
    if (remaining<0) abort();
    if (!remaining) {
        /* Preserve XRT's explicit-stop semantics: dropping references closes
         * pipes, not the child. Its native waiter still reaps the child. */
        xrtProcessDestroy(process->native);
        if (process->guard && !xrtMutexDestroy(process->guard)) abort();
        xrtFree(process);
    }
    xrtClearError(); xrtSetErrorTake(prior);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6ProcessHandlePolicyV1Get, xl6ProcessDestroyV1)
void* xl6ProcessSpawnV1(void* commandHandle)
{
    XL6_ProcessCommand* command=xl6ProcessCommandCheck(commandHandle);
    if (!command) return NULL;
    /* Allocate bookkeeping before spawn. Rollback never secretly terminates
     * an already-started child merely because a wrapper allocation failed. */
    XL6_ProcessLive* process=xrtCalloc(1,sizeof(*process));
    if (!process) return NULL;
    process->refs=1;
    process->guard=xrtMutexCreate();
    if (!process->guard) goto fail;
    const xprocessconfig* config=&command->config;
    if (config->Terminal) process->pipes=(1u<<XPROCESS_STDIN)|(1u<<XPROCESS_STDOUT);
    else {
        if (config->Stdin.Mode==XPROCESS_IO_PIPE) process->pipes|=1u<<XPROCESS_STDIN;
        if (config->Stdout.Mode==XPROCESS_IO_PIPE) process->pipes|=1u<<XPROCESS_STDOUT;
        if (config->Stderr.Mode==XPROCESS_IO_PIPE) process->pipes|=1u<<XPROCESS_STDERR;
    }
    process->native=xrtProcessSpawn(config);
    if (!process->native) goto fail;
    return process;
fail:
    xl6ProcessDestroyV1(process); return NULL;
}
uint64_t xl6ProcessIdV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p ? xrtProcessId(p->native) : 0; }
int64_t xl6ProcessStateV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p ? xrtProcessState(p->native) : 0; }
bool xl6ProcessRunningV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p && xrtProcessState(p->native)==XPROCESS_RUNNING; }
bool xl6ProcessCloseV1(void* handle,int64_t stream)
{
    XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); unsigned bit;
    if (!p || !xl6ProcessStreamBit(stream,&bit)) return false;
    xl6ProcessLiveLock(p);
    if (!(p->pipes&bit)) {
        xl6ProcessLiveUnlock(p); xrtSetErrorKind(XERR_STATE); return false;
    }
    /* Retire even on OS close failure; XRT detaches the endpoint first. */
    p->closed|=bit;
    bool ok=xrtProcessClose(p->native,(xprocessstream)stream);
    xl6ProcessLiveUnlock(p); return ok;
}
bool xl6ProcessInterruptV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p && xrtProcessInterrupt(p->native); }
bool xl6ProcessTerminateV1(void* handle)
{
    XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); if (!p) return false;
    xl6ProcessLiveLock(p); p->closed|=1u<<XPROCESS_STDIN; xl6ProcessLiveUnlock(p);
    return xrtProcessTerminate(p->native);
}
bool xl6ProcessKillV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p && xrtProcessKill(p->native); }
bool xl6ProcessKillTreeV1(void* handle)
{ XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); return p && xrtProcessKillTree(p->native); }
bool xl6ProcessWaitReadyV1(void* handle,int64_t timeoutMs)
{
    XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); if (!p) return false;
    xdeadline deadline=XRT_DEADLINE_NEVER;
    if (timeoutMs!=-1) {
        uint64 duration,now=xrtClock();
        if (!xl6ProcessDuration(timeoutMs,&duration)) return false;
        if (duration>=UINT64_MAX-now) { xrtSetErrorKind(XERR_RANGE); return false; }
        deadline=now+duration;
    }
    xwaitresult result=xrtProcessWaitUntil(p->native,deadline);
    /* Timeout is absence, not an invented exit code. A successful wait does
     * not join the native waiter: future callbacks may still be retiring. */
    if (result==XWAIT_OK) return true;
    if (result!=XWAIT_TIMEOUT && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    return false;
}
void* xl6ProcessLiveStatusV1(void* handle)
{
    XL6_ProcessLive* p=xl6ProcessLiveCheck(handle); if (!p) return NULL;
    xprocessstatus status;
    if (!xrtProcessStatus(p->native,&status)) return NULL;
    xprocessstatus* snapshot=xrtMalloc(sizeof(*snapshot));
    if (snapshot) *snapshot=status;
    return snapshot;
}
