#ifndef XLIR_LIFETIME_PLATFORM_H
#define XLIR_LIFETIME_PLATFORM_H
/* Test-only native threads and callback-library mapping observations. No
 * runtime API is replaced. Join precedes retirement; only the actual loader
 * closes a library, and physical code-page unmapping is asserted separately. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
typedef volatile LONG XLIR_TestFlag;
typedef HANDLE XLIR_TestThread;
typedef HMODULE XLIR_TestLibrary;
#define XLIR_TEST_THREAD_RESULT DWORD
#define XLIR_TEST_THREAD_CALL WINAPI
#define XLIR_TEST_EXPORT __declspec(dllexport)
static uint64_t xlirTestMilliseconds(void) { return GetTickCount64(); }
static void xlirTestYield(void) { SwitchToThread(); }
static void xlirTestSleep(unsigned milliseconds) { Sleep(milliseconds); }
static int xlirTestFlagGet(XLIR_TestFlag* flag) { return InterlockedCompareExchange(flag,0,0); }
static void xlirTestFlagSet(XLIR_TestFlag* flag,int value) { InterlockedExchange(flag,value); }
static void xlirTestFlagIncrement(XLIR_TestFlag* flag) { InterlockedIncrement(flag); }
static bool xlirTestThreadStart(XLIR_TestThread* thread,DWORD (WINAPI *run)(void*),void* context)
{ *thread=CreateThread(NULL,0,run,context,0,NULL); return *thread!=NULL; }
static void xlirTestThreadJoin(XLIR_TestThread thread)
{ assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0 && CloseHandle(thread)); }
static XLIR_TestLibrary xlirTestLibraryOpen(const char* path) { return LoadLibraryA(path); }
static void* xlirTestLibrarySymbol(XLIR_TestLibrary library,const char* name)
{ return (void*)GetProcAddress(library,name); }
static void xlirTestLibraryCloseUnmapped(XLIR_TestLibrary library,void* address)
{
    MEMORY_BASIC_INFORMATION memory;
    assert(FreeLibrary(library));
    assert(VirtualQuery(address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
}
#else
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
typedef _Atomic int XLIR_TestFlag;
typedef pthread_t XLIR_TestThread;
typedef void* XLIR_TestLibrary;
#define XLIR_TEST_THREAD_RESULT void*
#define XLIR_TEST_THREAD_CALL
#define XLIR_TEST_EXPORT __attribute__((visibility("default")))
static uint64_t xlirTestMilliseconds(void)
{
    struct timespec now; assert(clock_gettime(CLOCK_MONOTONIC,&now)==0);
    return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
}
static void xlirTestYield(void) { sched_yield(); }
static void xlirTestSleep(unsigned milliseconds)
{
    struct timespec duration={(time_t)(milliseconds/1000u),(long)(milliseconds%1000u)*1000000L};
    while(nanosleep(&duration,&duration)!=0)assert(errno==EINTR);
}
static int xlirTestFlagGet(XLIR_TestFlag* flag) { return atomic_load_explicit(flag,memory_order_seq_cst); }
static void xlirTestFlagSet(XLIR_TestFlag* flag,int value) { atomic_store_explicit(flag,value,memory_order_seq_cst); }
static void xlirTestFlagIncrement(XLIR_TestFlag* flag) { atomic_fetch_add_explicit(flag,1,memory_order_seq_cst); }
static bool xlirTestThreadStart(XLIR_TestThread* thread,void* (*run)(void*),void* context)
{ return pthread_create(thread,NULL,run,context)==0; }
static void xlirTestThreadJoin(XLIR_TestThread thread)
{
    struct timespec stop; assert(clock_gettime(CLOCK_REALTIME,&stop)==0); stop.tv_sec+=10;
    assert(pthread_timedjoin_np(thread,NULL,&stop)==0);
}
static XLIR_TestLibrary xlirTestLibraryOpen(const char* path) { return dlopen(path,RTLD_NOW|RTLD_LOCAL); }
static void* xlirTestLibrarySymbol(XLIR_TestLibrary library,const char* name) { return dlsym(library,name); }
static void xlirTestLibraryCloseUnmapped(XLIR_TestLibrary library,void* address)
{
    long size=sysconf(_SC_PAGESIZE); unsigned char residency; Dl_info information;
    assert(size>0 && address && dlclose(library)==0);
    uintptr_t page=(uintptr_t)address-(uintptr_t)address%(uintptr_t)size;
    errno=0;
    /* dladdr alone only observes loader registration. mincore proves that the
     * callback's actual page is absent, before any subsequent allocation. */
    assert(mincore((void*)page,(size_t)size,&residency)==-1 && errno==ENOMEM);
    assert(dladdr(address,&information)==0);
}
#endif
#endif
