#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

/* Raw native threads deliberately bypass the language executor. Each publisher
 * owns the whole group until its last typed thunk has returned. */
static xatomic32 pins, acquisitions, closed, ready, proceed, calls;
static int code_acquire(void* unused)
{
    (void)unused;
    if(xrtAtomic32Load(&closed,XMEMORY_ACQUIRE))return 0;
    xrtAtomic32FetchAdd(&acquisitions,1,XMEMORY_RELAXED);
    xrtAtomic32FetchAdd(&pins,1,XMEMORY_RELAXED);return 1;
}
static void code_release(void* unused)
{(void)unused;assert(xrtAtomic32FetchSub(&pins,1,XMEMORY_ACQ_REL)>0);}
static void invoke(void)
{xrtAtomic32FetchAdd(&calls,1,XMEMORY_RELAXED);}
static void yield_thread(void)
{
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}
typedef struct Publisher {XL6_NativeCallbackGroupV1* group;size_t member;} Publisher;
static void typed_thunk(Publisher* publisher)
{
    const XL6_NativeCallbackGroupV1* group=publisher->group;
    assert(group && group->Members && group->Count==2);
    XL6_NativeCallbackScopeV1 scope={0};
    const XL6_NativeCallbackRuntimeV1* api=xl6NativeCallbackRuntimeV1Get();
    assert(api->Enter(group->Members[publisher->member],41+publisher->member,&scope,NULL,0));
    scope.closure.closure->invoke();
    xl6_error_result result=api->Leave(&scope,(xl6_error_result){true,NULL});
    assert(result.ok && !result.error && !xrtGetError());
}
#ifdef _WIN32
static DWORD WINAPI publish(void* raw)
#else
static void* publish(void* raw)
#endif
{
    Publisher* publisher=raw;
    xrtAtomic32FetchAdd(&ready,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&proceed,XMEMORY_ACQUIRE))yield_thread();
    for(unsigned i=0;i<1000;++i)typed_thunk(publisher);
    assert(xl6NativeCallbackGroupRuntimeV1Get()->DropNative(publisher->group));
    return 0;
}
int main(void)
{
    xrtAtomic32Init(&pins,0);xrtAtomic32Init(&acquisitions,0);xrtAtomic32Init(&closed,0);
    xrtAtomic32Init(&ready,0);xrtAtomic32Init(&proceed,0);xrtAtomic32Init(&calls,0);
    XL6_CodeOwner owner={sizeof(owner),NULL,code_acquire,code_release};
    const XL6_ClosureRuntimeV1* closures=xl6ClosureRuntimeV1Get();
    XL6_Closure* function=xl6ClosureRuntimeV2Get()->Create(&owner,invoke,NULL,NULL,NULL,NULL,NULL,0);
    assert(function);
    XL6_Closure* members[]={function,function};uint64_t signatures[]={41,42};
    const XL6_NativeCallbackGroupRuntimeV1* api=xl6NativeCallbackGroupRuntimeV1Get();
    XL6_NativeCallbackGroupV1* group=api->Create(&owner,members,NULL,signatures,2);assert(group);
    assert(xrtAtomic32Load(&acquisitions,XMEMORY_RELAXED)==2);
    Publisher publishers[4];
#ifdef _WIN32
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    for(unsigned i=0;i<4;++i){
        assert(api->HoldNative(group));publishers[i]=(Publisher){group,i%2};
#ifdef _WIN32
        threads[i]=CreateThread(NULL,0,publish,&publishers[i],0,NULL);assert(threads[i]);
#else
        assert(!pthread_create(&threads[i],NULL,publish,&publishers[i]));
#endif
    }
    while(xrtAtomic32Load(&ready,XMEMORY_ACQUIRE)!=4)yield_thread();
    /* Remove all caller roots and reject NEW code admissions. The accepted
     * shared pin must still protect every callback, including its return tail. */
    xrtAtomic32Store(&closed,1,XMEMORY_RELEASE);
    closures->Release(function);assert(api->Release(group));
    assert(xrtAtomic32Load(&pins,XMEMORY_RELAXED)==2);
    xrtAtomic32Store(&proceed,1,XMEMORY_RELEASE);
    for(unsigned i=0;i<4;++i){
#ifdef _WIN32
        assert(WaitForSingleObject(threads[i],60000)==WAIT_OBJECT_0);assert(CloseHandle(threads[i]));
#else
        assert(!pthread_join(threads[i],NULL));
#endif
    }
    assert(xrtAtomic32Load(&calls,XMEMORY_RELAXED)==4000);
    assert(!xrtAtomic32Load(&pins,XMEMORY_RELAXED) && !xrtGetError());
    puts("GROUP_THREADS_OK threads=4 calls=4000 callerRoots=0 codePins=0");return 0;
}
