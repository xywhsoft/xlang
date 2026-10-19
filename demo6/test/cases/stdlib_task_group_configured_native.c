#include "src/api/xl_task_group_bridge.h" /* Must be independently includable. */
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static void balanced(const xmemdebugsnapshot* before)
{
    assert(!xrtGetError());xmemdebugsnapshot after={0};xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static XL6_TaskGroupConfigV1 options(uint64_t limit,uint32_t policy)
{return (XL6_TaskGroupConfigV1){sizeof(XL6_TaskGroupConfigV1),NULL,limit,policy};}
typedef struct Starter {xl6_task_group* group;xfuture* source;unsigned calls;bool fail;} Starter;
static xfuture* start(void* raw)
{
    Starter* state=raw;++state->calls;XL6_TaskGroupStatsV1 stats={0};
    assert(xl6TaskGroupGetV1(state->group,&stats)&&stats.active>=1); /* Reentrant, no inherited lock. */
    return state->fail?NULL:xrtFutureRef(state->source);
}
static void invalid(void)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);
    XL6_TaskGroupConfigV1 config=options(0,UINT32_C(0x80000000));
    assert(!xl6TaskGroupCreateConfiguredV1(&config)&&xrtGetError());xrtClearError();
    config=options(0,0);--config.size;assert(!xl6TaskGroupCreateConfiguredV1(&config));xrtClearError();
    xl6_task_group* group=xl6TaskGroupCreate();assert(group);
    XL6_TaskGroupStatsV1 stats,saved;memset(&stats,0xA5,sizeof(stats));memcpy(&saved,&stats,sizeof(saved));
    assert(!xl6TaskGroupGetV1(NULL,&stats)&&!memcmp(&stats,&saved,sizeof(stats)));xrtClearError();
    assert(!xl6TaskGroupGetV1(group,(XL6_TaskGroupStatsV1*)group));xrtClearError();
    assert(!xl6TaskGroupStartV1(group,NULL,NULL));xrtClearError();
    xerror* error=NULL;assert(xl6TaskGroupErrorCopyV1(group,&error)&&!error);
    xcancel* token=NULL;assert(xl6TaskGroupCancellationV1(group,&token)&&token);
    xcancel* prior=token;assert(!xl6TaskGroupCancellationV1(group,&token)&&token==prior);xrtClearError();
    assert(xl6TaskGroupCancel(group)&&xrtCancelRequested(token));xrtCancelDestroy(token);
    xl6TaskGroupDestroy(group);balanced(&before);
}
static void reserve(void)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);XL6_TaskGroupConfigV1 config=options(1,0);
    xl6_task_group* group=xl6TaskGroupCreateConfiguredV1(&config);assert(group);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
    Starter state={group,source,0,false};xfuture* output=xl6TaskGroupStartV1(group,start,&state);assert(output&&state.calls==1);
    assert(!xl6TaskGroupStartV1(group,start,&state)&&state.calls==1);xrtClearError();
    XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(group,&stats)&&stats.active==1&&stats.added==1&&stats.rejected==1);
    assert(xrtPromiseResolve(promise,NULL));assert(xl6TaskGroupGetV1(group,&stats)&&!stats.active&&stats.completed==1&&stats.succeeded==1&&!stats.hasFirst);
    uint64_t reaped=UINT64_MAX;assert(xl6TaskGroupReapV1(group,&reaped)&&reaped==1);
    assert(xl6TaskGroupReapV1(group,&reaped)&&reaped==0);
    state.fail=true;assert(!xl6TaskGroupStartV1(group,start,&state)&&state.calls==2);xrtClearError();
    assert(xl6TaskGroupGetV1(group,&stats)&&!stats.active);
    state.fail=false;xfuture* again=xl6TaskGroupStartV1(group,start,&state);assert(again&&state.calls==3);
    assert(xl6TaskGroupJoin(group));assert(!xl6TaskGroupStartV1(group,start,&state)&&state.calls==3);xrtClearError();
    xrtFutureDestroy(again);xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);
    xl6TaskGroupDestroy(group);balanced(&before);
}
static void policy(unsigned cancelOn)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);XL6_TaskGroupConfigV1 config=options(0,cancelOn);
    xl6_task_group* group=xl6TaskGroupCreateConfiguredV1(&config);assert(group);
    xfuture *first=NULL,*second=NULL;
    xpromise *one=xrtPromiseCreate(&first,NULL),*two=xrtPromiseCreate(&second,NULL);assert(one&&two);
    Starter state={group,first,0,false};xfuture* output=xl6TaskGroupStartV1(group,start,&state);assert(output);
    assert(xl6TaskGroupAdd(group,second));xrtSetErrorKind(XERR_RANGE);xerror* failure=xrtTakeError();assert(failure);
    assert(xrtPromiseReject(one,failure));xrtErrorFree(failure);
    XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(group,&stats));
    assert(stats.failed==1&&stats.active==1&&stats.hasFirst&&stats.firstIndex==0&&stats.firstState==XFUTURE_FAILED);
    xcancel* token=xrtFutureCancelToken(second);assert(token&&xrtCancelRequested(token)==(cancelOn!=0));xrtCancelDestroy(token);
    assert(stats.cancelling==(cancelOn!=0));
    if(cancelOn)assert(xrtPromiseCancel(two));else assert(xrtPromiseResolve(two,NULL));
    assert(xl6TaskGroupJoin(group)&&xl6TaskGroupGetV1(group,&stats)&&stats.completed==2&&!stats.active);
    assert(stats.failed==1&&stats.succeeded==!cancelOn&&stats.cancelled==(cancelOn!=0));
    xerror* retained=NULL;assert(xl6TaskGroupErrorCopyV1(group,&retained)&&retained&&xrtErrorKind(retained)==XERR_RANGE);
    xl6TaskGroupDestroy(group);assert(xrtErrorKind(retained)==XERR_RANGE);xrtErrorFree(retained);
    xrtFutureDestroy(output);xrtPromiseDestroy(one);xrtPromiseDestroy(two);xrtFutureDestroy(first);xrtFutureDestroy(second);balanced(&before);
}
static void parent(void)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);xcancel* cancel=xrtCancelCreate();assert(cancel);
    XL6_TaskGroupConfigV1 config=options(0,0);config.cancel=cancel;
    xl6_task_group* group=xl6TaskGroupCreateConfiguredV1(&config);assert(group);assert(xrtCancelRequest(cancel));
    xcancel* token=NULL;assert(xl6TaskGroupCancellationV1(group,&token)&&xrtCancelRequested(token));
    xrtCancelDestroy(cancel);xrtCancelDestroy(token);xl6TaskGroupDestroy(group);balanced(&before);
    xrtMemDebugSnapshot(&before);group=xl6TaskGroupCreate();assert(group);config=options(2,XRT_TASK_GROUP_CANCEL_ON_STOPPED);
    xl6_task_group* child=xl6TaskGroupChildConfiguredV1(group,&config);assert(child);
    XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(group,&stats)&&stats.active==1);
    assert(xl6TaskGroupClose(child)&&xl6TaskGroupJoin(group));
    xl6TaskGroupDestroy(child);xl6TaskGroupDestroy(group);balanced(&before);
}
static void waiter(void)
{
    xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);
    xl6_task_group* group=xl6TaskGroupCreate();assert(group);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise&&xl6TaskGroupAdd(group,source));
    xcancel* cancel=xrtCancelCreate();assert(cancel&&xrtCancelRequest(cancel));
    assert(xl6TaskGroupWaitV1(group,XRT_DEADLINE_NEVER,cancel)==XWAIT_CANCELLED);
    XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(group,&stats)&&stats.active==1&&!stats.cancelling&&!stats.accepting);
    xcancel* token=xrtFutureCancelToken(source);assert(token&&!xrtCancelRequested(token));xrtCancelDestroy(token);
    assert(xrtPromiseResolve(promise,NULL)&&xl6TaskGroupWaitV1(group,0,NULL)==XWAIT_OK);
    xrtCancelDestroy(cancel);xrtPromiseDestroy(promise);xrtFutureDestroy(source);xl6TaskGroupDestroy(group);balanced(&before);
}
typedef struct Race {Starter starter;atomic_bool entered,go;xfuture* output;} Race;
static xfuture* race_start(void* raw)
{
    Race* state=raw;atomic_store_explicit(&state->entered,true,memory_order_release);
    xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
    while(!atomic_load_explicit(&state->go,memory_order_acquire)){assert(!xrtDeadlineExpired(due));xrtThreadYield();}
    return start(&state->starter);
}
static int32 race_thread(void* raw)
{Race* state=raw;state->output=xl6TaskGroupStartV1(state->starter.group,race_start,state);return 0;}
static void close_race(void)
{
    for(unsigned round=0;round<32;++round){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);xl6_task_group* group=xl6TaskGroupCreate();assert(group);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        Race state={.starter={group,source,0,false}};atomic_init(&state.entered,false);atomic_init(&state.go,false);
        xthread* thread=xrtThreadCreate(race_thread,&state,0);assert(thread);
        xdeadline due=xrtDeadlineAfter(UINT64_C(10000000));
        while(!atomic_load_explicit(&state.entered,memory_order_acquire)){assert(!xrtDeadlineExpired(due));xrtThreadYield();}
        assert(xl6TaskGroupClose(group)&&!xl6TaskGroupJoinFor(group,0));
        atomic_store_explicit(&state.go,true,memory_order_release);
        assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);assert(state.output&&state.starter.calls==1);
        assert(xrtPromiseResolve(promise,NULL)&&xl6TaskGroupJoin(group));
        xrtFutureDestroy(state.output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);xl6TaskGroupDestroy(group);balanced(&before);
    }
}
static void prefixes(void)
{
    for(unsigned operation=0;operation<3;++operation)for(size_t point=0;point<128;++point){
        xmemdebugsnapshot before={0};xrtMemDebugSnapshot(&before);XL6_TaskGroupConfigV1 config=options(1,XRT_TASK_GROUP_CANCEL_ON_FAILED);
        xl6_task_group* group=operation?xl6TaskGroupCreate():NULL;assert(!operation||group);
        xfuture* source=NULL;xpromise* promise=operation==2?xrtPromiseCreate(&source,NULL):NULL;assert(operation!=2||promise);
        Starter state={group,source,0,false};xl6_task_group* child=NULL;xfuture* output=NULL;
        assert(xrtMemDebugFailAfter(point));
        if(!operation)group=xl6TaskGroupCreateConfiguredV1(&config);
        else if(operation==1)child=xl6TaskGroupChildConfiguredV1(group,&config);
        else output=xl6TaskGroupStartV1(group,start,&state);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(hit){
            assert(operation==0?!group:operation==1?!child:!output);
            if(operation==2){XL6_TaskGroupStatsV1 stats={0};assert(xl6TaskGroupGetV1(group,&stats)&&!stats.active&&state.calls==0);}
        }else assert(operation==0?group!=NULL:operation==1?child!=NULL:output!=NULL);
        if(child){assert(xl6TaskGroupClose(child));xl6TaskGroupDestroy(child);}
        if(promise){assert(xrtPromiseResolve(promise,NULL));xrtPromiseDestroy(promise);xrtFutureDestroy(source);}
        if(output)xrtFutureDestroy(output);
        if(group)xl6TaskGroupDestroy(group);
        balanced(&before);
        if(!hit){printf("configured group operation=%u prefix=%zu balanced\n",operation,point);break;}assert(point<127);
    }
}
int main(void)
{
    assert(xrtMemDebugEnable(true));invalid();reserve();policy(0);policy(XRT_TASK_GROUP_CANCEL_ON_FAILED);
    parent();waiter();close_race();prefixes();
    puts("configured TaskGroup: exact policy/config, real owned error/token, atomic admission, reentrant starter, close/reserve barrier, rollback; balanced");
    return 0;
}
