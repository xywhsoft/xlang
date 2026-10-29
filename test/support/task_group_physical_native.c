/* Exact physical TaskGroup graph; no collector admission is implied. */
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#ifndef XL6_TASK_GROUP_GRAPH_LINKED
#ifdef XL6_TASK_GROUP_GRAPH_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#else
#define XRT_IMPLEMENTATION
#endif
#endif
#ifdef XL6_TASK_GROUP_GRAPH_XRUNTIME
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
static void testRequire(bool condition,const char* message)
{ if(!condition){fprintf(stderr,"TaskGroup graph: %s\n",message);exit(1);} }
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

static void freeze(xrtownershipscope* scope)
{ double until=xrtTimer()+5;while(!xrtOwnershipFreezeTryBegin(scope)){testRequire(xrtTimer()<until,"bounded freeze");testThreadYield();} }
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope),"scope end"); }
static void graph(xrtownershipref root,size_t nodes,size_t edges,size_t roots,size_t physical)
{
    xrtownershipscope scope={0};xrtownershipresult result={0};size_t count=0;freeze(&scope);
    testRequire(root.Ops->Count(root.Data,&count) && count==physical,"actual root reference count");
    testRequire(xrtOwnershipInspect(&root,1,NULL,0,&result) && result.NodeCount==nodes && result.EdgeCount==edges &&
        result.ExternalRootCount==roots,"exact physical nodes, duplicate slots and external roots");end(&scope);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount,"full graph allocation/release ledger balanced");
}
static void ordinary(bool destroy_pending)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xtaskgroup* group=xrtTaskGroupCreate(NULL);testRequire(group!=NULL,"group");
    graph(xrtTaskGroupOwnership(group),5,8,1,3);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);testRequire(promise && source,"source");
    testRequire(xrtTaskGroupAdd(group,source),"source admission");graph(xrtTaskGroupOwnership(group),8,13,2,4);
    const xfuturewatchownershipv1* watches[1]={xrtTaskGroupWatchPolicyV1Get()};const xrtownershippreparationv1* preparation=NULL;
    xrtownershipscope scope={0};freeze(&scope);
    testRequire(xrtFutureOwnershipAdapterV3(xrtFutureOwnership(source),NULL,0,NULL,0,watches,1,&preparation) && preparation,
        "source registration has exact immutable direct-owner policy");end(&scope);
    if(destroy_pending){
        xrtTaskGroupDestroy(group); /* Only the public owner's Watch right is consumed. */
        graph(xrtFutureOwnership(source),7,10,1,3);
    }
    testRequire(xrtPromiseResolve(promise,NULL),"source completion");
    if(!destroy_pending){
        graph(xrtTaskGroupOwnership(group),5,8,1,3);testRequire(xrtTaskGroupClose(group),"close");
        graph(xrtTaskGroupOwnership(group),5,6,1,2);xrtTaskGroupDestroy(group);
    }
    xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
}
static void child(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xtaskgroup* parent=xrtTaskGroupCreate(NULL);
    xtaskgroup* nested=xrtTaskGroupChild(parent,NULL);testRequire(parent && nested,"nested groups");
    graph(xrtTaskGroupOwnership(parent),11,21,2,4);
    testRequire(xrtTaskGroupClose(parent),"parent close propagates outside mutation");
    graph(xrtTaskGroupOwnership(parent),5,6,1,2);graph(xrtTaskGroupOwnership(nested),5,6,1,2);
    xrtTaskGroupDestroy(nested);xrtTaskGroupDestroy(parent);balanced(&before);
}
static void producer(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xtaskgroup* group=xrtTaskGroupCreate(NULL);
    testRequire(group!=NULL,"producer group");xfuture* done=xrtTaskGroupFuture(group);testRequire(done!=NULL,"Done observer");
    const xfutureproducerownershipv1* policies[]={xrtTaskGroupProducerPolicyV1Get()};
    const xfutureproducerownershipv1* unknown[]={(const xfutureproducerownershipv1*)(uintptr_t)1};
    xfutureownershipadmissionv1 admission={0};admission.size=sizeof(admission);
    admission.ProducerPolicies=unknown;admission.ProducerPolicyCount=1;
    const xrtownershippreparationv1* preparation=(const xrtownershippreparationv1*)(uintptr_t)1;
    xrtownershipscope scope={0};unsigned token=0;freeze(&scope);
    testRequire(!xrtFutureOwnershipAdapterV4(xrtFutureOwnership(done),&admission,&preparation) &&
        preparation==(const xrtownershippreparationv1*)(uintptr_t)1,"unknown producer refused without dereference or output change");
    admission.ProducerPolicies=policies;
    const xrtownershipadapterv1* adapter=xrtFutureOwnershipAdapterV4(xrtFutureOwnership(done),&admission,&preparation);
    testRequire(adapter && preparation && !preparation->Ready(done) && adapter->Hold(done) && adapter->Claim(done,&token),
        "pending Done is genuinely produced even without user waiters");end(&scope);
    testRequire(preparation->Prepare(done,&token)==XRT_OWNERSHIP_PREPARE_BUSY && xrtFutureState(done)==XFUTURE_PENDING,
        "preparation cannot manufacture a closed result for an active producer");
    freeze(&scope);adapter->Restore(done,&token);end(&scope);adapter->Drop(done);
    testRequire(xrtTaskGroupClose(group) && xrtFutureState(done)==XFUTURE_RESOLVED,"normal completion retires producer before notification");
    graph(xrtTaskGroupOwnership(group),5,6,2,2);
    xrtFutureDestroy(done);xrtTaskGroupDestroy(group);balanced(&before);
}
static void construction(void)
{
    bool succeeded=false;
    for(unsigned fail=0;fail<32 && !succeeded;++fail){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);testRequire(xrtMemDebugFailAfter(fail),"constructor allocation sweep");
        xtaskgroup* group=xrtTaskGroupCreate(NULL);succeeded=group!=NULL;
        if(!group)testRequire(xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError())==XERR_MEMORY,"constructor preserves allocation failure");
        xrtMemDebugFailClear();xrtClearError();xrtTaskGroupDestroy(group);balanced(&before);
    }
    testRequire(succeeded,"all constructor failure points and successful producer transfer covered");
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xcancel* parent=xrtCancelCreate();
    testRequire(parent && xrtCancelRequest(parent),"already cancelled parent");xtaskgroupconfig config={0};config.Cancel=parent;
    xtaskgroup* group=xrtTaskGroupCreate(&config);testRequire(group!=NULL,"synchronously cancelled construction");
    xfuture* done=xrtTaskGroupFuture(group);testRequire(done && xrtFutureState(done)==XFUTURE_RESOLVED,"synchronous callback retires producer once");
    xrtFutureDestroy(done);xrtTaskGroupDestroy(group);xrtCancelDestroy(parent);balanced(&before);
}
typedef struct barrier {
	volatile int32 refs;
    xtaskgroup* group;
    xfuture* source;
    xpromise* promise;
    xatomic32 entered,proceed;
} barrier;
static void wait_entered(barrier* value)
{ double until=xrtTimer()+5;while(!xrtAtomic32Load(&value->entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"entered");testThreadYield();} }
static void hold(barrier* value)
{
    xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);double until=xrtTimer()+5;
    while(!xrtAtomic32Load(&value->proceed,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<until,"callback release");testThreadYield();}
}
static void refused(xtaskgroup* group)
{
    xrtownershipscope scope={0};size_t count=123;freeze(&scope);xrtownershipref ref=xrtTaskGroupOwnership(group);
    testRequire(!ref.Ops->Count(ref.Data,&count) && count==123,"busy group refuses without a synthetic count");end(&scope);
}
static xfuture* start(ptr data)
{ barrier* value=data;hold(value);value->promise=xrtPromiseCreate(&value->source,NULL);return value->source; }
static int run_start(ptr data)
{ barrier* value=data;testRequire(xrtTaskGroupStart(value->group,start,value)==value->source,"reserved start");return 0; }
static void reservation(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);barrier value={0};value.group=xrtTaskGroupCreate(NULL);
    testRequire(value.group!=NULL,"reservation group");testthread thread={0};thread.Proc=run_start;thread.Data=&value;
    testThreadsStart(&thread,1);wait_entered(&value);refused(value.group);
    xrtAtomic32Store(&value.proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);testRequire(thread.Result==0,"start joined");
    graph(xrtTaskGroupOwnership(value.group),8,13,2,4);testRequire(xrtPromiseResolve(value.promise,NULL),"resolve started source");
    xrtTaskGroupDestroy(value.group);xrtPromiseDestroy(value.promise);xrtFutureDestroy(value.source);balanced(&before);
}
/* The test owns the stack storage; each callback transfers a real separate
 * context right. Its group/source slots are borrowed test handles, not edges. */
static bool barrier_count(const void* data,size_t* count)
{ const barrier* value=data;if(!value || !count || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool barrier_trace(const void* data,xrtownershipvisitor visit,ptr context) { (void)context;return data && visit; }
static const xrtownershipops barrier_ops={barrier_count,barrier_trace};
static void blocked(ptr data) { hold(data); }
static void borrowed_test_drop(const void* data)
{
    xrtownershipscope scope={0};testRequire(xrtOwnershipMutationBegin(&scope),"callback context mutation");
    testRequire(xrtRefRelease(&((barrier*)data)->refs)==1,"actual callback context right returned");end(&scope);
}
static const xcancelwatchownershipv1 barrier_policy={sizeof(barrier_policy),blocked,borrowed_test_drop,&barrier_ops};
static int run_cancel(ptr data) { testRequire(xrtTaskGroupCancel(((barrier*)data)->group),"group cancel");return 0; }
static void callback(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);barrier value={0};value.group=xrtTaskGroupCreate(NULL);
    value.promise=xrtPromiseCreate(&value.source,NULL);testRequire(value.group && value.promise && value.source,"callback setup");
    testRequire(xrtTaskGroupAdd(value.group,value.source),"callback source");xcancel* token=xrtFutureCancelToken(value.source);
    value.refs=2;xcancelwatch* watch=xrtCancelWatchOwnedV1(token,&value,&barrier_policy);testRequire(token && watch,"owned blocking observer");
    testthread thread={0};thread.Proc=run_cancel;thread.Data=&value;testThreadsStart(&thread,1);wait_entered(&value);refused(value.group);
    xrtAtomic32Store(&value.proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);testRequire(thread.Result==0,"cancel joined");
    xrtCancelUnwatch(watch);testRequire(value.refs==1 && xrtRefRelease(&value.refs)==0,"caller releases actual test context right");
    xrtCancelDestroy(token);testRequire(xrtPromiseCancel(value.promise),"terminal source");
    xrtTaskGroupDestroy(value.group);xrtPromiseDestroy(value.promise);xrtFutureDestroy(value.source);balanced(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"debug ledger");setvbuf(stdout,NULL,_IONBF,0);
    ordinary(false);ordinary(true);child();producer();construction();reservation();callback();
    puts("TaskGroup physical graph: 5/8, 8/13 and nested 11/21; Done producer, list and callbacks own real refs; active freeze refusal; full ledger balanced");return 0;
}
