/* Mirrors the certified XRT retirement fixture, without an external repository dependency. */
#define XRT_MODULE_CANCEL
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#ifndef XL6_RETIREMENT_LINKED
#ifdef XL6_RETIREMENT_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#else
#define XRT_IMPLEMENTATION
#endif
#endif
#ifdef XL6_RETIREMENT_XRUNTIME
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
static void testRequire(bool condition, const char* message)
{ if(!condition){fprintf(stderr,"cancel retirement: %s\n",message);exit(1);} }
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



/* A worker's result is stored in its slot and inspected after join. */
typedef int (*testthreadproc)(ptr pData);



/* Cross-platform joinable test worker. */
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
/* Windows worker entry. */
static DWORD WINAPI testThreadEntry(LPVOID pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return (DWORD)pThread->Result;
}
#else
/* POSIX worker entry. */
static void* testThreadEntry(void* pData)
{
	testthread* pThread = (testthread*)pData;

	pThread->Result = pThread->Proc(pThread->Data);
	return NULL;
}
#endif



/* Start workers with their prepared callback and input. */
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



/* Join workers before inspecting their results. */
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



/* Yield the current worker's time slice. */
static inline void testThreadYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}

#endif

typedef struct events { unsigned calls, drops; } events;
typedef struct owner {
    volatile int32 refs;
    xcancel* cancel;
    xcancelwatch* watch;
    events* events;
    xatomic32 entered, proceed;
    bool block;
} owner;
static void begin(xrtownershipscope* scope) { testRequire(xrtOwnershipMutationBegin(scope),"owner mutation"); }
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope),"owner scope end"); }
static void frozen(xrtownershipscope* scope)
{ double deadline=xrtTimer()+5;while(!xrtOwnershipFreezeTryBegin(scope)){testRequire(xrtTimer()<deadline,"bounded freeze");testThreadYield();} }
static bool count_owner(const void* data,size_t* count)
{ const owner* value=data;if(!value || !count || value->refs<=0)return false;*count=(size_t)value->refs;return true; }
static bool trace_owner(const void* data,xrtownershipvisitor visit,ptr context)
{
    const owner* value=data;
    return visit && (!value->cancel || visit(xrtCancelOwnership(value->cancel),context)) &&
        (!value->watch || visit(xrtCancelWatchOwnership(value->watch),context));
}
static const xrtownershipops owner_ops={count_owner,trace_owner};
static void drop_owner(const void* data)
{
    owner* value=(owner*)data;xrtownershipscope scope={0},freeze={0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"Data Drop outside API mutation");end(&freeze);
    begin(&scope);
    if(!xrtRefRelease(&value->refs)){
        testRequire(!value->watch && !value->cancel,"owner slots detached before final Data Drop");
        ++value->events->drops;xrtFree(value);
    }
    end(&scope);
}
static void notify_owner(ptr data)
{
    owner* value=data;++value->events->calls;
    xrtAtomic32Store(&value->entered,1,XMEMORY_RELEASE);
    if(value->block){double deadline=xrtTimer()+5;
        while(!xrtAtomic32Load(&value->proceed,XMEMORY_ACQUIRE)){
            testRequire(xrtTimer()<deadline,"bounded callback barrier");testThreadYield();
        }
    }
}
static const xcancelwatchownershipv1 policy={sizeof(policy),notify_owner,drop_owner,&owner_ops};
static owner* make(events* counters,xcancel* parent,bool owned)
{
    owner* value=xrtCalloc(1,sizeof(*value));testRequire(value!=NULL,"owner allocation");
    value->refs=1;value->events=counters;value->cancel=xrtCancelChild(parent);
    xrtAtomic32Init(&value->entered,0);xrtAtomic32Init(&value->proceed,0);
    testRequire(value->cancel!=NULL,"child token");
    value->watch=owned?xrtCancelWatchOwnedV1(value->cancel,value,&policy):xrtCancelWatch(value->cancel,notify_owner,value);
    testRequire(value->watch!=NULL,"registration");return value;
}
static void detach(owner* value)
{
    xrtownershipscope scope={0};xcancelwatch* watch;xcancel* cancel;
    begin(&scope);watch=value->watch;cancel=value->cancel;value->watch=NULL;value->cancel=NULL;end(&scope);
    xrtCancelUnwatch(watch);xrtCancelDestroy(cancel);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount,"complete retirement release ledger balanced");
}
static void cycle(bool watch_first,bool finish_first,bool request,bool restore)
{
    events counters={0};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xcancel* parent=xrtCancelCreate();testRequire(parent!=NULL,"external parent");owner* value=make(&counters,parent,true);
    xrtownershipref refs[2];refs[0]=xrtCancelWatchOwnership(value->watch);refs[1]=xrtCancelOwnership(value->cancel);
    const xcancelwatchownershipv1* policies[1]={&policy};
    const xcancelwatchownershipv1* unknown[1]={(const xcancelwatchownershipv1*)(uintptr_t)1};
    const xrtownershipadapterv1* adapters[2];const xrtownershipfinishv1* finishes[2];
    xrtownershipscope freeze={0};xrtownershipresult graph={0};unsigned token=0,other=1;frozen(&freeze);
    testRequire(xrtOwnershipInspect(refs,1,NULL,0,&graph) && graph.NodeCount==4 && graph.EdgeCount==5 &&
        graph.ExternalRootCount==1,"four physical nodes, five real edges, only the external parent rooted");
    for(unsigned i=0;i<2;++i){
        finishes[i]=(const xrtownershipfinishv1*)(uintptr_t)1;
        testRequire(!(i?xrtCancelOwnershipAdapterV3(refs[i],unknown,1,&finishes[i]):
            xrtCancelWatchOwnershipAdapterV2(refs[i],unknown,1,&finishes[i])) &&
            finishes[i]==(const xrtownershipfinishv1*)(uintptr_t)1,"unknown policy never changes output");
        adapters[i]=i?xrtCancelOwnershipAdapterV3(refs[i],policies,1,&finishes[i]):
            xrtCancelWatchOwnershipAdapterV2(refs[i],policies,1,&finishes[i]);
        testRequire(adapters[i] && finishes[i] && finishes[i]->Adapter==adapters[i] &&
            adapters[i]->Hold(refs[i].Data) && adapters[i]->Claim(refs[i].Data,&token),"exact adapter/finish, actual pin and claim");
        testRequire(!adapters[i]->Claim(refs[i].Data,&other),"conflicting claimant refused");
    }
    testRequire(xrtRefRetain(&value->refs)==2,"actual Data plan pin");
    if(restore){for(unsigned i=0;i<2;++i)adapters[i]->Restore(refs[i].Data,&token);}
    else{unsigned first=watch_first?0:1;adapters[first]->Clear(refs[first].Data,&token);adapters[1-first]->Clear(refs[1-first].Data,&token);}
    end(&freeze);
    if(restore){
        testRequire(xrtCancelRequest(parent) && counters.calls==1,"abort leaves original notification armed");
    }else{
        testRequire(xrtMemDebugFailAfter(0),"arm BUSY allocation check");
        testRequire(finishes[0]->Step(refs[0].Data,&token)==XRT_OWNERSHIP_FINISH_BUSY &&
            finishes[1]->Step(refs[1].Data,&token)==XRT_OWNERSHIP_FINISH_BUSY &&
            !xrtMemDebugFailTriggered() && !counters.calls && !counters.drops && value->watch && value->cancel,
            "BUSY neither allocates, drops, notifies nor consumes registration");
        xrtMemDebugFailClear();
        if(request)testRequire(xrtCancelRequest(parent) && !counters.calls,"external ancestor cannot notify a cleared Watch");
    }
    detach(value); /* Only the actual owner consumes the registration right. */
    if(!restore){unsigned first=finish_first?0:1;
        testRequire(finishes[first]->Step(refs[first].Data,&token)==XRT_OWNERSHIP_FINISH_READY &&
            finishes[1-first]->Step(refs[1-first].Data,&token)==XRT_OWNERSHIP_FINISH_READY,"both physical Finish orders settle");
        for(unsigned i=0;i<2;++i)testRequire(finishes[i]->Step(refs[i].Data,&token)==XRT_OWNERSHIP_FINISH_READY,"repeated Finish is idempotent");
    }
    for(unsigned i=0;i<2;++i)adapters[i]->Drop(refs[i].Data);
    testRequire(!counters.drops,"Data still protected by its actual plan pin");drop_owner(value);
    testRequire(counters.drops==1,"one actual terminal Data Drop");xrtCancelDestroy(parent);balanced(&before);
}
static void legacy(void)
{
    events counters={0};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    owner* value=make(&counters,NULL,false);const xcancelwatchownershipv1* policies[1]={&policy};
    const xrtownershipfinishv1* finish=(const xrtownershipfinishv1*)(uintptr_t)1;xrtownershipscope freeze={0};frozen(&freeze);
    testRequire(!xrtCancelWatchOwnershipAdapterV2(xrtCancelWatchOwnership(value->watch),policies,1,&finish) &&
        !xrtCancelOwnershipAdapterV3(xrtCancelOwnership(value->cancel),policies,1,&finish) &&
        finish==(const xrtownershipfinishv1*)(uintptr_t)1,"legacy observers remain unadmitted");
    end(&freeze);detach(value);drop_owner(value);balanced(&before);
}
static int request_parent(ptr data) { testRequire(xrtCancelRequest(data),"thread parent request");return 0; }
static void active(void)
{
    events counters={0};xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xcancel* parent=xrtCancelCreate();
    owner* value=make(&counters,parent,true);value->block=true;testthread thread={0};
    thread.Proc=request_parent;thread.Data=parent;testThreadsStart(&thread,1);double deadline=xrtTimer()+5;
    while(!xrtAtomic32Load(&value->entered,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<deadline,"callback entered");testThreadYield();}
    const xcancelwatchownershipv1* policies[1]={&policy};const xrtownershipfinishv1* finish=(const xrtownershipfinishv1*)(uintptr_t)1;
    xrtownershipscope freeze={0};frozen(&freeze);
    testRequire(!xrtCancelWatchOwnershipAdapterV2(xrtCancelWatchOwnership(value->watch),policies,1,&finish) &&
        !xrtCancelOwnershipAdapterV3(xrtCancelOwnership(value->cancel),policies,1,&finish) &&
        finish==(const xrtownershipfinishv1*)(uintptr_t)1,"active callbacks/requests rejected atomically");
    end(&freeze);xrtAtomic32Store(&value->proceed,1,XMEMORY_RELEASE);testThreadsJoin(&thread,1);
    testRequire(thread.Result==0,"join actual request");
    testRequire(counters.calls==1,"one real callback");detach(value);testRequire(counters.drops==1,"ordinary post-callback Drop");
    xrtCancelDestroy(parent);balanced(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"memory ledger");setvbuf(stdout,NULL,_IONBF,0);
    for(unsigned bits=0;bits<16;++bits)cycle((bits&1)!=0,(bits&2)!=0,(bits&4)!=0,(bits&8)!=0);
    legacy();active();puts("cancel retirement: 16 lifecycle orders; physical 4/5 graph; no early Unwatch; post-Clear suppression; BUSY/READY and full ledger balanced");return 0;
}
