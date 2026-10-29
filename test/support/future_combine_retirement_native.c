/* Native Combine/TaskGroup post-Clear integration, not global language admission. */
#define XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#ifndef XL6_COMBINE_RETIREMENT_LINKED
#ifdef XL6_COMBINE_RETIREMENT_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#else
#define XRT_IMPLEMENTATION
#endif
#endif
#ifdef XL6_COMBINE_RETIREMENT_XRUNTIME
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#else
#include "lib/xrt/xrt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition,const char* message)
{ if(!condition){fprintf(stderr,"combine retirement: %s\n",message);exit(1);} }
#ifndef XRT_TEST_THREAD_H
#define XRT_TEST_THREAD_H

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/windows.h>
	#else
		#include <windows.h>
	#endif
#else
	#include <pthread.h>
	#include <sched.h>
#endif



/* 测试线程返回零表示成功，非零值直接保存在槽中。 */
typedef int (*testthreadproc)(ptr pData);



/* 一个可启动和回收的跨平台测试线程槽。 */
typedef struct testthread {
	testthreadproc Proc;
	ptr Data;
	int Result;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Handle;
	#else
		pthread_t Handle;
	#endif
} testthread;



#if defined(_WIN32) || defined(_WIN64)
/* Windows 测试线程统一入口。 */
static DWORD WINAPI testThreadEntry(LPVOID pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return (DWORD)pThread->Result;
}
#else
/* POSIX 测试线程统一入口。 */
static void* testThreadEntry(void* pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return NULL;
}
#endif



/* 启动一组已经填入回调和数据的测试线程。 */
static void testThreadsStart(testthread* arrThread, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		arrThread[i].Result = -1;
		#if defined(_WIN32) || defined(_WIN64)
			arrThread[i].Handle = CreateThread(NULL, 0, testThreadEntry, &arrThread[i], 0, NULL);
			testRequire(arrThread[i].Handle != NULL, "CreateThread failed");
		#else
			testRequire(pthread_create(&arrThread[i].Handle, NULL, testThreadEntry, &arrThread[i]) == 0,
				"pthread_create failed");
		#endif
	}
}



/* 等待并回收一组已经启动的测试线程。 */
static void testThreadsJoin(testthread* arrThread, size_t iCount)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		#if defined(_WIN32) || defined(_WIN64)
			testRequire(WaitForSingleObject(arrThread[i].Handle, INFINITE) == WAIT_OBJECT_0,
				"thread wait failed");
			CloseHandle(arrThread[i].Handle);
		#else
			testRequire(pthread_join(arrThread[i].Handle, NULL) == 0, "pthread_join failed");
		#endif
	}
}



/* 让出当前测试线程的处理器时间片。 */
static inline void testThreadYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}

#endif

typedef struct events { unsigned maps,drop; } events;
typedef struct owner {
    volatile int32 refs;
    struct owner* self;
    xfuture *sources[2],*output;
    xpromise* promises[2];
    xtaskgroup* group;
    const void* claim;
    bool cleared;
    events* events;
} owner;
static const xrtownershipops owner_ops;
static void begin(xrtownershipscope* scope) { testRequire(xrtOwnershipMutationBegin(scope),"mutation"); }
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope),"scope end"); }
static void frozen(xrtownershipscope* scope) { testRequire(xrtOwnershipFreezeTryBegin(scope),"freeze"); }
static xrtownershipref view(owner* value) { return (xrtownershipref){value,value?&owner_ops:NULL}; }
static bool count_owner(const void* data,size_t* count)
{ const owner* value=data;if(!value || !count || value->cleared || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool trace_owner(const void* data,xrtownershipvisitor visit,ptr context)
{
    const owner* value=data;
    if(!visit || value->cleared || !visit(view(value->self),context))return false;
    for(unsigned i=0;i<2;++i)if(!visit(xrtFutureOwnership(value->sources[i]),context) || !visit(xrtPromiseOwnership(value->promises[i]),context))return false;
    return visit(xrtFutureOwnership(value->output),context) && (!value->group || visit(xrtTaskGroupOwnership(value->group),context));
}
static const xrtownershipops owner_ops={count_owner,trace_owner};
static bool hold_owner(const void* data) { return xrtRefRetain(&((owner*)data)->refs)>0; }
static void drop_owner(const void* data)
{
    if(!data)return;
    owner* value=(owner*)data;xrtownershipscope scope={0},probe={0};frozen(&probe);end(&probe);begin(&scope);
    int32 refs=xrtRefRelease(&value->refs);testRequire(refs>=0,"actual owner reference");
    if(!refs){testRequire(!value->self && !value->output && !value->group && !value->sources[0] && !value->sources[1],"all owner slots consumed");++value->events->drop;xrtFree(value);}
    end(&scope);
}
static bool claim_owner(const void* data,const void* token)
{ owner* value=(owner*)data;if(!token || value->cleared || (value->claim && value->claim!=token))return false;value->claim=token;return true; }
static void restore_owner(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(value->claim==token && !value->cleared,"restore");value->claim=NULL; }
static void clear_owner(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(value->claim==token && !value->cleared,"clear");value->cleared=true; }
static void dispose(owner* value)
{
    xrtownershipscope scope={0};begin(&scope);owner* self=value->self;xfuture* output=value->output;xtaskgroup* group=value->group;
    xfuture *a=value->sources[0],*b=value->sources[1];xpromise *pa=value->promises[0],*pb=value->promises[1];
    value->self=NULL;value->output=NULL;value->group=NULL;value->sources[0]=value->sources[1]=NULL;value->promises[0]=value->promises[1]=NULL;end(&scope);
    xrtTaskGroupDestroy(group);xrtFutureDestroy(output);xrtPromiseDestroy(pa);xrtPromiseDestroy(pb);xrtFutureDestroy(a);xrtFutureDestroy(b);drop_owner(self);
}
static bool finish_owner(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(value->claim==token && value->cleared,"owner completion");dispose(value);return true; }
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),hold_owner,drop_owner,claim_owner,restore_owner,NULL,clear_owner,finish_owner};
static void mapped_all(const xfutureall* inputs,xpromise* promise,ptr data)
{
    owner* value=data;xrtownershipscope probe={0};frozen(&probe);end(&probe);
    testRequire(!value->cleared && inputs->Count==3 && inputs->Futures[0]==inputs->Futures[1],"only normal map, duplicate real sources");
    ++value->events->maps;testRequire(xrtPromiseResolve(promise,NULL),"mapped result");
}
static void mapped_pick(const xfuturepick* input,xpromise* promise,ptr data)
{
    owner* value=data;xrtownershipscope probe={0};frozen(&probe);end(&probe);
    testRequire(!value->cleared && input->Index<3,"only normal pick");++value->events->maps;
    testRequire(xrtPromiseResolve(promise,NULL),"pick result");
}
static const xfuturecombineownershipv1 map_policy={sizeof(map_policy),mapped_all,mapped_pick,drop_owner,&owner_ops};
static owner* create(events* counters,unsigned mode,bool mapped,bool grouped)
{
    owner* value=xrtCalloc(1,sizeof(*value));testRequire(value!=NULL,"owner allocation");value->refs=1;value->self=value;value->events=counters;
    for(unsigned i=0;i<2;++i){value->promises[i]=xrtPromiseCreate(&value->sources[i],NULL);testRequire(value->promises[i] && value->sources[i],"real source endpoint rights");}
    xfuture* inputs[]={value->sources[0],value->sources[0],value->sources[1]};
    if(mapped){testRequire(hold_owner(value),"actual mapped Data right");
        value->output=mode==0?xrtFutureAllMapOwnedPolicyV1(inputs,3,value,&map_policy):mode==1?xrtFutureAnyMapOwnedPolicyV1(inputs,3,value,&map_policy):xrtFutureRaceMapOwnedPolicyV1(inputs,3,value,&map_policy);
    }else value->output=mode==0?xrtFutureAll(inputs,3):mode==1?xrtFutureAny(inputs,3):xrtFutureRace(inputs,3);
    testRequire(value->output!=NULL,"aggregate");
    if(grouped){value->group=xrtTaskGroupCreate(NULL);testRequire(value->group && xrtTaskGroupAdd(value->group,value->output),"real TaskGroup registration");}
    return value; /* The initial reference belongs to the self slot, not this alias. */
}
enum { LIMIT=40 };
typedef struct node { xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershipfinishv1* completion;size_t count,incoming;bool rooted,done,combine; } node;
typedef struct plan { node nodes[LIMIT];unsigned edges[LIMIT][LIMIT];size_t size,current; } plan;
static bool visit(xrtownershipref ref,ptr data)
{
    plan* value=data;testRequire(ref.Data && ref.Ops,"every physical edge");size_t index=0;
    while(index<value->size && (value->nodes[index].ref.Data!=ref.Data || value->nodes[index].ref.Ops!=ref.Ops))++index;
    if(index==value->size){testRequire(value->size<LIMIT,"graph bound");value->nodes[value->size++].ref=ref;}
    ++value->nodes[index].incoming;++value->edges[value->current][index];return true;
}
static void snapshot(plan* value,owner* root)
{
    const xfuturepayloadownershipv1* payloads[]={xrtFutureCombineAllPayloadPolicyV1Get(),xrtFutureCombinePickPayloadPolicyV1Get()};
    const xfutureproducerownershipv1* producers[]={xrtFutureCombineProducerPolicyV1Get(),xrtTaskGroupProducerPolicyV1Get()};
    const xfuturewatchownershipv1* direct[]={xrtTaskGroupWatchPolicyV1Get()};
    const xfuturewatchownershipv2* projected[]={xrtFutureCombineWatchPolicyV2Get()};
    const xcancelwatchownershipv1* cancels[]={xrtFutureCombineCancelPolicyV1Get(),xrtTaskGroupCancelPolicyV1Get()};
    const xfuturecombineownershipv1* maps[]={&map_policy};
    const xfutureownershipadmissionv1 admission={sizeof(admission),payloads,2,producers,2,direct,1,projected,1,cancels,2};
    *value=(plan){0};
    value->nodes[0].ref=view(root);value->size=1;
    for(size_t i=0;i<value->size;++i){node* item=&value->nodes[i];value->current=i;
        if(item->ref.Ops==&owner_ops)item->adapter=&owner_adapter;
        else{
            item->adapter=xrtFutureCombineOwnershipAdapterV2(item->ref,maps,1,&item->completion);
            item->combine=item->adapter!=NULL;
            if(!item->adapter)item->adapter=xrtTaskGroupOwnershipAdapterV1(item->ref,&item->completion);
            if(!item->adapter)item->adapter=xrtFutureOwnershipAdapterV5(item->ref,&admission,&item->completion);
            if(!item->adapter)item->adapter=xrtCancelWatchOwnershipAdapterV2(item->ref,cancels,2,&item->completion);
            if(!item->adapter)item->adapter=xrtCancelOwnershipAdapterV3(item->ref,cancels,2,&item->completion);
        }
        testRequire(item->adapter && item->ref.Ops->Count(item->ref.Data,&item->count) && item->ref.Ops->Trace(item->ref.Data,visit,value),"independent closed admission before Count/Trace");
    }
    size_t edges=0,roots=0;
    for(size_t i=0;i<value->size;++i){testRequire(value->nodes[i].count>=value->nodes[i].incoming,"no invented reference");
        value->nodes[i].rooted=value->nodes[i].count>value->nodes[i].incoming;
        if(value->nodes[i].rooted)++roots;
        for(size_t j=0;j<value->size;++j)edges+=value->edges[i][j];}
    for(size_t pass=0;pass<value->size;++pass)for(size_t i=0;i<value->size;++i)if(value->nodes[i].rooted)
        for(size_t j=0;j<value->size;++j)if(value->edges[i][j])value->nodes[j].rooted=true;
    xrtownershipresult result={0};xrtownershipref anchor=view(root);
    testRequire(xrtOwnershipInspect(&anchor,1,NULL,0,&result) && result.NodeCount==value->size && result.EdgeCount==edges && result.ExternalRootCount==roots,"independent pre-pin inspector agrees on real nodes, duplicate edges and external roots");
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount && before->InvalidFreeCount==after.InvalidFreeCount &&
        before->DoubleFreeCount==after.DoubleFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount,"exact terminal ownership and allocator ledger");
}
static void cycle(unsigned mode,bool mapped,bool grouped,unsigned external,bool reverse,bool abort_plan)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);events counters={0};owner* root=create(&counters,mode,mapped,grouped);
    xfuture* extra=external?xrtFutureRef(external==1?root->sources[0]:root->output):NULL;
    plan value;xrtownershipscope scope={0};unsigned token=0,conflict=1;frozen(&scope);snapshot(&value,root);
    size_t active=0;
    for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];testRequire(item->adapter->Hold(item->ref.Data),"real plan pin");
        if(!item->rooted){++active;testRequire(item->adapter->Claim(item->ref.Data,&token) && !item->adapter->Claim(item->ref.Data,&conflict),"exclusive claims");}}
    end(&scope);testRequire(!counters.maps && xrtFutureState(root->output)==XFUTURE_PENDING,"pre-finalization stays pending and does not map");
    if(abort_plan || value.nodes[0].rooted){
        frozen(&scope);for(size_t i=0;i<value.size;++i)if(!value.nodes[i].rooted)value.nodes[i].adapter->Restore(value.nodes[i].ref.Data,&token);end(&scope);
        dispose(root);
    }else{
        frozen(&scope);for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;
            if(!value.nodes[i].rooted)value.nodes[i].adapter->Clear(value.nodes[i].ref.Data,&token);}end(&scope);
        testRequire(xrtMemDebugFailAfter(0),"allocation-free finish");
        for(size_t pass=0;active && pass<value.size+1;++pass){size_t progress=0;
            for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;node* item=&value.nodes[i];if(item->rooted || item->done)continue;
                xrtownershipfinishresult result=item->completion?item->completion->Step(item->ref.Data,&token):item->adapter->Finish(item->ref.Data,&token)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
                testRequire(result!=XRT_OWNERSHIP_FINISH_FAILED,"explicit completion status");
                if(result==XRT_OWNERSHIP_FINISH_READY){item->done=true;--active;++progress;}}
            testRequire(progress || !active,"bounded participant progress");}
        testRequire(!active && !xrtMemDebugFailTriggered() && !counters.maps && !counters.drop,"no allocation, eager map or early pin release");xrtMemDebugFailClear();
        for(size_t i=0;i<value.size;++i)if(!value.nodes[i].rooted){node* item=&value.nodes[i];
            testRequire(item->completion?item->completion->Step(item->ref.Data,&token)==XRT_OWNERSHIP_FINISH_READY:item->adapter->Finish(item->ref.Data,&token),"idempotent READY");}
    }
    for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;value.nodes[i].adapter->Drop(value.nodes[i].ref.Data);}
    testRequire(counters.drop==1 && counters.maps<=1,"exact owner return and at most one normal map");xrtFutureDestroy(extra);balanced(&before);
}
typedef struct barrier { xatomic32 entered,proceed; } barrier;
static int hold_mutation(ptr data)
{
    barrier* value=data;xrtownershipscope scope={0};begin(&scope);xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);
    double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&value->proceed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"bounded mutation barrier");testThreadYield();}
    end(&scope);return 0;
}
static void busy(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);events counters={0};owner* root=create(&counters,0,true,false);
    plan value;xrtownershipscope scope={0};unsigned token=0;size_t combine=LIMIT;frozen(&scope);snapshot(&value,root);
    for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];testRequire(!item->rooted && item->adapter->Hold(item->ref.Data) && item->adapter->Claim(item->ref.Data,&token),"unrooted real BUSY graph");if(item->combine)combine=i;}
    testRequire(combine<LIMIT,"actual native producer");
    const xfuturecombineownershipv1* unknown[]={(const xfuturecombineownershipv1*)(uintptr_t)1};
    const xrtownershipfinishv1* untouched=(const xrtownershipfinishv1*)(uintptr_t)1;
    testRequire(!xrtFutureCombineOwnershipAdapterV2(value.nodes[combine].ref,unknown,1,&untouched) && untouched==(const xrtownershipfinishv1*)(uintptr_t)1,"unknown map refused without dereference or output write");
    for(size_t i=0;i<value.size;++i)value.nodes[i].adapter->Clear(value.nodes[i].ref.Data,&token);
    end(&scope);
    barrier blocking;testthread thread={0};xrtAtomic32Init(&blocking.entered,0);xrtAtomic32Init(&blocking.proceed,0);
    thread.Proc=hold_mutation;thread.Data=&blocking;testThreadsStart(&thread,1);double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&blocking.entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"bounded barrier entry");testThreadYield();}
    int32 refs=root->refs;testRequire(xrtMemDebugFailAfter(0),"BUSY no allocation");
    for(unsigned i=0;i<8;++i)testRequire(value.nodes[combine].completion->Step(value.nodes[combine].ref.Data,&token)==XRT_OWNERSHIP_FINISH_BUSY &&
        root->refs==refs && !counters.maps && !counters.drop && !xrtMemDebugFailTriggered(),"BUSY preserves every right, mapper and pin");
    xrtAtomic32Store(&blocking.proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);testRequire(thread.Result==0,"actual mutation tail joined");
    size_t remaining=value.size;
    for(size_t pass=0;remaining && pass<=value.size;++pass){size_t progress=0;
        for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];if(item->done)continue;
            xrtownershipfinishresult result=item->completion?item->completion->Step(item->ref.Data,&token):item->adapter->Finish(item->ref.Data,&token)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
            testRequire(result!=XRT_OWNERSHIP_FINISH_FAILED,"BUSY retry status");if(result==XRT_OWNERSHIP_FINISH_READY){item->done=true;--remaining;++progress;}}
        testRequire(progress || !remaining,"BUSY retry bounded progress");}
    testRequire(!remaining && !counters.drop && !counters.maps && !xrtMemDebugFailTriggered(),"all pins retained after ready");xrtMemDebugFailClear();
    for(size_t i=value.size;i--!=0;)value.nodes[i].adapter->Drop(value.nodes[i].ref.Data);
    testRequire(counters.drop==1,"busy owner retires exactly once");balanced(&before);
}
typedef struct dispatch { volatile int32 refs;xcancel* token;xatomic32 entered,proceed; } dispatch;
static bool dispatch_count(const void* data,size_t* count)
{ const dispatch* value=data;*count=(size_t)value->refs;return value->refs>0; }
static bool dispatch_trace(const void* data,xrtownershipvisitor visit,ptr context)
{ (void)context;return data && visit; /* token is borrowed from a separate real external handle. */ }
static const xrtownershipops dispatch_ops={dispatch_count,dispatch_trace};
static void dispatch_drop(const void* data)
{ xrtownershipscope scope={0};begin(&scope);testRequire(xrtRefRelease(&((dispatch*)data)->refs)==1,"one external callback Data right");end(&scope); }
static void dispatch_notify(ptr data)
{
    dispatch* value=data;xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&value->proceed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"bounded real dispatch");testThreadYield();}
}
static const xcancelwatchownershipv1 dispatch_policy={sizeof(dispatch_policy),dispatch_notify,dispatch_drop,&dispatch_ops};
static int request_token(ptr data) { testRequire(xrtCancelRequest(((dispatch*)data)->token),"real external request");return 0; }
static void dispatch_tail(bool reverse)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);events counters={0};owner* root=create(&counters,2,true,true);
    xcancel* token=xrtFutureCancelToken(root->output);testRequire(token!=NULL,"real external cancel root");
    plan value;xrtownershipscope scope={0};unsigned claim=0;frozen(&scope);snapshot(&value,root);size_t remaining=0;
    testRequire(!value.nodes[0].rooted,"token does not own its borrowed observer list");
    for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];testRequire(item->adapter->Hold(item->ref.Data),"dispatch plan pin");
        if(!item->rooted){++remaining;testRequire(item->adapter->Claim(item->ref.Data,&claim),"dispatch claim");}}
    for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;if(!value.nodes[i].rooted)value.nodes[i].adapter->Clear(value.nodes[i].ref.Data,&claim);}
    end(&scope);
    dispatch blocked={0};blocked.refs=2;blocked.token=token;xrtAtomic32Init(&blocked.entered,0);xrtAtomic32Init(&blocked.proceed,0);
    xcancelwatch* watch=xrtCancelWatchOwnedV1(token,&blocked,&dispatch_policy);testRequire(watch!=NULL,"separate post-Clear registration");
    testthread thread={0};thread.Proc=request_token;thread.Data=&blocked;testThreadsStart(&thread,1);double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&blocked.entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"dispatch barrier entry");testThreadYield();}
    bool released=false;testRequire(xrtMemDebugFailAfter(0),"dispatch completion no allocation");
    for(size_t pass=0;remaining && pass<value.size+2;++pass){size_t progress=0;
        for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;node* item=&value.nodes[i];if(item->rooted || item->done)continue;
            xrtownershipfinishresult result=item->completion?item->completion->Step(item->ref.Data,&claim):item->adapter->Finish(item->ref.Data,&claim)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
            testRequire(result!=XRT_OWNERSHIP_FINISH_FAILED,"dispatch completion status");
            if(result==XRT_OWNERSHIP_FINISH_READY){item->done=true;--remaining;++progress;}}
        if(!progress && remaining && !released){
            bool combineReady=false;for(size_t i=0;i<value.size;++i)if(value.nodes[i].combine)combineReady=value.nodes[i].done;
            testRequire(combineReady && !counters.maps && !counters.drop,"Combine READY keeps all pins while Watch is BUSY");
            xrtAtomic32Store(&blocked.proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);released=true;
            testRequire(thread.Result==0,"actual dispatch tail joined");
        }else testRequire(progress || !remaining,"bounded dispatch progress");
    }
    testRequire(released && !remaining && !xrtMemDebugFailTriggered() && !counters.maps && !counters.drop,"no early mapper/pin release in dispatch");xrtMemDebugFailClear();
    testRequire(xrtCancelUnwatchChecked(watch) && blocked.refs==1 && xrtRefRelease(&blocked.refs)==0,"external registration and creator returned");
    for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;value.nodes[i].adapter->Drop(value.nodes[i].ref.Data);}
    testRequire(counters.drop==1,"one dispatch owner disposal");xrtCancelDestroy(token);balanced(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"debug ledger");xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned mode=0;mode<3;++mode)for(unsigned mapped=0;mapped<2;++mapped)for(unsigned grouped=0;grouped<2;++grouped)
        for(unsigned external=0;external<3;++external)for(unsigned reverse=0;reverse<2;++reverse)for(unsigned restore=0;restore<2;++restore)
            cycle(mode,mapped!=0,grouped!=0,external,reverse!=0,restore!=0);
    busy();
    dispatch_tail(false);dispatch_tail(true);
    puts("Combine retirement: 144 real ALL/ANY/RACE cycles, owned/raw maps, duplicate inputs, mixed TaskGroup graphs, exact external roots, both retirement orders and aborts; 8 mutation BUSY turns and 2 real dispatch tails; no eager map and full ledger balanced");return 0;
}
