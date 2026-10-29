/* Actual XRT process/Future implementation and resident bool payload policy.
 * A private native observer joins the waiter tail before checking the heap;
 * public Process.wait/await deliberately do not promise a waiter-thread join. */
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
#if !defined(XL6_PROCESS_SOURCE_CONTRACTS)
#include "tools/xrt_single_header_impl.c"
#endif
#include "src/runtime/xl6_native_handle.c"
#include "src/runtime/xl6_process_command.c"
#include "src/runtime/xl6_process_live.c"
#include "src/runtime/xl6_process_future.c"
#endif
#include <assert.h>
#include <stdio.h>

static xstrview view(const char* text) { return (xstrview){text,strlen(text)}; }
static XL6_ProcessLive* spawn(const char* child, bool terminal)
{
    xvalue* args=xrtValueArray(); assert(args);
    assert(xrtValueArrayAppendNew(args,xrtValueString(view(terminal ? "exit" : "echo"))));
    if(terminal) assert(xrtValueArrayAppendNew(args,xrtValueString(view("7"))));
    XL6_ProcessCommand* command=xl6ProcessCommandDirectV1(view(child),args,-1,-1);
    xrtValueRelease(args); assert(command);
    xl6ProcessCommandStdioV1(command,1,XPROCESS_IO_NULL);
    xl6ProcessCommandStdioV1(command,2,XPROCESS_IO_NULL);
    XL6_ProcessLive* process=xl6ProcessSpawnV1(command); assert(process);
    xl6ProcessCommandDestroyV1(command);
    return process;
}
static void join(xprocess* native)
{
    assert(xrtProcessWait(native)==XWAIT_OK);
    assert(xrtThreadWait(native->Waiter)==XWAIT_OK);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount);
    assert(after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void ready(xfuture* future)
{
    assert(xrtFutureState(future)==XFUTURE_RESOLVED);
    const void* value=xrtFutureValue(future);
    assert(value && *(const bool*)value);
    const xfuturepayloadownershipv1* allowed[]={xl6PayloadPolicyGet(XL6_FUTURE_PAYLOAD_VALUE)};
    assert(xrtFutureOwnershipAdapterV1(xrtFutureOwnership(future),allowed,1));
}
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
static void* mapperFrame(void)
{
    static const XL6_CaptureLayoutV2 empty={sizeof(empty),1,0,NULL};
    void* frame=xl6FutureCallbackRuntimeV4Get()->Allocate(NULL,1,processReady,&empty);
    assert(frame);return frame;
}
static unsigned mapperSweep(void)
{
    for(size_t point=0;point<8;++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
        void* frame=mapperFrame();
        xprocessstatus status={0}; xfutureresult input={0};
        input.State=XFUTURE_RESOLVED; input.Value=&status;
        xrtSetErrorKind(XERR_IO); const xerror* prior=xrtGetError();
        assert(xrtMemDebugFailAfter(point)); processReady(&input,promise,NULL,frame);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(xrtGetError()==prior);
        if(hit) assert(xrtFutureState(future)==XFUTURE_FAILED && xrtErrorKind(xrtFutureError(future))==XERR_MEMORY);
        else {
            ready(future);
            /* Duplicate publication must not consume/leak the new bool box. */
            processReady(&input,promise,NULL,frame); assert(xrtGetError()==prior); ready(future);
        }
        xl6FutureCallbackRuntimeV1Get()->Discard(frame);
        xrtPromiseDestroy(promise); xrtFutureDestroy(future); xrtClearError(); balanced(&before);
        if(!hit) return (unsigned)point;
    }
    abort();
}
#endif
static unsigned factorySweep(const char* child,bool terminal)
{
    for(size_t point=0;point<32;++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        XL6_ProcessLive* process=spawn(child,terminal);
        xprocess* observer=xrtProcessRef(process->native); assert(observer);
        if(terminal) join(observer);
        assert(xrtMemDebugFailAfter(point));
        xfuture* result=xl6ProcessWaitReadyAsyncV1(process);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) {
            if(result) assert(terminal && xrtFutureState(result)==XFUTURE_FAILED && xrtErrorKind(xrtFutureError(result))==XERR_MEMORY);
            else assert(xrtErrorKind(xrtGetError())==XERR_MEMORY);
        } else assert(result && !xrtGetError());
        xrtClearError();
        /* Release the language handle before terminal publication. The mapping
         * has no borrowed process pointer; the real observer owns our EOF seam. */
        xl6ProcessDestroyV1(process);
        if(!terminal) assert(xrtProcessClose(observer,XPROCESS_STDIN));
        join(observer);
        if(result && !hit) ready(result);
        xrtFutureDestroy(result); xrtProcessDestroy(observer); xrtClearError(); balanced(&before);
        if(!hit) return (unsigned)point;
    }
    abort();
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    assert(!xl6ProcessWaitReadyAsyncV1(NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
#if !defined(XL6_PROCESS_PRODUCT_OBSERVER)
    unsigned mapper=mapperSweep();
    for(unsigned guard=0;guard<2;++guard) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
        void* frame=mapperFrame();
        xprocessstatus status={0}; xfutureresult input={0};
        input.State=guard ? XFUTURE_RESOLVED : XFUTURE_FAILED; input.Value=guard ? NULL : &status;
        processReady(&input,promise,NULL,frame); assert(xrtFutureState(future)==XFUTURE_CLOSED && !xrtGetError());
        xl6FutureCallbackRuntimeV1Get()->Discard(frame);
        xrtPromiseDestroy(promise); xrtFutureDestroy(future); balanced(&before);
    }
#endif
    unsigned pending=factorySweep(argv[1],false),terminal=factorySweep(argv[1],true);
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    XL6_ProcessLive* process=spawn(argv[1],false);
    xprocess* observer=xrtProcessRef(process->native); assert(observer);
    xfuture* shared=xrtProcessWaitAsync(observer); assert(shared);
    xfuture* first=xl6ProcessWaitReadyAsyncV1(process),*second=xl6ProcessWaitReadyAsyncV1(process); assert(first && second);
    /* A plain native Then records cooperative cancellation, not an invented
     * terminal. Language waitAsync detaches its own watch and unwinds promptly;
     * that stronger CFG contract is checked by stdlib_process_future_cancel. */
    assert(xrtFutureCancel(first) && xrtFutureState(first)==XFUTURE_PENDING);
    xcancel* firstToken=xrtFutureCancelToken(first); assert(firstToken && xrtCancelRequested(firstToken)); xrtCancelDestroy(firstToken);
    assert(xrtFutureState(shared)==XFUTURE_PENDING && xrtFutureState(second)==XFUTURE_PENDING);
    xcancel* sourceToken=xrtFutureCancelToken(shared); assert(sourceToken && !xrtCancelRequested(sourceToken)); xrtCancelDestroy(sourceToken);
    assert(xrtProcessState(observer)==XPROCESS_RUNNING);
    xl6ProcessDestroyV1(process);
    assert(xrtProcessClose(observer,XPROCESS_STDIN)); join(observer);
    ready(second);
    assert(xrtFutureState(first)==XFUTURE_CANCELLED && !xrtFutureValue(first));
    assert(xrtFutureState(shared)==XFUTURE_RESOLVED);
    xprocessstatus* status=xrtFutureValue(shared); assert(status && status->Kind==XPROCESS_EXIT_CODE && status->Code==0);
    xrtProcessDestroy(observer); /* Both independently owned snapshots remain. */
    ready(second); assert(status->Code==0);
    xrtFutureDestroy(first); xrtFutureDestroy(second); xrtFutureDestroy(shared); balanced(&before);
#if defined(XL6_PROCESS_PRODUCT_OBSERVER)
    printf("Process Future product: shared source untouched, parent-drop readiness, exact bool/resident ownership; pending/terminal OOM %u/%u failures + 2 first no-hit, waiter retired, balanced\n",pending,terminal);
#else
    printf("Process Future: shared source untouched, parent-drop readiness, exact bool/resident ownership, 3 guards, duplicate/ambient; mapper/pending/terminal OOM %u/%u/%u failures + 3 first no-hit, waiter retired, balanced\n",mapper,pending,terminal);
#endif
    return 0;
}
