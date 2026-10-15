#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

static xvalue* box(xfuture* future)
{ xvalue* value=xrtValueFuture(future);assert(value);return value; }
static void observed(void* data) { *(bool*)data=true; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void methods(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* group=xl6SignalTaskGroupCreateV1();assert(group);
    assert(xl6_xrt_task_group_wrap(group)==group && xl6_xrt_task_group_count(group)==0);
    xvalue* wrong=xrtValueInt(12);assert(wrong && !xl6_xrt_task_group_add(group,wrong) && !xl6_xrt_task_group_bind_parent(group,wrong));
    xrtValueRelease(wrong);assert(!xrtGetError());
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise && source);
    xvalue* value=box(source);assert(xl6_xrt_task_group_add(group,value) && xl6_xrt_task_group_count(group)==1);
    assert(!xl6_xrt_task_group_wait_timeout(group,0) && !xrtGetError());
    assert(!xl6_xrt_task_group_add(group,value) && !xrtGetError());
    assert(xrtPromiseResolve(promise,NULL) && xl6_xrt_task_group_wait(group));
    assert(xl6_xrt_task_group_join(group) && xl6_xrt_task_group_join_timeout(group,0));
    int64_t until=(int64_t)(xrtClock()/1000)+1000;
    assert(xl6_xrt_task_group_join_until(group,until) && xl6_xrt_task_group_wait_until(group,until));
    assert(!xl6_xrt_task_group_count(group) && xl6_xrt_task_group_reap(group)==1 && xl6_xrt_task_group_reap(group)==0);
    xl6SignalTaskGroupDestroyV1(group);xrtValueRelease(value);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    puts("TaskGroup resident methods: dynamic admission, close, count/reap, relative/absolute wait and join balanced");
}
static void binding(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* group=xl6SignalTaskGroupCreateV1();assert(group);
    xfuture *first=NULL,*second=NULL,*source=NULL;
    xpromise *one=xrtPromiseCreate(&first,NULL),*two=xrtPromiseCreate(&second,NULL),*task=xrtPromiseCreate(&source,NULL);
    assert(one && two && task);xvalue *a=box(first),*b=box(second),*value=box(source);
    assert(xl6_xrt_task_group_bind_parent(group,a) && xrtFutureCancel(second));
    assert(!xl6_xrt_task_group_bind_parent(group,b) && !xrtGetError());
    assert(xl6_xrt_task_group_add(group,value)); /* Rejected cancelled parent had no effect. */
    assert(xrtFutureCancel(first));xcancel* token=xrtFutureCancelToken(source);assert(token && xrtCancelRequested(token));xrtCancelDestroy(token);
    assert(xrtPromiseCancel(task) && xl6_xrt_task_group_join(group));
    xl6SignalTaskGroupDestroyV1(group);xrtValueRelease(a);xrtValueRelease(b);xrtValueRelease(value);
    xrtPromiseDestroy(one);xrtPromiseDestroy(two);xrtPromiseDestroy(task);xrtFutureDestroy(first);xrtFutureDestroy(second);xrtFutureDestroy(source);
    balanced(&before);puts("TaskGroup parent binding: rejected cancelled second parent cannot cancel; accepted parent propagates cancellation");
}
static void consumers(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* parent=xl6SignalTaskGroupCreateV1();assert(parent);
    void* group=xl6_xrt_task_group_child(parent);assert(group);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);xvalue* value=box(source);
    assert(xl6_xrt_task_group_add(group,value));
    xfuture *one=xl6_xrt_task_group_join_async(group),*two=xl6_xrt_task_group_join_async(group);assert(one && two && one!=two);
    assert(xrtFutureCancel(one) && !xrtFutureDone(two));
    xcancel* token=xrtFutureCancelToken(source);assert(token && !xrtCancelRequested(token));xrtCancelDestroy(token);
    assert(xrtPromiseResolve(promise,NULL) && xrtFutureWaitFor(two,UINT64_C(5000000))==XWAIT_OK);
    assert(xrtFutureState(one)==XFUTURE_CANCELLED && xrtFutureState(two)==XFUTURE_RESOLVED && !xrtFutureValue(two));
    assert(xl6_xrt_task_group_join(parent) && xl6_xrt_task_group_reap(parent)==1 && xl6_xrt_task_group_reap(group)==1);
    xl6SignalTaskGroupDestroyV1(group);xl6SignalTaskGroupDestroyV1(parent);xrtValueRelease(value);
    xrtFutureDestroy(one);xrtFutureDestroy(two);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    puts("TaskGroup async join: real child completion, null payload, independent consumers and cancellation balanced");
}
static void synchronousObserver(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* group=xl6SignalTaskGroupCreateV1();assert(group);
    xfuture* shared=xl6TaskGroupFuture(group);assert(shared);bool called=false;xfuturewatch watch={0};
    assert(xrtFutureWatchInit(&watch,observed,NULL,&called));
    assert(xrtFutureWatchAdd(shared,&watch)==XFUTURE_WATCH_PENDING);
    xfuture* output=xl6_xrt_task_group_join_async(group);assert(output && called);
    assert(xrtFutureState(output)==XFUTURE_RESOLVED);xrtFutureWatchRemove(shared,&watch);
    xrtFutureDestroy(output);xrtFutureDestroy(shared);xl6SignalTaskGroupDestroyV1(group);balanced(&before);
    puts("TaskGroup joinAsync can synchronously notify existing native observers; conservative execution effect required");
}
static void prefixes(void)
{
    for(unsigned operation=0;operation<5;++operation){unsigned point;
        for(point=0;point<128;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* group=operation?xl6SignalTaskGroupCreateV1():NULL;
            xfuture* source=NULL;xpromise* promise=operation>=2?xrtPromiseCreate(&source,NULL):NULL;
            xvalue* value=source?box(source):NULL;assert(!operation || group);assert(operation<2 || (promise && value));
            if(operation==4)assert(xl6_xrt_task_group_add(group,value));
            xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope) && xrtMemDebugFailAfter(point));
            void* output=NULL;bool accepted=false;
            if(operation==0)output=xl6SignalTaskGroupCreateV1();
            else if(operation==1)output=xl6_xrt_task_group_child(group);
            else if(operation==2)accepted=xl6_xrt_task_group_add(group,value);
            else if(operation==3)accepted=xl6_xrt_task_group_bind_parent(group,value);
            else output=xl6_xrt_task_group_join_async(group);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
            assert(outcome.ok==!hit && (hit?(!output && !accepted):(output || accepted)));xrtErrorFree(outcome.error);xrtClearError();
            if(hit){
                if(operation==0)output=xl6SignalTaskGroupCreateV1();
                else if(operation==1)output=xl6_xrt_task_group_child(group);
                else if(operation==2)accepted=xl6_xrt_task_group_add(group,value);
                else if(operation==3)accepted=xl6_xrt_task_group_bind_parent(group,value);
                else output=xl6_xrt_task_group_join_async(group);
                assert(output || accepted);
            }
            if(promise)assert(xrtPromiseResolve(promise,NULL));
            if(operation==4){assert(xrtFutureWaitFor(output,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(output);}
            else if(output)xl6SignalTaskGroupDestroyV1(output);
            xl6SignalTaskGroupDestroyV1(group);xrtValueRelease(value);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
            if(!hit)break;
        }
        assert(point>0 && point<128);printf("TaskGroup operation=%u prefixes=%u first-no-hit=%u balanced\n",operation,point+1,point);
    }
}
int main(void)
{
    assert(xrtMemDebugEnable(true));setvbuf(stdout,NULL,_IONBF,0);xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    methods();binding();consumers();synchronousObserver();prefixes();assert(!xrtGetError());return 0;
}
