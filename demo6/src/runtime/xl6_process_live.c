#include "src/runtime/xl6_process_live.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_scope.h"

void xl6ProcessDestroyV1(void* handle)
{
    XL6_ProcessLive* process=handle;
    if (!process) return;
    xerror* prior=xrtTakeError();
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    int32 remaining=xrtRefRelease(&process->refs);
    if (remaining<0) abort();
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if (!remaining) {
        /* Preserve XRT's explicit-stop semantics: dropping references closes
         * pipes, not the child. Its native waiter still reaps the child. */
        xrtProcessDestroy(process->native);
        if (process->guard && !xrtMutexDestroy(process->guard)) abort();
        xrtFree(process);
    }
    xrtClearError(); xrtSetErrorTake(prior);
}
static bool processCount(const void* raw,size_t* count)
{
    const XL6_ProcessLive* process=raw;int32 refs;
    if(!process || !count || process->cleared || !process->native)return false;
    refs=process->refs; /* Every owning transition participates in Freeze. */
    if(refs<=0)return false;
    *count=(size_t)refs;return true;
}
static bool processEdges(const void* raw,xrtownershipvisitor visit,void* context)
{
    const XL6_ProcessLive* process=raw;size_t count;
    return visit && processCount(raw,&count) && visit(xrtProcessOwnership(process->native),context);
}
static const xrtownershipops processOps={processCount,processEdges};
xrtownershipref xl6ProcessLiveOwnershipV1(const void* raw)
{ return (xrtownershipref){raw,raw?&processOps:NULL}; }
static bool processHold(const void* raw)
{
    XL6_ProcessLive* process=(XL6_ProcessLive*)raw;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    bool held=!process->cleared && xrtRefRetain(&process->refs)>0;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return held;
}
static void processDrop(const void* raw) { xl6ProcessDestroyV1((void*)raw); }
static bool processClaim(const void* raw,const void* token)
{
    XL6_ProcessLive* process=(XL6_ProcessLive*)raw;size_t count;
    if(!token || !processCount(raw,&count) || (process->claim && process->claim!=token))return false;
    process->claim=token;return true;
}
static void processRestore(const void* raw,const void* token)
{
    XL6_ProcessLive* process=(XL6_ProcessLive*)raw;
    if(!token || process->claim!=token || process->cleared)abort();
    process->claim=NULL;
}
static void processClear(const void* raw,const void* token)
{
    XL6_ProcessLive* process=(XL6_ProcessLive*)raw;
    if(!token || process->claim!=token || process->cleared)abort();
    process->cleared=true; /* Actual native user endpoint is retired at Finish. */
}
static bool processFinish(const void* raw,const void* token)
{
    XL6_ProcessLive* process=(XL6_ProcessLive*)raw;xrtownershipscope mutation={0};xprocess* native;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!token || process->claim!=token || !process->cleared)abort();
    native=process->native;process->native=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* A collector pin on native is not a user endpoint. Last-user close must
     * still deliver EOF, outside Freeze, even if its waiter remains active. */
    xrtProcessDestroy(native);return true;
}
const xrtownershipadapterv1* xl6ProcessLiveOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),processHold,processDrop,
        processClaim,processRestore,NULL,processClear,processFinish};size_t count;
    return ref.Ops==&processOps && processCount(ref.Data,&count)?&adapter:NULL;
}
static bool processTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;
    return visit && xrtValueGetHandle(value,&raw,NULL,NULL) && raw && visit(xl6ProcessLiveOwnershipV1(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6ProcessHandlePolicyV1Get, xl6ProcessDestroyV1, processTrace)
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
