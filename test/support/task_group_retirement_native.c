/* Native post-finalization retirement, not automatic language admission. */
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#ifndef XL6_TASK_GROUP_RETIREMENT_LINKED
#ifdef XL6_TASK_GROUP_RETIREMENT_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#else
#define XRT_IMPLEMENTATION
#endif
#endif
#ifdef XL6_TASK_GROUP_RETIREMENT_XRUNTIME
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
{ if(!condition){fprintf(stderr,"TaskGroup retirement: %s\n",message);exit(1);} }
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

typedef struct events { unsigned notify,drop; } events;
typedef struct owner {
    volatile int32 refs;
    xtaskgroup *group,*child;
    xfuture* source;
    xpromise* promise;
    xcancel* parent;
    xcancelwatch* watch;
    const void* claim;
    bool cleared;
    events* events;
} owner;
static void begin(xrtownershipscope* scope) { testRequire(xrtOwnershipMutationBegin(scope),"mutation"); }
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope),"scope end"); }
static void freeze(xrtownershipscope* scope) { testRequire(xrtOwnershipFreezeTryBegin(scope),"exclusive freeze"); }
static bool owner_count(const void* data,size_t* count)
{ const owner* value=data;if(!value || !count || value->cleared || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool owner_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const owner* value=data;if(!visit || value->cleared)return false;
    return (!value->group || visit(xrtTaskGroupOwnership(value->group),context)) &&
        (!value->child || visit(xrtTaskGroupOwnership(value->child),context)) &&
        (!value->source || visit(xrtFutureOwnership(value->source),context)) &&
        (!value->promise || visit(xrtPromiseOwnership(value->promise),context)) &&
        (!value->parent || visit(xrtCancelOwnership(value->parent),context)) &&
        (!value->watch || visit(xrtCancelWatchOwnership(value->watch),context));
}
static const xrtownershipops owner_ops={owner_count,owner_trace};
static void owner_drop(const void* data)
{
    owner* value=(owner*)data;xrtownershipscope scope={0},probe={0};freeze(&probe);end(&probe);begin(&scope);
    if(!xrtRefRelease(&value->refs)){
        testRequire(!value->group && !value->child && !value->source && !value->promise && !value->parent && !value->watch,"all owner slots returned");
        ++value->events->drop;xrtFree(value);
    }
    end(&scope);
}
static void notified(ptr data)
{ owner* value=data;++value->events->notify;testRequire(xrtTaskGroupCancel(value->group),"original owner notification"); }
static const xcancelwatchownershipv1 owner_policy={sizeof(owner_policy),notified,owner_drop,&owner_ops};
static bool owner_hold(const void* data)
{ owner* value=(owner*)data;xrtownershipscope scope={0};begin(&scope);bool held=!value->cleared && xrtRefRetain(&value->refs)>0;end(&scope);return held; }
static bool owner_claim(const void* data,const void* token)
{ owner* value=(owner*)data;if(!token || value->cleared || (value->claim && value->claim!=token))return false;value->claim=token;return true; }
static void owner_restore(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(token && value->claim==token && !value->cleared,"owner restore");value->claim=NULL; }
static void owner_clear(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(token && value->claim==token && !value->cleared,"owner clear");value->cleared=true; }
static void dispose(owner* value)
{
    xrtownershipscope scope={0};begin(&scope);
    xtaskgroup *group=value->group,*child=value->child;xfuture* source=value->source;
    xpromise* promise=value->promise;xcancel* parent=value->parent;xcancelwatch* watch=value->watch;
    value->group=NULL;value->child=NULL;value->source=NULL;value->promise=NULL;value->parent=NULL;value->watch=NULL;end(&scope);
    xrtCancelUnwatch(watch);xrtTaskGroupDestroy(group);xrtTaskGroupDestroy(child);
    xrtPromiseDestroy(promise);xrtFutureDestroy(source);xrtCancelDestroy(parent);
}
static bool owner_finish(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(token && value->claim==token && value->cleared,"owner finish");dispose(value);return true; }
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),owner_hold,owner_drop,owner_claim,owner_restore,NULL,owner_clear,owner_finish};
static owner* create(events* counters,bool nested)
{
    owner* value=xrtCalloc(1,sizeof(*value));testRequire(value!=NULL,"owner");value->refs=1;value->events=counters;
    value->parent=xrtCancelCreate();testRequire(value->parent!=NULL,"parent");xtaskgroupconfig config={0};config.Cancel=value->parent;
    value->group=xrtTaskGroupCreate(&config);testRequire(value->group!=NULL,"group");
    if(nested){value->child=xrtTaskGroupChild(value->group,NULL);testRequire(value->child!=NULL,"child");}
    value->promise=xrtPromiseCreate(&value->source,NULL);testRequire(value->promise && value->source,"source");
    testRequire(xrtTaskGroupAdd(nested?value->child:value->group,value->source),"active source");
    value->watch=xrtCancelWatchOwnedV1(value->parent,value,&owner_policy);testRequire(value->watch!=NULL,"owner cycle");
    return value; /* The only Owner reference belongs to Watch, not this alias. */
}
enum { LIMIT=64 };
typedef struct node {
    xrtownershipref ref;
    const xrtownershipadapterv1* adapter;
    const xrtownershipfinishv1* completion;
    size_t count,incoming;
    bool live,done;
} node;
typedef struct plan {
    node nodes[LIMIT];unsigned edges[LIMIT][LIMIT];size_t size,current;
    xfutureownershipadmissionv1 admission;
    const xfuturewatchownershipv1* watches[1];
    const xfutureproducerownershipv1* producers[1];
    const xcancelwatchownershipv1* cancels[2];
} plan;
static size_t find(plan* value,xrtownershipref ref)
{ for(size_t i=0;i<value->size;++i)if(value->nodes[i].ref.Data==ref.Data && value->nodes[i].ref.Ops==ref.Ops)return i;return LIMIT; }
static bool visit(xrtownershipref ref,ptr context)
{
    plan* value=context;testRequire(ref.Data && ref.Ops,"no omitted node");size_t index=find(value,ref);
    if(index==LIMIT){testRequire(value->size<LIMIT,"bounded physical graph");index=value->size++;value->nodes[index].ref=ref;}
    ++value->edges[value->current][index];++value->nodes[index].incoming;return true;
}
static const xrtownershipadapterv1* query(plan* value,xrtownershipref ref,const xrtownershipfinishv1** completion)
{
    if(ref.Ops==&owner_ops)return &owner_adapter;
    const xrtownershipadapterv1* result=xrtTaskGroupOwnershipAdapterV1(ref,completion);
    if(!result)result=xrtFutureOwnershipAdapterV5(ref,&value->admission,completion);
    if(!result)result=xrtCancelWatchOwnershipAdapterV2(ref,value->cancels,2,completion);
    if(!result)result=xrtCancelOwnershipAdapterV3(ref,value->cancels,2,completion);
    return result;
}
static void snapshot(plan* value,owner* root)
{
    memset(value,0,sizeof(*value));value->watches[0]=xrtTaskGroupWatchPolicyV1Get();
    value->cancels[0]=xrtTaskGroupCancelPolicyV1Get();value->cancels[1]=&owner_policy;
    value->admission.size=sizeof(value->admission);value->admission.WatchPolicies=value->watches;value->admission.WatchPolicyCount=1;
    value->producers[0]=xrtTaskGroupProducerPolicyV1Get();
    value->admission.ProducerPolicies=value->producers;value->admission.ProducerPolicyCount=1;
    value->admission.CancelWatchPolicies=value->cancels;value->admission.CancelWatchPolicyCount=2;
    value->nodes[0].ref.Data=root;value->nodes[0].ref.Ops=&owner_ops;value->size=1;
    for(size_t i=0;i<value->size;++i){
        node* item=&value->nodes[i];item->adapter=query(value,item->ref,&item->completion);
        testRequire(item->adapter && (!item->completion || item->completion->Adapter==item->adapter),"admit exact protocol before Count/Trace");
        testRequire(item->ref.Ops->Count(item->ref.Data,&item->count),"physical count");value->current=i;
        testRequire(item->ref.Ops->Trace(item->ref.Data,visit,value),"all strong slots");
    }
    for(size_t i=0;i<value->size;++i){testRequire(value->nodes[i].count>=value->nodes[i].incoming,"no overcounted edge");value->nodes[i].live=value->nodes[i].count>value->nodes[i].incoming;}
    for(size_t pass=0;pass<value->size;++pass)for(size_t i=0;i<value->size;++i)if(value->nodes[i].live)
        for(size_t j=0;j<value->size;++j)if(value->edges[i][j])value->nodes[j].live=true;
    xrtownershipresult observed={0};xrtownershipref root_ref=value->nodes[0].ref;size_t edges=0,roots=0;
    for(size_t i=0;i<value->size;++i){if(value->nodes[i].count>value->nodes[i].incoming)++roots;
        for(size_t j=0;j<value->size;++j)edges+=value->edges[i][j];}
    testRequire(xrtOwnershipInspect(&root_ref,1,NULL,0,&observed) && observed.NodeCount==value->size &&
        observed.EdgeCount==edges && observed.ExternalRootCount==roots,"independent physical inspector agrees before any plan pins");
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount,"full cyclic retirement ledger balanced");
}
typedef struct dispatch {
    volatile int32 refs;
    xatomic32 entered,proceed;
    xcancel* parent; /* Borrowed from the separately rooted test handle. */
} dispatch;
static bool dispatch_count(const void* data,size_t* count)
{ const dispatch* value=data;if(!value || !count || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool dispatch_trace(const void* data,xrtownershipvisitor visit,ptr context)
{ (void)context;return data && visit; }
static const xrtownershipops dispatch_ops={dispatch_count,dispatch_trace};
static void dispatch_drop(const void* data)
{ xrtownershipscope scope={0};begin(&scope);testRequire(xrtRefRelease(&((dispatch*)data)->refs)==1,"real external callback context right");end(&scope); }
static void dispatch_notify(ptr data)
{
    dispatch* value=data;xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&value->proceed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"bounded dispatch release");testThreadYield();}
}
static const xcancelwatchownershipv1 dispatch_policy={sizeof(dispatch_policy),dispatch_notify,dispatch_drop,&dispatch_ops};
static int request_parent(ptr data) { testRequire(xrtCancelRequest(((dispatch*)data)->parent),"external threaded request");return 0; }
static void cycle(unsigned bits,bool restore)
{
    bool nested=(bits&1)!=0,external_parent=(bits&2)!=0,external_done=(bits&4)!=0;
    events counters={0};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);owner* root=create(&counters,nested);
    dispatch blocked={0};testthread thread={0};xcancelwatch* external_watch=NULL;bool released=false;
    xtaskgroup* group=root->group;xfuture* source=root->source;xcancel* parent=external_parent?xrtCancelRef(root->parent):NULL;
    xfuture* done=external_done?xrtTaskGroupFuture(group):NULL;plan value;xrtownershipscope scope={0};unsigned token=0,conflict=1;
    freeze(&scope);snapshot(&value,root);
    if(!done)testRequire(!value.nodes[0].live,"the real owner cycle has no external owner");
    if(done){size_t index=find(&value,xrtTaskGroupOwnership(group));
        testRequire(index<LIMIT && value.nodes[index].live,"rooted pending Done keeps its real producer live");}
    size_t active=0;
    for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];testRequire(item->adapter->Hold(item->ref.Data),"actual plan pin");
        if(!item->live){++active;testRequire(item->adapter->Claim(item->ref.Data,&token) && !item->adapter->Claim(item->ref.Data,&conflict),"exclusive claim");}}
    end(&scope);
    xtaskgroupstats stats={0};testRequire(xrtTaskGroupGet(group,&stats) && stats.Accepting && stats.Active==1 &&
        xrtFutureState(source)==XFUTURE_PENDING && !counters.notify && !counters.drop,"finalizers still see unchanged semantic slots");
    if(restore || done){
        freeze(&scope);for(size_t i=0;i<value.size;++i)if(!value.nodes[i].live)value.nodes[i].adapter->Restore(value.nodes[i].ref.Data,&token);end(&scope);
        if(restore)testRequire(parent && xrtCancelRequest(parent) && counters.notify==1,"abort preserves original cancellation");
        dispose(root);
        if(done)testRequire(xrtFutureState(done)==XFUTURE_RESOLVED,"rooted Done completes normally rather than losing its producer");
    }else{
        freeze(&scope);for(size_t step=0;step<value.size;++step){size_t i=(bits&8)?value.size-1-step:step;
            if(!value.nodes[i].live)value.nodes[i].adapter->Clear(value.nodes[i].ref.Data,&token);}end(&scope);
        testRequire(!xrtTaskGroupGet(group,&stats),"cleared group cannot escape");xrtClearError();
        if(bits&64){
            testRequire(parent!=NULL,"external root for threaded dispatch");blocked.refs=2;blocked.parent=parent;
            xrtAtomic32Init(&blocked.entered,0);xrtAtomic32Init(&blocked.proceed,0);
            /* This separate live host registration is added AFTER global
             * Clear. It cannot reactivate a retired owned registration. */
            external_watch=xrtCancelWatchOwnedV1(parent,&blocked,&dispatch_policy);testRequire(external_watch!=NULL,"external observer");
            thread.Proc=request_parent;thread.Data=&blocked;testThreadsStart(&thread,1);double until=xrtTimer()+5;
            while(!xrtAtomic32Load(&blocked.entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"bounded dispatch entry");testThreadYield();}
        }
        testRequire(xrtMemDebugFailAfter(0),"allocation-free finish");
        size_t source_index=LIMIT;for(size_t i=0;i<value.size;++i)if(value.nodes[i].ref.Data==source)source_index=i;
        testRequire(source_index<LIMIT && value.nodes[source_index].completion->Step(source,&token)==XRT_OWNERSHIP_FINISH_BUSY,
            "Future is BUSY before its real registration owner detaches");
        if(parent && !(bits&64))testRequire(xrtCancelRequest(parent) && !counters.notify,"Clear suppresses external-parent notification without an early Unwatch");
        for(size_t pass=0;active && pass<value.size+1;++pass){
            size_t progress=0;
            for(size_t step=0;step<value.size;++step){size_t i=(bits&16)?value.size-1-step:step;node* item=&value.nodes[i];
                if(item->live || item->done)continue;
                xrtownershipfinishresult result=item->completion?item->completion->Step(item->ref.Data,&token):
                    item->adapter->Finish(item->ref.Data,&token)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
                testRequire(result!=XRT_OWNERSHIP_FINISH_FAILED,"no silent cleanup failure");
                if(result==XRT_OWNERSHIP_FINISH_READY){item->done=true;--active;++progress;}
            }
            if(!progress && active && external_watch && !released){
                /* Group/item/owner READY does not release ANY plan pin while
                 * the real Watch dispatch tail still reports BUSY. */
                testRequire(pass>0 && !counters.drop,"all pins retained across a real dispatch dependency");
                xrtAtomic32Store(&blocked.proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);released=true;
                testRequire(thread.Result==0 && !counters.notify,"cleared callbacks never start in the queued tail");
            }else testRequire(progress || !active,"bounded dependency progress");
        }
        testRequire(!active && !xrtMemDebugFailTriggered() && !counters.notify,"all physical completion duties settle without allocation/notification");
        xrtMemDebugFailClear();
        if(external_watch){testRequire(released,"real BUSY dispatch was exercised");xrtCancelUnwatch(external_watch);
            begin(&scope);testRequire(blocked.refs==1 && xrtRefRelease(&blocked.refs)==0,"external context creator right");end(&scope);}
        for(size_t i=0;i<value.size;++i)if(!value.nodes[i].live){node* item=&value.nodes[i];
            testRequire(item->completion?item->completion->Step(item->ref.Data,&token)==XRT_OWNERSHIP_FINISH_READY:
                item->adapter->Finish(item->ref.Data,&token),"idempotent completion with every real pin retained");}
        if(done)testRequire(xrtFutureState(done)==XFUTURE_RESOLVED,"separately rooted Done Future receives normal completion");
    }
    for(size_t step=0;step<value.size;++step){size_t i=(bits&32)?value.size-1-step:step;value.nodes[i].adapter->Drop(value.nodes[i].ref.Data);}
    testRequire(counters.drop==1,"exactly one terminal owner release");xrtFutureDestroy(done);xrtCancelDestroy(parent);balanced(&before);
}
static void legacy_notify(ptr data) { ++*(unsigned*)data; }
static void refusal(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);
    testRequire(promise && source,"legacy source");xfuturewatch watch={0};unsigned calls=0;
    testRequire(xrtFutureWatchInit(&watch,legacy_notify,NULL,&calls) &&
        xrtFutureWatchAdd(source,&watch)==XFUTURE_WATCH_PENDING,"legacy registration");
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    const xrtownershipfinishv1* completion=(const xrtownershipfinishv1*)(uintptr_t)1;xrtownershipscope scope={0};freeze(&scope);
    testRequire(!xrtFutureOwnershipAdapterV5(xrtFutureOwnership(source),&admission,&completion) &&
        completion==(const xrtownershipfinishv1*)(uintptr_t)1,"legacy observer remains unadmitted with output untouched");
    xrtownershipref unknown;unknown.Data=(const void*)(uintptr_t)1;unknown.Ops=&owner_ops;
    testRequire(!xrtTaskGroupOwnershipAdapterV1(unknown,&completion) && completion==(const xrtownershipfinishv1*)(uintptr_t)1,
        "unknown identity is not dereferenced or treated as a leaf");end(&scope);
    xrtFutureWatchRemove(source,&watch);testRequire(!calls,"refusal has no notification");
    xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"memory ledger");setvbuf(stdout,NULL,_IONBF,0);
    for(unsigned bits=0;bits<64;++bits)cycle(bits,false);
    for(unsigned bits=2;bits<8;bits+=4){cycle(bits,true);cycle(bits|1,true);}
    cycle(64|2,false);cycle(64|2|1|4|8|16|32,false);
    refusal();
    puts("TaskGroup retirement: 32 unrooted cycles and 32 rooted-Done retention cases, 4 aborts and a threaded tail; BUSY/READY; all pins and full ledger balanced");return 0;
}
